cmake_minimum_required (VERSION 3.10)
project(Cache CXX)

option(BUILD_SHARED_LIBS "Build libraries as DLLs" FALSE)
set(${PROJECT_NAME}_DOXYGEN_OUTPUT_DIRECTORY "" CACHE PATH "Doxygen output directory (empty to disable)")

if(${PROJECT_NAME}_DOXYGEN_OUTPUT_DIRECTORY)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_custom_target(doxygen ALL
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
            ${DOXYGEN_EXECUTABLE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen" VERBATIM
        )
    endif(DOXYGEN_FOUND)
endif()

#set(Cache_VERSION_MAJOR 0)
#set(Cache_VERSION_MINOR 1)
#configure_file("${PROJECT_SOURCE_DIR}/Version.h.in" "${PROJECT_BINARY_DIR}/Version.h")

set(PUBLIC_INCLUDE_PATHS
    include
)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Cache/Cache.h
)
source_group(Sources FILES ${SOURCES})

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${SOURCES})
target_include_directories(${PROJECT_NAME} INTERFACE ${PUBLIC_INCLUDE_PATHS})

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    message(STATUS "Testing is enabled. Turn on BUILD_TESTING to build tests.")
    if(BUILD_TESTING)
        add_subdirectory(test)
    endif()
endif()
