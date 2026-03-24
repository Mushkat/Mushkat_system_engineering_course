cmake_minimum_required(VERSION 3.16)
project(fuse_cpu_fs CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

find_package(Catch2 QUIET)
find_package(Doxygen QUIET)

add_library(cpu_core
        src/cpu_executor.cpp
        src/cpu_state.cpp
        src/path_utils.cpp
        src/cpu_fs.cpp
)

target_include_directories(cpu_core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${FUSE3_INCLUDE_DIRS}
)

target_link_libraries(cpu_core
        PUBLIC
        ${FUSE3_LIBRARIES}
)

target_compile_options(cpu_core PRIVATE ${FUSE3_CFLAGS_OTHER})

add_executable(fuse_cpu_fs
        src/main.cpp
)

target_link_libraries(fuse_cpu_fs PRIVATE cpu_core)

if(BUILD_TESTING)
    if(Catch2_FOUND)
        add_executable(cpu_core_tests
                tests/catch_main.cpp
                tests/executor_tests.cpp
                tests/path_tests.cpp
                tests/state_tests.cpp
        )
        target_link_libraries(cpu_core_tests PRIVATE cpu_core Catch2::Catch2)
        add_test(NAME cpu_core_tests COMMAND cpu_core_tests)
    else()
        message(WARNING "Catch2 not found: tests will not be built")
    endif()
endif()

if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating Doxygen documentation"
            VERBATIM
    )
endif()

install(TARGETS fuse_cpu_fs
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

set(CPACK_PACKAGE_NAME "fuse_cpu_fs")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_GENERATOR "TGZ")
include(CPack)