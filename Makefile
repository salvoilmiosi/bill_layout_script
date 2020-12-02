CXX = g++
CXXFLAGS = -Wall --std=c++17 -Isrc/shared
LDFLAGS = 

SRC_DIR = src
OBJ_DIR = obj/$(build)
BIN_DIR = bin/$(build)

ifndef build
  build = debug
endif

ifeq ($(build),release)
  CXXFLAGS += -O2
  LDFLAGS += -s -DNDEBUG
else ifeq ($(build),debug)
  CXXFLAGS += -g -DDEBUG
else
  $(error "Unknown build option $(build)")
endif

BIN_SHARED = layout_shared.so
BIN_EDITOR = layout_editor
BIN_READER = layout_reader
BIN_COMPILER = layout_compiler

INCLUDE = `wx-config --cxxflags` `pkg-config --cflags jsoncpp fmt`

LIBS_SHARED = `pkg-config --libs jsoncpp fmt`
LIBS_EDITOR = `wx-config --libs std,stc` $(LIBS_SHARED)
LIBS_READER = `wx-config --libs base` $(LIBS_SHARED)
LIBS_COMPILER = $(LIBS_SHARED)

SOURCES_SHARED = $(wildcard $(SRC_DIR)/shared/*.cpp $(SRC_DIR)/shared/**/*.cpp)
OBJECTS_SHARED = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_SHARED)))

SOURCES_EDITOR = $(wildcard $(SRC_DIR)/editor/*.cpp $(SRC_DIR)/editor/**/*.cpp)
OBJECTS_EDITOR = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_EDITOR)))

SOURCES_READER = $(wildcard $(SRC_DIR)/reader/*.cpp $(SRC_DIR)/reader/**/*.cpp)
OBJECTS_READER = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_READER)))

SOURCES_COMPILER = $(wildcard $(SRC_DIR)/compiler/*.cpp $(SRC_DIR)/compiler/**/*.cpp)
OBJECTS_COMPILER = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_COMPILER)))

LAYOUT_DIR = layout
LAYOUTS = $(patsubst $(LAYOUT_DIR)/%.bls,$(LAYOUT_DIR)/%.out,$(wildcard $(LAYOUT_DIR)/*.bls))

SOURCES = $(SOURCES_SHARED) $(SOURCES_EDITOR) $(SOURCES_READER) $(SOURCES_COMPILER)
OBJECTS = $(OBJECTS_SHARED) $(OBJECTS_EDITOR) $(OBJECTS_READER) $(OBJECTS_COMPILER)

IMAGES = $(wildcard resources/*.png)

RESOURCES = $(OBJ_DIR)/images.o

ifeq ($(OS),Windows_NT)
	BIN_SHARED := $(BIN_SHARED:.so=.dll)
	BIN_EDITOR := $(BIN_EDITOR).exe
	BIN_READER := $(BIN_READER).exe
	BIN_COMPILER := $(BIN_COMPILER).exe
	RESOURCES += $(patsubst resources/%.rc,$(OBJ_DIR)/%.res,$(wildcard resources/*.rc))
endif

all: editor reader compiler

release:
	$(MAKE) build=release

layouts: $(LAYOUTS)

clean:
	rm -f $(BIN_DIR)/$(BIN_EDITOR) $(BIN_DIR)/$(BIN_READER) $(BIN_DIR)/$(BIN_COMPILER) $(BIN_DIR)/$(BIN_SHARED) $(OBJECTS) $(RESOURCES) $(LAYOUTS) $(OBJECTS:.o=.d)

.PHONY: all release clean editor reader compiler layouts

$(shell mkdir -p $(BIN_DIR) >/dev/null)

$(LAYOUT_DIR)/%.out: $(LAYOUT_DIR)/%.bls $(BIN_DIR)/$(BIN_COMPILER)
	$(BIN_DIR)/$(BIN_COMPILER) -d -o $@ $< > $(@:.out=.txt)

shared: $(BIN_DIR)/$(BIN_SHARED)
$(BIN_DIR)/$(BIN_SHARED): $(OBJECTS_SHARED)
	$(CXX) -shared -o $(BIN_DIR)/$(BIN_SHARED) $(LDFLAGS) $(OBJECTS_SHARED) $(LIBS_SHARED)

editor: $(BIN_DIR)/$(BIN_EDITOR)
$(BIN_DIR)/$(BIN_EDITOR): $(OBJECTS_EDITOR) $(BIN_DIR)/$(BIN_SHARED) $(RESOURCES)
	$(CXX) -o $(BIN_DIR)/$(BIN_EDITOR) $(LDFLAGS) $(OBJECTS_EDITOR) $(RESOURCES) $(BIN_DIR)/$(BIN_SHARED) $(LIBS_EDITOR)

$(OBJ_DIR)/images.o : $(IMAGES)
	$(LD) -r -b binary -o $@ $^

$(OBJ_DIR)/%.res : resources/%.rc
	`wx-config --rescomp` -O coff -o $@ -i $< 

reader: $(BIN_DIR)/$(BIN_READER)
$(BIN_DIR)/$(BIN_READER): $(OBJECTS_READER) $(BIN_DIR)/$(BIN_SHARED)
	$(CXX) -o $(BIN_DIR)/$(BIN_READER) $(LDFLAGS) $(OBJECTS_READER) $(BIN_DIR)/$(BIN_SHARED) $(LIBS_READER)

compiler: $(BIN_DIR)/$(BIN_COMPILER)
$(BIN_DIR)/$(BIN_COMPILER): $(OBJECTS_COMPILER) $(BIN_DIR)/$(BIN_SHARED)
	$(CXX) -o $(BIN_DIR)/$(BIN_COMPILER) $(LDFLAGS) $(OBJECTS_COMPILER) $(BIN_DIR)/$(BIN_SHARED) $(LIBS_COMPILER) 

DEPFLAGS = -MT $@ -MMD -MP -MF $(OBJ_DIR)/$*.Td

$(OBJ_DIR)/shared/%.o : $(SRC_DIR)/shared/%.cpp
$(OBJ_DIR)/shared/%.o : $(SRC_DIR)/shared/%.cpp $(OBJ_DIR)/shared/%.d
	@mkdir -p $(dir $@)
	$(CXX) $(DEPFLAGS) -fPIC $(CXXFLAGS) -c $(INCLUDE) -o $@ $<
	@mv -f $(OBJ_DIR)/$*.Td $(OBJ_DIR)/$*.d && touch $@

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp $(OBJ_DIR)/%.d
	@mkdir -p $(dir $@)
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) -c $(INCLUDE) -o $@ $<
	@mv -f $(OBJ_DIR)/$*.Td $(OBJ_DIR)/$*.d && touch $@

$(OBJ_DIR)/%.d: ;
.PRECIOUS: $(OBJ_DIR)/%.d

include $(wildcard $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.d,$(basename $(SOURCES))))
