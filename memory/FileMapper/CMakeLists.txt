cmake_minimum_required(VERSION 3.20)
project(FileMapper CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(filemapper_lib
        src/mmap_file.cpp
        src/sorter.cpp
)

target_include_directories(filemapper_lib PUBLIC src)

if (WIN32)
    target_link_libraries(filemapper_lib PUBLIC)
endif()

add_executable(sort_data
        src/main.cpp
)

target_link_libraries(sort_data PRIVATE filemapper_lib)

add_executable(generate_data
        tools/generate_data.cpp
)

add_executable(check_sorted
        tools/check_sorted.cpp
)