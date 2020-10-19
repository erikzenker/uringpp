#include <fcntl.h>
#include <stdio.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "uringpp/uringpp.h"

using namespace uringpp;

class BasicSetupTests : public ::testing::Test {
protected:
  BasicSetupTests()
      : m_file("test2.txt"), m_fd(getFileDescriptor(m_file)),
        m_buffer(std::filesystem::file_size(m_file), '!'), m_maxQueueEntries(1), m_userData(std::make_shared<int>(0)),
        m_ring(m_maxQueueEntries) {}


protected:
  auto getFileDescriptor(const std::filesystem::path &file) -> int {
    auto fd = open(file.c_str(), O_RDWR);
    if (fd < 0) {
      throw std::runtime_error("Failed to open file");
    }
    return fd;
  }

  auto readFile(const std::filesystem::path& path) -> std::vector<std::uint8_t>
  {
    std::ifstream stream(path.filename(), std::ios::binary);
    std::vector<std::uint8_t> fileContent;

    std::string line;
    while(std::getline(stream, line))
    {
        fileContent.insert(fileContent.end(), line.begin(), line.end());
    }

    return fileContent;    
  }

protected:
  std::filesystem::path m_file;
  int m_fd;
  std::vector<std::uint8_t> m_buffer;
  const std::size_t m_maxQueueEntries;
  std::shared_ptr<int> m_userData;
  Ring m_ring;
};

TEST_F(BasicSetupTests, should_construct_ring) {}

TEST_F(BasicSetupTests,
       should_fail_to_construct_ring_with_to_large_queue_size) {
  const auto toLargeQueueSize = std::numeric_limits<std::size_t>::max();
  ASSERT_THROW(Ring{toLargeQueueSize}, std::runtime_error);
}

TEST_F(BasicSetupTests, should_submit_empty_queue) {
  m_ring.submit();
}

TEST_F(BasicSetupTests, should_fail_to_peek_on_empty_completion_queue) {
  ASSERT_FALSE(m_ring.peek());
}

class PrepareReadvTests : public BasicSetupTests {
};

TEST_F(PrepareReadvTests, should_prepare_readv) {
  ASSERT_TRUE(m_ring.prepare_readv(m_fd, m_userData, m_buffer));
}

TEST_F(PrepareReadvTests, should_submit_readv) {
  m_ring.prepare_readv(m_fd, m_userData, m_buffer);    
  m_ring.submit();
}

TEST_F(PrepareReadvTests, should_wait_for_submitted_readv) {
  m_ring.prepare_readv(m_fd, m_userData, m_buffer);    
  m_ring.submit();
  auto result = m_ring.wait();
  ASSERT_EQ(std::filesystem::file_size(m_file), result.get()->res);
  ASSERT_EQ(0, result.get()->flags);
  ASSERT_EQ(readFile(m_file), m_buffer);
}

TEST_F(PrepareReadvTests, should_peek_for_submitted_readv) {
  m_ring.prepare_readv(m_fd, m_userData, m_buffer);    
  m_ring.submit();
  auto result = m_ring.peek();
  ASSERT_EQ(std::filesystem::file_size(m_file), result->get()->res);
  ASSERT_EQ(0, result->get()->flags);
  ASSERT_EQ(readFile(m_file), m_buffer);
}

TEST_F(PrepareReadvTests, should_fail_to_peek_when_completion_queue_was_cleared_by_peek) {
  m_ring.prepare_readv(m_fd, m_userData, m_buffer);    
  m_ring.submit();
  auto completion = m_ring.peek();
  m_ring.seen(*completion);
  ASSERT_FALSE(m_ring.peek());
}

TEST_F(PrepareReadvTests, should_fail_to_peek_when_completion_queue_was_cleared_by_wait) {
  m_ring.prepare_readv(m_fd, m_userData, m_buffer);    
  m_ring.submit();
  auto completion = m_ring.wait();
  m_ring.seen(completion);
  ASSERT_FALSE(m_ring.peek());
}

TEST_F(PrepareReadvTests, should_fail_to_enqueue_more_entries_then_available) {
  ASSERT_TRUE(m_ring.prepare_readv(m_fd, m_userData, m_buffer));
  ASSERT_FALSE(m_ring.prepare_readv(m_fd, m_userData, m_buffer));
}

TEST_F(PrepareReadvTests, should_enqueue_custom_user_data)
{
    auto userData = std::make_shared<int>(10);
    ASSERT_TRUE(m_ring.prepare_readv(m_fd, userData, m_buffer));
    m_ring.submit();
    auto completion = m_ring.wait();
    auto a = reinterpret_cast<int*>(completion.get()->user_data);
    ASSERT_EQ(10, *a);
}

class PrepareWritevTests : public BasicSetupTests {
};


TEST_F(PrepareWritevTests, should_fail_to_enqueue_more_entries_then_available) {
    m_ring.prepare_writev(m_fd, m_userData, m_buffer);
    m_ring.submit();
    m_ring.wait();
}
