cmake_minimum_required(VERSION 3.6.0)
project(bill_layout_script VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
pkg_check_modules(poppler REQUIRED poppler-cpp>=0.88 IMPORTED_TARGET)

add_library(bls SHARED ${libbls_SRCS})

set(bls_include_dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/decimal_for_cpp/include
    ${Boost_INCLUDE_DIRS}
)

add_subdirectory(cxxopts)
add_subdirectory(json)

set(bls_libraries PkgConfig::poppler)

if(MSVC AND MSVC_VERSION GREATER_EQUAL 1929)
    target_compile_definitions(bls PUBLIC HAVE_STD_FORMAT)
else()
    find_package(fmt REQUIRED)
    if (TARGET fmt::fmt-header-only)
        list(APPEND bls_libraries fmt::fmt-header-only)
    else()
        list(APPEND bls_libraries fmt::fmt)
    endif()
endif()

if((MSVC AND MSVC_VERSION GREATER_EQUAL 1929) OR (CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 11))
    target_compile_definitions(bls PUBLIC HAVE_STD_CALENDAR)
    target_compile_definitions(bls PUBLIC HAVE_STD_LOCALE)
else()
    target_compile_definitions(bls PUBLIC USE_WXWIDGETS)

    find_package(wxWidgets REQUIRED COMPONENTS base)
    include(${wxWidgets_USE_FILE})
    
    list(APPEND bls_libraries ${wxWidgets_LIBRARIES})
endif()

if((MSVC AND MSVC_VERSION GREATER_EQUAL 1914) OR (CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 11))
    target_compile_definitions(bls PUBLIC HAVE_FLOAT_TO_CHARS)
endif()

target_include_directories(bls PUBLIC ${bls_include_dirs})
target_link_libraries(bls ${bls_libraries})

add_executable(blsexec src/main.cpp)
target_link_libraries(blsexec bls cxxopts taocpp-json)

install(TARGETS bls blsexec)

set(BLS_BUILD_UTILS ON CACHE BOOL "Build BLS Utils")

if(${BLS_BUILD_UTILS})
    add_executable(blsdump utils/blsdump.cpp)
    target_link_libraries(blsdump bls cxxopts)

    add_executable(sizes utils/sizes.cpp)
    target_link_libraries(sizes bls)

    install(TARGETS blsdump sizes)
endif()