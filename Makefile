CXX ?= g++
.DEFAULT_GOAL := all
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wno-missing-field-initializers

RAYLIB_DIR := subprojects/raylib
RAYLIB_SRC_DIR := $(RAYLIB_DIR)/src
RAYLIB_LIB := $(RAYLIB_SRC_DIR)/libraylib.a

INCLUDES := -I. -isystem subprojects/clay -isystem subprojects/raylib/src

# Windows is the primary target. The Linux and macOS branches exist so the demo
# can also be built and run headlessly - CI regenerates the README screenshots
# on Linux under Xvfb with Mesa's software rasterizer.
ifeq ($(OS),Windows_NT)
    PLATFORM_OS := Windows
else
    PLATFORM_OS := $(shell uname -s)
endif

ifeq ($(PLATFORM_OS),Windows)
    EXE := .exe
    # Static-link the GCC/C++ runtime and pthreads so the exe has no non-system
    # DLL dependencies (no libgcc_s_seh-1.dll / libstdc++-6.dll /
    # libwinpthread-1.dll). raylib is already a static .a; the font is baked in
    # via assets/generated/embedded-font.h.
    STATIC_RUNTIME := -static -static-libgcc -static-libstdc++
    LDLIBS := -lraylib -lopengl32 -lgdi32 -lwinmm
else ifeq ($(PLATFORM_OS),Darwin)
    EXE :=
    STATIC_RUNTIME :=
    LDLIBS := -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework OpenGL
else
    EXE :=
    # No -static here: X11/GL are system libraries and are linked dynamically.
    STATIC_RUNTIME :=
    LDLIBS := -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

LDFLAGS := -L$(RAYLIB_SRC_DIR) $(STATIC_RUNTIME)

BUILD_DIR := build
LOG := $(BUILD_DIR)/build.log

APP := clay-widgets-demo$(EXE)
SRC := demo/main.cpp
OBJ := $(BUILD_DIR)/main.o
DEP := $(OBJ:.o=.d)

# Headless unit tests: the widget library driven with synthetic input and a
# fake text measurer. No raylib, no window - runs anywhere a compiler does.
TEST_APP := $(BUILD_DIR)/test-widgets$(EXE)
TEST_SRC := tests/test-widgets.cpp

# Single-file amalgam of the library (generated, not committed). test-amalgam
# compiles the same unit tests against it - the amalgam dir shadows the split
# tree on the include path, so tests/#include "clay-widgets/widgets.h"
# resolves to the generated file and the split headers can't leak in.
AMALGAM := $(BUILD_DIR)/clay-widgets.h
AMALGAM_SHADOW := $(BUILD_DIR)/amalgam/clay-widgets/widgets.h
AMALGAM_TEST_APP := $(BUILD_DIR)/test-widgets-amalgam$(EXE)

# Header trees the single translation unit pulls in: the widget library, the
# raylib backend, the demo screens and the generated font header. Listed so an
# edit to any of them forces a rebuild even before the -MMD dependency file exists.
HEADERS := $(wildcard clay-widgets/*.h backends/raylib/*.h demo/*.h demo/screens/*.h assets/generated/*.h)

.PHONY: all raylib run test amalgam test-amalgam clean raylib-clean log font

.PHONY: test-backend

# Requires a graphics context; run locally, separately from headless CI.
build/test-raylib$(EXE): tests/test-raylib.cpp $(HEADERS) subprojects/clay/clay.h $(RAYLIB_LIB) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) tests/test-raylib.cpp -o $@ $(LDFLAGS) $(LDLIBS)

test-backend: build/test-raylib$(EXE)
	./build/test-raylib$(EXE)

all: $(APP)

# Regenerate the baked-in font header from the source TTF (committed, so this is
# only needed when the font changes).
font:
	python tools/embed_font.py

raylib: $(RAYLIB_LIB)

# clay and raylib are git submodules; fail with a hint instead of a cryptic
# "No rule to make target" if they haven't been fetched yet.
$(RAYLIB_LIB):
	$(if $(wildcard $(RAYLIB_SRC_DIR)/Makefile),,$(error raylib source not found in $(RAYLIB_SRC_DIR) - run: git submodule update --init))
	$(MAKE) -C $(RAYLIB_SRC_DIR) PLATFORM=PLATFORM_DESKTOP RAYLIB_BUILD_MODE=RELEASE

$(BUILD_DIR):
	-mkdir $(BUILD_DIR)

$(OBJ): $(SRC) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $(SRC) -o $(OBJ)

$(APP): $(OBJ) $(RAYLIB_LIB)
	$(CXX) $(OBJ) -o $(APP) $(LDFLAGS) $(LDLIBS)

run: $(APP)
	./$(APP)

$(TEST_APP): $(TEST_SRC) $(wildcard tests/*.h) $(HEADERS) subprojects/clay/clay.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. -isystem subprojects/clay $(TEST_SRC) -o $(TEST_APP) $(STATIC_RUNTIME)

test: $(TEST_APP)
	./$(TEST_APP)

$(AMALGAM): $(wildcard clay-widgets/*.h) tools/amalgamate.py | $(BUILD_DIR)
	python tools/amalgamate.py -o $(AMALGAM)

amalgam: $(AMALGAM)

$(AMALGAM_TEST_APP): $(AMALGAM) $(TEST_SRC) $(wildcard tests/*.h) subprojects/clay/clay.h
	mkdir -p $(BUILD_DIR)/amalgam/clay-widgets
	cp $(AMALGAM) $(AMALGAM_SHADOW)
	$(CXX) $(CXXFLAGS) -I$(BUILD_DIR)/amalgam -isystem subprojects/clay $(TEST_SRC) -o $(AMALGAM_TEST_APP) $(STATIC_RUNTIME)

test-amalgam: $(AMALGAM_TEST_APP)
	./$(AMALGAM_TEST_APP)

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
