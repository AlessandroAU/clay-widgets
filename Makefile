CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic

RAYLIB_DIR := subprojects/raylib
RAYLIB_SRC_DIR := $(RAYLIB_DIR)/src
RAYLIB_LIB := $(RAYLIB_SRC_DIR)/libraylib.a

INCLUDES := -I. -Isubprojects/clay -Isubprojects/raylib/src
LDFLAGS := -L$(RAYLIB_SRC_DIR)
LDLIBS := -lraylib -lopengl32 -lgdi32 -lwinmm

BUILD_DIR := build

APP := clay-widgets-demo.exe
SRC := main.cpp
OBJ := $(BUILD_DIR)/main.o
DEP := $(OBJ:.o=.d)

.PHONY: all raylib run clean raylib-clean

all: $(APP)

raylib: $(RAYLIB_LIB)

$(RAYLIB_LIB):
	$(MAKE) -C $(RAYLIB_SRC_DIR) PLATFORM=PLATFORM_DESKTOP RAYLIB_BUILD_MODE=RELEASE

$(BUILD_DIR):
	-mkdir $(BUILD_DIR)

$(OBJ): $(SRC) clay-widgets.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $(SRC) -o $(OBJ)

$(APP): $(OBJ) | $(RAYLIB_LIB)
	$(CXX) $(OBJ) -o $(APP) $(LDFLAGS) $(LDLIBS)

run: $(APP)
	./$(APP)

clean:
	rm -f $(APP)
	rm -rf $(BUILD_DIR)

raylib-clean:
	$(MAKE) -C $(RAYLIB_SRC_DIR) clean

-include $(DEP)
