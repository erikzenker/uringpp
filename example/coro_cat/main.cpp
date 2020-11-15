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

auto make_generator
    = [](uringpp::RingService& service,
         const path& inputFile,
         const std::vector<std::size_t>& splits) -> async_generator<std::vector<std::uint8_t>> {

    const auto inputFd = openFile(inputFile, O_RDONLY);
    auto offset = 0;

    for (const auto& bufferSize : splits) {
        std::vector<std::uint8_t> buffer(bufferSize);
        co_await service.read(inputFd, buffer, offset);
        co_yield buffer;

        offset += buffer.size();
    }
};

int main(int argc, char** argv)
{
    uringpp::RingService service;

    cppcoro::sync_wait([&service, argc, argv]() -> cppcoro::task<> {
        if (argc < 2) {
            throw std::runtime_error("usage: cat [FILE]");
        }

        service.run();

        const auto inputFile = path(argv[1]);
        const auto bufferSize = 1024;
        auto splits = split(inputFile, bufferSize);
        auto gen = make_generator(service, inputFile, splits);

        for (auto it = co_await gen.begin(); it != gen.end(); co_await ++it) {
            std::vector<std::uint8_t> vec = *it;
            std::cout << std::string { vec.begin(), vec.end() } << std::endl;
        }

        co_await service.stop();
    }());

    return 0;
}
