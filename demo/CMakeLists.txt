# Demo programs that exercise the project's utility library.
#
# - server         : blocking + thread-per-connection
# - epoll_server   : non-blocking + EpollEventLoop (Linux only)
# - client         : line-driven echo client

add_executable(demo_server server.cpp)
target_link_libraries(demo_server PRIVATE util_debug)

add_executable(demo_client client.cpp)
target_link_libraries(demo_client PRIVATE util_debug)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(demo_epoll_server epoll_server.cpp)
  target_link_libraries(demo_epoll_server PRIVATE util_debug)
endif()
