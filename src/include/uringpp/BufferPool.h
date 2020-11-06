#pragma once

#include <cstdint>
#include <span>
#include <vector>

class BufferPool {
  public:
    BufferPool(std::size_t numberOfBuffers, std::size_t sizePerBuffer, std::size_t groupId)
        : m_groupId(groupId)
        , m_numberOfBuffers(numberOfBuffers)
        , m_sizePerBuffer(sizePerBuffer)
        , m_storage(m_numberOfBuffers * m_sizePerBuffer)
    {
    }

  public:
    auto data() -> std::uint8_t*
    {
        return m_storage.data();
    }

    auto at(std::size_t bufferIdx) -> std::span<std::uint8_t>
    {
        // TODO: check bounds
        return std::span<std::uint8_t>(
            m_storage.data() + (bufferIdx * m_sizePerBuffer), m_sizePerBuffer);
    }

    auto clear(std::size_t bufferIdx) -> void
    {
        std::memset(at(bufferIdx).data(), 0x00, m_sizePerBuffer);
    }

    auto pool_size() const -> std::size_t
    {
        return m_numberOfBuffers;
    }

    auto buffer_size() const -> std::size_t
    {
        return m_sizePerBuffer;
    }

    auto group_id() const -> std::size_t
    {
        return m_groupId;
    }

  private:
    std::size_t m_groupId;
    std::size_t m_numberOfBuffers;
    std::size_t m_sizePerBuffer;
    std::vector<std::uint8_t> m_storage;
};