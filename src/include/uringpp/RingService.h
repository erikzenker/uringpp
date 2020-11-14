#pragma once

#include "uringpp/Ring.h"

#include "asyncly/executor/ThreadPoolExecutorController.h"
#include "asyncly/executor/Strand.h"
#include "asyncly/executor/IStrand.h"
#include "asyncly/future/Future.h"

#include "boost/asio.hpp"

#include <memory>

namespace uringpp {

struct Request {
    Request(std::uint64_t id, asyncly::Promise<void>&& promise) : id(id), promise(promise){}
    std::uint64_t id;
    asyncly::Promise<void> promise;
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
    }

    auto nop() -> asyncly::Future<void>
    {
        auto [future, promise] = asyncly::make_lazy_future<void>();        
        auto request = std::make_shared<Request>(m_id++, std::move(promise));

        m_requests.insert({ request.get(), request });
        m_ring.prepare_nop(request);
        m_ring.submit();
        return future;
    }

    template<ContinuousMemory Container>
    auto readv(int fd, Container& buffer, std::size_t offset) -> asyncly::Future<void>
    {
        auto [future, promise] = asyncly::make_lazy_future<void>();
        auto request = std::make_shared<Request>(m_id++, std::move(promise));

        m_requests.insert({ request.get(), request });
        m_ring.prepare_readv(fd, buffer, offset, request);
        m_ring.submit();
        return future;
    }

    auto run_once() -> void
    {
        m_strand->post([this]() {
            auto completion = m_ring.wait();
            completion.userData()->promise.set_value();
            m_requests.erase(completion.userData());
            m_ring.seen(completion);
        });
    }
};

}