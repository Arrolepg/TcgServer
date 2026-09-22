cmake_minimum_required(VERSION 3.15)
project(TcgServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

add_executable(TcgServer
    src/main.cpp

    src/server/Server.cpp
    src/server/Server.h
    src/session/Session.cpp
    src/session/Session.h
    src/room/Room.cpp
    src/room/Room.h
    src/protocol/Protocol.cpp
    src/protocol/Protocol.h
    src/logger/Logger.cpp
    src/logger/Logger.h
    src/utils/Utils.cpp
    src/utils/Utils.h
)

target_include_directories(TcgServer PRIVATE src)

target_link_libraries(TcgServer PRIVATE
    Boost::boost
    Threads::Threads
)

if(WIN32)
    target_compile_definitions(TcgServer PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(TcgServer PRIVATE ws2_32 mswsock)
endif()