#[[
########################################################################################
##
##  This file is part of SPNV, a simple panorama viewer for Hugin panoramas.
##  Copyright (C) 2022, 2025 M. Frohne
##
##  SPNV is free software: you can redistribute it and/or modify
##  it under the terms of the GNU Affero General Public License as published
##  by the Free Software Foundation, either version 3 of the License,
##  or (at your option) any later version.
##
##  SPNV is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
##  See the GNU Affero General Public License for more details.
##
##  You should have received a copy of the GNU Affero General Public License
##  along with SPNV. If not, see <https://www.gnu.org/licenses/>.
##
########################################################################################
#]]

cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED CACHE{CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

#Tweak CMake defaults for compiler flags

if(NOT MSVC)
    set(DEFAULT_FLAGS "")
    set(DEFAULT_FLAGS_DEBUG "-g -Og")
    set(DEFAULT_FLAGS_MINSIZEREL "-Os -DNDEBUG")
    set(DEFAULT_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(DEFAULT_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
else()
    #Those MSVC flags are probably not tested
    set(DEFAULT_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1")
    set(DEFAULT_FLAGS_MINSIZEREL "/MD /O1 /Ob1 /DNDEBUG")
    set(DEFAULT_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
    set(DEFAULT_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob1 /DNDEBUG")
endif()

if(NOT DEFINED CACHE{CMAKE_CXX_FLAGS})
    if(NOT MSVC)
        set(CMAKE_CXX_FLAGS "${DEFAULT_FLAGS}" CACHE STRING "Flags used by the CXX compiler during all build types." FORCE)
    endif()
endif()
if(NOT DEFINED CACHE{CMAKE_CXX_FLAGS_DEBUG})
    set(CMAKE_CXX_FLAGS_DEBUG "${DEFAULT_FLAGS_DEBUG}" CACHE STRING "Flags used by the CXX compiler during DEBUG builds." FORCE)
endif()
if(NOT DEFINED CACHE{CMAKE_CXX_FLAGS_MINSIZEREL})
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${DEFAULT_FLAGS_MINSIZEREL}" CACHE STRING "Flags used by the CXX compiler during MINSIZEREL builds." FORCE)
endif()
if(NOT DEFINED CACHE{CMAKE_CXX_FLAGS_RELEASE})
    set(CMAKE_CXX_FLAGS_RELEASE "${DEFAULT_FLAGS_RELEASE}" CACHE STRING "Flags used by the CXX compiler during RELEASE builds." FORCE)
endif()
if(NOT DEFINED CACHE{CMAKE_CXX_FLAGS_RELWITHDEBINFO})
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${DEFAULT_FLAGS_RELWITHDEBINFO}" CACHE STRING "Flags used by the CXX compiler during RELWITHDEBINFO builds." FORCE)
endif()

#

project(SPNV VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

include(CheckPIESupported)
check_pie_supported()

#

if(NOT MSVC)
    string(CONCAT TMP_CXX_FLAGS "-Wall;-Wextra;-Wshadow;-pedantic-errors;"
                                "-pipe;-march=x86-64;-mtune=generic;-Wp,-D_FORTIFY_SOURCE=3;-Wp,-D_GLIBCXX_ASSERTIONS;"
                                "-fno-plt;-fstack-protector-strong;-fstack-clash-protection;-fcf-protection=full;-flto=auto")
    set(SPNV_CXX_FLAGS "${TMP_CXX_FLAGS}" CACHE STRING "Additional compiler flags.")
    set(SPNV_LINK_FLAGS "-Wl,--sort-common;-Wl,--as-needed;-Wl,-z,defs;-Wl,-z,relro;-Wl,-z,now;-Wl,-z,pack-relative-relocs"
        CACHE STRING "Additional linker flags.")
    set(SPNV_LINK_FLAGS_DEBUG "" CACHE STRING "Additional linker flags to add for DEBUG builds.")
    set(SPNV_LINK_FLAGS_RELEASE "-Wl,-O1" CACHE STRING "Additional linker flags to add for non-DEBUG builds.")
else()
    #Those MSVC flags are probably not tested
    string(CONCAT TMP_CXX_FLAGS "/W4;/permissive-;"
                                "/GL;/GS;/guard:cf;/guard:ehcont")
    set(SPNV_CXX_FLAGS "${TMP_CXX_FLAGS}" CACHE STRING "Additional compiler flags.")
    set(SPNV_LINK_FLAGS "/CETCOMPAT;/DYNAMICBASE;/OPT;/LTCG" CACHE STRING "Additional linker flags.")
    set(SPNV_LINK_FLAGS_DEBUG "/DEBUG" CACHE STRING "Additional linker flags to add for DEBUG builds.")
    set(SPNV_LINK_FLAGS_RELEASE "" CACHE STRING "Additional linker flags to add for non-DEBUG builds.")
endif()

if(NOT MSVC)
    set(SPNV_ENABLE_GLIBCXX_DEBUG_MODE OFF CACHE BOOL "Add compiler flags to enable the debug mode of libstdc++.")
endif()

set(SPNV_DISABLE_OMP OFF CACHE BOOL "Disable OpenMP parallelization.")
set(SPNV_BUILD_DOCUMENTATION OFF CACHE BOOL "Build documentation with Doxygen.")

set(SPNV_CMAKE_LICENSEDIR "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${CMAKE_PROJECT_NAME}"
    CACHE PATH "Install path for the LICENSE file (relative to CMAKE_INSTALL_PREFIX).")

#

if(NOT MSVC)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()
find_package(Threads REQUIRED)

if(NOT SPNV_DISABLE_OMP)
    find_package(OpenMP REQUIRED)
endif()

find_package(SFML 2.5 REQUIRED COMPONENTS graphics system window)

if(SPNV_BUILD_DOCUMENTATION)
    find_package(Doxygen 1.9.3 REQUIRED)
endif()

#

include_directories("${PROJECT_SOURCE_DIR}/src")

set(SOURCE_FILE_NAMES
    main.cpp
    panoramawindow.cpp
    projector.cpp
    scenemetadata.cpp
    version.cpp
)
set(HEADER_FILE_NAMES
    panoramawindow.h
    projector.h
    scenemetadata.h
    version.h
)

foreach(fileName ${SOURCE_FILE_NAMES})
    list(APPEND SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/${fileName}")
endforeach()

foreach(fileName ${HEADER_FILE_NAMES})
    list(APPEND HEADER_FILES "${PROJECT_SOURCE_DIR}/src/${fileName}")
endforeach()

#

add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${SPNV_CXX_FLAGS}>")

if((NOT MSVC) AND SPNV_ENABLE_GLIBCXX_DEBUG_MODE)
    add_compile_options(-Wp,-D_GLIBCXX_DEBUG -Wp,-D_GLIBCXX_DEBUG_PEDANTIC)
endif()

add_link_options("$<$<LINK_LANGUAGE:CXX>:${SPNV_LINK_FLAGS}>")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_link_options("$<$<LINK_LANGUAGE:CXX>:${SPNV_LINK_FLAGS_DEBUG}>")
else()
    add_link_options("$<$<LINK_LANGUAGE:CXX>:${SPNV_LINK_FLAGS_RELEASE}>")
endif()

#

add_executable("${CMAKE_PROJECT_NAME}" ${SOURCE_FILES} ${HEADER_FILES})
set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}")
set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE Threads::Threads)
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE sfml-graphics sfml-system sfml-window)
if(NOT SPNV_DISABLE_OMP)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${OpenMP_CXX_FLAGS})
    target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE "$<$<LINK_LANGUAGE:CXX>:${OpenMP_CXX_LIB_NAMES}>")
else()
    target_compile_definitions("${CMAKE_PROJECT_NAME}" PRIVATE DISABLE_OMP)
endif()

#

if(SPNV_BUILD_DOCUMENTATION)
    set(DOXYFILE_IN "${PROJECT_SOURCE_DIR}/doc/Doxyfile.in")
    set(DOXYFILE "${PROJECT_BINARY_DIR}/Doxyfile")
    set(DOXYGEN_PROJECT_NUMBER "${CMAKE_PROJECT_VERSION}")
    set(TEMPLATE_DIR "${PROJECT_SOURCE_DIR}/doc/template")
    set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
    configure_file("${DOXYFILE_IN}" "${DOXYFILE}" @ONLY)
    add_custom_target(Documentation ALL ${DOXYGEN_EXECUTABLE} "${DOXYFILE}" WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" VERBATIM)
endif()

#

install(TARGETS "${CMAKE_PROJECT_NAME}" RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${SPNV_CMAKE_LICENSEDIR}")
