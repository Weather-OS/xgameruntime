# ------------------------------------------------------------
# IDL generation
# ------------------------------------------------------------

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

    add_custom_command(
            OUTPUT
            ${GENERATED_H}

            COMMAND
            ${CMAKE_COMMAND} -E make_directory
            ${IDL_BINARY_DIR}

            COMMAND
            ${WIDL}
            -h
            -o ${GENERATED_H}
            ${IDL_FILE}

            DEPENDS
            ${IDL_FILE}

            VERBATIM )

    list(APPEND IDL_GENERATED_FILES
            ${GENERATED_H} )
endforeach()

# ------------------------------------------------------------
# xgameruntime
# ------------------------------------------------------------

add_library(xgameruntime SHARED
        main.cpp
        xgameruntime.def
        ${IDL_GENERATED_FILES} )

if(WIN32)
    set_target_properties(xgameruntime PROPERTIES
            PREFIX "" )
endif()

set_target_properties(xgameruntime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )

target_link_libraries( xgameruntime PRIVATE
        xgameruntime-common )

target_link_options(xgameruntime PRIVATE
        -static
        -static-libgcc
        -static-libstdc++ )

target_include_directories(xgameruntime
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${IDL_BINARY_DIR} )

target_compile_features(xgameruntime
        PUBLIC
        cxx_std_23 )