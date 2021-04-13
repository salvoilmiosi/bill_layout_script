set(readergui_SRCS
src/main.cpp
src/gui/readergui.cpp
)

find_package(wxWidgets REQUIRED COMPONENTS core)
include(${wxWidgets_USE_FILE})

include_directories(${liblayout_INCLUDE_DIRS})

add_executable(layout_readergui ${readergui_SRCS} resources/resources.rc)
set_target_properties(layout_readergui PROPERTIES PREFIX "../")
target_link_libraries(layout_readergui ${wxWidgets_LIBRARIES} layout)