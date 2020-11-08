#pragma once

#include "uringpp/Ring.h"

#include "boost/asio.hpp"

#include <memory>

namespace uringpp {

struct UserData {
    std::uint64_t id;
    std::function<void()> handler;
};

class RingService {
  private:
    Ring<UserData> m_ring;
    boost::asio::io_context m_ioContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work;
    std::thread m_workThread;
    std::map<UserData*, std::shared_ptr<UserData>> m_handlers;
    std::uint64_t m_id = 0;

  public:
    RingService()
        : m_ring(64)
        , m_ioContext()
        , m_work(boost::asio::make_work_guard(m_ioContext))
        , m_workThread([this]() { m_ioContext.run(); })
    {
    }

    ~RingService()
    {
        m_work.reset();
        m_workThread.join();
    }

    template <class Handler> auto nop(const Handler& handler) -> void
    {
        auto userData = std::make_shared<UserData>(m_id++, handler);
        m_handlers.insert({ userData.get(), userData });
        m_ring.prepare_nop(userData);
        m_ring.submit();
    }

    auto run_once() -> void
    {
        boost::asio::post(m_ioContext, [this]() {
            auto completion = m_ring.wait();
            completion.userData()->handler();
            m_handlers.erase(completion.userData());
            m_ring.seen(completion);
        });
    }
};

}