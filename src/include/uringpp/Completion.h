#pragma once

#include <exception>

#include "liburing.h"

template <class UserData>
class Completion
{
public:    
    Completion(io_uring_cqe *cqe) : m_cqe(cqe){
        if(!m_cqe){
            throw std::runtime_error("Completion queue entry is null");
        }
    }

    auto get() const -> io_uring_cqe * {
        return m_cqe;    
    }

    auto result() const -> std::int32_t
    {
        return m_cqe->res;
    }

    auto flags() const -> std::uint32_t
    {
        return m_cqe->flags;
    }
    auto userData() const -> UserData*
    {
        return reinterpret_cast<UserData*>(m_cqe->user_data);
    }

private:    
    io_uring_cqe *m_cqe;
};