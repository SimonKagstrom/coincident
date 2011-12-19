cmake_minimum_required (VERSION 2.6)


# ====================================
# project name and version
# ====================================
project (ut)
set (TGT ut)

set (${TGT}_SRCS
    ../src/apis/pthreads/mutex.cc
	../src/apis/pthreads/pthreads.cc
    ../src/apis/semaphore.cc
	../src/apis/semaphore-helpers.cc
    ../src/disassembly.cc
    ../src/elf.cc
    ../src/thread.cc
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

add_executable (${TGT} ${${TGT}_SRCS})

target_link_libraries(${TGT}
	crpcut
	dl
	elf
	gmock
	gtest
	pthread
	udis86)
