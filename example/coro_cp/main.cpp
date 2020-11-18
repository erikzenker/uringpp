#include "uringpp/RingService.h"
#include "uringpp/uringpp.h"

#include <fcntl.h>
#include <stdio.h>

#include <algorithm>
#include <coroutine>
#include <deque>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <vector>

#include "cppcoro/async_generator.hpp"
#include "cppcoro/sync_wait.hpp"

using namespace std::filesystem;
using namespace cppcoro;

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

auto coro_cp
    = [](uringpp::RingService& service,
         const path& inputFile,
         const path& outputFile,
         const std::vector<std::size_t>& splits) -> cppcoro::task<>
{
    const auto inputFd = openFile(inputFile, O_RDONLY);
    const auto outputFd = openFile(outputFile, O_WRONLY);

    service.run();

    auto gen = service.read2(inputFd, splits);
    auto offset = 0;

    for (auto it = co_await gen.begin(); it != gen.end(); co_await ++it) {
        std::vector<std::uint8_t> vec = *it;

        std::cout << std::string { vec.begin(), vec.end() } << std::endl;
        // co_await service.write(outputFd, vec, offset);
        offset += vec.size();
    }

    co_await service.stop();

    // for (const auto& bufferSize : splits) {
    //     auto buffer = co_awai
    //     co_await service.write(outputFd, buffer, offset);
    //     offset += buffer.size();
    // }
};

int main(int argc, char** argv)
{
    uringpp::RingService service;

    cppcoro::sync_wait([&service, argc, argv]() -> cppcoro::task<> {
        if (argc < 3) {
            throw std::runtime_error("usage: coro_cp [INPUT_FILE] [OUTPUT_FILE]");
        }

        const auto inputFile = path(argv[1]);
        const auto outputFile = path(argv[2]);
        const auto bufferSize = 16 * 1024;
        auto splits = split(inputFile, bufferSize);
        co_await coro_cp(service, inputFile, outputFile, splits);
    }());

    return 0;
}
