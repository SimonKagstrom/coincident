cmake_minimum_required (VERSION 2.6)

project (coincident-ut)
set (CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH}
                        ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
find_package (LibUDIS86 REQUIRED)
find_package (LibCRPCUT REQUIRED)

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
include_directories (${LIBUDIS86_INCLUDE_DIRS})
include_directories (${LIBCRPCUT_INCLUDE_DIRS})
link_directories (/home/ska/local/lib)

add_executable (${TGT} ${${TGT}_SRCS})

target_link_libraries(${TGT}
	${LIBCRPCUT_LIBRARIES}
	${LIBUDIS86_LIBRARIES}
	dl
	elf
	gmock
	gtest
	pthread
	udis86)
