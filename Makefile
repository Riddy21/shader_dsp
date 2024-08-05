# Variables
CXX = g++
CXXFLAGS = -Wall -std=c++20
LDFLAGS = -lportaudio -lCatch2Main -lCatch2 -lGL -lGLEW -lglut
SRC_DIR = src
LIB_DIR = lib
INCLUDE_DIR = include
BUILD_DIR = build
TEST_DIR = tests
PLAYGROUND_DIR = playground

OBJECT_DIR = $(BUILD_DIR)/objects
BIN_DIR = $(BUILD_DIR)/bin
TEST_EXE_DIR = $(BUILD_DIR)/tests
PLAYGROUND_EXE_DIR = $(BUILD_DIR)/playground

TARGET_NAMES = main

TESTS = $(wildcard $(TEST_DIR)/*_test.cpp)
TEST_TARGETS = $(patsubst $(TEST_DIR)/%_test.cpp, $(TEST_EXE_DIR)/%_test, $(TESTS))
TARGETS = $(addprefix $(BIN_DIR)/, $(TARGET_NAMES))
LIB_SOURCES = $(wildcard $(LIB_DIR)/*.cpp)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)
OBJECTS = $(patsubst $(LIB_DIR)/%.cpp, $(OBJECT_DIR)/%.o, $(LIB_SOURCES))

.SECONADY : $(OBJECTS)

all : $(TARGETS)	
test : $(TEST_TARGETS)

# Building all final targets
$(BIN_DIR)/% : $(SRC_DIR)/%.cpp $(OBJECTS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -o $@ $^ $(LDFLAGS)

# Building the library object files
$(OBJECT_DIR)/%.o : $(LIB_DIR)/%.cpp $(HEADERS)
	mkdir -p $(OBJECT_DIR)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -c -o $@ $<

$(TEST_EXE_DIR)/%_test : $(TEST_DIR)/%_test.cpp $(OBJECT_DIR)/%.o
	mkdir -p $(TEST_EXE_DIR)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -o $@ $^ $(LDFLAGS)
	./$@

$(PLAYGROUND_EXE_DIR)/% : $(PLAYGROUND_DIR)/%.cpp $(OBJECTS)
	mkdir -p $(PLAYGROUND_EXE_DIR)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -o $@ $^ $(LDFLAGS)
	./$@

clean :
	rm -rf $(BUILD_DIR)

PHONY : all clean test