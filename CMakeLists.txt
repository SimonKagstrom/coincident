cmake_minimum_required (VERSION 2.6)


# ====================================
# project name and version
# ====================================
project (race-condition-finder)
set (PROJECT_VERSION_MAJOR  1)
set (PROJECT_VERSION_MINOR  0)
set (PROJECT_VERSION_PATCH  0)
set (PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set (TGT race-condition-finder)

set (${TGT}_SRCS
	src/disassembly.cc
	src/elf.cc
    )

include_directories (${GLIB2_INCLUDE_DIRS})
include_directories (src/include/)
include_directories (/home/ska/local/include)
link_directories (/home/ska/local/lib)

add_executable (${PROJECT_NAME} ${${TGT}_SRCS})

target_link_libraries(${TGT} ${glib2_LIBRARIES} udis86)
