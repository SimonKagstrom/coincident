cmake_minimum_required (VERSION 2.6)


# ====================================
# project name and version
# ====================================
project (race-condition-finder)
set (PROJECT_VERSION_MAJOR  1)
set (PROJECT_VERSION_MINOR  0)
set (PROJECT_VERSION_PATCH  0)
set (PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set (LIB coincident)
set (TGT self-test)

set (CMAKE_BUILD_TYPE debug)

set (${LIB}_SRCS
	src/apis/pthreads/mutex.cc
	src/apis/semaphore.cc
	src/controller.cc
	src/disassembly.cc
	src/elf.cc
	src/ptrace.cc
	src/thread-ia32.cc
    )
    
set (${TGT}_SRCS
	self-test/main.cc
	)

include_directories (src/include/)
include_directories (/home/ska/local/include)
link_directories (/home/ska/local/lib)


add_library (${LIB} ${${LIB}_SRCS})

add_executable (${TGT} ${${TGT}_SRCS})

target_link_libraries(${TGT} ${LIB} udis86 elf gmock gtest crpcut)
