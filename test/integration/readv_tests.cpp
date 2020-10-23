#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "tests_base.h"
#include "uringpp/uringpp.h"

using namespace uringpp;

class ReadvTests : public ::testing::Test {
  protected:
    ReadvTests()
        : m_file("test2.txt")
        , m_fd(getFileDescriptor(m_file))
        , m_buffer(std::filesystem::file_size(m_file), '!')
        , m_maxQueueEntries(1)
        , m_userData(std::make_shared<int>(0))
        , m_ring(m_maxQueueEntries)
    {
    }

  protected:
    using UserData = int;
    std::filesystem::path m_file;
    int m_fd;
    std::vector<std::uint8_t> m_buffer;
    const std::size_t m_maxQueueEntries;
    std::shared_ptr<UserData> m_userData;
    Ring m_ring;
};

TEST_F(ReadvTests, should_prepare_readv)
{
    ASSERT_TRUE(m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData));
}

TEST_F(ReadvTests, should_submit_readv)
{
    m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData);
    m_ring.submit();
}

TEST_F(ReadvTests, should_wait_for_submitted_readv)
{
    m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData);
    m_ring.submit();
    auto result = m_ring.wait<UserData>();
    ASSERT_EQ(std::filesystem::file_size(m_file), result.get()->res);
    ASSERT_EQ(0, result.get()->flags);
    ASSERT_EQ(readFile(m_file), m_buffer);
}

TEST_F(ReadvTests, should_peek_for_submitted_readv)
{
    m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData);
    m_ring.submit();
    auto result = m_ring.peek<UserData>();
    ASSERT_EQ(std::filesystem::file_size(m_file), result->get()->res);
    ASSERT_EQ(0, result->get()->flags);
    ASSERT_EQ(readFile(m_file), m_buffer);
}

TEST_F(ReadvTests, should_fail_to_peek_when_completion_queue_was_cleared_by_peek)
{
    m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData);
    m_ring.submit();
    auto completion = m_ring.peek<UserData>();
    m_ring.seen(*completion);
    ASSERT_FALSE(m_ring.peek<UserData>());
}

TEST_F(ReadvTests, should_fail_to_peek_when_completion_queue_was_cleared_by_wait)
{
    m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData);
    m_ring.submit();
    auto completion = m_ring.wait<UserData>();
    m_ring.seen(completion);
    ASSERT_FALSE(m_ring.peek<UserData>());
}

TEST_F(ReadvTests, should_fail_to_enqueue_more_entries_then_available)
{
    ASSERT_TRUE(m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData));
    ASSERT_FALSE(m_ring.prepare_readv(m_fd, m_buffer, 0, m_userData));
}

TEST_F(ReadvTests, should_enqueue_custom_user_data)
{
    auto userData = std::make_shared<int>(10);
    ASSERT_TRUE(m_ring.prepare_readv(m_fd, m_buffer, 0, userData));
    m_ring.submit();
    auto completion = m_ring.wait<UserData>();
    ASSERT_EQ(10, *completion.userData());
}

class PrepareWritevTests : public ReadvTests {
};

TEST_F(PrepareWritevTests, should_fail_to_enqueue_more_entries_then_available)
{
    m_ring.prepare_writev(m_fd, m_buffer, 0, m_userData);
    m_ring.submit();
    m_ring.wait<UserData>();
}
