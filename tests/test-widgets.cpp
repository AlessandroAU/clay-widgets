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

static Clay_BoundingBox IntersectBox(Clay_BoundingBox a, Clay_BoundingBox b) {
    float x1 = a.x > b.x ? a.x : b.x;
    float y1 = a.y > b.y ? a.y : b.y;
    float x2 = (a.x + a.width) < (b.x + b.width) ? (a.x + a.width) : (b.x + b.width);
    float y2 = (a.y + a.height) < (b.y + b.height) ? (a.y + a.height) : (b.y + b.height);
    Clay_BoundingBox r = { x1, y1, x2 > x1 ? x2 - x1 : 0.0f, y2 > y1 ? y2 - y1 : 0.0f };
    return r;
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

    CHECK(ui.focusedId == modalClose.id); // the modal assigns initial focus
    ui.focusedId = inside.id; // activate a body control, not the close button
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

// A button clicks on press+release over it (not press alone, not a release
// elsewhere), activates from the keyboard when focused, and is inert disabled.
static void TestButtonClickAndKeyboard(void) {
    Clay_ElementId buttonId = CLAY_ID("ClickButton");
    bool clicked = false;
    auto body = [&]() {
        clicked = ClayWidgets_Button(&ui, buttonId, CLAY_STRING("Click"));
    };

    Frame(MakeInput(), body); // establish geometry
    Clay_ElementData buttonData = Clay_GetElementData(buttonId);
    CHECK(buttonData.found);
    float cx = buttonData.boundingBox.x + buttonData.boundingBox.width * 0.5f;
    float cy = buttonData.boundingBox.y + buttonData.boundingBox.height * 0.5f;

    // Press alone is not a click; the release over the button completes it.
    Frame(PressAt(cx, cy), body);
    CHECK(!clicked);
    Frame(ReleaseAt(cx, cy), body);
    CHECK(clicked);

    // Press on the button but release off it: no click.
    Frame(PressAt(cx, cy), body);
    Frame(ReleaseAt(-50.0f, -50.0f), body);
    CHECK(!clicked);

    // Keyboard: Tab focuses the button, Enter activates it.
    ClayWidgets_Input tab = MakeInput();
    tab.keyTab = true;
    Frame(tab, body);
    CHECK(ui.focusedId == buttonId.id);
    ClayWidgets_Input enter = MakeInput();
    enter.keyEnter = true;
    Frame(enter, body);
    CHECK(clicked);

    // Disabled: never clicks, never takes focus.
    Clay_ElementId disabledId = CLAY_ID("DisabledButton");
    bool disabledClicked = false;
    ClayWidgets_ButtonOptions disabledOptions = {};
    disabledOptions.disabled = true;
    auto disabledBody = [&]() {
        disabledClicked = ClayWidgets_ButtonEx(&ui, disabledId, CLAY_STRING("Nope"), disabledOptions);
    };
    ui.focusedId = 0;
    Frame(MakeInput(), disabledBody);
    Clay_ElementData disabledData = Clay_GetElementData(disabledId);
    float dx = disabledData.boundingBox.x + disabledData.boundingBox.width * 0.5f;
    float dy = disabledData.boundingBox.y + disabledData.boundingBox.height * 0.5f;
    Frame(PressAt(dx, dy), disabledBody);
    Frame(ReleaseAt(dx, dy), disabledBody);
    CHECK(!disabledClicked);
    Frame(tab, disabledBody);
    CHECK(ui.focusedId == 0);
}

// Clicking a checkbox flips the bound value each time and reports the change;
// keyboard activation toggles too; disabled checkboxes are inert.
static void TestCheckboxToggle(void) {
    Clay_ElementId checkId = CLAY_ID("ToggleCheckbox");
    bool value = false;
    bool changed = false;
    auto body = [&]() {
        changed = ClayWidgets_Checkbox(&ui, checkId, CLAY_STRING("Check me"), &value);
    };

    Frame(MakeInput(), body);
    Clay_ElementData checkData = Clay_GetElementData(checkId);
    CHECK(checkData.found);
    float cx = checkData.boundingBox.x + checkData.boundingBox.width * 0.5f;
    float cy = checkData.boundingBox.y + checkData.boundingBox.height * 0.5f;

    // Click checks it, a second click unchecks it.
    Frame(PressAt(cx, cy), body);
    Frame(ReleaseAt(cx, cy), body);
    CHECK(changed);
    CHECK(value == true);
    Frame(PressAt(cx, cy), body);
    Frame(ReleaseAt(cx, cy), body);
    CHECK(changed);
    CHECK(value == false);

    // Keyboard: Tab focuses, Enter toggles.
    ClayWidgets_Input tab = MakeInput();
    tab.keyTab = true;
    Frame(tab, body);
    CHECK(ui.focusedId == checkId.id);
    ClayWidgets_Input enter = MakeInput();
    enter.keyEnter = true;
    Frame(enter, body);
    CHECK(value == true);

    // Disabled: value inert, focus unreachable.
    Clay_ElementId disabledId = CLAY_ID("DisabledCheckbox");
    bool disabledValue = false;
    auto disabledBody = [&]() {
        ClayWidgets_CheckboxEx(&ui, disabledId, CLAY_STRING("Inert"), &disabledValue, true);
    };
    ui.focusedId = 0;
    Frame(MakeInput(), disabledBody);
    Clay_ElementData disabledData = Clay_GetElementData(disabledId);
    float dx = disabledData.boundingBox.x + disabledData.boundingBox.width * 0.5f;
    float dy = disabledData.boundingBox.y + disabledData.boundingBox.height * 0.5f;
    Frame(PressAt(dx, dy), disabledBody);
    Frame(ReleaseAt(dx, dy), disabledBody);
    CHECK(disabledValue == false);
    Frame(tab, disabledBody);
    CHECK(ui.focusedId == 0);
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

// Multi-line editing in the text area: Enter inserts newlines, Up/Down move
// by line remembering the preferred column, Home/End are line-scoped,
// backspace at a line start merges lines, copy carries newlines, and paste
// keeps them (normalizing CRLF and lone CR to LF).
static void TestTextAreaEditingAndNavigation(void) {
    Clay_ElementId areaId = CLAY_ID("EditArea");
    char buffer[128] = "";
    bool changed = false;
    ClayWidgets_TextAreaOptions options = {};
    auto body = [&]() {
        changed = ClayWidgets_TextArea(&ui, areaId, CLAY_STRING(""), buffer, (int32_t)sizeof(buffer), options);
    };

    Frame(MakeInput(), body); // establish geometry
    Clay_ElementData areaData = Clay_GetElementData(areaId);
    CHECK(areaData.found);
    float cx = areaData.boundingBox.x + areaData.boundingBox.width * 0.5f;
    float cy = areaData.boundingBox.y + areaData.boundingBox.height * 0.5f;
    Frame(PressAt(cx, cy), body);
    CHECK(ui.focusedId == areaId.id);
    Frame(ReleaseAt(cx, cy), body);

    // Type three lines: "abcdef" / "ab" / "abcdef".
    ClayWidgets_Input enter = MakeInput();
    enter.keyEnter = true;
    ClayWidgets_Input typeLong = MakeInput();
    typeLong.textUtf8 = "abcdef";
    typeLong.textUtf8Length = 6;
    ClayWidgets_Input typeShort = MakeInput();
    typeShort.textUtf8 = "ab";
    typeShort.textUtf8Length = 2;
    Frame(typeLong, body);
    Frame(enter, body);
    CHECK(changed);
    Frame(typeShort, body);
    Frame(enter, body);
    Frame(typeLong, body);
    CHECK(std::strcmp(buffer, "abcdef\nab\nabcdef") == 0);
    CHECK(ui.textCursor == 16);

    // Up into the short line clamps to its end; Up again restores the
    // remembered column in the long first line. Down retraces both moves.
    ClayWidgets_Input up = MakeInput();
    up.keyUp = true;
    ClayWidgets_Input down = MakeInput();
    down.keyDown = true;
    Frame(up, body);
    CHECK(ui.textCursor == 9); // "ab" line: clamped to column 2 (offset 7 + 2)
    Frame(up, body);
    CHECK(ui.textCursor == 6); // first line: preferred column 6 restored
    Frame(down, body);
    CHECK(ui.textCursor == 9);
    Frame(down, body);
    CHECK(ui.textCursor == 16);
    Frame(up, body);
    Frame(up, body);
    Frame(up, body); // Up with no line above goes to the very start
    CHECK(ui.textCursor == 0);

    // Home/End work within the caret's line, not the whole document.
    Frame(down, body);
    CHECK(ui.textCursor == 7); // line 1, column 0
    ClayWidgets_Input end = MakeInput();
    end.keyEnd = true;
    Frame(end, body);
    CHECK(ui.textCursor == 9);
    ClayWidgets_Input home = MakeInput();
    home.keyHome = true;
    Frame(home, body);
    CHECK(ui.textCursor == 7);

    // Backspace at a line start merges the line into the previous one.
    ClayWidgets_Input backspace = MakeInput();
    backspace.keyBackspace = true;
    Frame(backspace, body);
    CHECK(std::strcmp(buffer, "abcdefab\nabcdef") == 0);
    CHECK(ui.textCursor == 6);

    // Copy carries the newline; multi-line paste replaces the selection and
    // normalizes CRLF and lone CR to LF.
    ClayWidgets_Input selectAll = MakeInput();
    selectAll.keySelectAll = true;
    Frame(selectAll, body);
    ClayWidgets_Input copy = MakeInput();
    copy.keyCopy = true;
    Frame(copy, body);
    CHECK(g_clipboard == "abcdefab\nabcdef");

    g_clipboard = "one\r\ntwo\rthree";
    Frame(selectAll, body);
    ClayWidgets_Input paste = MakeInput();
    paste.keyPaste = true;
    Frame(paste, body);
    CHECK(changed);
    CHECK(std::strcmp(buffer, "one\ntwo\nthree") == 0);

    // Escape releases focus.
    ClayWidgets_Input escape = MakeInput();
    escape.keyEscape = true;
    Frame(escape, body);
    CHECK(ui.focusedId == 0);
}

// Text area pointer and scroll behavior: a click places the caret on the hit
// line and column, moving the caret out of view scrolls it back in (both
// directions), and a disabled area is inert.
static void TestTextAreaPointerAndScroll(void) {
    Clay_ElementId areaId = CLAY_ID("ScrollArea");
    Clay_ElementId contentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextAreaContent"), areaId.id);
    char buffer[128] = "line0\nline1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9";
    const int32_t textLength = (int32_t)std::strlen(buffer);
    ClayWidgets_TextAreaOptions options = {};
    options.height = 3.0f * kFakeLineHeight; // far less than the 10 lines of content
    auto body = [&]() {
        ClayWidgets_TextArea(&ui, areaId, CLAY_STRING(""), buffer, (int32_t)sizeof(buffer), options);
    };

    Frame(MakeInput(), body);
    Frame(MakeInput(), body);
    Clay_ElementData contentData = Clay_GetElementData(contentId);
    CHECK(contentData.found);

    // Click on line 1 between its 2nd and 3rd characters: the caret lands at
    // that line and column ("line0\n" is 6 bytes, so offset 6 + 2).
    float px = contentData.boundingBox.x + 2.0f * kFakeCharWidth + 1.0f;
    float py = contentData.boundingBox.y + 1.5f * kFakeLineHeight;
    Frame(PressAt(px, py), body);
    CHECK(ui.focusedId == areaId.id);
    CHECK(ui.textCursor == 8);
    Frame(ReleaseAt(px, py), body);

    // Caret to the end of the document (select-all, then Right collapses to
    // the selection end): the view scrolls down to follow it.
    ClayWidgets_Input selectAll = MakeInput();
    selectAll.keySelectAll = true;
    Frame(selectAll, body);
    ClayWidgets_Input right = MakeInput();
    right.keyRight = true;
    Frame(right, body);
    CHECK(ui.textCursor == textLength);
    Frame(MakeInput(), body);
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.found);
    CHECK(scrollData.scrollPosition != NULL);
    CHECK(scrollData.scrollPosition->y < 0.0f);

    // Caret back to the start: the view scrolls up again.
    Frame(selectAll, body);
    ClayWidgets_Input left = MakeInput();
    left.keyLeft = true;
    Frame(left, body);
    CHECK(ui.textCursor == 0);
    Frame(MakeInput(), body);
    scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.scrollPosition->y == 0.0f);

    // The wheel scrolls the overflowing content directly (Clay's scroll
    // container consumes it; no caret movement involved, and caret-follow
    // must not fight it back).
    ClayWidgets_Input wheel = MakeInput();
    wheel.mouseX = px;
    wheel.mouseY = py;
    wheel.scrollY = -3.0f;
    Frame(wheel, body);
    Frame(wheel, body);
    Frame(MakeInput(), body);
    scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.scrollPosition->y < 0.0f);

    // Disabled: not focusable, ignores clicks and typing, buffer untouched.
    ui.focusedId = 0;
    options.disabled = true;
    Frame(MakeInput(), body);
    Frame(PressAt(px, py), body);
    CHECK(ui.focusedId == 0);
    Frame(ReleaseAt(px, py), body);
    ClayWidgets_Input type = MakeInput();
    type.textUtf8 = "x";
    type.textUtf8Length = 1;
    Frame(type, body);
    CHECK(std::strcmp(buffer, "line0\nline1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9") == 0);
}

// Soft word wrap: a single long hard line becomes multiple visual rows,
// breaks fall after spaces, Up/Down move between rows of the same hard line,
// and noWrap collapses it back to one horizontally-scrolling row.
static void TestTextAreaSoftWrap(void) {
    Clay_ElementId areaId = CLAY_ID("WrapArea");
    Clay_ElementId contentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextAreaContent"), areaId.id);
    // One hard line of 40 five-byte words ("wwww "), 200 chars = 1600px at
    // the fake font: far wider than the ~770px content, so it must wrap.
    char buffer[256];
    for (int32_t i = 0; i < 40; ++i) {
        std::memcpy(buffer + i * 5, "wwww ", 5);
    }
    buffer[200] = '\0';
    ClayWidgets_TextAreaOptions options = {};
    auto body = [&]() {
        ClayWidgets_TextArea(&ui, areaId, CLAY_STRING(""), buffer, (int32_t)sizeof(buffer), options);
    };

    Frame(MakeInput(), body); // establish geometry (first frame renders unwrapped)
    Frame(MakeInput(), body);
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.found);
    CHECK(scrollData.contentDimensions.height > kFakeLineHeight * 1.5f); // wrapped into rows

    // Clicking at the far left of the second visual row lands the caret at a
    // word start: wrap breaks fall after spaces, and words are 5-byte aligned.
    Clay_ElementData contentData = Clay_GetElementData(contentId);
    CHECK(contentData.found);
    float px = contentData.boundingBox.x + 1.0f;
    float py = contentData.boundingBox.y + 1.5f * kFakeLineHeight;
    Frame(PressAt(px, py), body);
    CHECK(ui.focusedId == areaId.id);
    int32_t rowTwoStart = ui.textCursor;
    CHECK(rowTwoStart > 0);
    CHECK(rowTwoStart < 200);
    CHECK(buffer[rowTwoStart - 1] == ' ');
    CHECK(rowTwoStart % 5 == 0);
    Frame(ReleaseAt(px, py), body);

    // Up/Down move between visual rows of the SAME hard line (there is no
    // '\n' anywhere in the buffer).
    ClayWidgets_Input up = MakeInput();
    up.keyUp = true;
    Frame(up, body);
    CHECK(ui.textCursor == 0);
    ClayWidgets_Input down = MakeInput();
    down.keyDown = true;
    Frame(down, body);
    CHECK(ui.textCursor == rowTwoStart);

    // noWrap: the same content is one row again (and scrolls horizontally).
    ui.focusedId = 0;
    options.noWrap = true;
    Frame(MakeInput(), body);
    Frame(MakeInput(), body);
    scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.found);
    CHECK(scrollData.contentDimensions.height <= kFakeLineHeight * 1.5f);
    CHECK(scrollData.contentDimensions.width > 1000.0f);
}

// A text area nested in a scroll panel, straddling the panel's bottom edge:
// its selection overlays and scrollbar must not be visible past the panel
// clip. Regression test - Clay scissors a floating root only to its own clip
// element's box (never the enclosing panel's), so these painted over the
// elements below the panel until the overlays were clamped by hand and the
// scrollbar got CLAY_CLIP_TO_ATTACHED_PARENT.
static void TestTextAreaOverlaysClippedToPanel(void) {
    Clay_ElementId panelId = CLAY_ID("ClipPanel");
    Clay_ElementId panelContentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollPanelContent"), panelId.id);
    Clay_ElementId areaId = CLAY_ID("ClipArea");
    Clay_ElementId areaContentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextAreaContent"), areaId.id);
    Clay_ElementId thumbId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarThumb"), areaContentId.id);
    char buffer[128] = "l0\nl1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9";
    ClayWidgets_ScrollPanelOptions panelOptions = {};
    panelOptions.width = CLAY_SIZING_FIXED(400);
    panelOptions.height = CLAY_SIZING_FIXED(150);
    ClayWidgets_TextAreaOptions options = {};
    options.height = 6.0f * kFakeLineHeight; // 10 lines of content: overflows, so the scrollbar shows
    auto body = [&]() {
        ClayWidgets_BeginScrollPanel(&ui, panelId, panelOptions);
        CLAY(CLAY_ID("ClipFiller"), {
            .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(60) } },
        }) {}
        ClayWidgets_TextArea(&ui, areaId, CLAY_STRING(""), buffer, (int32_t)sizeof(buffer), options);
        ClayWidgets_EndScrollPanel(&ui, panelId);
    };

    Frame(MakeInput(), body);
    Frame(MakeInput(), body);
    Clay_ElementData panelClipData = Clay_GetElementData(panelContentId);
    Clay_ElementData fieldData = Clay_GetElementData(areaContentId);
    CHECK(panelClipData.found);
    CHECK(fieldData.found);
    // The scenario only bites if the field actually straddles the panel edge.
    float panelBottom = panelClipData.boundingBox.y + panelClipData.boundingBox.height;
    CHECK(fieldData.boundingBox.y < panelBottom);
    CHECK(fieldData.boundingBox.y + fieldData.boundingBox.height > panelBottom);

    // Focus via a click in the visible sliver of the field, then select all.
    float px = fieldData.boundingBox.x + 4.0f;
    float py = (fieldData.boundingBox.y + panelBottom) * 0.5f;
    Frame(PressAt(px, py), body);
    CHECK(ui.focusedId == areaId.id);
    Frame(ReleaseAt(px, py), body);
    ClayWidgets_Input selectAll = MakeInput();
    selectAll.keySelectAll = true;
    Clay_RenderCommandArray commands = Frame(selectAll, body);

    // Walk the commands like a renderer: scissors nest by intersection and
    // apply to subsequent draws. Every visible piece of a selection rect or
    // the text area's scroll thumb must lie within the panel clip.
    std::vector<Clay_BoundingBox> scissors;
    int32_t selectionRects = 0;
    bool sawThumb = false;
    for (int32_t i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
            Clay_BoundingBox box = cmd->boundingBox;
            if (!scissors.empty()) {
                box = IntersectBox(scissors.back(), box);
            }
            scissors.push_back(box);
            continue;
        }
        if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
            if (!scissors.empty()) {
                scissors.pop_back();
            }
            continue;
        }
        if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
            continue;
        }
        bool isSelection = SameColor(cmd->renderData.rectangle.backgroundColor, ui.theme.accentMutedColor);
        bool isThumb = cmd->id == thumbId.id;
        if (!isSelection && !isThumb) {
            continue;
        }
        if (isSelection) {
            selectionRects++;
        }
        if (isThumb) {
            sawThumb = true;
        }
        Clay_BoundingBox visible = cmd->boundingBox;
        if (!scissors.empty()) {
            visible = IntersectBox(scissors.back(), visible);
        }
        if (visible.width <= 0.0f || visible.height <= 0.0f) {
            continue; // fully scissored away - fine
        }
        CHECK(visible.y >= panelClipData.boundingBox.y - 0.5f);
        CHECK(visible.y + visible.height <= panelBottom + 0.5f);
    }
    CHECK(selectionRects > 0);
    CHECK(sawThumb);
}

// A focused slider is keyboard-operable: arrows step, Home/End jump. Disabled
// sliders ignore keys and never take focus.
static void TestSliderKeyboard(void) {
    Clay_ElementId sliderId = CLAY_ID("KeySlider");
    float value = 5.0f;
    bool changed = false;
    ClayWidgets_SliderOptions options = {};
    options.minValue = 0.0f;
    options.maxValue = 10.0f;
    options.step = 1.0f;
    auto body = [&]() {
        changed = ClayWidgets_Slider(&ui, sliderId, &value, options);
    };

    Frame(MakeInput(), body);
    CHECK(!changed); // idle: value untouched
    ClayWidgets_Input tab = MakeInput();
    tab.keyTab = true;
    Frame(tab, body);
    CHECK(ui.focusedId == sliderId.id);

    ClayWidgets_Input right = MakeInput();
    right.keyRight = true;
    Frame(right, body);
    CHECK(value == 6.0f);
    CHECK(changed);

    // An out-of-range value is clamped in place, and that counts as changed.
    value = 42.0f;
    Frame(MakeInput(), body);
    CHECK(value == 10.0f);
    CHECK(changed);
    value = 6.0f;

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

// The per-widget state pool: zero-initialized on first claim, persistent and
// pointer-stable across frames, per-id isolated, invalid requests refused,
// stale slots recycled least-recently-used, and exhaustion reported when
// every slot is live.
static void TestStatePool(void) {
    Frame(MakeInput(), []() {});
    int32_t *a = (int32_t *)ClayWidgets_GetState(&ui, 101, (int32_t)sizeof(int32_t));
    CHECK(a != NULL);
    CHECK(*a == 0);
    *a = 42;

    Frame(MakeInput(), []() {});
    int32_t *aAgain = (int32_t *)ClayWidgets_GetState(&ui, 101, (int32_t)sizeof(int32_t));
    CHECK(aAgain == a);
    CHECK(*aAgain == 42);
    int32_t *b = (int32_t *)ClayWidgets_GetState(&ui, 202, (int32_t)sizeof(int32_t));
    CHECK(b != NULL);
    CHECK(b != a);
    CHECK(*b == 0);

    // Invalid requests: id 0 (the empty-slot sentinel) and sizes outside the
    // slot capacity are refused; the size overflow is reported.
    CHECK(ClayWidgets_GetState(&ui, 0, 4) == NULL);
    CHECK(ClayWidgets_GetState(&ui, 303, CLAY_WIDGETS_STATE_SLOT_SIZE + 1) == NULL);
    CHECK(ErrorsContain("CLAY_WIDGETS_STATE_SLOT_SIZE"));

    // Two idle frames age out ids 101/202, so claiming a full pool's worth of
    // fresh ids in one frame succeeds (empty slots first, then recycled ones)...
    g_errors.clear();
    Frame(MakeInput(), []() {});
    Frame(MakeInput(), []() {});
    for (uint32_t i = 0; i < CLAY_WIDGETS_MAX_STATE_SLOTS; ++i) {
        CHECK(ClayWidgets_GetState(&ui, 1000 + i, 8) != NULL);
    }
    CHECK(g_errors.empty());
    // ...but one more, while every slot was touched this frame, is refused loudly.
    CHECK(ClayWidgets_GetState(&ui, 9999, 8) == NULL);
    CHECK(g_errors.size() == 1);
    CHECK(ErrorsContain("state pool exhausted"));

    // Recycling is least-recently-used: keep one id warm across frames, then
    // claim a new id - a stale slot is recycled, the warm slot survives intact.
    int32_t *warm = (int32_t *)ClayWidgets_GetState(&ui, 1000, (int32_t)sizeof(int32_t));
    CHECK(warm != NULL);
    *warm = 7;
    Frame(MakeInput(), []() {});
    ClayWidgets_GetState(&ui, 1000, (int32_t)sizeof(int32_t));
    Frame(MakeInput(), []() {});
    ClayWidgets_GetState(&ui, 1000, (int32_t)sizeof(int32_t));
    Frame(MakeInput(), []() {});
    int32_t *fresh = (int32_t *)ClayWidgets_GetState(&ui, 7777, (int32_t)sizeof(int32_t));
    CHECK(fresh != NULL);
    CHECK(*fresh == 0);
    int32_t *warmAgain = (int32_t *)ClayWidgets_GetState(&ui, 1000, (int32_t)sizeof(int32_t));
    CHECK(warmAgain == warm);
    CHECK(*warmAgain == 7);
}

// Dragging the scroll bar thumb scrolls the container; releasing ends the
// drag. Exercises the per-thumb drag state in the state pool.
static void TestScrollBarThumbDrag(void) {
    Clay_ElementId panelId = CLAY_ID("DragPanel");
    Clay_ElementId contentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollPanelContent"), panelId.id);
    Clay_ElementId thumbId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarThumb"), contentId.id);
    ClayWidgets_ScrollPanelOptions options = {};
    options.width = CLAY_SIZING_FIXED(300);
    options.height = CLAY_SIZING_FIXED(200);
    auto body = [&]() {
        ClayWidgets_BeginScrollPanel(&ui, panelId, options);
        for (int32_t i = 0; i < 30; ++i) {
            CLAY(CLAY_IDI("DragRow", i), {
                .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(24) } },
            }) {}
        }
        ClayWidgets_EndScrollPanel(&ui, panelId);
    };

    Frame(MakeInput(), body);
    Frame(MakeInput(), body); // scroll bar appears once container geometry exists
    Clay_ElementData thumbData = Clay_GetElementData(thumbId);
    CHECK(thumbData.found);
    float cx = thumbData.boundingBox.x + thumbData.boundingBox.width * 0.5f;
    float cy = thumbData.boundingBox.y + thumbData.boundingBox.height * 0.5f;

    // Press the thumb, then drag downward while holding.
    Frame(PressAt(cx, cy), body);
    CHECK(ui.activeId == thumbId.id);
    ClayWidgets_Input dragInput = MakeInput();
    dragInput.mouseX = cx;
    dragInput.mouseY = cy + 50.0f;
    dragInput.pointerDown = true;
    Frame(dragInput, body);

    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.found);
    CHECK(scrollData.scrollPosition != NULL);
    CHECK(scrollData.scrollPosition->y < 0.0f);
    float draggedY = scrollData.scrollPosition->y;

    // Release ends the drag; further pointer movement leaves the scroll alone.
    Frame(ReleaseAt(cx, cy + 50.0f), body);
    CHECK(ui.activeId == 0);
    ClayWidgets_Input moveInput = MakeInput();
    moveInput.mouseX = cx;
    moveInput.mouseY = cy + 120.0f;
    Frame(moveInput, body);
    scrollData = Clay_GetScrollContainerData(contentId);
    CHECK(scrollData.scrollPosition->y == draggedY);
}

// A tooltip appears after the pointer dwells on its anchor, and leaving the
// anchor restarts the dwell timer. Exercises the per-anchor state pool timer.
static void TestTooltipDwell(void) {
    Clay_ElementId anchorId = CLAY_ID("TipAnchor");
    Clay_ElementId tooltipId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTooltip"), anchorId.id);
    const float delay = 0.03f; // ~2 frames at the test dt of 1/60
    auto body = [&]() {
        ClayWidgets_Button(&ui, anchorId, CLAY_STRING("Hover me"));
        ClayWidgets_TooltipEx(&ui, anchorId, CLAY_STRING("Tip"), delay);
    };

    Frame(MakeInput(), body);
    Clay_ElementData anchorData = Clay_GetElementData(anchorId);
    CHECK(anchorData.found);
    ClayWidgets_Input hover = MakeInput();
    hover.mouseX = anchorData.boundingBox.x + anchorData.boundingBox.width * 0.5f;
    hover.mouseY = anchorData.boundingBox.y + anchorData.boundingBox.height * 0.5f;

    // First hover frame starts the timer at zero: no tooltip yet.
    Clay_RenderCommandArray commands = Frame(hover, body);
    CHECK(!FindCommandById(commands, tooltipId.id, NULL));
    // Dwell past the delay: tooltip appears.
    Frame(hover, body);
    commands = Frame(hover, body);
    CHECK(FindCommandById(commands, tooltipId.id, NULL));

    // Leaving the anchor and returning restarts the timer from zero.
    Frame(MakeInput(), body);
    Frame(MakeInput(), body);
    commands = Frame(hover, body);
    CHECK(!FindCommandById(commands, tooltipId.id, NULL));
    Frame(hover, body);
    commands = Frame(hover, body);
    CHECK(FindCommandById(commands, tooltipId.id, NULL));
}

// Cursor hints: pointer over click targets, I-beam over editable text,
// default over nothing and over disabled widgets; the text area's scrollbar
// thumb overrides the I-beam; a slider drag keeps the pointer cursor after
// the mouse leaves the track.
static void TestCursorHints(void) {
    Clay_ElementId buttonId = CLAY_ID("CursorButton");
    Clay_ElementId disabledId = CLAY_ID("CursorDisabled");
    Clay_ElementId inputId = CLAY_ID("CursorInput");
    Clay_ElementId sliderId = CLAY_ID("CursorSlider");
    Clay_ElementId areaId = CLAY_ID("CursorArea");
    Clay_ElementId areaContentId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsTextAreaContent"), areaId.id);
    Clay_ElementId thumbId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarThumb"), areaContentId.id);
    char inputBuffer[16] = "x";
    char areaBuffer[64] = "l0\nl1\nl2\nl3\nl4\nl5\nl6\nl7";
    float sliderValue = 5.0f;
    ClayWidgets_ButtonOptions disabledOptions = {};
    disabledOptions.disabled = true;
    ClayWidgets_TextInputOptions inputOptions = {};
    ClayWidgets_SliderOptions sliderOptions = {};
    sliderOptions.minValue = 0.0f;
    sliderOptions.maxValue = 10.0f;
    ClayWidgets_TextAreaOptions areaOptions = {};
    areaOptions.height = 3.0f * kFakeLineHeight; // overflows: scrollbar appears
    auto body = [&]() {
        ClayWidgets_Button(&ui, buttonId, CLAY_STRING("Click"));
        ClayWidgets_ButtonEx(&ui, disabledId, CLAY_STRING("Nope"), disabledOptions);
        ClayWidgets_TextInput(&ui, inputId, CLAY_STRING(""), inputBuffer, (int32_t)sizeof(inputBuffer), inputOptions);
        ClayWidgets_Slider(&ui, sliderId, &sliderValue, sliderOptions);
        ClayWidgets_TextArea(&ui, areaId, CLAY_STRING(""), areaBuffer, (int32_t)sizeof(areaBuffer), areaOptions);
    };

    auto centerOf = [](Clay_ElementId id, float *x, float *y) {
        Clay_ElementData data = Clay_GetElementData(id);
        *x = data.boundingBox.x + data.boundingBox.width * 0.5f;
        *y = data.boundingBox.y + data.boundingBox.height * 0.5f;
        return data.found;
    };
    auto hoverAt = [&](float x, float y) {
        ClayWidgets_Input input = MakeInput();
        input.mouseX = x;
        input.mouseY = y;
        Frame(input, body);
    };

    Frame(MakeInput(), body); // establish geometry
    Frame(MakeInput(), body);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_DEFAULT); // nothing hovered

    float x = 0.0f;
    float y = 0.0f;
    CHECK(centerOf(buttonId, &x, &y));
    hoverAt(x, y);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_POINTER);

    CHECK(centerOf(disabledId, &x, &y));
    hoverAt(x, y);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_DEFAULT);

    CHECK(centerOf(inputId, &x, &y));
    hoverAt(x, y);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_TEXT);

    CHECK(centerOf(areaId, &x, &y));
    hoverAt(x, y);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_TEXT);

    // Over the text area's own scrollbar thumb, the pointer wins over the I-beam.
    CHECK(centerOf(thumbId, &x, &y));
    hoverAt(x, y);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_POINTER);

    // Dragging the slider keeps the pointer cursor even off the track.
    CHECK(centerOf(sliderId, &x, &y));
    Frame(PressAt(x, y), body);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_POINTER);
    ClayWidgets_Input dragAway = MakeInput();
    dragAway.mouseX = x;
    dragAway.mouseY = y + 200.0f; // far below the track
    dragAway.pointerDown = true;
    Frame(dragAway, body);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_POINTER);
    Frame(ReleaseAt(x, y + 200.0f), body);
    Frame(MakeInput(), body);
    CHECK(ClayWidgets_GetCursor(&ui) == CLAY_WIDGETS_CURSOR_DEFAULT);
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

// A beveled theme paints its 3D edges and drop shadows into the render command
// array after layout: extra rectangles carrying the tagged element's own id,
// spliced around that element's background rectangle. Flat themes emit none.
static void TestBeveledEdgesAndShadows(void) {
    auto countRects = [](Clay_RenderCommandArray commands, uint32_t id) {
        int32_t count = 0;
        for (int32_t i = 0; i < commands.length; ++i) {
            Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
            if (command->id == id && command->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) count++;
        }
        return count;
    };
    auto indexOf = [](Clay_RenderCommandArray commands, uint32_t id) {
        for (int32_t i = 0; i < commands.length; ++i) {
            if (Clay_RenderCommandArray_Get(&commands, i)->id == id) return i;
        }
        return -1;
    };

    Clay_ElementId buttonId = CLAY_ID("BevelButton");
    auto button = [&]() { ClayWidgets_Button(&ui, buttonId, CLAY_STRING("OK")); };

    // Flat theme: the button's fill, and nothing else under its id.
    Frame(MakeInput(), button);
    CHECK(countRects(Frame(MakeInput(), button), buttonId.id) == 1);

    ClayWidgets_Theme saved = ui.theme;
    ui.theme = ClayWidgets_ThemeWin95();

    Frame(MakeInput(), button);
    Clay_RenderCommandArray beveled = Frame(MakeInput(), button);
    // The fill, plus four runs for each of the two bands of a raised edge.
    CHECK(countRects(beveled, buttonId.id) == 9);

    int32_t at = indexOf(beveled, buttonId.id);
    CHECK(at >= 0);
    Clay_RenderCommand *fill = Clay_RenderCommandArray_Get(&beveled, at);
    Clay_RenderCommand *top = Clay_RenderCommandArray_Get(&beveled, at + 1);
    Clay_RenderCommand *bottom = Clay_RenderCommandArray_Get(&beveled, at + 3);
    Clay_RenderCommand *innerTop = Clay_RenderCommandArray_Get(&beveled, at + 5);
    CHECK(SameColor(fill->renderData.rectangle.backgroundColor, ui.theme.surfaceAltColor));
    // Outer band catches the light on top and falls away at the bottom; the
    // inner band repeats it one pixel in.
    CHECK(SameColor(top->renderData.rectangle.backgroundColor, ui.theme.edgeLightColor));
    CHECK(SameColor(bottom->renderData.rectangle.backgroundColor, ui.theme.edgeDarkColor));
    CHECK(SameColor(innerTop->renderData.rectangle.backgroundColor, ui.theme.edgeHighlightColor));
    CHECK(top->boundingBox.height == 1.0f);
    CHECK(top->boundingBox.y == floorf(fill->boundingBox.y));
    CHECK(bottom->boundingBox.y == floorf(fill->boundingBox.y + fill->boundingBox.height) - 1.0f);
    CHECK(innerTop->boundingBox.y == top->boundingBox.y + 1.0f);

    // A drop shadow goes in front of the element it falls behind, offset down
    // and to the right of it.
    Clay_ElementId panelId = CLAY_ID("BevelPanel");
    auto panel = [&]() {
        ClayWidgets_SetShadow(&ui, panelId);
        CLAY(panelId, {
            .layout = { .sizing = { CLAY_SIZING_FIXED(40), CLAY_SIZING_FIXED(20) } },
            .backgroundColor = ui.theme.surfaceColor,
        }) {}
    };
    Frame(MakeInput(), panel);
    Clay_RenderCommandArray shadowed = Frame(MakeInput(), panel);
    CHECK(countRects(shadowed, panelId.id) == 3);
    at = indexOf(shadowed, panelId.id);
    CHECK(at >= 0);
    Clay_RenderCommand *right = Clay_RenderCommandArray_Get(&shadowed, at);
    Clay_RenderCommand *under = Clay_RenderCommandArray_Get(&shadowed, at + 1);
    Clay_RenderCommand *surface = Clay_RenderCommandArray_Get(&shadowed, at + 2);
    CHECK(SameColor(right->renderData.rectangle.backgroundColor, ui.theme.shadowColor));
    CHECK(SameColor(under->renderData.rectangle.backgroundColor, ui.theme.shadowColor));
    CHECK(SameColor(surface->renderData.rectangle.backgroundColor, ui.theme.surfaceColor));
    CHECK(right->boundingBox.x == floorf(surface->boundingBox.x + surface->boundingBox.width));
    CHECK(under->boundingBox.y == floorf(surface->boundingBox.y + surface->boundingBox.height));
    CHECK(right->boundingBox.width == (float)ui.theme.shadowOffset);

    ui.theme = saved;
    // Back on a flat theme the same tags produce nothing again.
    Frame(MakeInput(), panel);
    CHECK(countRects(Frame(MakeInput(), panel), panelId.id) == 1);
}

// Clay clips to rectangles, so a child that reaches a rounded container's
// corner paints a square block outside the arc unless it carries the matching
// radius itself. A table's header can take it at declaration; its last row only
// becomes known at EndTable, which stamps the radius onto the row's fill after
// layout.
static void TestRoundedCornersClipChildren(void) {
    Clay_ElementId tableId = CLAY_ID("CornerTable");
    Clay_ElementId firstRowId = CLAY_ID("CornerRowA");
    Clay_ElementId lastRowId = CLAY_ID("CornerRowB");
    ClayWidgets_TableColumn columns[2] = {
        { CLAY_STRING("Name"), CLAY_SIZING_GROW(0) },
        { CLAY_STRING("Value"), CLAY_SIZING_GROW(0) },
    };
    Clay_String cells[2] = { CLAY_STRING("a"), CLAY_STRING("b") };
    auto body = [&]() {
        if (ClayWidgets_BeginTable(&ui, tableId, columns, 2)) {
            // Both selected, so both actually paint a fill to inspect.
            ClayWidgets_TableRow(&ui, firstRowId, cells, 2, 0, true);
            ClayWidgets_TableRow(&ui, lastRowId, cells, 2, 1, true);
            ClayWidgets_EndTable(&ui, tableId);
        }
    };
    Frame(MakeInput(), body);
    Clay_RenderCommandArray commands = Frame(MakeInput(), body);

    float radius = (float)ui.theme.radiusMd;
    CHECK(radius > 0.0f);

    Clay_RenderCommand last = {};
    CHECK(FindCommandById(commands, lastRowId.id, &last));
    CHECK(last.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    CHECK(last.renderData.rectangle.cornerRadius.bottomLeft == radius);
    CHECK(last.renderData.rectangle.cornerRadius.bottomRight == radius);
    // Only the bottom: the header owns the top corners.
    CHECK(last.renderData.rectangle.cornerRadius.topLeft == 0.0f);
    CHECK(last.renderData.rectangle.cornerRadius.topRight == 0.0f);

    // A row in the middle of the table touches no corner and stays square.
    Clay_RenderCommand first = {};
    CHECK(FindCommandById(commands, firstRowId.id, &first));
    CHECK(first.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    CHECK(first.renderData.rectangle.cornerRadius.bottomLeft == 0.0f);
    CHECK(first.renderData.rectangle.cornerRadius.topLeft == 0.0f);

    // The override is geometry, not style: it applies on flat themes, which is
    // where rounded corners exist at all.
    CHECK(ui.theme.edgeStyle == CLAY_WIDGETS_EDGE_STYLE_FLAT);
}

// The controls that sit inline with a label - check box, radio, toggle - are
// sized from the body text through one shared derivation, so they keep their
// proportion to the label beside them at any type scale instead of staying at a
// fixed pixel size.
static void TestControlsScaleWithType(void) {
    Clay_ElementId checkId = CLAY_ID("ScaleCheck");
    Clay_ElementId boxId = ClayWidgets__ChildId(checkId, CLAY_STRING("ClayWidgetsCheckboxBox"), 0);
    bool value = false;
    auto body = [&]() { ClayWidgets_Checkbox(&ui, checkId, CLAY_STRING("Label"), &value); };

    ClayWidgets_Theme saved = ui.theme;

    Frame(MakeInput(), body);
    Clay_RenderCommandArray before = Frame(MakeInput(), body);
    Clay_RenderCommand box = {};
    CHECK(FindCommandById(before, boxId.id, &box));
    CHECK(box.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE);
    CHECK(box.boundingBox.height == ClayWidgets__ControlSize(&ui));
    CHECK(box.boundingBox.width == box.boundingBox.height);
    float small = box.boundingBox.height;

    ui.theme.fontSizeBody = (uint16_t)(ui.theme.fontSizeBody + 6);
    Frame(MakeInput(), body);
    Clay_RenderCommandArray after = Frame(MakeInput(), body);
    CHECK(FindCommandById(after, boxId.id, &box));
    CHECK(box.boundingBox.height == small + 6.0f);
    CHECK(box.boundingBox.width == box.boundingBox.height);

    ui.theme = saved;
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

#include "test-improvements.h"

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
        { "review regressions", TestReviewRegressions },
        { "editor history, validation, adapters", TestEditorFeatures },
        { "virtual collections and panels", TestCollectionsAndPanels },
        { "keyboard menus", TestKeyboardMenu },
        { "fixed and draggable modals", TestDraggableModal },
        { "wheel momentum", TestWheelMomentum },
        { "nested focus and table scrolling", TestNestedComposition },
        { "focus traversal (Tab / Shift+Tab)", TestFocusTraversal },
        { "modal focus trap", TestModalFocusTrap },
        { "button click + keyboard + disabled", TestButtonClickAndKeyboard },
        { "checkbox toggle + keyboard + disabled", TestCheckboxToggle },
        { "text input editing + clipboard", TestTextInputEditingAndClipboard },
        { "text input disabled", TestTextInputDisabled },
        { "text area editing + line navigation", TestTextAreaEditingAndNavigation },
        { "text area pointer + caret-follow scroll", TestTextAreaPointerAndScroll },
        { "text area soft word wrap", TestTextAreaSoftWrap },
        { "text area overlays clipped to panel", TestTextAreaOverlaysClippedToPanel },
        { "slider keyboard + disabled", TestSliderKeyboard },
        { "stepper disabled", TestStepperDisabled },
        { "scratch overflow reported", TestScratchOverflowReported },
        { "focusables overflow reported", TestFocusablesOverflowReported },
        { "table columns overflow reported", TestTableColumnsOverflowReported },
        { "state pool: persistence, recycling, overflow", TestStatePool },
        { "scroll bar thumb drag", TestScrollBarThumbDrag },
        { "tooltip dwell + restart", TestTooltipDwell },
        { "cursor hints", TestCursorHints },
        { "toast queue", TestToastQueue },
        { "combo keyboard, edge flip, scroll", TestComboKeyboardFlipAndScroll },
        { "scroll panel wheel moves content", TestScrollPanelWheelMovesContent },
        { "field heights match", TestFieldHeightsMatch },
        { "toggle stable track id + click", TestToggleTrackAndClick },
        { "child id derivation", TestChildIdDerivation },
        { "danger color unified", TestDangerColorUnified },
        { "beveled edges and drop shadows", TestBeveledEdgesAndShadows },
        { "rounded corners clip children", TestRoundedCornersClipChildren },
        { "inline controls scale with type", TestControlsScaleWithType },
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
