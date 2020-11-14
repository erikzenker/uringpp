#include <uringpp/uringpp.h>

#include <fcntl.h>
#include <stdio.h>

#include <algorithm>
#include <deque>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <vector>
#include <coroutine>

#include "cppcoro/generator.hpp"

using namespace std::filesystem;
using namespace cppcoro;

enum class CompletionType : std::uint8_t { Read = 0 };

struct Data {
    Data(CompletionType type, std::size_t offset, std::size_t blockIdx)
        : type(type)
        , offset(offset)
        , blockIdx(blockIdx)
    {
    }
    CompletionType type;
    std::size_t offset;
    std::size_t blockIdx;
};

int openFile(const path& file, int mode)
{
    auto fd = open(file.c_str(), mode);
    if (fd < 0) {
        throw std::runtime_error(std::string("Failed to open file ") + file.c_str());
    }
    return fd;
}

std::vector<std::size_t> split(std::string inputFile, std::size_t maxPerSplit)
{
    auto totalSize = file_size(inputFile);
    std::vector<std::size_t> splits;

    while (totalSize > maxPerSplit) {
        splits.push_back(maxPerSplit);
        totalSize -= maxPerSplit;
    }

    splits.push_back(totalSize);
    return splits;
}

auto read(
    const path& inputFile,
    const std::vector<std::size_t>& splits,
    std::size_t queueSize = 64,
    std::size_t blockSize = 32 * 1024) -> generator<std::vector<std::uint8_t>>
{
    uringpp::Ring<Data> ring { queueSize };
    const auto inputFd = openFile(inputFile, O_RDONLY);
    auto bytesReadEnqueuedTotal = 0;
    std::vector<std::vector<std::uint8_t>> blocks;
    std::vector<std::shared_ptr<Data>> userData;

    for (std::size_t blockIdx = 0; blockIdx < splits.size(); ) {
        while (ring.capacity() && blockIdx < splits.size()) {
            blocks.emplace_back(splits.at(blockIdx));
            userData.push_back(
                std::make_shared<Data>(CompletionType::Read, bytesReadEnqueuedTotal, blockIdx));
            ring.prepare_readv(
                inputFd,
                blocks.at(userData.back()->blockIdx),
                userData.back()->offset,
                userData.back());

            bytesReadEnqueuedTotal += blocks.at(userData.back()->blockIdx).size();
            blockIdx++;
        }

        if (ring.preparedQueueEntries()) {
            ring.submit();
        }

        while (ring.submittedQueueEntries()) {
            auto completion = ring.wait();

            switch (completion.userData()->type) {
            case CompletionType::Read: {
                auto bytesRead = completion.result();
                if (bytesRead < 0) {
                    throw std::runtime_error("failed to read from file");
                }

                blocks.at(completion.userData()->blockIdx).resize(bytesRead);
                co_yield blocks.at(completion.userData()->blockIdx);
                break;
            }
            };
            ring.seen(completion);
        }
    }
}


int main(int argc, char** argv)
{
    if (argc < 2) {
        return 1;
    }

    const auto inputFile = path(argv[1]);
    const auto bufferSize = 1024;
    auto splits = split(inputFile, bufferSize);

    for(auto split : splits){
        std::cout << "split: " << split << std::endl;
    }

    for( auto buffer : read(inputFile, splits)){
        std::cout << std::string{buffer.begin(), buffer.end()} << std::endl;
    }

    return 0;
}
