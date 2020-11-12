#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "liburing.h"

#include "uringpp/BufferPool.h"
#include "uringpp/Completion.h"
#include "uringpp/concepts.h"

namespace uringpp {

template <class UserData> class Ring {
    const std::size_t m_maxQueueEntries;
    io_uring m_ring;
    io_uring_cqe* m_cqe;
    io_uring_params m_params;

  public:
    /*
     * Create Wrapper for io_ring. Detailed documentation about flags and features
     * of io_uring initialization can be found here:
     *
     * https://raw.githubusercontent.com/axboe/liburing/master/man/io_uring_setup.2
     *
     * Flags:
     *  IORING_SETUP_IOPOLL: Perform busy-waiting for an I/O completion
     *  IORING_SETUP_SQPOLL: perform submission queue polling
     *  ...
     *
     * @params[in] maxQueueEntries Number of entries in in submission and completion queue
     * @params[in] flags feature toggles
     */
    Ring(std::size_t maxQueueEntries, std::uint64_t flags = 0)
        : m_maxQueueEntries(maxQueueEntries)
    {
        std::memset(&m_params, 0, sizeof(m_params));
        m_params.flags = flags;
        const auto result = io_uring_queue_init_params(m_maxQueueEntries, &m_ring, &m_params);

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
    // FEATURE CHECKS
    //***************************************************************************

    /*
     * Return true if the two SQ and CQ rings can be mapped with a single mmap(2) call.
     */
    auto has_single_mmap() -> bool
    {
        return m_params.features & IORING_FEAT_SINGLE_MMAP;
    }

    /*
     * Return true if io_uring supports never dropping completion events.
     */
    auto has_nodrop() -> bool
    {
        return m_params.features & IORING_FEAT_NODROP;
    }

    /*
     * Return true if applications can be certain that any data for
     * async offload has been consumed when the kernel has consumed the SQE.
     */
    auto has_submit_stable() -> bool
    {
        return m_params.features & IORING_FEAT_SUBMIT_STABLE;
    }

    auto has_rw_cur_pos() -> bool
    {
        return m_params.features & IORING_FEAT_RW_CUR_POS;
    }

    auto has_cur_personality() -> bool
    {
        return m_params.features & IORING_FEAT_CUR_PERSONALITY;
    }

    /*
     * Returns true if io_uring supports using an internal poll mechanism
     * to drive data/space readiness. This means that requests that cannot read or
     * write data to a file no longer need to be punted to an async thread for
     * handling, instead they will being operation when the file is ready. This is
     * similar to doing poll + read/write in userspace, but eliminates the need to do
     * so. If this flag is set, requests waiting on space/data consume a lot less
     * resources doing so as they are not blocking a thread.
     */
    auto has_fast_poll() -> bool
    {
        return m_params.features & IORING_FEAT_FAST_POLL;
    }

    /*
     * Returns ture if this flag is set, the IORING_OP_POLL_ADD command
     * accepts the full 32-bit range of epoll based flags
     */
    auto has_poll_32bit() -> bool
    {
        return m_params.features & IORING_FEAT_POLL_32BITS;
    }

    //***************************************************************************
    // PUSH TO SUBMISSION QUEUE
    //***************************************************************************

    /*
     * Pushes a no op onto the uring submission queue
     */
    auto prepare_nop(const std::shared_ptr<UserData>& userData) -> bool
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        io_uring_prep_nop(submissionQueueEntry);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    /*
     * Pushes a readv system call onto the uring submission queue
     *
     * @param[in] fileDescriptor file descriptor which the kernel should read from
     * @param[out] buffer buffer which the kernel should read to
     * @param[in] offset offset in the buffer where start to read
     * @param[in] userData user data which will be returned on the completion
     */
    template <ContinuousMemory Container>
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

    /*
     * Pushes a writev system call onto the uring submission queue
     *
     * @param[in] fileDescriptor file descriptor which the kernel should read from
     * @param[out] buffer buffer which the kernel should write to
     * @param[in] offset offset in the buffer where start to write
     * @param[in] userData user data which will be returned on the completion
     */
    template <ContinuousMemory Container>
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

    template <ContinuousMemory Container>
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

    template <ContinuousMemory Container>
    auto prepare_send_bp(
        int fileDescriptor, Container&& buffer, const std::shared_ptr<UserData>& userData)
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

    template <ContinuousMemory Container>
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

    auto prepare_recv_bp(
        int fileDescriptor, BufferPool& bufferPool, const std::shared_ptr<UserData>& userData)
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            return false;
        }

        const int flags = 0;
        io_uring_prep_recv(
            submissionQueueEntry, fileDescriptor, nullptr, bufferPool.buffer_size(), flags);
        submissionQueueEntry->buf_group = bufferPool.group_id();
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());
        io_uring_sqe_set_flags(submissionQueueEntry, IOSQE_BUFFER_SELECT);

        return true;
    }

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

        io_uring_prep_epoll_ctl(
            submissionQueueEntry, epollFileDescriptor, fileDescriptor, op, epollEvent);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return true;
    }

    //***************************************************************************
    // BUFFER UTILS
    //***************************************************************************
    // TODO: Add user data and do not submit/wait/seen
    auto prepare_create_buffer_pool(
        std::size_t numberOfBuffers,
        std::size_t sizePerBuffer,
        const std::shared_ptr<UserData>& userData) -> BufferPool
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            throw std::runtime_error("Failed to get submission queue entry");
        }

        auto bufferId = 0;
        BufferPool bufferPool(numberOfBuffers, sizePerBuffer, bufferId);

        io_uring_prep_provide_buffers(
            submissionQueueEntry,
            bufferPool.data(),
            bufferPool.buffer_size(),
            bufferPool.pool_size(),
            bufferPool.group_id(),
            0);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return bufferPool;
    }

    auto prepare_readd_buffer(
        BufferPool& bufferPool, std::size_t bufferIdx, const std::shared_ptr<UserData>& userData)
        -> BufferPool
    {
        auto submissionQueueEntry = getSubmissionQueueEntry();
        if (!submissionQueueEntry) {
            throw std::runtime_error("Failed to get submission queue entry");
        }

        bufferPool.clear(bufferIdx);
        io_uring_prep_provide_buffers(
            submissionQueueEntry,
            bufferPool.at(bufferIdx).data(),
            bufferPool.at(bufferIdx).size(),
            1,
            bufferPool.group_id(),
            bufferIdx);
        io_uring_sqe_set_data(submissionQueueEntry, userData.get());

        return bufferPool;
    }

    //***************************************************************************
    // SUBMIT
    //***************************************************************************

    /*
     * Submits the commands in the submission queue to the kernel. The kernel will
     * start to process the commands asynchronously of the submission call.
     */
    auto submit() -> void
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
    auto wait() -> Completion<UserData>
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
    auto peek() -> std::optional<Completion<UserData>>
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
    auto seen(const Completion<UserData>& completion) -> void
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