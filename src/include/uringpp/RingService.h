#pragma once

#include "uringpp/Ring.h"

#include "asyncly/executor/IStrand.h"
#include "asyncly/executor/Strand.h"
#include "asyncly/executor/ThreadPoolExecutorController.h"
#include "asyncly/future/Future.h"

#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>

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

class RingService {
  private:
    Ring<Request> m_ring;
    std::unique_ptr<asyncly::ThreadPoolExecutorController> m_executorController;
    asyncly::IStrandPtr m_strand;
    std::map<Request*, std::shared_ptr<Request>> m_requests;
    std::uint64_t m_id = 0;

  public:
    RingService()
        : m_ring(64)
        , m_executorController(asyncly::ThreadPoolExecutorController::create(1))
        , m_strand(asyncly::create_strand(m_executorController->get_executor()))
    {
    }

    ~RingService()
    {
        m_executorController->finish();
        std::cerr << "~RingService" << std::endl;
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
    auto readv(int fd, Container& buffer, std::size_t offset) -> cppcoro::task<Container&>
    {
        cppcoro::single_consumer_event event;
        auto request = std::make_shared<Request>(m_id++, event);

        m_requests.insert({ request.get(), request });
        m_ring.prepare_readv(fd, buffer, offset, request);
        m_ring.submit();

        co_await event;
        co_return buffer;
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