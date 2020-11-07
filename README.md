# Uringpp

C++ interface and playground for io_uring / liburing

# Examples
* [naive cp](example/naive_cp/main.cpp)
* [fast cp](example/cp/main.cpp)
* [tcp echo poll](example/tcp_echo_poll/main.cpp)
  * Uses poll to register for async file descriptor notifications. 
    Polling might be necessary to increase the number of pending sockets.
* [tcp echo](example/tcp_echo/main.cpp)
  * Uses the io uring fast poll feature which makes it unnecessary to poll on file descriptors

# Dependencies

* liburing
* Linux >= 5.1