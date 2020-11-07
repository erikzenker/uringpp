
#include <gtest/gtest.h>

#include "tests_base.h"
#include "uringpp/uringpp.h"

using namespace uringpp;

class BufferTests : public ::testing::Test {
  protected:
    BufferTests()
        : m_maxQueueEntries(1)
        , m_ring(m_maxQueueEntries)
    {
    }

  protected:
    const std::size_t m_maxQueueEntries;
    Ring<void> m_ring;
};

TEST_F(BufferTests, should_create_buffer_pool)
{
    auto bufferPool = m_ring.prepare_create_buffer_pool(2, 1, std::make_shared<int>(0));

    ASSERT_EQ(2, bufferPool.pool_size());
    ASSERT_EQ(1, bufferPool.buffer_size());
    ASSERT_EQ(0, bufferPool.group_id());
}
