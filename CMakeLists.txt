cmake_minimum_required(VERSION 3.0.0)
project(layout_bolletta VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

set(liblayout_SRCS
src/keywords.cpp
src/parser.cpp
src/parsestr.cpp
src/lexer.cpp
src/functions.cpp
src/reader.cpp
src/variable.cpp
src/bytecode.cpp
src/layout.cpp
src/pdf_document.cpp
src/utils.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FMT REQUIRED fmt)
pkg_check_modules(POPPLER REQUIRED poppler-cpp)

find_package(wxWidgets REQUIRED COMPONENTS base)
include(${wxWidgets_USE_FILE})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/decimal_for_cpp/include
    ${FMT_INCLUDE_DIRS}
    ${POPPLER_INCLUDE_DIRS})

add_library(layout SHARED ${liblayout_SRCS})
target_link_libraries(layout ${wxWidgets_LIBRARIES} ${FMT_LIBRARIES} ${POPPLER_LIBRARIES})

add_subdirectory(editor)

pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_executable(reader src/main.cpp)
add_executable(compiler src/compiler.cpp)

target_include_directories(reader PRIVATE ${JSONCPP_INCLUDE_DIRS})
target_link_libraries(reader layout ${JSONCPP_LIBRARIES})

target_include_directories(compiler PRIVATE ${JSONCPP_INCLUDE_DIRS})
target_link_libraries(compiler layout ${JSONCPP_LIBRARIES})

set(ENABLE_PYREADER OFF CACHE BOOL "Enable Pyreader")

if(ENABLE_PYREADER)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    find_package(PythonLibs REQUIRED)

    Python3_add_library(pyreader src/pyreader.cpp ${liblayout_SRCS})
    target_include_directories(pyreader PRIVATE ${PYTHON_INCLUDE_DIRS})
    target_link_libraries(pyreader PRIVATE ${wxWidgets_LIBRARIES} ${FMT_LIBRARIES} ${POPPLER_LIBRARIES})
    install(TARGETS pyreader DESTINATION "${CMAKE_SOURCE_DIR}/script" COMPONENT "EXT")
endif()