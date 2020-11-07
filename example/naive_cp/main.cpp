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

auto readFile(uringpp::Ring<Data>& ring, const std::filesystem::path file)
    -> std::deque<std::vector<std::uint8_t>>
{
    const auto fd = openFile(file, O_RDONLY);
    const auto blockSize = 4 * 1024;
    auto bytesReadTotal = 0;
    auto submissionIndex = 0;
    std::deque<std::vector<std::uint8_t>> blocks;

    while (true) {
        blocks.emplace_back(blockSize);
        auto readData = std::make_shared<Data>(CompletionType::Read, submissionIndex++, blocks.back());
        ring.prepare_readv(fd, blocks.back(), bytesReadTotal, readData);
        ring.submit();

        auto completion = ring.wait();

        if (completion.userData()->type == CompletionType::Read) {
            auto bytesRead = completion.result();
            if (bytesRead < 0) {
                throw std::runtime_error("failed to read from file");
            }

            completion.userData()->block.resize(bytesRead);
            bytesReadTotal += bytesRead;
        }

        ring.seen(completion);        

        if (bytesReadTotal >= std::filesystem::file_size(file)) {
            break;
        }
    }
    return blocks;
}

auto writeFile(
    uringpp::Ring<Data>& ring,
    const std::filesystem::path file,
    std::deque<std::vector<std::uint8_t>> blocks)
{
    auto bytesWriteTotal = 0;
    auto submissionIndex = 0;
    const auto fd = openFile(file, O_WRONLY);

    for (auto& block : blocks) {
        auto writeData = std::make_shared<Data>(CompletionType::Write, submissionIndex++, block);
        ring.prepare_writev(fd, block, bytesWriteTotal, writeData);
        ring.submit();

        auto completion = ring.wait();

        if (completion.userData()->type == CompletionType::Write) {
            auto bytesWrite = completion.result();
            if (bytesWrite < 0) {
                throw std::runtime_error("failed to write to file");
            }

            bytesWriteTotal += bytesWrite;
        }

        ring.seen(completion);
    }
}

auto cp(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile)
{
    uringpp::Ring<Data> ring { 64 };
    writeFile(ring, outputFile, readFile(ring, inputFile));
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        return 1;
    }

    const auto inputFile = std::filesystem::path(argv[1]);
    const auto outputFile = std::filesystem::path(argv[2]);

    cp(inputFile, outputFile);

    return 0;
}