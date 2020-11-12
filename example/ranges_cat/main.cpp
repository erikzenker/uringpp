// #include <uringpp/uringpp.h>

// #include <fcntl.h>
// #include <stdio.h>

// #include <algorithm>
// #include <deque>
// #include <filesystem>
// #include <iostream>
// #include <ranges>
// #include <vector>

// using namespace std::filesystem;

// enum class CompletionType : std::uint8_t { Read = 0 };

// struct Data {
//     Data(CompletionType type, std::size_t offset, std::size_t blockIndex)
//         : type(type)
//         , offset(offset)
//         , blockIndex(blockIndex)
//     {
//     }
//     CompletionType type;
//     std::size_t offset;
//     std::size_t blockIndex;
// };

// int openFile(const path& file, int mode)
// {
//     auto fd = open(file.c_str(), mode);
//     if (fd < 0) {
//         throw std::runtime_error(std::string("Failed to open file ") + file.c_str());
//     }
//     return fd;
// }

// std::vector<std::size_t> split(std::string inputFile, std::size_t maxPerSplit)
// {
//     auto totalSize = file_size(inputFile);
//     std::vector<std::size_t> splits;

//     while (totalSize > maxPerSplit) {
//         splits.push_back(maxPerSplit);
//         totalSize -= maxPerSplit;
//     }

//     splits.push_back(totalSize);
//     return splits;
// }

// auto readUring(const path& inputFile, const std::vector<std::size_t>& splits, std::size_t queueSize = 64, std::size_t blockSize = 32 * 1024) -> generator<std::vector<std::uint8_t>>
// {
//     uringpp::Ring<Data> ring { queueSize };
//     const auto inputFd = openFile(inputFile, O_RDONLY);
//     const auto inputFileSize = file_size(inputFile);
//     auto bytesReadTotal = 0;
//     auto t = 0;
//     auto bytesReadEnqueuedTotal = 0;
//     auto blockIndex = 0;
//     std::vector<std::vector<std::uint8_t>> blocks;
//     std::vector<std::shared_ptr<Data>> userData;

//     for(const auto& split : splits){
//         while (ring.capacity() && bytesReadEnqueuedTotal < inputFileSize) {
//             blocks.emplace_back(blockSize);
//             userData.push_back(
//                 std::make_shared<Data>(CompletionType::Read, bytesReadEnqueuedTotal, blockIndex));
//             ring.prepare_readv(
//                 inputFd,
//                 blocks.at(userData.back()->blockIndex),
//                 userData.back()->offset,
//                 userData.back());
//             bytesReadEnqueuedTotal += blocks.at(userData.back()->blockIndex).size();
//             blockIndex++;
//         }

//         if (ring.preparedQueueEntries()) {
//             ring.submit();
//         }

//         while (ring.submittedQueueEntries()) {
//             auto completion = co_await ring.wait();
//             auto data = completion.userData();

//             switch (data->type) {
//             case CompletionType::Read: {
//                 auto bytesRead = completion.result();
//                 if (bytesRead < 0) {
//                     throw std::runtime_error("failed to read from file");
//                 }

//                 blocks.at(data->blockIndex).resize(bytesRead);
//                 bytesReadTotal += bytesRead;
//                 co_yield blocks.at(data->blockIndex);
//                 break;
//             }
//             };
//             ring.seen(completion);
//         }
//     }
//     }


//     while (bytesReadTotal < inputFileSize) {
//         while (ring.capacity() && bytesReadEnqueuedTotal < inputFileSize) {
//             blocks.emplace_back(blockSize);
//             userData.push_back(
//                 std::make_shared<Data>(CompletionType::Read, bytesReadEnqueuedTotal, blockIndex));
//             ring.prepare_readv(
//                 inputFd,
//                 blocks.at(userData.back()->blockIndex),
//                 userData.back()->offset,
//                 userData.back());
//             bytesReadEnqueuedTotal += blocks.at(userData.back()->blockIndex).size();
//             blockIndex++;
//         }

//         if (ring.preparedQueueEntries()) {
//             ring.submit();
//         }

//         while (ring.submittedQueueEntries()) {
//             auto completion = ring.wait();
//             auto data = completion.userData();

//             switch (data->type) {
//             case CompletionType::Read: {
//                 auto bytesRead = completion.result();
//                 if (bytesRead < 0) {
//                     throw std::runtime_error("failed to read from file");
//                 }

//                 blocks.at(data->blockIndex).resize(bytesRead);
//                 bytesReadTotal += bytesRead;

//                 break;
//             }
//             };
//             ring.seen(completion);
//         }
//     }
//     return blocks;
// }

// class Read {
//   public:
//     Read(std::filesystem::path path)
//         : m_path(path)
//     {
//     }

//     std::vector<std::uint8_t> operator()()
//     {
//     }

//     std::filesystem::path m_path;
// };

// Read read(std::filesystem::path path)
// {
//     return Read { path };
// }

// auto operator|(std::vector<std::size_t> splits, Read read) -> auto
// {
//     return readUring(read.m_path, splits);
// }

// auto print = [](auto element) {
//     std::cout << element << std::endl;
// };

// auto toString = [](const std::vector<std::uint8_t>& vec) {
//     return std::string { vec.begin(), vec.end() };
// };

// int main(int argc, char** argv)
// {
//     if (argc < 2) {
//         return 1;
//     }

//     const auto inputFile = path(argv[1]);

//     const auto bufferSize = 1024;

//     auto vec = split(inputFile, bufferSize);

//     auto rng = vec | read(inputFile) | std::ranges::views::transform(toString);

//     std::ranges::for_each(vec, print);

//     return 0;
// }


#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>
 
auto switch_to_new_thread(std::jthread& out) {
  struct awaitable {
    std::jthread* p_out;
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) {
      std::jthread& out = *p_out;
      if (out.joinable())
        throw std::runtime_error("Output jthread parameter not empty");
      out = std::jthread([h] { h.resume(); });
      // Potential undefined behavior: accessing potentially destroyed *this
      // std::cout << "New thread ID: " << p_out->get_id() << '\n';
      std::cout << "New thread ID: " << out.get_id() << '\n'; // this is OK
    }
    void await_resume() {}
  };
  return awaitable{&out};
}
 
struct task{
  struct promise_type {
    task get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};
 
task resuming_on_new_thread(std::jthread& out) {
  std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';
  co_await switch_to_new_thread(out);
  // awaiter destroyed here
  std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
}
 
int main() {
  std::jthread out;
  resuming_on_new_thread(out);
}