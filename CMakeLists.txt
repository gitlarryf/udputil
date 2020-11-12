cmake_minimum_required(VERSION 2.4)
cmake_policy(SET CMP0054 NEW)
project(udputil)

#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY bin)
set(CMAKE_CXX_STANDARD 11)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_compile_options(/EHsc /W4 /WX /FS /wd4324)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    add_compile_options(-Wall -Wextra -Werror -g)
endif ()

add_executable(udputil
    udputil.cpp
)

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
    include_directories(/usr/local/include)
endif ()

if (WIN32)
    target_link_libraries(udputil ws2_32)
endif (WIN32)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    target_link_libraries(udputil dl)
endif ()
if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    target_link_libraries(udputil pthread)
endif ()

