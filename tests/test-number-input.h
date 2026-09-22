// Numeric editing and shared click behaviour, using the headless harness.
static void TestNumberInput(void) {
    auto id = CLAY_ID("EditableNumber");
    int32_t value = 12;
    bool changed = false;
    ClayWidgets_NumberInputOptions options = {-100, 255, 1, false};
    auto draw = [&] { changed = ClayWidgets_NumberInput(&ui, id, CLAY_STRING("Value"), &value, options); };
    Frame(MakeInput(), draw);
    auto type = [&](const char *text) {
        ui.focusedId = id.id;
        ClayWidgets_Input input = MakeInput(); input.keySelectAll = true;
        Frame(input, draw);
        input = MakeInput(); input.textUtf8 = text; input.textUtf8Length = (int32_t)strlen(text);
        Frame(input, draw);
    };
    type("42"); CHECK(changed && value == 42);
    type("999"); CHECK(!changed && value == 42);
    ClayWidgets_Input input = MakeInput(); input.keyEnter = true;
    Frame(input, draw); CHECK(changed && value == 255);
    Frame(MakeInput(), draw);
    type("-17"); CHECK(changed && value == -17);
    type("abc"); CHECK(!changed && value == -17);
    type("-"); CHECK(!changed && value == -17);
    input = MakeInput(); input.keyEscape = true; Frame(input, draw);
    Frame(MakeInput(), draw); CHECK(value == -17);
    type("-9999999999999999999999999"); CHECK(value == -17);
    Frame(PressAt(-20, -20), draw); CHECK(changed && value == -100);
    Frame(MakeInput(), draw);
    ui.focusedId = id.id;
    input = MakeInput(); input.keyUp = true; Frame(input, draw); CHECK(changed && value == -99);
    value = 1000; options.disabled = true;
    Frame(input, draw); CHECK(!changed && value == 1000 && ui.focusedId == 0);
    options.disabled = false; options.minValue = INT32_MIN; options.maxValue = INT32_MAX;
    value = INT32_MAX; ui.focusedId = id.id;
    Frame(input, draw); CHECK(!changed && value == INT32_MAX);
    type("-2147483648"); CHECK(changed && value == INT32_MIN);
    type("999999999999999999999"); CHECK(value == INT32_MIN);
    Frame(MakeInput(), [] {}); // removing an editor discards its pending draft
    ui.focusedId = 0; Frame(MakeInput(), draw);
    CHECK(!changed && value == INT32_MIN);
    CHECK(g_errors.empty());
}

static void TestControlClickCapture(void) {
    for (int kind = 0; kind < 3; ++kind) {
        ResetUi();
        auto id = CLAY_IDI("ControlCapture", kind);
        auto target = kind == 2 ? Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsStepperPlus"), id.id) : id;
        bool value = false, disabled = false;
        int32_t number = 0;
        auto draw = [&] {
            if (kind == 0) ClayWidgets_CheckboxEx(&ui, id, CLAY_STRING("Check"), &value, disabled);
            if (kind == 1) ClayWidgets_ToggleEx(&ui, id, CLAY_STRING("Toggle"), &value, disabled);
            if (kind == 2) ClayWidgets_Stepper(&ui, id, &number, {0, 10, 1, disabled});
        };
        Frame(MakeInput(), draw);
        auto box = Clay_GetElementData(target).boundingBox;
        float x = box.x + box.width / 2, y = box.y + box.height / 2;
        Frame(PressAt(-10, -10), draw); Frame(ReleaseAt(x, y), draw);
        CHECK(!value && number == 0);
        Frame(PressAt(x, y), draw); Frame(ReleaseAt(-10, -10), draw);
        CHECK(!value && number == 0);
        Frame(PressAt(x, y), draw); Frame(ReleaseAt(x, y), draw);
        CHECK(kind == 2 ? number == 1 : value);
        Frame(PressAt(x, y), draw); disabled = true;
        Frame(ReleaseAt(x, y), draw);
        CHECK((kind == 2 ? number == 1 : value) && ui.activeId == 0 && ui.focusedId == 0);
    }
}

static void TestModalSizingAndDismissal(void) {
    auto id = CLAY_ID("BoundedModal"); bool open = true;
    ClayWidgets_ModalOptions options = {false, 400, 220, true, true};
    auto draw = [&] {
        if (ClayWidgets_BeginModalEx(&ui, id, CLAY_STRING("Tall content"), &open, options)) {
            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(100), .height = CLAY_SIZING_FIXED(800) } } }) {}
            ClayWidgets_EndModal(&ui, id);
        }
    };
    Frame(MakeInput(), draw); Frame(MakeInput(), draw);
    auto dialog = ClayWidgets__ModalDialogId(id);
    auto box = Clay_GetElementData(dialog).boundingBox;
    CHECK(box.height <= 220 && box.y >= 0 && box.y + box.height <= kLayoutHeight);
    auto body = ClayWidgets__ChildId(dialog, CLAY_STRING("ClayWidgetsModalBody"), 0);
    auto scroll = Clay_GetScrollContainerData(body);
    CHECK(scroll.found && scroll.contentDimensions.height > scroll.scrollContainerDimensions.height);
    CHECK(ui.scrollPanelDepth == 0);
    ClayWidgets_Input input = MakeInput(); input.keyEscape = true;
    Frame(input, draw); CHECK(open);
    Frame(PressAt(2, 2), draw); CHECK(open);
    options.noOutsideClose = false;
    Frame(PressAt(2, 2), draw); CHECK(!open);
    open = true; Frame(MakeInput(), draw);
    options.noEscapeClose = false;
    Frame(input, draw); CHECK(!open);
}

static void TestSliderLabelClipping(void) {
    auto panel = CLAY_ID("SliderClipPanel");
    float value = 37;
    auto draw = [&] {
        ClayWidgets_BeginScrollPanel(&ui, panel, {CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(100), 1, 1, 1});
        CLAY_AUTO_ID({ .layout = { .sizing = { .height = CLAY_SIZING_FIXED(120) } } }) {}
        ClayWidgets_Slider(&ui, CLAY_ID("ClippedSlider"), &value, {0, 100, 1, true});
        ClayWidgets_EndScrollPanel(&ui, panel);
    };
    Frame(MakeInput(), draw);
    auto commands = Frame(MakeInput(), draw);
    auto clip = Clay_GetElementData(ClayWidgets__ScrollPanelContentId(panel)).boundingBox;
    std::vector<Clay_BoundingBox> scissors;
    for (int i = 0; i < commands.length; ++i) {
        auto *cmd = Clay_RenderCommandArray_Get(&commands, i);
        if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
            scissors.push_back(scissors.empty() ? cmd->boundingBox : IntersectBox(scissors.back(), cmd->boundingBox));
        } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
            if (!scissors.empty()) scissors.pop_back();
        } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
            CHECK(!scissors.empty());
            if (!scissors.empty()) {
                auto visible = IntersectBox(scissors.back(), cmd->boundingBox);
                CHECK(visible.height <= 0 || visible.y + visible.height <= clip.y + clip.height);
            }
        }
    }
}
