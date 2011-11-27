cmake_minimum_required (VERSION 2.6)

# ====================================
# project name and version
# ====================================
project (race-condition-finder)
set (PROJECT_VERSION_MAJOR  1)
set (PROJECT_VERSION_MINOR  0)
set (PROJECT_VERSION_PATCH  0)
set (PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set (CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH}
                        ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package (LibUDIS86 REQUIRED)
find_package (LibCRPCUT REQUIRED)

set (LIB coincident)
set (TGT self-test)

set (CMAKE_BUILD_TYPE debug)

set (${LIB}_SRCS
	src/apis/pthreads/mutex.cc
	src/apis/semaphore.cc
	src/apis/semaphore-helpers.cc
	src/controller.cc
	src/disassembly.cc
	src/elf.cc
	src/ptrace.cc
	src/thread-ia32.cc
	src/thread.cc
    )
    
set (${TGT}_SRCS
	self-test/main.cc
	)

include_directories (src/include/)
include_directories (${LIBUDIS86_INCLUDE_DIRS})
include_directories (${LIBCRPCUT_INCLUDE_DIRS})


add_library (${LIB} ${${LIB}_SRCS})

add_executable (${TGT} ${${TGT}_SRCS})
target_link_libraries(${TGT}
	${LIB}
	elf
	${LIBCRPCUT_LIBRARIES}
	${LIBUDIS86_LIBRARIES})


# Installation rules
install(FILES
	src/include/coincident/coincident.h
	src/include/coincident/controller.hh
	src/include/coincident/thread.hh
	DESTINATION include/coincident
	)

install(TARGETS ${LIB}
	ARCHIVE DESTINATION lib
	)
