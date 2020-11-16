
#include <gtest/gtest.h>

#include "uringpp/RingService.h"

#include "asyncly/executor/CurrentExecutor.h"

#include "cppcoro/async_generator.hpp"
#include "cppcoro/sync_wait.hpp"

#include "tests_base.h"

using namespace uringpp;

class RingServiceTests : public testing::Test {
  protected:
    RingServiceTests()
        : m_controller(asyncly::ThreadPoolExecutorController::create(1))
        , m_file("test.txt")
        , m_fd(getFileDescriptor(m_file))
        , m_service()

    {
        asyncly::this_thread::set_current_executor(m_controller->get_executor());
    }

  protected:
    std::unique_ptr<asyncly::IExecutorController> m_controller;
    std::filesystem::path m_file;
    int m_fd;

    RingService m_service;
};

TEST_F(RingServiceTests, should_compile)
{
}

TEST_F(RingServiceTests, should_nop)
{
    m_service.run_once();

    cppcoro::sync_wait([this]() -> cppcoro::task<> {
        co_await m_service.nop();
    }());
}

TEST_F(RingServiceTests, should_read)
{
    m_service.run_once();

    cppcoro::sync_wait([this]() -> cppcoro::task<> {
        std::vector<std::uint8_t> buffer(1024);
        auto& readBuffer = co_await m_service.readv(m_fd, buffer, 0);
        EXPECT_EQ(readBuffer.size(), buffer.size());
    }());
}

TEST_F(RingServiceTests, should_do)
{
    m_service.run_once();

    cppcoro::sync_wait([this]() -> cppcoro::task<> {
        auto make_generator = [this]() -> cppcoro::async_generator<int> {
            co_await m_service.nop();
            co_yield 0;
        };

        auto gen = make_generator();
        auto it = co_await gen.begin();
        std::cout << *it << std::endl;
    }());
}