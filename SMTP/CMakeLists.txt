cmake_minimum_required(VERSION 3.22)
project(SMTP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Путь для исполняемых файлов
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/public")

find_package(OpenSSL REQUIRED)

set(SOURCES
        src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
        OpenSSL::SSL
        OpenSSL::Crypto
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
)