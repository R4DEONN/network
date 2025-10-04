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
)
