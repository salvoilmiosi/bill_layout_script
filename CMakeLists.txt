cmake_minimum_required(VERSION 3.0.0)
project(bill_layout_script VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

set(libbls_SRCS
src/binary_bls.cpp
src/functions.cpp
src/keywords.cpp
src/intl.cpp
src/layout.cpp
src/lexer.cpp
src/parser.cpp
src/pdf_document.cpp
src/reader.cpp
src/unicode.cpp
src/variable.cpp
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

add_library(bls SHARED ${libbls_SRCS})
target_link_libraries(bls ${wxWidgets_LIBRARIES} ${FMT_LIBRARIES} ${POPPLER_LIBRARIES})

pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_executable(reader src/main.cpp)
target_include_directories(reader PRIVATE ${JSONCPP_INCLUDE_DIRS})
target_link_libraries(reader bls ${JSONCPP_LIBRARIES})
