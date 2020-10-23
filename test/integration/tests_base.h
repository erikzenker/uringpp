#pragma once

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <vector>

inline auto getFileDescriptor(const std::filesystem::path& file) -> int
{
    auto fd = open(file.c_str(), O_RDWR);
    if (fd < 0) {
        throw std::runtime_error("Failed to open file");
    }
    return fd;
}

inline auto readFile(const std::filesystem::path& path) -> std::vector<std::uint8_t>
{
    std::ifstream stream(path.filename(), std::ios::binary);
    std::vector<std::uint8_t> fileContent;

    std::string line;
    while (std::getline(stream, line)) {
        fileContent.insert(fileContent.end(), line.begin(), line.end());
    }

    return fileContent;
}
