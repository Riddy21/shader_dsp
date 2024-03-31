# Variables
CXX = g++
CXXFLAGS = -Wall -std=c++20
LDFLAGS = -lportaudio -lCatch2 -lCatch2Main
SRC_DIR = src
LIB_DIR = lib
INCLUDE_DIR = include
BUILD_DIR = build
TEST_DIR = tests

OBJECT_DIR = $(BUILD_DIR)/objects
BIN_DIR = $(BUILD_DIR)/bin
TEST_EXE_DIR = $(BUILD_DIR)/tests

TARGET_NAMES = main

TESTS = $(wildcard $(TEST_DIR)/*_test.cpp)
TEST_TARGETS = $(patsubst $(TEST_DIR)/%_test.cpp, $(TEST_EXE_DIR)/%_test, $(TESTS))
TARGETS = $(addprefix $(BIN_DIR)/, $(TARGET_NAMES))
LIB_SOURCES = $(wildcard $(LIB_DIR)/*.cpp)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)
OBJECTS = $(patsubst $(LIB_DIR)/%.cpp, $(OBJECT_DIR)/%.o, $(LIB_SOURCES))

.SECONADY : $(OBJECTS)

all : mkdirs $(TARGETS)	
test : mkdirs $(TEST_TARGETS)

# Building all final targets
$(BIN_DIR)/% : $(SRC_DIR)/%.cpp $(OBJECTS)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -o $@ $^ $(LDFLAGS)

# Building the library object files
$(OBJECT_DIR)/%.o : $(LIB_DIR)/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -c -o $@ $<

mkdirs : $(OBJECT_DIR) $(BIN_DIR) $(TEST_EXE_DIR)

$(OBJECT_DIR):
	mkdir -p $(OBJECT_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TEST_EXE_DIR):
	mkdir -p $(TEST_EXE_DIR)

$(TEST_EXE_DIR)/%_test : $(TEST_DIR)/%_test.cpp $(OBJECT_DIR)/%.o
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -o $@ $^ $(LDFLAGS)
	$@

clean :
	rm -rf $(BUILD_DIR)

PHONY : all clean test mkdirs