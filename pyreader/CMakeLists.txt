find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(PythonLibs REQUIRED)

include_directories(${PYTHON_INCLUDE_DIRS})
Python3_add_library(pyreader pyreader.cpp)
target_link_libraries(pyreader PRIVATE layout)

install(TARGETS pyreader DESTINATION bin COMPONENT "EXT")