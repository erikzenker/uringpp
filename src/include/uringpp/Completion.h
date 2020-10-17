#pragma once

#include <exception>

#include "liburing.h"

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

private:    
    io_uring_cqe *m_cqe;
};