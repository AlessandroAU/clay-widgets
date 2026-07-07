CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic

RAYLIB_DIR := subprojects/raylib
RAYLIB_SRC_DIR := $(RAYLIB_DIR)/src
RAYLIB_LIB := $(RAYLIB_SRC_DIR)/libraylib.a

INCLUDES := -I. -Isubprojects/clay -Isubprojects/raylib/src
# Static-link the GCC/C++ runtime and pthreads so the exe has no non-system DLL
# dependencies (no libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll).
# raylib is already a static .a; the font is baked in via embedded_font.h.
LDFLAGS := -L$(RAYLIB_SRC_DIR) -static -static-libgcc -static-libstdc++
LDLIBS := -lraylib -lopengl32 -lgdi32 -lwinmm

BUILD_DIR := build
LOG := $(BUILD_DIR)/build.log

APP := clay-widgets-demo.exe
SRC := main.cpp
OBJ := $(BUILD_DIR)/main.o
DEP := $(OBJ:.o=.d)

# Header trees the single translation unit pulls in: the widget library, the
# raylib backend, and the demo screens. Listed so an edit to any of them forces
# a rebuild even before the -MMD dependency file exists.
HEADERS := $(wildcard clay-widgets/*.h backends/raylib/*.h demo/*.h demo/screens/*.h)

.PHONY: all raylib run clean raylib-clean log font

all: $(APP)

# Regenerate the baked-in font header from the source TTF (committed, so this is
# only needed when the font changes).
font:
	python tools/embed_font.py

raylib: $(RAYLIB_LIB)

$(RAYLIB_LIB):
	$(MAKE) -C $(RAYLIB_SRC_DIR) PLATFORM=PLATFORM_DESKTOP RAYLIB_BUILD_MODE=RELEASE

$(BUILD_DIR):
	-mkdir $(BUILD_DIR)

$(OBJ): $(SRC) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $(SRC) -o $(OBJ)

$(APP): $(OBJ) | $(RAYLIB_LIB)
	$(CXX) $(OBJ) -o $(APP) $(LDFLAGS) $(LDLIBS)

run: $(APP)
	./$(APP)

# Rebuild from scratch, capturing all compiler output into build/build.log
# (redirection syntax works under both cmd.exe and sh).
log: | $(BUILD_DIR)
	$(MAKE) -B all > $(LOG) 2>&1

clean:
	rm -f $(APP)
	rm -rf $(BUILD_DIR)

raylib-clean:
	$(MAKE) -C $(RAYLIB_SRC_DIR) clean

-include $(DEP)
