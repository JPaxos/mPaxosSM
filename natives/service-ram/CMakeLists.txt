cmake_minimum_required (VERSION 3.12)

project(jpaxos-service-ram)

set(CMAKE_CXX_STANDARD 20)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

add_compile_options(-Wall -Wextra -g)

find_package(JNI)

if(NOT JNI_FOUND)
        message(FATAL_ERROR "JNI not found")
endif()

include_directories(${JNI_INCLUDE_DIRS} ..)

add_library(jpaxos-service SHARED 
                jpaxos-service.cpp
                jni-bloat.cpp
                simple-kv-service.cpp
           )

#   .---------.
#   | ACHTUNG |    "debug" in target_link_libraries is a keyword
#   |    ☠    |    and means that forthcoming libs are linked in
#   |  MINEN  |    debug configurations only.
#   '---------'

target_link_libraries(jpaxos-service debug crypto)

