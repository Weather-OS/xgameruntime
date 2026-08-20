cmake_minimum_required( VERSION 3.20 )

project( xgameruntime LANGUAGES C CXX )

include( ExternalProject )

find_program(WIDL
        NAMES
        widl
        x86_64-w64-mingw32-widl
        REQUIRED )

message( STATUS "Using widl: ${WIDL}" )

# ------------------------------------------------------------
# IDL generation
# ------------------------------------------------------------

set(IDL_SOURCE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/include )

set(IDL_BINARY_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/include )

file(GLOB_RECURSE IDL_FILES
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