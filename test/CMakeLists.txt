cmake_minimum_required (VERSION 2.6)


# ====================================
# project name and version
# ====================================
project (ut)
set (TGT ut)

set (${TGT}_SRCS
    ../src/disassembly.cc
    main.cc
    tests-disassembly.cc
    )
set (CMAKE_BUILD_TYPE debug)


include_directories (../src/include/)
include_directories (/home/ska/local/include)
link_directories (/home/ska/local/lib)

add_executable (${PROJECT_NAME} ${${TGT}_SRCS})

target_link_libraries(${TGT} crpcut udis86)
