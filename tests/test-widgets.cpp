// Headless unit tests for the clay-widgets library.
//
// Clay computes layout without a GPU - all it needs is a text-measurement
// callback - so these tests drive real frames (BeginFrame / widget calls /
// EndFrame) with synthetic ClayWidgets_Input and assert on widget state,
// context state and the returned render commands. No raylib, no window; they
// run in CI. Text is measured with a fake monospace font (8px per byte,
// 16px tall) so caret math is deterministic.
//
// Build & run:  make test

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CLAY_IMPLEMENTATION
#include "clay.h"

// Overflow reports must be observable, not fatal: tests install an error
// handler and assert on the messages, so the abort() backstop is disabled.
#define CLAY_WIDGETS_ASSERT(message) ((void)0)
#define CLAY_WIDGETS_IMPLEMENTATION
#include "clay-widgets/widgets.h"

// ---------------------------------------------------------------------------
// Tiny test framework
// ---------------------------------------------------------------------------

static int g_checkCount = 0;
static int g_failCount = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checkCount++;                                                    \
        if (!(cond)) {                                                     \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failCount++;                                                 \
        }                                                                  \
    } while (0)

// ---------------------------------------------------------------------------
// Harness: fake platform
// ---------------------------------------------------------------------------

static const float kFakeCharWidth = 8.0f;
static const float kFakeLineHeight = 16.0f;
static const float kLayoutWidth = 800.0f;
static const float kLayoutHeight = 600.0f;

static ClayWidgets_Context ui;
static std::vector<std::string> g_errors;
static std::string g_clipboard;

static Clay_Dimensions FakeMeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    (void)config;
    (void)userData;
    Clay_Dimensions out = {};
    out.width = kFakeCharWidth * (float)text.length;
    out.height = kFakeLineHeight;
    return out;
}

static void TestErrorHandler(const char *message, void *userData) {
    (void)userData;
    g_errors.push_back(message);
}

static const char *TestGetClipboard(void *userData) {
    (void)userData;
    return g_clipboard.c_str();
}

static void TestSetClipboard(const char *text, void *userData) {
    (void)userData;
    g_clipboard = text;
}

static bool ErrorsContain(const char *needle) {
    for (const std::string &message : g_errors) {
        if (message.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Fresh widget context per test. Clay itself is initialized once; tests use
// distinct element ids so persisted layout data can't leak between them.
// Animations are off so colors and eased scalars land on their targets
// immediately, making assertions exact.
static void ResetUi(void) {
    ClayWidgets_Init(&ui, ClayWidgets_DefaultTheme());
    ClayWidgets_SetMeasureTextFunction(&ui, FakeMeasureText, nullptr);
    ClayWidgets_SetClipboardFunctions(&ui, TestGetClipboard, TestSetClipboard, nullptr);
    ClayWidgets_SetErrorHandler(&ui, TestErrorHandler, nullptr);
    ui.animationsEnabled = false;
    g_errors.clear();
    g_clipboard.clear();
}

static ClayWidgets_Input MakeInput(void) {
    ClayWidgets_Input input = {};
    input.deltaTime = 1.0f / 60.0f;
    input.mouseX = -100.0f; // off-layout so nothing is hovered by default
    input.mouseY = -100.0f;
    return input;
}

// Runs one frame: BeginFrame, a full-window root column, the test body, EndFrame.
template <typename Body>
static Clay_RenderCommandArray Frame(ClayWidgets_Input input, Body &&body) {
    ClayWidgets_BeginFrame(&ui, input, (Clay_Dimensions){kLayoutWidth, kLayoutHeight}, false);
    CLAY(CLAY_ID("TestRoot"), {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            .childGap = 8,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    }) {
        body();
    }
    return ClayWidgets_EndFrame(&ui);
}

static bool FindCommandById(Clay_RenderCommandArray commands, uint32_t id, Clay_RenderCommand *out) {
    for (int32_t i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
        if (command->id == id) {
            if (out) {
                *out = *command;
            }
            return true;
        }
    }
    return false;
}

static bool SameColor(Clay_Color a, Clay_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// Pointer-click helper: returns the input for the "press" frame; follow with
// ReleaseAt. ConsumeClick fires on the release frame.
static ClayWidgets_Input PressAt(float x, float y) {
    ClayWidgets_Input input = MakeInput();
    input.mouseX = x;
    input.mouseY = y;
    input.pointerDown = true;
    input.pointerPressed = true;
    return input;
}

static ClayWidgets_Input ReleaseAt(float x, float y) {
    ClayWidgets_Input input = MakeInput();
    input.mouseX = x;
    input.mouseY = y;
    input.pointerReleased = true;
    return input;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Tab walks focus forward through registration order and wraps; Shift+Tab
// walks backward and wraps the other way.
static void TestFocusTraversal(void) {
    Clay_ElementId a = CLAY_ID("FocusA");
    Clay_ElementId b = CLAY_ID("FocusB");
    Clay_ElementId c = CLAY_ID("FocusC");
    auto body = [&]() {
        ClayWidgets_Button(&ui, a, CLAY_STRING("A"));
        ClayWidgets_Button(&ui, b, CLAY_STRING("B"));
        ClayWidgets_Button(&ui, c, CLAY_STRING("C"));
    };

    Frame(MakeInput(), body); // register order

    ClayWidgets_Input tab = MakeInput();
    tab.keyTab = true;
    Frame(tab, body);
    CHECK(ui.focusedId == a.id);
    Frame(tab, body);
    CHECK(ui.focusedId == b.id);

    ClayWidgets_Input shiftTab = MakeInput();
    shiftTab.keyTab = true;
    shiftTab.shiftDown = true;
    Frame(shiftTab, body);
    CHECK(ui.focusedId == a.id);
    Frame(shiftTab, body); // wraps backward from the first entry
    CHECK(ui.focusedId == c.id);
}

// While a modal is open, Tab only reaches widgets inside it, and a background
// widget that had focus loses it (so Enter can't activate through the scrim).
static void TestModalFocusTrap(void) {
    Clay_ElementId outside = CLAY_ID("TrapOutside");
    Clay_ElementId inside = CLAY_ID("TrapInside");
    Clay_ElementId modal = CLAY_ID("TrapModal");
    Clay_ElementId modalClose = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalClose"), modal.id);
    bool open = false;
    bool outsideClicked = false;
    auto body = [&]() {
        outsideClicked = ClayWidgets_Button(&ui, outside, CLAY_STRING("Outside"));
        if (ClayWidgets_BeginModal(&ui, modal, CLAY_STRING("Dialog"), &open)) {
            ClayWidgets_Button(&ui, inside, CLAY_STRING("Inside"));
            ClayWidgets_EndModal(&ui, modal);
        }
    };

    // Focus the outside button while no modal is open.
    Frame(MakeInput(), body);
    ClayWidgets_Input tab = MakeInput();
    tab.keyTab = true;
    Frame(tab, body);
    CHECK(ui.focusedId == outside.id);

    // Open the modal. The trap arms this frame and takes effect next frame.
    open = true;
    Frame(MakeInput(), body);

    // Enter must NOT click the still-focused background button; the trap
    // strips its focus during registration instead.
    ClayWidgets_Input enter = MakeInput();
    enter.keyEnter = true;
    Frame(enter, body);
    CHECK(!outsideClicked);
    CHECK(ui.focusedId != outside.id);

    // Tab now cycles only the modal's own controls (close button, then body).
    Frame(tab, body);
    CHECK(ui.focusedId == modalClose.id);
    Frame(tab, body);
    CHECK(ui.focusedId == inside.id);
    Frame(tab, body); // wraps within the modal
    CHECK(ui.focusedId == modalClose.id);

    // Closing the modal restores the outside button on the next frames.
    open = false;
    Frame(MakeInput(), body);
    Frame(MakeInput(), body);
    Frame(tab, body);
    CHECK(ui.focusedId == outside.id);
}

// Click to focus, edit UTF-8 text, and drive the clipboard through the
// injected functions.
static void TestTextInputEditingAndClipboard(void) {
    Clay_ElementId inputId = CLAY_ID("EditField");
    char buffer[64] = "hello";
    bool changed = false;
    ClayWidgets_TextInputOptions options = {};
    auto body = [&]() {
        changed = ClayWidgets_TextInput(&ui, inputId, CLAY_STRING(""), buffer, (int32_t)sizeof(buffer), options);
    };

    Frame(MakeInput(), body); // establish geometry
    Clay_ElementData fieldData = Clay_GetElementData(inputId);
    CHECK(fieldData.found);
    float cx = fieldData.boundingBox.x + fieldData.boundingBox.width * 0.5f;
    float cy = fieldData.boundingBox.y + fieldData.boundingBox.height * 0.5f;

    Frame(PressAt(cx, cy), body); // click focuses the field
    CHECK(ui.focusedId == inputId.id);
    Frame(ReleaseAt(cx, cy), body);

    // Select all + type replaces the content.
    ClayWidgets_Input selectAll = MakeInput();
    selectAll.keySelectAll = true;
    Frame(selectAll, body);
    ClayWidgets_Input type = MakeInput();
    type.textUtf8 = "ab";
    type.textUtf8Length = 2;
    Frame(type, body);
    CHECK(changed);
    CHECK(std::strcmp(buffer, "ab") == 0);

    // A two-byte UTF-8 codepoint inserts and deletes as one unit.
    ClayWidgets_Input typeAccent = MakeInput();
    typeAccent.textUtf8 = "\xC3\xA9"; // é
    typeAccent.textUtf8Length = 2;
    Frame(typeAccent, body);
    CHECK(std::strcmp(buffer, "ab\xC3\xA9") == 0);
    ClayWidgets_Input backspace = MakeInput();
    backspace.keyBackspace = true;
    Frame(backspace, body);
    CHECK(std::strcmp(buffer, "ab") == 0);

    // Copy: selection lands in the (fake) platform clipboard.
    Frame(selectAll, body);
    ClayWidgets_Input copy = MakeInput();
    copy.keyCopy = true;
    Frame(copy, body);
    CHECK(g_clipboard == "ab");
    CHECK(std::strcmp(buffer, "ab") == 0); // copy does not modify

    // Cut: clipboard updated, buffer emptied.
    Frame(selectAll, body);
    ClayWidgets_Input cut = MakeInput();
    cut.keyCut = true;
    Frame(cut, body);
    CHECK(g_clipboard == "ab");
    CHECK(buffer[0] == '\0');

    // Paste inserts at the caret; a multi-line paste stops at the newline.
    g_clipboard = "world";
    ClayWidgets_Input paste = MakeInput();
    paste.keyPaste = true;
    Frame(paste, body);
    CHECK(std::strcmp(buffer, "world") == 0);
    g_clipboard = "!line2\nline3";
    Frame(paste, body);
    CHECK(std::strcmp(buffer, "world!line2") == 0);
}

// A disabled text input is not focusable and ignores editing.
static void TestTextInputDisabled(void) {
    Clay_ElementId inputId = CLAY_ID("DisabledField");
    char buffer[32] = "keep";
    ClayWidgets_TextInputOptions options = {};
    options.disabled = true;
    auto body = [&]() {
        ClayWidgets_TextInput(&ui, inputId, CLAY_STRING(""), buffer, (int32_t)sizeof(buffer), options);
    };

    Frame(MakeInput(), body);
    Clay_ElementData fieldData = Clay_GetElementData(inputId);
    CHECK(fieldData.found);
    float cx = fieldData.boundingBox.x + fieldData.boundingBox.width * 0.5f;
    float cy = fieldData.boundingBox.y + fieldData.boundingBox.height * 0.5f;
    Frame(PressAt(cx, cy), body);
    CHECK(ui.focusedId == 0);
    Frame(ReleaseAt(cx, cy), body);

    ClayWidgets_Input type = MakeInput();
    type.textUtf8 = "x";
    type.textUtf8Length = 1;
    Frame(type, body);
    CHECK(std::strcmp(buffer, "keep") == 0);
}

// A focused slider is keyboard-operable: arrows step, Home/End jump. Disabled
// sliders ignore keys and never take focus.
static void TestSliderKeyboard(void) {
    Clay_ElementId sliderId = CLAY_ID("KeySlider");
    float value = 5.0f;
    ClayWidgets_SliderOptions options = {};
    options.minValue = 0.0f;
    options.maxValue = 10.0f;
    options.step = 1.0f;
    auto body = [&]() {
        value = ClayWidgets_Slider(&ui, sliderId, value, options);
    };

    Frame(MakeInput(), body);
    ClayWidgets_Input tab = MakeInput();
    tab.keyTab = true;
    Frame(tab, body);
    CHECK(ui.focusedId == sliderId.id);

    ClayWidgets_Input right = MakeInput();
    right.keyRight = true;
    Frame(right, body);
    CHECK(value == 6.0f);

    ClayWidgets_Input end = MakeInput();
    end.keyEnd = true;
    Frame(end, body);
    CHECK(value == 10.0f);
    Frame(right, body); // clamped at max
    CHECK(value == 10.0f);

    ClayWidgets_Input left = MakeInput();
    left.keyLeft = true;
    Frame(left, body);
    CHECK(value == 9.0f);

    ClayWidgets_Input home = MakeInput();
    home.keyHome = true;
    Frame(home, body);
    CHECK(value == 0.0f);

    // Disabled: value inert, focus unreachable.
    options.disabled = true;
    ui.focusedId = 0;
    Frame(MakeInput(), body);
    Frame(right, body);
    CHECK(value == 0.0f);
    Frame(tab, body);
    CHECK(ui.focusedId != sliderId.id);
}

// A disabled stepper ignores clicks and keys.
static void TestStepperDisabled(void) {
    Clay_ElementId stepperId = CLAY_ID("DisabledStepper");
    int32_t value = 3;
    ClayWidgets_StepperOptions options = {};
    options.minValue = 0;
    options.maxValue = 9;
    options.step = 1;
    options.disabled = true;
    auto body = [&]() {
        ClayWidgets_Stepper(&ui, stepperId, &value, options);
    };

    Frame(MakeInput(), body);
    ClayWidgets_Input up = MakeInput();
    up.keyUp = true;
    Frame(up, body);
    CHECK(value == 3);
    ClayWidgets_Input tab = MakeInput();
    tab.keyTab = true;
    Frame(tab, body);
    CHECK(ui.focusedId == 0);
}

// More dynamic strings than scratch slots in one frame is reported (it would
// render corrupted text); staying at the cap is silent.
static void TestScratchOverflowReported(void) {
    int32_t values[CLAY_WIDGETS_TEXT_SCRATCH_COUNT + 1] = {0};
    ClayWidgets_StepperOptions options = {};
    options.minValue = 0;
    options.maxValue = 100;
    options.step = 1;

    Frame(MakeInput(), [&]() {
        for (int32_t i = 0; i < CLAY_WIDGETS_TEXT_SCRATCH_COUNT; ++i) {
            ClayWidgets_Stepper(&ui, CLAY_IDI("ScratchOk", i), &values[i], options);
        }
    });
    CHECK(g_errors.empty());

    Frame(MakeInput(), [&]() {
        for (int32_t i = 0; i < CLAY_WIDGETS_TEXT_SCRATCH_COUNT + 1; ++i) {
            ClayWidgets_Stepper(&ui, CLAY_IDI("ScratchOver", i), &values[i], options);
        }
    });
    CHECK(g_errors.size() == 1);
    CHECK(ErrorsContain("scratch"));
}

// Exceeding the focus-order capacity is reported once per frame.
static void TestFocusablesOverflowReported(void) {
    Frame(MakeInput(), [&]() {
        for (int32_t i = 0; i < CLAY_WIDGETS_MAX_FOCUSABLES + 2; ++i) {
            ClayWidgets_Button(&ui, CLAY_IDI("ManyButtons", i), CLAY_STRING("B"));
        }
    });
    CHECK(g_errors.size() == 1);
    CHECK(ErrorsContain("focus order full"));
}

// Exceeding the table column capture is reported.
static void TestTableColumnsOverflowReported(void) {
    ClayWidgets_TableColumn columns[13];
    for (int32_t i = 0; i < 13; ++i) {
        columns[i].title = CLAY_STRING("Col");
        columns[i].width = CLAY_SIZING_FIXED(20);
    }
    Frame(MakeInput(), [&]() {
        ClayWidgets_BeginTable(&ui, CLAY_ID("WideTable"), columns, 13);
        ClayWidgets_EndTable(&ui, CLAY_ID("WideTable"));
    });
    CHECK(g_errors.size() == 1);
    CHECK(ErrorsContain("columns"));
}

// Toasts queue up to the cap, expire on their own timers, and evict oldest
// first when full.
static void TestToastQueue(void) {
    auto body = [&]() {
        ClayWidgets_ToastLayer(&ui);
    };

    ClayWidgets_ShowToast(&ui, CLAY_STRING("first"), CLAY_WIDGETS_BADGE_ACCENT, 1.0f);
    ClayWidgets_ShowToast(&ui, CLAY_STRING("second"), CLAY_WIDGETS_BADGE_SUCCESS, 0.05f);
    ClayWidgets_ShowToast(&ui, CLAY_STRING("third"), CLAY_WIDGETS_BADGE_DANGER, 1.0f);
    CHECK(ui.toastCount == 3);

    Frame(MakeInput(), body);
    CHECK(ui.toastCount == 3);

    // ~0.07s later the short-lived middle toast expires; order is preserved.
    Frame(MakeInput(), body);
    Frame(MakeInput(), body);
    Frame(MakeInput(), body);
    CHECK(ui.toastCount == 2);
    CHECK(std::strcmp(ui.toasts[0].message, "first") == 0);
    CHECK(std::strcmp(ui.toasts[1].message, "third") == 0);

    // Overflow: the oldest is evicted so the newest always shows. Two live
    // toasts ("first", "third") plus enough fillers to reach the cap pushes
    // "first" out while "third" survives at the head...
    for (int32_t i = 0; i < CLAY_WIDGETS_MAX_TOASTS - 1; ++i) {
        ClayWidgets_ShowToast(&ui, CLAY_STRING("filler"), CLAY_WIDGETS_BADGE_NEUTRAL, 1.0f);
    }
    CHECK(ui.toastCount == CLAY_WIDGETS_MAX_TOASTS);
    CHECK(std::strcmp(ui.toasts[0].message, "third") == 0);
    // ...and one more evicts "third" too.
    ClayWidgets_ShowToast(&ui, CLAY_STRING("last"), CLAY_WIDGETS_BADGE_NEUTRAL, 1.0f);
    CHECK(ui.toastCount == CLAY_WIDGETS_MAX_TOASTS);
    CHECK(std::strcmp(ui.toasts[0].message, "filler") == 0);
    CHECK(std::strcmp(ui.toasts[CLAY_WIDGETS_MAX_TOASTS - 1].message, "last") == 0);
}

// Combo: click opens, arrows move the highlight (wrapping), Enter selects,
// and a long list near the bottom edge opens upward and scrolls its highlight
// into view.
static void TestComboKeyboardFlipAndScroll(void) {
    static const Clay_String kItems[20] = {
        CLAY_STRING("Item00"), CLAY_STRING("Item01"), CLAY_STRING("Item02"), CLAY_STRING("Item03"),
        CLAY_STRING("Item04"), CLAY_STRING("Item05"), CLAY_STRING("Item06"), CLAY_STRING("Item07"),
        CLAY_STRING("Item08"), CLAY_STRING("Item09"), CLAY_STRING("Item10"), CLAY_STRING("Item11"),
        CLAY_STRING("Item12"), CLAY_STRING("Item13"), CLAY_STRING("Item14"), CLAY_STRING("Item15"),
        CLAY_STRING("Item16"), CLAY_STRING("Item17"), CLAY_STRING("Item18"), CLAY_STRING("Item19"),
    };
    Clay_ElementId comboId = CLAY_ID("FlipCombo");
    Clay_ElementId triggerId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboTrigger"), comboId.id);
    Clay_ElementId dropdownId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboDropdown"), comboId.id);
    int32_t selected = 0;
    bool changed = false;
    // A grow spacer pushes the combo to the bottom edge of the 600px layout,
    // so the 20-item dropdown cannot fit below the trigger.
    auto body = [&]() {
        CLAY(CLAY_ID("FlipSpacer"), {
            .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } },
        }) {}
        changed = ClayWidgets_Combo(&ui, comboId, CLAY_STRING(""), kItems, 20, &selected);
    };

    Frame(MakeInput(), body);
    Clay_ElementData triggerData = Clay_GetElementData(triggerId);
    CHECK(triggerData.found);
    CHECK(triggerData.boundingBox.y > kLayoutHeight * 0.7f); // sits near the bottom
    float cx = triggerData.boundingBox.x + triggerData.boundingBox.width * 0.5f;
    float cy = triggerData.boundingBox.y + triggerData.boundingBox.height * 0.5f;

    // Click the trigger: press, then release opens the dropdown.
    Frame(PressAt(cx, cy), body);
    Clay_RenderCommandArray commands = Frame(ReleaseAt(cx, cy), body);
    CHECK(ui.openComboId == comboId.id);

    // The dropdown opened upward: its bottom edge meets the trigger's top.
    Clay_RenderCommand dropdownCommand = {};
    CHECK(FindCommandById(commands, dropdownId.id, &dropdownCommand));
    float dropdownBottom = dropdownCommand.boundingBox.y + dropdownCommand.boundingBox.height;
    CHECK(dropdownBottom <= triggerData.boundingBox.y + 1.0f);
    CHECK(dropdownCommand.boundingBox.y >= -1.0f); // fully on screen

    // The capped dropdown is shorter than the full 20-item list, so it
    // scrolls. Up from item 0 wraps the highlight to the last item and the
    // dropdown scrolls it into view.
    ClayWidgets_Input up = MakeInput();
    up.keyUp = true;
    Frame(up, body);
    CHECK(ui.comboHighlightIndex == 19);
    Frame(MakeInput(), body); // scroll applied from last frame's geometry
    Clay_ScrollContainerData dropScroll = Clay_GetScrollContainerData(dropdownId);
    CHECK(dropScroll.found);
    CHECK(dropScroll.scrollPosition != NULL);
    CHECK(dropScroll.contentDimensions.height > dropScroll.scrollContainerDimensions.height);
    CHECK(dropScroll.scrollPosition->y < 0.0f);

    // Enter chooses the highlighted item and closes the dropdown.
    ClayWidgets_Input enter = MakeInput();
    enter.keyEnter = true;
    Frame(enter, body);
    CHECK(changed);
    CHECK(selected == 19);
    CHECK(ui.openComboId == 0);
}

// Wheel-scrolling a scroll panel must move the rendered content, not just the
// internal scroll position. Regression test for an evaluation-order bug where
// Clay_GetScrollOffset() was read before the content element opened, freezing
// the clip offset at zero while the scrollbar thumb still moved.
static void TestScrollPanelWheelMovesContent(void) {
    Clay_ElementId panelId = CLAY_ID("WheelPanel");
    Clay_ElementId contentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollPanelContent"), panelId.id);
    Clay_ElementId firstRowId = CLAY_IDI("WheelRow", 0);
    ClayWidgets_ScrollPanelOptions options = {};
    options.width = CLAY_SIZING_FIXED(300);
    options.height = CLAY_SIZING_FIXED(200);
    auto body = [&]() {
        ClayWidgets_BeginScrollPanel(&ui, panelId, options);
        for (int32_t i = 0; i < 30; ++i) {
            CLAY(CLAY_IDI("WheelRow", i), {
                .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(24) } },
            }) {}
        }
        ClayWidgets_EndScrollPanel(&ui, panelId);
    };

    Frame(MakeInput(), body);
    Clay_ElementData panelData = Clay_GetElementData(panelId);
    Clay_ElementData rowBefore = Clay_GetElementData(firstRowId);
    CHECK(panelData.found);
    CHECK(rowBefore.found);

    // Wheel down over the panel for a few frames.
    ClayWidgets_Input wheel = MakeInput();
    wheel.mouseX = panelData.boundingBox.x + panelData.boundingBox.width * 0.5f;
    wheel.mouseY = panelData.boundingBox.y + panelData.boundingBox.height * 0.5f;
    wheel.scrollY = -3.0f;
    Frame(wheel, body);
    Frame(wheel, body);
    Frame(MakeInput(), body);

    // The internal scroll position moved...
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.found);
    CHECK(scrollData.scrollPosition != NULL);
    CHECK(scrollData.scrollPosition->y < 0.0f);
    // ...and so did the rendered content (this is what regressed).
    Clay_ElementData rowAfter = Clay_GetElementData(firstRowId);
    CHECK(rowAfter.found);
    CHECK(rowAfter.boundingBox.y < rowBefore.boundingBox.y - 1.0f);
}

// Text input fields and combo triggers share one field-height formula, so
// mixed rows line up exactly.
static void TestFieldHeightsMatch(void) {
    static const Clay_String kItems[2] = { CLAY_STRING("One"), CLAY_STRING("Two") };
    char buffer[16] = "x";
    int32_t selected = 0;
    Clay_ElementId inputId = CLAY_ID("HeightInput");
    Clay_ElementId comboId = CLAY_ID("HeightCombo");
    Clay_ElementId fieldId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextInputField"), inputId.id);
    Clay_ElementId triggerId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboTrigger"), comboId.id);

    Frame(MakeInput(), [&]() {
        ClayWidgets_TextInputOptions options = {};
        ClayWidgets_TextInput(&ui, inputId, CLAY_STRING(""), buffer, (int32_t)sizeof(buffer), options);
        ClayWidgets_Combo(&ui, comboId, CLAY_STRING(""), kItems, 2, &selected);
    });

    Clay_ElementData fieldData = Clay_GetElementData(fieldId);
    Clay_ElementData triggerData = Clay_GetElementData(triggerId);
    CHECK(fieldData.found);
    CHECK(triggerData.found);
    CHECK(fieldData.boundingBox.height == triggerData.boundingBox.height);
}

// The toggle's animated track has a stable derived id (a Clay transition
// requirement), and clicking the toggle flips the bound value.
static void TestToggleTrackAndClick(void) {
    Clay_ElementId toggleId = CLAY_ID("StableToggle");
    Clay_ElementId trackId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsToggleTrack"), toggleId.id);
    bool value = false;
    auto body = [&]() {
        ClayWidgets_Toggle(&ui, toggleId, CLAY_STRING("Toggle"), &value);
    };

    Frame(MakeInput(), body);
    Clay_ElementData trackData = Clay_GetElementData(trackId);
    CHECK(trackData.found);

    Clay_ElementData toggleData = Clay_GetElementData(toggleId);
    float cx = toggleData.boundingBox.x + 10.0f;
    float cy = toggleData.boundingBox.y + toggleData.boundingBox.height * 0.5f;
    Frame(PressAt(cx, cy), body);
    Frame(ReleaseAt(cx, cy), body);
    CHECK(value == true);

    // Disabled toggles are inert.
    bool disabledValue = false;
    auto disabledBody = [&]() {
        ClayWidgets_ToggleEx(&ui, CLAY_ID("InertToggle"), CLAY_STRING("Inert"), &disabledValue, true);
    };
    Frame(MakeInput(), disabledBody);
    Clay_ElementData inertData = Clay_GetElementData(CLAY_ID("InertToggle"));
    float ix = inertData.boundingBox.x + 10.0f;
    float iy = inertData.boundingBox.y + inertData.boundingBox.height * 0.5f;
    Frame(PressAt(ix, iy), disabledBody);
    Frame(ReleaseAt(ix, iy), disabledBody);
    CHECK(disabledValue == false);
}

// Child ids derived from different parents can't collide, and are stable.
static void TestChildIdDerivation(void) {
    Clay_ElementId parentA = CLAY_ID("ChildIdA");
    Clay_ElementId parentB = CLAY_ID("ChildIdB");
    Clay_String label = CLAY_STRING("Item");
    CHECK(ClayWidgets__ChildId(parentA, label, 0).id == ClayWidgets__ChildId(parentA, label, 0).id);
    CHECK(ClayWidgets__ChildId(parentA, label, 0).id != ClayWidgets__ChildId(parentA, label, 1).id);
    CHECK(ClayWidgets__ChildId(parentA, label, 0).id != ClayWidgets__ChildId(parentB, label, 0).id);
}

// One danger red everywhere: the badge/toast semantic color and the danger
// button's idle fill are all the theme's dangerColor.
static void TestDangerColorUnified(void) {
    CHECK(SameColor(ClayWidgets__SemanticColor(&ui, CLAY_WIDGETS_BADGE_DANGER), ui.theme.dangerColor));

    Clay_ElementId buttonId = CLAY_ID("DangerButton");
    ClayWidgets_ButtonOptions options = {};
    options.variant = CLAY_WIDGETS_BUTTON_DANGER;
    auto body = [&]() {
        ClayWidgets_ButtonEx(&ui, buttonId, CLAY_STRING("Delete"), options);
    };
    Frame(MakeInput(), body);
    Clay_RenderCommandArray commands = Frame(MakeInput(), body);
    Clay_RenderCommand buttonCommand = {};
    CHECK(FindCommandById(commands, buttonId.id, &buttonCommand));
    CHECK(buttonCommand.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    CHECK(SameColor(buttonCommand.renderData.rectangle.backgroundColor, ui.theme.dangerColor));
}

// UTF-8 boundary and word-bound helpers used by the text input.
static void TestUtf8Helpers(void) {
    const char *text = "a\xC3\xA9!b"; // a, é (2 bytes), '!', b
    int32_t length = 5;
    CHECK(ClayWidgets__Utf8NextBoundary(text, length, 0) == 1);
    CHECK(ClayWidgets__Utf8NextBoundary(text, length, 1) == 3); // skips the continuation byte
    CHECK(ClayWidgets__Utf8PrevBoundary(text, 3) == 1);
    CHECK(ClayWidgets__Utf8PrevBoundary(text, 1) == 0);

    const char *words = "hello world";
    int32_t wordStart = 0;
    int32_t wordEnd = 0;
    ClayWidgets__FindWordBounds(words, 11, 3, &wordStart, &wordEnd);
    CHECK(wordStart == 0);
    CHECK(wordEnd == 5);
    ClayWidgets__FindWordBounds(words, 11, 8, &wordStart, &wordEnd);
    CHECK(wordStart == 6);
    CHECK(wordEnd == 11);
}

// ---------------------------------------------------------------------------

struct TestCase {
    const char *name;
    void (*fn)(void);
};

int main(void) {
    uint32_t clayMemorySize = Clay_MinMemorySize();
    void *clayMemory = std::malloc(clayMemorySize);
    if (!clayMemory) {
        std::printf("failed to allocate Clay arena\n");
        return 1;
    }
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, clayMemory);
    Clay_Initialize(arena, (Clay_Dimensions){kLayoutWidth, kLayoutHeight}, (Clay_ErrorHandler){nullptr, nullptr});
    Clay_SetMeasureTextFunction(FakeMeasureText, nullptr);

    const TestCase tests[] = {
        { "focus traversal (Tab / Shift+Tab)", TestFocusTraversal },
        { "modal focus trap", TestModalFocusTrap },
        { "text input editing + clipboard", TestTextInputEditingAndClipboard },
        { "text input disabled", TestTextInputDisabled },
        { "slider keyboard + disabled", TestSliderKeyboard },
        { "stepper disabled", TestStepperDisabled },
        { "scratch overflow reported", TestScratchOverflowReported },
        { "focusables overflow reported", TestFocusablesOverflowReported },
        { "table columns overflow reported", TestTableColumnsOverflowReported },
        { "toast queue", TestToastQueue },
        { "combo keyboard, edge flip, scroll", TestComboKeyboardFlipAndScroll },
        { "scroll panel wheel moves content", TestScrollPanelWheelMovesContent },
        { "field heights match", TestFieldHeightsMatch },
        { "toggle stable track id + click", TestToggleTrackAndClick },
        { "child id derivation", TestChildIdDerivation },
        { "danger color unified", TestDangerColorUnified },
        { "utf-8 helpers", TestUtf8Helpers },
    };

    for (const TestCase &test : tests) {
        ResetUi();
        std::printf("[test] %s\n", test.name);
        test.fn();
    }

    std::printf("\n%d checks, %d failures\n", g_checkCount, g_failCount);
    std::free(clayMemory);
    return g_failCount == 0 ? 0 : 1;
}
