cmake_minimum_required(VERSION 3.0.0)
project(bill_layout_script VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_DEBUG_POSTFIX _d)

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

find_package(fmt REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED poppler-cpp)

find_package(wxWidgets REQUIRED COMPONENTS base)
include(${wxWidgets_USE_FILE})

add_library(bls SHARED ${libbls_SRCS})
target_include_directories(bls PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/decimal_for_cpp/include
    ${POPPLER_INCLUDE_DIRS}
)
target_link_libraries(bls
    ${wxWidgets_LIBRARIES}
    ${POPPLER_LIBRARIES}
    fmt::fmt-header-only
)

add_executable(blsexec src/main.cpp)
target_include_directories(blsexec PRIVATE ${JSON_INC_PATH})
target_link_libraries(blsexec bls jsoncpp_static)

install(TARGETS bls blsexec)