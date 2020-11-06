#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uringpp/uringpp.h>

#include <array>
#include <map>
#include <string>

enum class CompletionType : std::uint8_t {
    Accept = 0,
    Recv = 1,
    Send = 2,
    Poll = 3,
    CreateBufferPool = 4,
    ReaddBufferPool = 5
};

struct Data {
    Data(CompletionType type, int fd = 0, std::size_t bufferIdx = 0)
        : type(type)
        , bufferIdx(bufferIdx)
        , fd(fd)
    {
    }

    CompletionType type;
    int fd;
    std::size_t bufferIdx;
};

int listen(std::uint16_t port)
{
    int listenFd;
    struct sockaddr_in address;
    int opt = 1;

    if ((listenFd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        throw std::runtime_error("failed create socket");
    }

    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        throw std::runtime_error("failed to setsocketopt");
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(listenFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("failed to bind");
    }

    if (listen(listenFd, 3) < 0) {
        throw std::runtime_error("failed to listen");
    }

    return listenFd;
}

void accept(uringpp::Ring& ring, std::map<Data*, std::shared_ptr<Data>>& datas, int listenFd)
{
    while (ring.capacity()) {
        auto data = std::make_shared<Data>(CompletionType::Accept);
        datas.emplace(data.get(), data);
        ring.prepare_accept(listenFd, nullptr, nullptr, data);
    }
}

void recv(
    uringpp::Ring& ring,
    std::map<Data*, std::shared_ptr<Data>>& datas,
    int fd,
    BufferPool& bufferPool)
{
    auto data = std::make_shared<Data>(CompletionType::Recv, fd);
    datas.emplace(data.get(), data);
    ring.prepare_recv_bp(fd, bufferPool, data);
}

void send(
    uringpp::Ring& ring,
    std::map<Data*, std::shared_ptr<Data>>& datas,
    int fd,
    BufferPool& bufferPool,
    std::size_t bufferIdx)
{
    auto data = std::make_shared<Data>(CompletionType::Send, fd, bufferIdx);
    datas.emplace(data.get(), data);
    ring.prepare_send_bp(fd, bufferPool.at(bufferIdx), data);
}

void poll(uringpp::Ring& ring, std::map<Data*, std::shared_ptr<Data>>& datas, int fd)
{
    auto data = std::make_shared<Data>(CompletionType::Poll, fd);
    datas.emplace(data.get(), data);
    ring.prepare_poll_add(fd, data);
}

auto createBufferPool(
    uringpp::Ring& ring,
    std::map<Data*, std::shared_ptr<Data>>& datas,
    std::size_t numberOfBuffers,
    std::size_t sizePerBuffer) -> BufferPool
{
    auto data = std::make_shared<Data>(CompletionType::CreateBufferPool);
    datas.emplace(data.get(), data);
    return ring.prepare_create_buffer_pool(numberOfBuffers, sizePerBuffer, data);
}

void readdBufferPool(
    uringpp::Ring& ring,
    std::map<Data*, std::shared_ptr<Data>>& datas,
    BufferPool& bufferPool,
    std::size_t bufferIdx)
{
    auto data = std::make_shared<Data>(CompletionType::ReaddBufferPool);
    data->bufferIdx = bufferIdx;
    datas.emplace(data.get(), data);
    ring.prepare_readd_buffer(bufferPool, bufferIdx, data);
}

auto echo(uringpp::Ring& ring, std::size_t bufferPoolSize, int listenFd) -> auto
{
    std::map<Data*, std::shared_ptr<Data>> datas;
    auto bufferPool = createBufferPool(ring, datas, bufferPoolSize, 1024);

    accept(ring, datas, listenFd);
    ring.submit();

    while (true) {
        auto completion = ring.wait<Data>();

        switch (completion.userData()->type) {
        case CompletionType::Accept: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to accept ") + strerror(-completion.result()));
            }

            auto acceptedSocketFd = completion.result();

            std::cout << "* Accepted[" << acceptedSocketFd << "]" << std::endl;

            poll(ring, datas, acceptedSocketFd);
            accept(ring, datas, listenFd);
            break;
        }

        case CompletionType::Recv: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to recv from socket ") + strerror(-completion.result()));
            }

            int bufferIdx = completion.get()->flags >> 16;

            std::cout << "* Received[" << completion.userData()->fd
                      << "]: " << bufferPool.at(completion.userData()->bufferIdx).data()
                      << std::endl;

            send(ring, datas, completion.userData()->fd, bufferPool, bufferIdx);
            break;
        }

        case CompletionType::Send: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to send to socket") + strerror(-completion.result()));
            }

            std::cout << "* Send[" << completion.userData()->fd
                      << "]: " << bufferPool.at(completion.userData()->bufferIdx).data()
                      << std::endl;
            readdBufferPool(ring, datas, bufferPool, completion.userData()->bufferIdx);
            break;
        }

        case CompletionType::Poll: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to poll ") + strerror(-completion.result()));
            }

            std::cout << "* Poll[" << completion.userData()->fd << "] " << completion.result()
                      << std::endl;
            recv(ring, datas, completion.userData()->fd, bufferPool);
            poll(ring, datas, completion.userData()->fd);
            break;
        }

        case CompletionType::CreateBufferPool: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to create buffer pool ") + strerror(-completion.result()));
            }

            std::cout << "* Create buffer pool" << std::endl;
            break;
        }

        case CompletionType::ReaddBufferPool: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to add buffer pool ") + strerror(-completion.result()));
            }

            std::cout << "* Readded buffer pool " << completion.userData()->bufferIdx << std::endl;
            break;
        }
        }
        datas.erase(completion.userData());
        ring.seen(completion);
        ring.submit();
    }
}

int main(int argc, char const* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: tcp_echo <PORT>" << std::endl;
        return 1;
    }

    const auto port = std::stoi(argv[1]);
    const auto queueSize = 64;
    const auto bufferPoolSize = 2;
    uringpp::Ring ring { queueSize };

    std::cout << "Tcp echo server started. Listening on port " << port << "." << std::endl;

    echo(ring, bufferPoolSize, listen(port));

    return 0;
}