cmake_minimum_required(VERSION 3.0.0)
project(layout_bolletta VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED jsoncpp fmt)

find_package(wxWidgets REQUIRED COMPONENTS base)
include(${wxWidgets_USE_FILE})

include_directories(${DEPS_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/src/shared)

include(src/shared/shared.cmake)
add_library(shared SHARED ${SOURCES_SHARED})
target_link_libraries(shared ${DEPS_LIBRARIES})

include(src/compiler/compiler.cmake)
add_executable(compiler ${SOURCES_COMPILER})
target_link_libraries(compiler ${wxWidgets_LIBRARIES} shared)

include(src/reader/reader.cmake)
add_executable(reader ${SOURCES_READER})
target_link_libraries(reader ${wxWidgets_LIBRARIES} shared)

function(embed_resource resource_file_name source_file_name variable_name)

    file(READ ${resource_file_name} hex_content HEX)
    file(SIZE ${resource_file_name} content_size)

    string(REPEAT "[0-9a-f]" 32 column_pattern)
    string(REGEX REPLACE "(${column_pattern})" "\\1\n" content "${hex_content}")

    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " content "${content}")

    string(REGEX REPLACE ", \n?$" "" content "${content}")

    set(array_definition "const char ${variable_name}[${content_size}] = {\n${content}\n};")
    set(length_definition "const int ${variable_name}_length = ${content_size};")

    set(source "// Auto generated file.\n${array_definition}\n${length_definition}\n")

    file(WRITE "${source_file_name}" "${source}")

endfunction()

include(resources/icons.cmake)
foreach(file ${ICONS_PNG})
    get_filename_component(basename "${file}" NAME)
    string(MAKE_C_IDENTIFIER basename "${basename}")
    string(REPLACE "." "_" basename "${basename}")

    set(file_png_c "${CMAKE_BINARY_DIR}/${file}.c")
    list(APPEND ICONS_PNG_C "${file_png_c}")

    embed_resource("${CMAKE_SOURCE_DIR}/${file}" "${file_png_c}" "__resource__${basename}")
endforeach()

find_package(wxWidgets REQUIRED COMPONENTS core stc)

include(src/editor/editor.cmake)
add_executable(editor ${SOURCES_EDITOR} ${ICONS_PNG_C} resources/resources.rc)
target_link_libraries(editor ${wxWidgets_LIBRARIES} shared)

pkg_check_modules(POPPLER poppler>=21.01.0)
if("${POPPLER_FOUND}")
    target_compile_definitions(editor PUBLIC POPPLER_FIX)
endif()