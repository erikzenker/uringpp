
#include <gtest/gtest.h>

#include "tests_base.h"
#include "uringpp/uringpp.h"

using namespace uringpp;

class WritevTests : public ::testing::Test {
  protected:
    WritevTests()
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

TEST_F(WritevTests, should_fail_to_enqueue_more_entries_then_available)
{
    m_ring.prepare_writev(m_fd, m_buffer, 0, m_userData);
    m_ring.submit();
    m_ring.wait<UserData>();
}
