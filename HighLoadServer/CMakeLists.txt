cmake_minimum_required(VERSION 3.22)
project(HighLoadServer)

set(CMAKE_CXX_STANDARD 23)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(DEBUG_TRACE_EXECUTION true)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/server/Server.cpp
        src/server/Server.h
        src/client/Client.cpp
        src/client/Client.h
        src/socket/Socket.cpp
        src/socket/Socket.h
        src/socket/TcpClient.cpp
        src/socket/TcpClient.h
        src/socket/TcpServer.cpp
        src/socket/TcpServer.h
        src/common/parseQuery.h
        src/common/Query.h
        src/common/printInfo.h
        src/common/constructQuery.h
        src/socket/EpollServer.cpp
        src/socket/EpollServer.h
        src/common/ThreadPool.cpp
        src/common/ThreadPool.h
)
