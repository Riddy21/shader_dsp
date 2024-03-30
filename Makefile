# Variables
CXX = g++
CXXFLAGS = -Wall
LDFLAGS = -lportaudio
SRC_DIR = src
LIB_DIR = lib
INCLUDE_DIR = include
BUILD_DIR = build
TEST_DIR = test

OBJECT_DIR = $(BUILD_DIR)/objects
BIN_DIR = $(BUILD_DIR)/bin

TARGET_NAMES = main test

TARGETS = $(addprefix $(BIN_DIR)/, $(TARGET_NAMES))
LIB_SOURCES = $(wildcard $(LIB_DIR)/*.cpp)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)
OBJECTS = $(patsubst $(LIB_DIR)/%.cpp, $(OBJECT_DIR)/%.o, $(LIB_SOURCES))

all : $(BIN_DIR) $(OBJECT_DIR) $(TARGETS)	

# Building all final targets
$(BIN_DIR)/% : $(SRC_DIR)/%.cpp $(OBJECTS)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -o $@ $^ $(LDFLAGS)

# Building the library object files
$(OBJECT_DIR)/%.o : $(LIB_DIR)/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -I $(INCLUDE_DIR) -c -o $@ $<

$(OBJECT_DIR):
	mkdir -p $(OBJECT_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean :
	rm -rf $(BUILD_DIR)

PHONY : all clean