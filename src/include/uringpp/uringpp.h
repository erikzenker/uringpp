#pragma once

#include <exception>
#include <optional>

#include "liburing.h"

#include "uringpp/Completion.h"

namespace uringpp {
class Ring {

  const std::size_t m_maxQueueEntries;
  io_uring m_ring;
  io_uring_cqe *m_cqe;
  std::vector<std::shared_ptr<iovec>> m_buffers;
  std::vector<std::shared_ptr<bool>> m_datas;

public:
  enum class Result { Succes, QueueFull };

  Ring(std::size_t maxQueueEntries) : m_maxQueueEntries(maxQueueEntries) {
    const auto flags = 0;
    const auto result = io_uring_queue_init(m_maxQueueEntries, &m_ring, flags);

    if (result < 0) {
      throw std::runtime_error(std::string{"Failed to init uring queue: "} +
                               strerror(-result));
    }
  }

  ~Ring() { io_uring_queue_exit(&m_ring); }

  //***************************************************************************
  // PUSH
  //***************************************************************************

  /*
   * Pushes a readv system call onto the uring submission queue
   *
   * @param[in] fileDescriptor file descriptor which the kernel should read from
   * @param[in] userData user data which will be returned on the completion
   * @param[out] buffer buffer which the kernel should write to
   */
  auto prepare_readv(int fileDescriptor, std::uint64_t userData,
                     std::vector<std::uint8_t> &buffer) -> bool {

    const std::size_t nBuffer = 1;
    const std::size_t offset = 0;

    auto vec = makeIovec(buffer);

    auto submissionQueueEntry = getSubmissionQueueEntry();
    if (!submissionQueueEntry) {
      return false;
    }

    io_uring_prep_readv(submissionQueueEntry, fileDescriptor, vec.get(),
                        nBuffer, offset);

    submissionQueueEntry->user_data = userData;

    return true;
  }

  auto prepare_writev(int fileDescriptor, std::uint64_t userData,
                      std::vector<std::uint8_t> &buffer) -> bool {

    const std::size_t nBuffer = 1;
    const std::size_t offset = 0;

    auto submissionQueueEntry = getSubmissionQueueEntry();
    if (!submissionQueueEntry) {
      return false;
    }

    auto vec = makeIovec(buffer);

    io_uring_prep_writev(submissionQueueEntry, fileDescriptor, vec.get(),
                         nBuffer, offset);

    submissionQueueEntry->user_data = userData;
    return true;
  }

  //***************************************************************************
  // SUBMIT
  //***************************************************************************

  /*
   * Submits the commands in the submission queue to the kernel. The kernel will
   * start to process the commands asynchronously of the submission call.
   */
  auto submit() {
    auto result = io_uring_submit(&m_ring);
    if (result < 0) {
      throw std::runtime_error(std::string{"Failed to submit: "} +
                               strerror(-result));
    }
  }

  //***************************************************************************
  // PULL
  //***************************************************************************

  /*
   * Waits until command from the submission queue are processed.
   * The function call will block if no completion entry is ready.
   *
   * @return Completion
   */
  auto wait() -> Completion {
    auto result = io_uring_wait_cqe(&m_ring, &m_cqe);

    if (result < 0) {
      throw std::runtime_error(std::string{"Failed to wait: "} +
                               strerror(-result));
    }

    return Completion{m_cqe};
  }

  /*
   * Returns a completion if available otherwise a nullptr
   *
   * @return Completion completion of submitted command
   */
  auto peek() -> std::optional<Completion> {
    auto result = io_uring_peek_cqe(&m_ring, &m_cqe);

    if (result < 0) {
      return {};
    }

    return Completion(m_cqe);
  }

  /*
   * Remove the completion entry from the completion queue
   *
   * @param[in] completion completion that should be removed
   */
  auto seen(const Completion &completion) {
    io_uring_cqe_seen(&m_ring, completion.get());
  }

private:
  auto getSubmissionQueueEntry() -> io_uring_sqe * {
    return io_uring_get_sqe(&m_ring);
  }

  auto makeIovec(std::vector<std::uint8_t> &buffer) -> std::shared_ptr<iovec> {
    auto vec = std::make_shared<iovec>();
    vec->iov_base = buffer.data();
    vec->iov_len = buffer.size();
    m_buffers.push_back(vec);
    return vec;
  }
};
} // namespace uringpp