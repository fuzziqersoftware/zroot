cmake_minimum_required(VERSION 3.10)



# Project setup

project(zroot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

include_directories("/usr/local/include")
link_directories("/usr/local/lib")



# Executable definitions

add_executable(zroot Complex.cc Iterate.cc JuliaSet.cc Main.cc)
target_link_libraries(zroot phosg)



# Installation configuration

install(TARGETS zroot DESTINATION bin)
