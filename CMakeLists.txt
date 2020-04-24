cmake_minimum_required(VERSION 3.5)

project(pulsartests LANGUAGES CXX VERSION 0.1.0)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL 1.1 REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.67 REQUIRED COMPONENTS
        system
        program_options
        coroutine
        log
        )

include_directories(${Boost_INCLUDE_DIR})

find_library(PULSAR pulsar)
if (${PULSAR} STREQUAL "PULSAR-NOTFOUND")
    message(FATAL_ERROR, "Missing pulsar client library")
endif()

message(STATUS "Using Pulsar client: ${PULSAR}")

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/Sender.cpp
    src/Receiver.cpp
    src/pulsartests.h
    )

target_link_libraries(${PROJECT_NAME}
    PUBLIC ${PULSAR} ${Boost_LIBRARIES} stdc++fs
    )
target_compile_definitions(${PROJECT_NAME} PUBLIC
    -DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1
    -DPROJECT_VERSION=\"${PROJECT_VERSION}\"
    )
