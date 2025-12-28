cmake_minimum_required(VERSION 3.22)
project(UDP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Путь для исполняемых файлов
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/public")

add_executable(UDP-Server
        src/server.cpp
)

add_executable(UDP-Client
        src/client.cpp
)