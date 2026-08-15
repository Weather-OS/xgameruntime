cmake_minimum_required( VERSION 3.20 )

project( xgameruntime LANGUAGES C CXX )

include( ExternalProject )

find_program(WIDL
        NAMES
        widl
        x86_64-w64-mingw32-widl )

if(WIDL)
    message(STATUS "Using system widl: ${WIDL}")
else()
    message(STATUS "widl not found; building host widl")

    set(WIDL_SOURCE_DIR
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/widl )

    set(WIDL_BINARY_DIR
            ${CMAKE_CURRENT_BINARY_DIR}/widl-host )

    ExternalProject_Add(widl_host
            SOURCE_DIR ${WIDL_SOURCE_DIR}
            BINARY_DIR ${WIDL_BINARY_DIR}

            CONFIGURE_COMMAND ""

            BUILD_COMMAND
            ${CMAKE_COMMAND} -E env
            BINDIR=${WIDL_BINARY_DIR}
            make -C ${WIDL_SOURCE_DIR} all

            INSTALL_COMMAND "" )

    set(WIDL
            ${WIDL_BINARY_DIR}/widl )

    # Make targets using WIDL wait for the fallback build.
    set(WIDL_DEPENDENCY widl_host)
endif()


# ------------------------------------------------------------
# IDL generation
# ------------------------------------------------------------

set(IDL_SOURCE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/include )

set(IDL_BINARY_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/include )

file(GLOB IDL_FILES
        CONFIGURE_DEPENDS
        ${IDL_SOURCE_DIR}/*.idl )

set(IDL_GENERATED_FILES)

foreach(IDL_FILE ${IDL_FILES})
    get_filename_component(IDL_NAME
            ${IDL_FILE}
            NAME_WE )

    set(GENERATED_H
            ${IDL_BINARY_DIR}/${IDL_NAME}.h )

    set(GENERATED_C
            ${IDL_BINARY_DIR}/${IDL_NAME}.c )

    add_custom_command(
            OUTPUT
            ${GENERATED_H}
            ${GENERATED_C}

            COMMAND
            ${CMAKE_COMMAND} -E make_directory
            ${IDL_BINARY_DIR}

            COMMAND
            ${WIDL}
            -h
            -o ${GENERATED_H}
            ${IDL_FILE}

            COMMAND
            ${WIDL}
            -c
            -o ${GENERATED_C}
            ${IDL_FILE}

            DEPENDS
            ${WIDL_DEPENDENCY}
            ${IDL_FILE}

            VERBATIM )

    list(APPEND IDL_GENERATED_FILES
            ${GENERATED_H}
            ${GENERATED_C} )
endforeach()


# ------------------------------------------------------------
# xgameruntime
# ------------------------------------------------------------

add_library(xgameruntime SHARED
        src/main.cpp
        ${IDL_GENERATED_FILES} )

if(WIDL_DEPENDENCY)
    add_dependencies(xgameruntime ${WIDL_DEPENDENCY})
endif()

if(WIN32)
    set_target_properties(xgameruntime PROPERTIES
            PREFIX "" )
endif()

target_include_directories(xgameruntime
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${IDL_BINARY_DIR} )

target_compile_features(xgameruntime
        PUBLIC
        cxx_std_23 )