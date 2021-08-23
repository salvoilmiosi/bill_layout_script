cmake_minimum_required(VERSION 3.6.0)
project(bill_layout_script VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/Zc:__cplusplus")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

add_library(bls SHARED
    src/datetime.cpp
    src/functions.cpp
    src/keywords.cpp
    src/layout.cpp
    src/lexer.cpp
    src/parser.cpp
    src/pdf_document.cpp
    src/reader.cpp
    src/translations.cpp
    src/unicode.cpp
    src/variable.cpp
)
add_library(bls::bls ALIAS bls)

add_subdirectory(messages)
target_link_libraries(bls PUBLIC translation_obj)

target_include_directories(bls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(PkgConfig REQUIRED)
pkg_check_modules(poppler REQUIRED poppler-cpp>=0.88 IMPORTED_TARGET)
target_link_libraries(bls PUBLIC PkgConfig::poppler)

add_subdirectory(external/decimal_for_cpp)
target_link_libraries(bls PUBLIC decimal_for_cpp::decimal_for_cpp)

add_subdirectory(external/magic_enum)
target_link_libraries(bls PUBLIC magic_enum::magic_enum)

set(BOOST_LOCALE_ENABLE_ICONV OFF)
set(BOOST_LOCALE_ENABLE_ICU ON)
set(BOOST_LOCALE_ENABLE_POSIX OFF)
set(BOOST_LOCALE_ENABLE_STD OFF)
set(BOOST_LOCALE_ENABLE_WINAPI OFF)
add_subdirectory(external/boostlocale)
target_link_libraries(bls PUBLIC Boost::locale)

function(check_cpp_function TEST_FILE_NAME OUT_RESULT)
    set(TEST_FILE ".cmake/.test_${TEST_FILE_NAME}.cpp")
    try_compile(TEST_RESULT
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_FILE}
        CXX_STANDARD 20)
    if (TEST_RESULT)
        message("-- Using STL ${TEST_FILE_NAME}")
    endif()
    set(${OUT_RESULT} ${TEST_RESULT} PARENT_SCOPE)
endfunction()

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

find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(blsexec src/main.cpp)
target_link_libraries(blsexec bls::bls Boost::program_options)

install(TARGETS bls blsexec)

option(BLS_BUILD_UTILS "Build BLS Utils" ON)
if(BLS_BUILD_UTILS)
    add_executable(blsdump utils/blsdump.cpp)
    target_link_libraries(blsdump bls::bls)

    add_executable(blssizes utils/sizes.cpp)
    target_link_libraries(blssizes bls::bls)

    install(TARGETS blsdump blssizes)
endif()
