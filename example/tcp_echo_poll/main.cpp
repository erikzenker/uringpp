#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uringpp/uringpp.h>

#include <array>
#include <string>
#include <map>

enum class CompletionType : std::uint8_t { Accept = 0, Recv = 1, Send = 2, Poll = 3 };

struct Data {
    Data(CompletionType type, int fd = 0, std::uint8_t* buffer = nullptr)
        : type(type)
        , buffer(buffer)
        , fd(fd)
    {
    }

    CompletionType type;
    int fd;
    std::uint8_t* buffer;
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
    while(ring.capacity()){
        auto data = std::make_shared<Data>(CompletionType::Accept);
        datas.emplace(data.get(), data);
        ring.prepare_accept(listenFd, nullptr, nullptr, data);
    }
}

void recv(uringpp::Ring& ring, std::map<Data*, std::shared_ptr<Data>>& datas, int fd, std::vector<std::vector<std::uint8_t>>& buffers)
{
    buffers.emplace_back(1024);
    auto data = std::make_shared<Data>(CompletionType::Recv, fd, buffers.back().data());
    datas.emplace(data.get(), data);
    ring.prepare_recv(fd, buffers.back(), data);
}

void send(uringpp::Ring& ring, std::map<Data*, std::shared_ptr<Data>>& datas, int fd, std::vector<std::uint8_t>& buffer)
{
    auto data = std::make_shared<Data>(CompletionType::Send, fd, buffer.data());
    datas.emplace(data.get(), data);
    ring.prepare_send(fd, buffer, data);
}

void poll(uringpp::Ring& ring, std::map<Data*, std::shared_ptr<Data>>& datas,int fd)
{
    auto data = std::make_shared<Data>(CompletionType::Poll, fd);
    datas.emplace(data.get(), data);
    ring.prepare_poll_add(fd, data);
}

auto echo(uringpp::Ring& ring, int listenFd) -> auto
{
    std::vector<std::vector<std::uint8_t>> buffers;
    std::map<Data*, std::shared_ptr<Data>> datas;

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
                    std::string("failed to read from socket ") + strerror(-completion.result()));
            }

            std::cout << "* Received[" << completion.userData()->fd
                      << "]: " << completion.userData()->buffer << std::endl;
            buffers.emplace_back(completion.result());
            std::copy(
                completion.userData()->buffer,
                completion.userData()->buffer + completion.result(),
                buffers.back().begin());

            send(ring, datas, completion.userData()->fd, buffers.back());
            break;
        }

        case CompletionType::Send: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to write to socket") + strerror(-completion.result()));
            }

            std::cout << "* Send[" << completion.userData()->fd
                      << "]: " << completion.userData()->buffer << std::endl;
            recv(ring, datas, completion.userData()->fd, buffers);
            break;
        }

        case CompletionType::Poll: {
            if (completion.result() < 0) {
                throw std::runtime_error(
                    std::string("failed to poll ") + strerror(-completion.result()));
            }

            std::cout << "* Poll[" << completion.userData()->fd << "] " << completion.result()
                      << std::endl;
            recv(ring, datas, completion.userData()->fd, buffers);
            poll(ring, datas, completion.userData()->fd);
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
    uringpp::Ring ring { queueSize };

    std::cout << "Tcp echo server started. Listening on port " << port << "." << std::endl;

    echo(ring, listen(port));

    return 0;
}