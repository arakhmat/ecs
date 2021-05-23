cmake_minimum_required(VERSION 3.11)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang")

project(ecs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-std=c++1z")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -glldb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION True)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

set(CONDA_ENV ecs)
set(CONDA_ENV_PY_VERSION 3.8)

if (NOT DEFINED ENV{ANACONDA_PATH})
    message(FATAL_ERROR "ANACONDA_PATH needs to be set." )
endif()

if (DEFINED ENV{CONDA_ENVS_PATH})
    set(CONDA_ENVS_PATH $ENV{CONDA_ENVS_PATH})
else()
    set(CONDA_ENVS_PATH $ENV{ANACONDA_PATH}/envs)
endif()

set(CONDA_ENV_PATH ${CONDA_ENVS_PATH}/${CONDA_ENV})

set(PYTHON_EXECUTABLE  ${CONDA_ENV_PATH}/bin/python)
set(PYTHON_INCLUDE_DIR  ${CONDA_ENV_PATH}/include/python${CONDA_ENV_PY_VERSION}m)
if(UNIX AND NOT APPLE)
    set(PYTHON_LIBRARY  ${CONDA_ENV_PATH}/lib/libpython${CONDA_ENV_PY_VERSION}m.so)
else()
    set(PYTHON_LIBRARY  ${CONDA_ENV_PATH}/lib/libpython${CONDA_ENV_PY_VERSION}m.dylib)
endif()


find_package(Boost REQUIRED)
find_package(Catch2 REQUIRED)
add_subdirectory(third_party/pybind11)


add_executable(
        ecs_cpp_tests
        ecs_cpp/tests/main.cpp
        ecs_cpp/tests/test_mutable_ecs.cpp
)

target_include_directories(
        ecs_cpp_tests
        PRIVATE
        ecs_cpp/src
)

target_link_libraries(
        ecs_cpp_tests
        PRIVATE
        c++
        Catch2::Catch2
)

target_compile_options(
        ecs_cpp_tests
        PRIVATE
        -fPIC
        -pedantic
        -Werror
        -Wall
        -Wextra
        -Wno-unused-command-line-argument
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-c11-extensions
        -Wno-error=deprecated-declarations
)

pybind11_add_module(
        ecs_cpp
        MODULE
        ecs_cpp/python_bindings/ecs_cpp.cpp
        ecs_cpp/python_bindings/mutable_ecs.cpp
)

target_include_directories(
        ecs_cpp
        PRIVATE
        ecs_cpp/src
)

target_link_libraries(
        ecs_cpp
        PRIVATE
        c++
)

target_compile_options(
        ecs_cpp
        PRIVATE
        -fPIC
        -pedantic
        -Werror
        -Wall
        -Wextra
        -Wno-unused-command-line-argument
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-c11-extensions
        -Wno-error=deprecated-declarations
)

target_compile_definitions(
        ecs_cpp
        PRIVATE
        ECDB_PYTHON_WRAPPER
)
