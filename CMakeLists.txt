cmake_minimum_required(VERSION 3.0.0)
project(layout_bolletta VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

set(liblayout_SRCS
src/assembler.cpp
src/keywords.cpp
src/parser.cpp
src/parsestr.cpp
src/lexer.cpp
src/dates.cpp
src/functions.cpp
src/reader.cpp
src/strings.cpp
src/variable.cpp
src/bytecode.cpp
src/layout.cpp
src/pdf_document.cpp
src/utils.cpp
)

add_subdirectory(subprocess)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/decimal_for_cpp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/subprocess/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED jsoncpp fmt poppler-cpp)

find_package(wxWidgets REQUIRED COMPONENTS base)
include(${wxWidgets_USE_FILE})

include_directories(${DEPS_INCLUDE_DIRS})

add_library(layout SHARED ${liblayout_SRCS})
target_link_libraries(layout ${wxWidgets_LIBRARIES} ${DEPS_LIBRARIES})

add_subdirectory(editor)
add_subdirectory(utils)