cmake_minimum_required (VERSION 2.6)


# ====================================
# project name and version
# ====================================
project (ut)
set (TGT ut)

set (TEST_ELF test-elf)
set (${TEST_ELF}_SRCS
	binary-build.c
)

set (${TGT}_SRCS
    ../src/apis/semaphore.cc
    ../src/disassembly.cc
    ../src/elf.cc
    main.cc
    mock-thread.cc
    tests-controller.cc
    tests-disassembly.cc
    tests-elf.cc
    )
set (CMAKE_BUILD_TYPE debug)


include_directories (../src/include/)
include_directories (/home/ska/local/include)
link_directories (/home/ska/local/lib)

add_executable (${TEST_ELF} ${${TEST_ELF}_SRCS})

add_executable (${TGT} ${${TGT}_SRCS})

target_link_libraries(${TGT} crpcut udis86 elf gmock gtest)
