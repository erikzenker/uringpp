
#include <gtest/gtest.h>

#include "uringpp/RingService.h"

using namespace uringpp;

class RingServiceTests : public ::testing::Test {
};

TEST_F(RingServiceTests, should_compile)
{
    RingService service;
}

TEST_F(RingServiceTests, should_call_handler)
{
    RingService service;

    std::promise<void> handlerCalled;

    auto handler = [&handlerCalled]() { handlerCalled.set_value(); };

    service.nop(handler);
    service.run_once();

    ASSERT_EQ(
        std::future_status::ready, handlerCalled.get_future().wait_for(std::chrono::seconds(1)));
}
