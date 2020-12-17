CC = gcc
CXX = g++
CFLAGS = -Isrc/shared -Wall $(shell wx-config --cflags) $(shell pkg-config --cflags jsoncpp fmt)
CXXFLAGS = $(CFLAGS) --std=c++17
LDFLAGS = 

SRC_DIR = src
OBJ_DIR = obj/$(build)
BIN_DIR = bin/$(build)
BLS_DIR = bls
LAYOUT_DIR = layout

ifndef build
  build = debug
endif

ifeq ($(build),release)
  CFLAGS += -O2 -DNDEBUG
  LDFLAGS += -s
else ifeq ($(build),debug)
  CFLAGS += -g -DDEBUG
else
  $(error "Unknown build option $(build)")
endif

ifneq (,$(findstring 20.12.1,$(shell pdfinfo -v 2>&1)))
  CFLAGS += -DPOPPLER_FIX
endif

BIN_SHARED = layout_shared.so
BIN_EDITOR = layout_editor
BIN_READER = layout_reader
BIN_COMPILER = layout_compiler

OUT_LAYOUT_TXT = 1

LIBS_SHARED = $(shell pkg-config --libs jsoncpp fmt)
LIBS_EDITOR = $(shell wx-config --libs core,stc) $(LIBS_SHARED)
LIBS_READER = $(shell wx-config --libs base) $(LIBS_SHARED)
LIBS_COMPILER = $(shell wx-config --libs base) $(LIBS_SHARED)

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SOURCES_SHARED = $(call rwildcard,$(SRC_DIR)/shared,*.cpp *.c)
OBJECTS_SHARED = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_SHARED)))

SOURCES_EDITOR = $(call rwildcard,$(SRC_DIR)/editor,*.cpp *.c)
OBJECTS_EDITOR = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_EDITOR)))

SOURCES_READER = $(call rwildcard,$(SRC_DIR)/reader,*.cpp *.c)
OBJECTS_READER = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_READER)))

SOURCES_COMPILER = $(call rwildcard,$(SRC_DIR)/compiler,*.cpp *.c)
OBJECTS_COMPILER = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES_COMPILER)))

SOURCES_BLS = $(call rwildcard,$(BLS_DIR),*.bls)
LAYOUTS = $(patsubst $(BLS_DIR)/%.bls,$(LAYOUT_DIR)/%.out,$(SOURCES_BLS))

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
	rm -f $(BIN_DIR)/$(BIN_EDITOR) $(BIN_DIR)/$(BIN_READER) $(BIN_DIR)/$(BIN_COMPILER) $(BIN_DIR)/$(BIN_SHARED) $(OBJECTS) $(RESOURCES) $(LAYOUTS) $(LAYOUTS:.out=.txt) $(OBJECTS:.o=.d)

.PHONY: all release clean editor reader compiler layouts

$(shell mkdir -p $(BIN_DIR) >/dev/null)

$(LAYOUT_DIR)/%.out: $(BLS_DIR)/%.bls $(BIN_DIR)/$(BIN_COMPILER)
	$(BIN_DIR)/$(BIN_COMPILER) -o $@ $< $(if $(filter $(OUT_LAYOUT_TXT),1),> $(@:.out=.txt),-q)

shared: $(BIN_DIR)/$(BIN_SHARED)
$(BIN_DIR)/$(BIN_SHARED): $(OBJECTS_SHARED)
	$(CXX) -shared -o $(BIN_DIR)/$(BIN_SHARED) $(LDFLAGS) $(OBJECTS_SHARED) $(LIBS_SHARED)

editor: $(BIN_DIR)/$(BIN_EDITOR)
$(BIN_DIR)/$(BIN_EDITOR): $(OBJECTS_EDITOR) $(BIN_DIR)/$(BIN_SHARED) $(RESOURCES)
	$(CXX) -o $(BIN_DIR)/$(BIN_EDITOR) $(LDFLAGS) $(OBJECTS_EDITOR) $(RESOURCES) $(BIN_DIR)/$(BIN_SHARED) $(LIBS_EDITOR)

$(OBJ_DIR)/images.o : $(IMAGES)
	$(LD) -r -b binary -o $@ $^

$(OBJ_DIR)/%.res : resources/%.rc
	$(shell wx-config --rescomp) -O coff -o $@ -i $< 

reader: $(BIN_DIR)/$(BIN_READER)
$(BIN_DIR)/$(BIN_READER): $(OBJECTS_READER) $(BIN_DIR)/$(BIN_SHARED)
	$(CXX) -o $(BIN_DIR)/$(BIN_READER) $(LDFLAGS) $(OBJECTS_READER) $(BIN_DIR)/$(BIN_SHARED) $(LIBS_READER)

compiler: $(BIN_DIR)/$(BIN_COMPILER)
$(BIN_DIR)/$(BIN_COMPILER): $(OBJECTS_COMPILER) $(BIN_DIR)/$(BIN_SHARED)
	$(CXX) -o $(BIN_DIR)/$(BIN_COMPILER) $(LDFLAGS) $(OBJECTS_COMPILER) $(BIN_DIR)/$(BIN_SHARED) $(LIBS_COMPILER) 

DEPFLAGS = -MT $@ -MMD -MP -MF $(OBJ_DIR)/$*.Td

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp $(OBJ_DIR)/%.d
	@mkdir -p $(dir $@)
	$(CXX) $(DEPFLAGS) $(if $(filter $(SOURCES_SHARED),$<),-fPIC) $(CXXFLAGS) -c -o $@ $<
	@mv -f $(OBJ_DIR)/$*.Td $(OBJ_DIR)/$*.d && touch $@

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c $(OBJ_DIR)/%.d
	@mkdir -p $(dir $@)
	$(CC) $(DEPFLAGS) $(if $(filter $(SOURCES_SHARED),$<),-fPIC) $(CFLAGS) -c -o $@ $<
	@mv -f $(OBJ_DIR)/$*.Td $(OBJ_DIR)/$*.d && touch $@

$(OBJ_DIR)/%.d: ;
.PRECIOUS: $(OBJ_DIR)/%.d

include $(wildcard $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.d,$(basename $(SOURCES))))
