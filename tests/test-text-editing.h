// Text input and text area tests; use the shared headless harness.

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
