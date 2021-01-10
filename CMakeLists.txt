cmake_minimum_required(VERSION 3.0.0)
project(layout_bolletta VERSION 0.1.0)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED jsoncpp fmt)

find_package(wxWidgets REQUIRED COMPONENTS base)
include(${wxWidgets_USE_FILE})

set(INCLUDES ${wxWidgets_INCLUDE_DIRS} ${DEPS_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/src/shared)

include_directories(${INCLUDES})

include(src/shared/shared.cmake)
add_library(shared SHARED ${SOURCES_SHARED})
target_link_libraries(shared ${DEPS_LIBRARIES})

include(src/compiler/compiler.cmake)
add_executable(compiler ${SOURCES_COMPILER})
target_link_libraries(compiler ${wxWidgets_LIBRARIES} shared)

include(src/reader/reader.cmake)
add_executable(reader ${SOURCES_READER})
target_link_libraries(reader ${wxWidgets_LIBRARIES} shared)

find_package(wxWidgets REQUIRED COMPONENTS core stc)

include(resources/icons.cmake)
foreach(_f ${ICONS_PNG})
    file(RELATIVE_PATH _r ${PROJECT_SOURCE_DIR}/resources ${PROJECT_SOURCE_DIR}/${_f})
    list(APPEND ICONS_PNG_RELATIVE ${_r})
endforeach()
add_custom_command(
    OUTPUT images.o
    COMMAND ld -r -b binary -o ${PROJECT_BINARY_DIR}/images.o ${ICONS_PNG_RELATIVE}
    DEPENDS ${ICONS_PNG}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/resources)

include(src/editor/editor.cmake)
add_executable(editor ${SOURCES_EDITOR} images.o resources/resources.rc)
target_link_libraries(editor ${wxWidgets_LIBRARIES} shared)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++20")