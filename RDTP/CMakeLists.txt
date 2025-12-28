cmake_minimum_required(VERSION 3.22)
project(RDTP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Путь для исполняемых файлов
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/public")

add_executable(RDTP-Sender
        src/sender.cpp
        src/utils.h
)

add_executable(RDTP-Receiver
        src/receiver.cpp
        src/utils.h
)