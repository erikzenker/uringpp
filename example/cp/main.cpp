#include <uringpp/uringpp.h>

#include <fcntl.h>
#include <stdio.h>

#include <deque>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace std::filesystem;

enum class CompletionType : std::uint8_t { Read = 0, Write = 1 };

struct Data {
    Data(CompletionType type, std::size_t offset, std::size_t blockIndex)
        : type(type)
        , offset(offset)
        , blockIndex(blockIndex)
    {
    }
    CompletionType type;
    std::size_t offset;
    std::size_t blockIndex;
};

int openFile(const path& file, int mode)
{
    auto fd = open(file.c_str(), mode);
    if (fd < 0) {
        throw std::runtime_error(std::string("Failed to open file ") + file.c_str());
    }
    return fd;
}

auto cp(const path& inputFile, const path& outputFile, std::size_t queueSize = 64, std::size_t blockSize = 32 * 1024)
{
    uringpp::Ring<Data> ring { queueSize };
    const auto inputFd = openFile(inputFile, O_RDONLY);
    const auto outputFd = openFile(outputFile, O_WRONLY);
    const auto inputFileSize = file_size(inputFile);
    auto bytesReadTotal = 0;
    auto t = 0;
    auto bytesReadEnqueuedTotal = 0;
    auto bytesWriteTotal = 0;
    auto blockIndex = 0;
    std::vector<std::vector<std::uint8_t>> blocks;
    std::vector<std::shared_ptr<Data>> userData;

    while (bytesReadTotal < inputFileSize && bytesWriteTotal < inputFileSize) {
        while(ring.capacity() && bytesReadEnqueuedTotal < inputFileSize){
            blocks.emplace_back(blockSize);
            userData.push_back(std::make_shared<Data>(CompletionType::Read, bytesReadEnqueuedTotal, blockIndex));
            ring.prepare_readv(
                inputFd,
                blocks.at(userData.back()->blockIndex),
                userData.back()->offset,
                userData.back());
            bytesReadEnqueuedTotal += blocks.at(userData.back()->blockIndex).size();
            blockIndex++;
        }

        if(ring.preparedQueueEntries()){
            ring.submit();
        }

        while(ring.submittedQueueEntries()){
            auto completion = ring.wait();
            auto data = completion.userData();

            switch(data->type){
                case CompletionType::Read:
                {
                    auto bytesRead = completion.result();
                    if (bytesRead < 0) {
                        throw std::runtime_error("failed to read from file");
                    }

                    blocks.at(data->blockIndex).resize(bytesRead);
                    bytesReadTotal += bytesRead;

                    userData.push_back(std::make_shared<Data>(CompletionType::Write, data->offset, data->blockIndex));
                    ring.prepare_writev(
                        outputFd,
                        blocks.at(userData.back()->blockIndex),
                        userData.back()->offset,
                        userData.back());
                    ring.submit();
                    break;   
                }
                case  CompletionType::Write:      
                {
                    auto bytesWrite = completion.result();

                    if (bytesWrite < 0) {
                        throw std::runtime_error(std::string("failed to write to file: ") + strerror(-bytesWrite) );
                    }

                    bytesWriteTotal += bytesWrite;
                    break;
                }                
            };
            // ring.seen(completion);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        return 1;
    }

    const auto inputFile = path(argv[1]);
    const auto outputFile = path(argv[2]);

    cp(inputFile, outputFile);

    return 0;
}