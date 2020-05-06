CXX = g++
CFLAGS = -g -Wall --std=c++17 `wx-config --cflags`

LIBS = `wx-config --libs`

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

BIN_NAME = layoutbolletta

INCLUDE = 

SOURCES = $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/**/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.o,$(basename $(SOURCES)))

ifeq ($(OS),Windows_NT)
	BIN_NAME := $(BIN_NAME).exe
endif

all: $(BIN_DIR)/$(BIN_NAME)

clean:
	rm -f $(BIN_DIR)/$(BIN_NAME) $(OBJECTS) $(OBJECTS:.o=.d)

$(shell mkdir -p $(BIN_DIR) >/dev/null)

$(BIN_DIR)/$(BIN_NAME): $(OBJECTS)
	$(CXX) -o $(BIN_DIR)/$(BIN_NAME) $(LDFLAGS) $(OBJECTS) $(LIBS)

DEPFLAGS = -MT $@ -MMD -MP -MF $(OBJ_DIR)/$*.Td

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp $(OBJ_DIR)/%.d
	@mkdir -p $(dir $@)
	$(CXX) $(DEPFLAGS) $(CFLAGS) -c $(INCLUDE) -o $@ $<
	@mv -f $(OBJ_DIR)/$*.Td $(OBJ_DIR)/$*.d && touch $@

$(OBJ_DIR)/%.d: ;
.PRECIOUS: $(OBJ_DIR)/%.d

include $(wildcard $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%.d,$(basename $(SOURCES))))
