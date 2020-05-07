CXX = g++
CFLAGS = -g -Wall --std=c++17

LIBS = `wx-config --libs`

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

BIN_NAME = layoutbolletta

INCLUDE = `wx-config --cflags`

SOURCES = $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/**/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES)))

RC_FILES = $(wildcard resources/*.rc)
RESOURCES =

ifeq ($(OS),Windows_NT)
	BIN_NAME := $(BIN_NAME).exe
	RESOURCES := $(patsubst resources/%,$(OBJ_DIR)/%.res,$(basename $(RC_FILES)))
endif

all: $(BIN_DIR)/$(BIN_NAME)

clean:
	rm -f $(BIN_DIR)/$(BIN_NAME) $(OBJECTS) $(OBJECTS:.o=.d)

$(shell mkdir -p $(BIN_DIR) >/dev/null)

$(BIN_DIR)/$(BIN_NAME): $(OBJECTS) $(RESOURCES)
	$(CXX) -o $(BIN_DIR)/$(BIN_NAME) $(LDFLAGS) $(OBJECTS) $(RESOURCES) $(LIBS)

$(OBJ_DIR)/%.res : resources/%.rc
	windres $(INCLUDE) -O coff -i $< -o $@

DEPFLAGS = -MT $@ -MMD -MP -MF $(OBJ_DIR)/$*.Td

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp $(OBJ_DIR)/%.d
	@mkdir -p $(dir $@)
	$(CXX) $(DEPFLAGS) $(CFLAGS) -c $(INCLUDE) -o $@ $<
	@mv -f $(OBJ_DIR)/$*.Td $(OBJ_DIR)/$*.d && touch $@

$(OBJ_DIR)/%.d: ;
.PRECIOUS: $(OBJ_DIR)/%.d

include $(wildcard $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.d,$(basename $(SOURCES))))
