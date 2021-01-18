add_executable(compiler compiler.cpp)
target_link_libraries(compiler layout)

add_executable(reader reader.cpp)
target_link_libraries(reader layout)