#include <uringpp/uringpp.h>

#include <fcntl.h>
#include <stdio.h>

#include <deque>
#include <filesystem>
#include <iostream>
#include <vector>

int openFile(const std::filesystem::path& file, int mode)
{
    auto fd = open(file.c_str(), mode);
    if (fd < 0) {
        throw std::runtime_error(std::string("Failed to open file ") + file.c_str());
    }
    return fd;
}

enum class CompletionType : std::uint8_t { Read = 0, Write = 1 };

struct Data {
    Data(CompletionType type, std::size_t submissionIndex, std::vector<std::uint8_t>& block)
        : type(type)
        , submissionIndex(submissionIndex)
        , block(block)
    {
    }
    CompletionType type;
    std::size_t submissionIndex;
    std::vector<std::uint8_t>& block;
};

auto readFile(uringpp::Ring& ring, const std::filesystem::path file)
    -> std::deque<std::vector<std::uint8_t>>
{
    const auto fd = openFile(file, O_RDONLY);
    const auto blockSize = 4 * 1024;
    auto bytesReadTotal = 0;
    auto submissionIndex = 0;
    std::deque<std::vector<std::uint8_t>> blocks;

    while (true) {
        blocks.emplace_back(blockSize);
        auto readData
            = std::make_unique<Data>(CompletionType::Read, submissionIndex++, blocks.back());
        ring.prepare_readv(
            fd, reinterpret_cast<std::uint64_t>(&*readData), blocks.back(), bytesReadTotal);
        ring.submit();

        auto completion = ring.wait();
        auto data = reinterpret_cast<Data*>(completion.get()->user_data);
        ring.seen(completion);

        if (data->type == CompletionType::Read) {
            auto bytesRead = completion.get()->res;
            if (bytesRead < 0) {
                throw std::runtime_error("failed to read from file");
            }

            data->block.resize(bytesRead);
            bytesReadTotal += bytesRead;
        }

        if (bytesReadTotal >= std::filesystem::file_size(file)) {
            break;
        }
    }
    return blocks;
}

auto writeFile(
    uringpp::Ring& ring,
    const std::filesystem::path file,
    std::deque<std::vector<std::uint8_t>> blocks)
{
    auto bytesWriteTotal = 0;
    auto submissionIndex = 0;
    const auto fd = openFile(file, O_WRONLY);

    for (auto& block : blocks) {
        auto writeData = std::make_unique<Data>(CompletionType::Write, submissionIndex++, block);
        ring.prepare_writev(
            fd, reinterpret_cast<std::uint64_t>(&*writeData), block, bytesWriteTotal);
        ring.submit();

        auto completion = ring.wait();
        auto data = reinterpret_cast<Data*>(completion.get()->user_data);
        ring.seen(completion);

        if (data->type == CompletionType::Write) {
            auto bytesWrite = completion.get()->res;
            if (bytesWrite < 0) {
                throw std::runtime_error("failed to write to file");
            }

            bytesWriteTotal += bytesWrite;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        return 1;
    }

    const auto inputFile = std::filesystem::path(argv[1]);
    const auto outputFile = std::filesystem::path(argv[2]);

    uringpp::Ring ring { 64 };

    writeFile(ring, outputFile, readFile(ring, inputFile));

    return 0;
}