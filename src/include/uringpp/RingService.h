#pragma once

#include "uringpp/Ring.h"

#include "asyncly/executor/IStrand.h"
#include "asyncly/executor/Strand.h"
#include "asyncly/executor/ThreadPoolExecutorController.h"
#include "asyncly/future/Future.h"

#include <cppcoro/async_generator.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>

#include <future>
#include <list>
#include <memory>

namespace uringpp {

struct Request {
    Request(std::uint64_t id, cppcoro::single_consumer_event& event, bool stop = false)
        : id(id)
        , event(event)
        , stop(stop)
    {
    }
    std::uint64_t id;
    cppcoro::single_consumer_event& event;
    bool stop;
};

struct Request2 {
    Request2(std::uint64_t id, bool stop = false)
        : id(id)
        , stop(stop)
    {
    }
    std::uint64_t id;
    bool stop;
};

class RingService {
  private:
    Ring<Request> m_ring;
    Ring<Request2> m_ring2;
    std::unique_ptr<asyncly::ThreadPoolExecutorController> m_executorController;
    asyncly::IStrandPtr m_strand;
    asyncly::IStrandPtr m_strand2;
    std::map<Request*, std::shared_ptr<Request>> m_requests;
    std::uint64_t m_id = 0;

  public:
    RingService()
        : m_ring(64)
        , m_ring2(64)
        , m_executorController(asyncly::ThreadPoolExecutorController::create(2))
        , m_strand(asyncly::create_strand(m_executorController->get_executor()))
        , m_strand2(asyncly::create_strand(m_executorController->get_executor()))
    {
    }

    ~RingService()
    {
        m_executorController->finish();
    }

    auto ring() -> auto
    {
        return m_ring;
    }

    auto nop(bool stopFlag = false) -> cppcoro::task<>
    {
        cppcoro::single_consumer_event event;
        auto request = std::make_shared<Request>(m_id++, event, stopFlag);

        m_requests.insert({ request.get(), request });
        m_ring.prepare_nop(request);
        m_ring.submit();

        co_await event;
        co_return;
    }

    template <ContinuousMemory Container>
    auto read(int fd, Container& buffer, std::size_t offset) -> cppcoro::task<Container&>
    {
        cppcoro::single_consumer_event event;
        auto request = std::make_shared<Request>(m_id++, event);

        m_requests.insert({ request.get(), request });
        m_ring.prepare_readv(fd, buffer, offset, request);
        m_ring.submit();

        co_await event;
        co_return buffer;
    }

    auto read2(int fd, std::vector<std::size_t> splits)
        -> cppcoro::async_generator<std::vector<std::uint8_t>>
    {
        std::size_t buffersRead = 0;
        std::vector<std::shared_ptr<Request2>> requests;
        std::vector<std::vector<std::uint8_t>> buffers;
        for (const auto& split : splits) {
            buffers.push_back(std::vector<std::uint8_t>(split, 0));
        }

        std::vector<std::uint8_t> buffer(1024); // buffers.at(0);

        std::cout << "buffers " << buffers.size() << std::endl;

        for (std::size_t i = 0; i < buffers.size() || buffersRead < buffers.size();) {
            std::cout << "i " << i << " buffersRead " << buffersRead << std::endl;
            if (m_ring2.capacity() && i < buffers.size()) {
                std::cout << "read " << fd << " " << i << " " << buffers.at(i).size() << std::endl;
                requests.push_back(std::make_shared<Request2>(i));
                // m_ring2.prepare_readv(fd, buffers.at(i), 0, requests.back());
                m_ring2.prepare_readv(fd, buffer, 0, requests.back());
                i++;
            } else {
                if (m_ring2.preparedQueueEntries()) {
                    m_ring2.submit();
                }
                while (m_ring2.submittedQueueEntries()) {
                    std::cout << "wait" << std::endl;
                    auto completion = co_await coro_wait();

                    if (completion.result() < 0) {
                        throw std::runtime_error(
                            std::string("failed to read from file: ")
                            + strerror(-completion.result()));
                    }

                    std::cout << "yield " << completion.userData()->id << " " << completion.result()
                              << " " << buffers.at(completion.userData()->id).size() << std::endl;
                    // co_yield buffers.at(completion.userData()->id);
                    co_yield buffer;
                    buffersRead++;
                    m_ring2.seen(completion);
                }
            }
        }
    }

    template <ContinuousMemory Container>
    auto write(int fd, Container& buffer, std::size_t offset) -> cppcoro::task<Container&>
    {
        cppcoro::single_consumer_event event;
        auto request = std::make_shared<Request>(m_id++, event);

        m_requests.insert({ request.get(), request });
        m_ring.prepare_writev(fd, buffer, offset, request);
        m_ring.submit();

        co_await event;
        co_return buffer;
    }

    auto coro_wait() -> cppcoro::task<Completion<Request2>>
    {
        cppcoro::single_consumer_event event;
        std::promise<Completion<Request2>> promise;
        m_strand2->post([this, &event, &promise]() {
            promise.set_value(m_ring2.wait());
            event.set();
        });

        co_await event;
        co_return promise.get_future().get();
    }

    auto wait_once() -> bool
    {
        auto completion = m_ring.wait();
        completion.userData()->event.set();
        auto stop = completion.userData()->stop;
        m_requests.erase(completion.userData());
        m_ring.seen(completion);
        return stop;
    }

    //***************************************************************************
    // CONTROL THE LOOP
    //***************************************************************************
    auto run() -> void
    {
        m_strand->post([this]() {
            while (true) {
                auto stop = wait_once();

                if (stop) {
                    break;
                }
            }
        });
    }

    auto run_once() -> void
    {
        m_strand->post([this]() { wait_once(); });
    }

    auto stop() -> cppcoro::task<void>
    {
        auto stopFlag = true;
        co_await nop(stopFlag);
    }
};

}