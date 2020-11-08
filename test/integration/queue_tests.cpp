#include <fcntl.h>
#include <stdio.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>

#include <gtest/gtest.h>

#include "uringpp/uringpp.h"

using namespace uringpp;

class QueueTests : public ::testing::Test {
  protected:
    QueueTests()
        : m_maxQueueEntries(1)
        , m_userData(std::make_shared<int>(0))
        , m_ring(m_maxQueueEntries)
    {
    }

  protected:
    using UserData = int;
    const std::size_t m_maxQueueEntries;
    std::shared_ptr<UserData> m_userData;
    Ring<void> m_ring;
};

TEST_F(QueueTests, should_construct_ring)
{
}

TEST_F(QueueTests, should_return_capacity)
{
    ASSERT_EQ(m_maxQueueEntries, m_ring.capacity());
    m_ring.prepare_nop(m_userData);
    ASSERT_EQ(m_maxQueueEntries - 1, m_ring.capacity());    
}

TEST_F(QueueTests, should_return_number_of_prepared_queue_entries)
{
    ASSERT_EQ(0, m_ring.preparedQueueEntries());
    m_ring.prepare_nop(m_userData);
    ASSERT_EQ(1, m_ring.preparedQueueEntries());
}

TEST_F(QueueTests, should_return_number_of_submitted_queue_entries)
{
    ASSERT_EQ(0, m_ring.submittedQueueEntries());
    m_ring.prepare_nop(m_userData);
    ASSERT_EQ(0, m_ring.submittedQueueEntries());
    m_ring.submit();
    ASSERT_EQ(1, m_ring.submittedQueueEntries());
}


TEST_F(QueueTests, should_fail_to_construct_ring_with_to_large_queue_size)
{
    const auto toLargeQueueSize = 4096 + 1;
    ASSERT_THROW(Ring<void> { toLargeQueueSize }, std::runtime_error);
}

TEST_F(QueueTests, should_submit_empty_queue)
{
    m_ring.submit();
}

TEST_F(QueueTests, should_fail_to_peek_on_empty_completion_queue)
{
    ASSERT_FALSE(m_ring.peek());
}