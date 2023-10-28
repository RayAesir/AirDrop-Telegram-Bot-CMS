cmake_minimum_required(VERSION 3.25)
project(AirDropBot VERSION 0.9.9)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

# vcpkg dependencies
set(Boost_USE_MULTITHREADED ON)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(CURL)
find_package(spdlog CONFIG REQUIRED)
find_package(uni-algo CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    add_definitions(-DHAVE_CURL)
endif()

# set path to TgBot library
# https://github.com/reo7sp/tgbot-cpp/blob/master/samples/echobot/CMakeLists.txt
set(TG_LIB "UNSUPPORTED COMPILER")
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
set(TG_LIB ${VCPKG_INSTALLED_DIR}/x64-windows/lib/TgBot.lib)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
set(TG_LIB ${VCPKG_INSTALLED_DIR}/x64-linux/lib/libTgBot.a)
endif()

# gather all output in the local bin folder
# generator expressions force Visual Studio to use specified path
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<0:>${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY $<0:>${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY $<0:>${CMAKE_SOURCE_DIR}/bin)

# create source group
include(cmake/sourcelist.cmake)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

include(cmake/flags.cmake)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${TG_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    spdlog::spdlog
    uni-algo::uni-algo
    yaml-cpp
)
