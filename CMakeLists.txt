cmake_minimum_required(VERSION 3.20)

project(Lavoe_Salsa
    VERSION 0.1.0
    DESCRIPTION "Visualizador de audio overlay (homenagem a Hector Lavoe) - C++20 + Lua 5.4"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_PYTHON "Compila os bindings Python (pybind11)" ON)

# ---------------------------------------------------------------- deps
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)

# Lua 5.4: tenta varios nomes de .pc
pkg_check_modules(LUA IMPORTED_TARGET lua5.4)
if(NOT LUA_FOUND)
    pkg_check_modules(LUA IMPORTED_TARGET lua-5.4)
endif()
if(NOT LUA_FOUND)
    pkg_check_modules(LUA IMPORTED_TARGET lua54)
endif()
if(NOT LUA_FOUND)
    pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua)
endif()

find_package(X11 REQUIRED COMPONENTS Xrender Xext)

# ---------------------------------------------------------------- flags
add_library(lavoe_project_flags INTERFACE)
target_compile_features(lavoe_project_flags INTERFACE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lavoe_project_flags INTERFACE -Wall -Wextra)
endif()

# ---------------------------------------------------------------- lib dsp (header-only)
add_library(lavoe_dsp INTERFACE)
target_include_directories(lavoe_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(lavoe_dsp INTERFACE lavoe_project_flags PkgConfig::FFTW3F)

# ---------------------------------------------------------------- config padrao embutido
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/lavoe.lua LAVOE_DEFAULT_CONFIG_BODY)
set(LAVOE_DEFAULT_CONFIG "R\"LAVOECFG(${LAVOE_DEFAULT_CONFIG_BODY})LAVOECFG\"")
configure_file(cmake/default_config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/default_config.hpp @ONLY)

# ---------------------------------------------------------------- binario principal
add_executable(lavoe-salsa
    src/main.cpp
    src/core/cli.cpp
    src/core/config.cpp
    src/core/geometry.cpp
    src/audio/pulse_capture.cpp
    src/render/x_renderer.cpp)

target_include_directories(lavoe-salsa PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(lavoe-salsa PRIVATE
    lavoe_dsp
    lavoe_project_flags
    PkgConfig::PULSE
    PkgConfig::LUA
    X11::X11
    X11::Xrender
    X11::Xext
    Threads::Threads)

install(TARGETS lavoe-salsa RUNTIME DESTINATION bin)

# ---------------------------------------------------------------- bindings python (opcional)
if(BUILD_PYTHON)
    find_package(pybind11 CONFIG QUIET)
    if(pybind11_FOUND)
        pybind11_add_module(lavoe_py python/bindings.cpp)
        target_link_libraries(lavoe_py PRIVATE lavoe_dsp lavoe_project_flags)
        # numpy (pybind11/numpy.h)
        execute_process(
            COMMAND python3 -c "import numpy; print(numpy.get_include())"
            OUTPUT_VARIABLE NUMPY_INCLUDE_DIR
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NUMPY_INCLUDE_DIR)
            target_include_directories(lavoe_py PRIVATE ${NUMPY_INCLUDE_DIR})
        endif()
        message(STATUS "Bindings Python (lavoe_py): habilitados")
    else()
        message(STATUS "pybind11 nao encontrado - pulando bindings Python")
    endif()
endif()
