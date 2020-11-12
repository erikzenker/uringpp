
#include <gtest/gtest.h>

#include "uringpp/RingService.h"

#include "asyncly/test/FutureTest.h"
#include "asyncly/executor/CurrentExecutor.h"

using namespace uringpp;

class RingServiceTests : public asyncly::test::FutureTest {
  protected:
    RingServiceTests() : m_controller(asyncly::ThreadPoolExecutorController::create(1)), m_service()
    {
        asyncly::this_thread::set_current_executor(m_controller->get_executor());    
    }

  protected:
    std::unique_ptr<asyncly::IExecutorController> m_controller;
    RingService m_service;
};

TEST_F(RingServiceTests, should_compile)
{
}

TEST_F(RingServiceTests, should_nop)
{
    auto future = m_service.nop().then([](){ return asyncly::make_ready_future();});
    m_service.run_once();

    wait_for_future(std::move(future));
}

TEST_F(RingServiceTests, should_read)
{
    int fd = 0  ;
    std::vector<std::uint8_t> buffer(1024);

    auto future = m_service.readv(fd, buffer, 0);
    m_service.run_once();

    wait_for_future(std::move(future));    
}