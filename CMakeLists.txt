cmake_minimum_required(VERSION 3.14)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose type of build (Debug or Release)")
endif(NOT CMAKE_BUILD_TYPE)

project(SPNV VERSION 0.1.0 LANGUAGES CXX)

set(EXECUTABLE_NAME ${PROJECT_NAME})

include(GNUInstallDirs)
include(CheckPIESupported)

check_pie_supported()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS ON)

set(BUILD_DOCUMENTATION 0 CACHE BOOL "Build documentation with Doxygen.")

if(BUILD_DOCUMENTATION)
    find_package(Doxygen)
endif()

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

set(FILENAMES
    panoramawindow
    projector
    scenemetadata
    version
    )

foreach(filename ${FILENAMES})
    set(SOURCES "${SOURCES}" "${PROJECT_SOURCE_DIR}/src/${filename}.cpp")
endforeach(filename)

foreach(filename ${FILENAMES})
    set(HEADERS "${HEADERS}" "${filename}.h")
    configure_file("${PROJECT_SOURCE_DIR}/src/${filename}.h" "${PROJECT_BINARY_DIR}/include/${filename}.h" COPYONLY)
endforeach(filename)

configure_file("${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_BINARY_DIR}/LICENSE" COPYONLY)

set(LIB_SOURCES "${SOURCES}")
set(SOURCES "${SOURCES}" "${PROJECT_SOURCE_DIR}/src/main.cpp")

add_executable("${EXECUTABLE_NAME}-bin" ${SOURCES})
add_library("${EXECUTABLE_NAME}-lib" ${LIB_SOURCES})

set_target_properties("${EXECUTABLE_NAME}-bin" "${EXECUTABLE_NAME}-lib" PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME}
                                                                                   POSITION_INDEPENDENT_CODE ON)
set_target_properties("${EXECUTABLE_NAME}-lib" PROPERTIES VERSION ${PROJECT_VERSION})

target_link_libraries("${EXECUTABLE_NAME}-bin" sfml-graphics sfml-window sfml-system)
target_link_libraries("${EXECUTABLE_NAME}-lib" sfml-graphics sfml-window sfml-system)

include_directories("${PROJECT_BINARY_DIR}/include")
include_directories(${SFML_INCLUDE_DIR})

set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "" FORCE)

set(CXX_FLAGS_RELEASE "-pipe -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -Wall -Wextra"
        CACHE STRING "Flags used by the compiler for release builds.")
set(CXX_FLAGS_DEBUG "-pipe -g -Og -fstack-protector-strong -Wall -Wextra"
        CACHE STRING "Flags used by the compiler for debug builds.")
set(LINKER_FLAGS_RELEASE "-Wl,-z,relro,-z,now"
        CACHE STRING "Some flags used by the linker for release builds.")
set(LINKER_FLAGS_DEBUG "-Wl,-z,relro,-z,now"
        CACHE STRING "Some flags used by the linker for debug builds.")

set(CMAKE_CXX_FLAGS_RELEASE "${CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_DEBUG "${CXX_FLAGS_DEBUG}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${LINKER_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${LINKER_FLAGS_DEBUG}")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${LINKER_FLAGS_DEBUG}")

if(BUILD_DOCUMENTATION AND DOXYGEN_FOUND)
  set(doxyfile_in "${PROJECT_SOURCE_DIR}/doc/Doxyfile.in")
  set(doxyfile "${PROJECT_BINARY_DIR}/Doxyfile")
  set(TEMPLATE_DIR "${PROJECT_SOURCE_DIR}/doc/template")
  set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
  configure_file("${doxyfile_in}" "${doxyfile}" @ONLY)
  add_custom_target(Documentation ALL ${DOXYGEN_EXECUTABLE} "${doxyfile}" WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" VERBATIM)
endif()

install(TARGETS "${EXECUTABLE_NAME}-bin" RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS "${EXECUTABLE_NAME}-lib" LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY "${PROJECT_BINARY_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")
install(FILES "${PROJECT_BINARY_DIR}/LICENSE" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
if(BUILD_DOCUMENTATION AND DOXYGEN_FOUND)
    install(DIRECTORY "${PROJECT_BINARY_DIR}/doc/" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
endif()
