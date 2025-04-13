cmake_minimum_required(VERSION 3.30)
project(visca_server)

set(CMAKE_CXX_STANDARD 14)

option(BUILD_TESTS false) # don't need CppLinuxSerial tests
add_subdirectory(modules/CppLinuxSerial)

add_executable(visca_server main.cpp)
target_include_directories(visca_server PUBLIC modules/args/)

# Force the script to use FetchContent and not CPM internally to fetch asio
option(ASIO_USE_CPM "Download Asio with CPM instead of FetchContent" OFF)
option(
        ASIO_CPM_FETCHCONTENT_COMPAT
        "Should asio be declared with FetchContent functions to be compatible. This doesn't not allow CPM cache to work."
        ON
)
# Download this repository
include(FetchContent)
FetchContent_Declare(
        asiocmake
        GIT_REPOSITORY "https://github.com/OlivierLDff/asio.cmake"
        GIT_TAG        "main"
)
FetchContent_MakeAvailable(asiocmake)


target_link_libraries(visca_server CppLinuxSerial::CppLinuxSerial asio::asio)

