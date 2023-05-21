cmake_minimum_required(VERSION 3.6.0)
project(bill_layout_script VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/Zc:__cplusplus")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

add_library(bls SHARED
    src/utils/translations.cpp
    src/utils/unicode.cpp
    src/datetime.cpp
    src/functions.cpp
    src/keywords.cpp
    src/layout.cpp
    src/lexer.cpp
    src/parser.cpp
    src/pdf_document.cpp
    src/reader.cpp
    src/variable.cpp
)
add_library(bls::bls ALIAS bls)

add_subdirectory(messages)
target_link_libraries(bls PUBLIC translation_obj)

target_include_directories(bls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(PkgConfig REQUIRED)
pkg_check_modules(poppler REQUIRED poppler IMPORTED_TARGET)
target_link_libraries(bls PUBLIC PkgConfig::poppler)

add_subdirectory(external/decimal_for_cpp)
target_link_libraries(bls PUBLIC decimal_for_cpp::decimal_for_cpp)

set(BOOST_LOCALE_ENABLE_ICONV OFF)
set(BOOST_LOCALE_ENABLE_ICU ON)
set(BOOST_LOCALE_ENABLE_POSIX OFF)
set(BOOST_LOCALE_ENABLE_STD OFF)
set(BOOST_LOCALE_ENABLE_WINAPI OFF)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(external/boostlocale)

target_link_libraries(bls PUBLIC Boost::locale)

macro(check_cpp_function TEST_FILE_NAME OUT_RESULT)
    cmake_parse_arguments(CHECK_ARGS "" "" "LIBRARIES" ${ARGN})
    
    set(TEST_FILE ".cmake/.test_${TEST_FILE_NAME}.cpp")
    try_compile(TEST_RESULT
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_FILE}
        CXX_STANDARD 20
        LINK_LIBRARIES ${CHECK_ARGS_LIBRARIES})
    set(${OUT_RESULT} ${TEST_RESULT} PARENT_SCOPE)
endmacro()

check_cpp_function(format HAVE_STD_FORMAT)
if(NOT HAVE_STD_FORMAT)
    target_compile_definitions(bls PUBLIC USE_FMTLIB)
    find_package(fmt REQUIRED)
    if (TARGET fmt::fmt-header-only)
        target_link_libraries(bls PUBLIC fmt::fmt-header-only)
    else()
        target_link_libraries(bls PUBLIC fmt::fmt)
    endif()
endif()

check_cpp_function(map USE_UNORDERED_MAP)
if(USE_UNORDERED_MAP)
    target_compile_definitions(bls PUBLIC USE_UNORDERED_MAP)
endif()

find_package(cxxopts REQUIRED)

add_executable(blsexec src/main.cpp)
target_link_libraries(blsexec bls::bls cxxopts::cxxopts)

add_executable(blsdump src/blsdump.cpp)
target_link_libraries(blsdump bls::bls)

install(TARGETS bls blsexec blsdump)
