cmake_minimum_required(VERSION 3.6.0)
project(bill_layout_script VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_DEBUG_POSTFIX _d)

if(MSVC)
    add_compile_options("/Zc:__cplusplus")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

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

find_package(Boost REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(poppler REQUIRED poppler-cpp IMPORTED_TARGET)

find_package(wxWidgets REQUIRED COMPONENTS base)
include(${wxWidgets_USE_FILE})

add_library(bls SHARED ${libbls_SRCS})
target_include_directories(bls PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/decimal_for_cpp/include
    ${Boost_INCLUDE_DIRS}
)

set(bls_libraries
    ${wxWidgets_LIBRARIES}
    PkgConfig::poppler
)

if(MSVC AND MSVC_VERSION GREATER_EQUAL 1929)
    target_compile_definitions(bls PUBLIC HAVE_STD_FORMAT)
else()
    find_package(fmt REQUIRED)
    list(APPEND bls_libraries fmt::fmt-header-only)
endif()

target_link_libraries(bls ${bls_libraries})

find_package(jsoncpp REQUIRED)

add_executable(blsexec src/main.cpp)
target_include_directories(blsexec PRIVATE ${JSON_INC_PATH})
target_link_libraries(blsexec bls jsoncpp_lib)

install(TARGETS bls blsexec)