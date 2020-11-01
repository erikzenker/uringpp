#pragma once

#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "liburing.h"

#include "uringpp/Completion.h"

namespace uringpp {

template <class T> concept ContinuousMemory = requires(T t)
{
    t.data();
    t.size();
};

class Ring {
    const std::size_t m_maxQueueEntries;
    io_uring m_ring;
    io_uring_cqe* m_cqe;

  public:
    Ring(std::size_t maxQueueEntries)
        : m_maxQueueEntries(maxQueueEntries)
    {
        const auto flags = 0;
        const auto result = io_uring_queue_init(m_maxQueueEntries, &m_ring, flags);

        if (result < 0) {
            throw std::runtime_error(
                std::string { "Failed to init uring queue: " } + strerror(-result));
        }
    }

    ~Ring()
    {
        io_uring_queue_exit(&m_ring);
    }

    //***************************************************************************
    // PUSH TO SUBMISSION QUEUE
    //***************************************************************************

    /*
     * Pushes a no op onto the uring submission queue
     */
    auto prepare_nop() -> bool
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        io_uring_prep_nop(submissionQueueEntry);

        return true;
    }

    /*
     * Pushes a readv system call onto the uring submission queue
     *
     * @param[in] fileDescriptor file descriptor which the kernel should read from
     * @param[in] userData user data which will be returned on the completion
     * @param[out] buffer buffer which the kernel should write to
     */
    template <ContinuousMemory Container, class UserData>
    auto prepare_readv(
        int fileDescriptor,
        Container& buffer,
        std::size_t offset,
        const std::shared_ptr<UserData>& userData) -> bool
    {
        const std::size_t nBuffer = 1;

        auto vec = makeIovec(buffer);

        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        io_uring_prep_readv(submissionQueueEntry, fileDescriptor, vec.get(), nBuffer, offset);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    template <ContinuousMemory Container, class UserData>
    auto prepare_writev(
        int fileDescriptor,
        Container& buffer,
        std::size_t offset,
        const std::shared_ptr<UserData>& userData) -> bool
    {
        const std::size_t nBuffer = 1;

        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        auto vec = makeIovec(buffer);

        io_uring_prep_writev(submissionQueueEntry, fileDescriptor, vec.get(), nBuffer, offset);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    template <class UserData>
    auto prepare_accept(
        int fileDescriptor,
        struct sockaddr* addr,
        socklen_t* addrlen,
        const std::shared_ptr<UserData>& userData)
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        const int flags = 0;
        io_uring_prep_accept(submissionQueueEntry, fileDescriptor, addr, addrlen, flags);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    template <ContinuousMemory Container, class UserData>
    auto
    prepare_send(int fileDescriptor, Container& buffer, const std::shared_ptr<UserData>& userData)
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        const int flags = 0;
        io_uring_prep_send(
            submissionQueueEntry, fileDescriptor, buffer.data(), buffer.size(), flags);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    template <ContinuousMemory Container, class UserData>
    auto
    prepare_recv(int fileDescriptor, Container& buffer, const std::shared_ptr<UserData>& userData)
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        const int flags = 0;
        io_uring_prep_recv(
            submissionQueueEntry, fileDescriptor, buffer.data(), buffer.size(), flags);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    template <class UserData>
    auto prepare_poll_add(int fileDescriptor, const std::shared_ptr<UserData>& userData)
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }


       io_uring_prep_poll_add(submissionQueueEntry, fileDescriptor, POLL_IN);
       io_uring_sqe_set_data(submissionQueueEntry, userData.get());
       return true;
    }    

    template <class UserData>
    auto prepare_epoll_ctl(
        int epollFileDescriptor,
        int fileDescriptor,
        int op,
        epoll_event* epollEvent,
        const std::shared_ptr<UserData>& userData)
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        const int flags = 0;
        io_uring_prep_epoll_ctl(
            submissionQueueEntry, epollFileDescriptor, fileDescriptor, op, epollEvent);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    //***************************************************************************
    // SUBMIT
    //***************************************************************************

    /*
     * Submits the commands in the submission queue to the kernel. The kernel will
     * start to process the commands asynchronously of the submission call.
     */
    auto submit()
    {
        auto result = io_uring_submit(&m_ring);
        if (result < 0) {
            throw std::runtime_error(std::string { "Failed to submit: " } + strerror(-result));
        }
    }

    //***************************************************************************
    // PULL FROM COMPLETION QUEUE
    //***************************************************************************

    /*
     * Waits until command from the submission queue are processed.
     * The function call will block if no completion entry is ready.
     *
     * @return Completion
     */
    template <class UserData> auto wait() -> Completion<UserData>
    {
        auto result = io_uring_wait_cqe(&m_ring, &m_cqe);

        if (result < 0) {
            throw std::runtime_error(std::string { "Failed to wait: " } + strerror(-result));
        }

        return Completion<UserData> { m_cqe };
    }

    /*
     * Returns a completion if available otherwise a nullptr
     *
     * @return Completion completion of submitted command
     */
    template <class UserData> auto peek() -> std::optional<Completion<UserData>>
    {
        auto result = io_uring_peek_cqe(&m_ring, &m_cqe);

        if (result < 0) {
            return {};
        }

        return Completion<UserData> { m_cqe };
    }

    /*
     * Remove the completion entry from the completion queue
     *
     * @param[in] completion completion that should be removed
     */
    template <class UserData> auto seen(const Completion<UserData>& completion)
    {
        io_uring_cqe_seen(&m_ring, completion.get());
    }

    /*
     * Returns the number of free slots in the submission queue
     */
    auto capacity()
    {
        return io_uring_sq_space_left(&m_ring);
    }

    /*
     * Returns the number of prepared, not yet submitted, entries
     */
    auto preparedQueueEntries()
    {
        return io_uring_sq_ready(&m_ring);
    }

    /*
     * Returns the number of entries in the submission queue
     */
    auto submittedQueueEntries()
    {
        return io_uring_cq_ready(&m_ring);
    }

  private:
    auto getSubmissionQueueEntry() -> io_uring_sqe*
    {
        return io_uring_get_sqe(&m_ring);
    }

    template <class Container> auto makeIovec(Container& buffer) -> std::shared_ptr<iovec>
    {
        auto vec = std::make_shared<iovec>();
        vec->iov_base = buffer.data();
        vec->iov_len = buffer.size();
        return vec;
    }
};
} // namespace uringpp