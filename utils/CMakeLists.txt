include_directories(${liblayout_INCLUDE_DIRS} ${DEPS_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

include_directories(${JSONCPP_INCLUDE_DIRS})

add_executable(compiler compiler.cpp)
target_link_libraries(compiler layout)

add_executable(reader reader.cpp)
target_link_libraries(reader layout ${JSONCPP_LIBRARIES})

install(TARGETS compiler reader DESTINATION "${CMAKE_SOURCE_DIR}/bin")

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(PythonLibs REQUIRED)

include_directories(${PYTHON_INCLUDE_DIRS})
Python3_add_library(pyreader pyreader.cpp)
target_link_libraries(pyreader PRIVATE layout)

install(TARGETS pyreader DESTINATION "${CMAKE_SOURCE_DIR}/script" COMPONENT "EXT")