// Colour picker tests; included by the shared headless test runner.

struct PickerFixture {
    Clay_ElementId id;
    Clay_Color color = {255, 0, 0, 128};
    ClayWidgets_ColorPickerOptions options = {true, false, true};
    bool changed = false;

    explicit PickerFixture(const char *name) : id(Clay_GetElementId(
        Clay_String{false, (int32_t)std::strlen(name), name})) {}
    Clay_ElementId Child(const char *name, int index = 0) const {
        return ClayWidgets__ChildId(id, Clay_String{false, (int32_t)std::strlen(name), name}, index);
    }
    void Draw() { changed = ClayWidgets_ColorPicker(&ui, id, &color, options); }
    void Run(ClayWidgets_Input input = MakeInput()) { Frame(input, [&]() { Draw(); }); }
    void Click(Clay_ElementId target) {
        Clay_BoundingBox box = Clay_GetElementData(target).boundingBox;
        float x = box.x + box.width / 2, y = box.y + box.height / 2;
        Run(PressAt(x, y)); Run(ReleaseAt(x, y));
    }
    void Open() {
        ui.focusedId = Child("Trigger").id;
        ClayWidgets_Input input = MakeInput(); input.keyEnter = true;
        Run(input); Run();
    }
};

static void TestColorConversion(void) {
    const Clay_Color colors[] = {{255,0,0,128}, {0,255,0,255}, {0,0,255,255},
        {30,90,210,100}, {128,128,128,255}, {0,0,0,255}, {255,255,255,255}};
    for (Clay_Color color : colors) {
        ClayWidgets__ColorState state = {};
        ClayWidgets__ColorSync(&state, color);
        Clay_Color result = ClayWidgets__HslColor(state.hue, state.saturation, state.lightness, color.a);
        CHECK(fabsf(result.r-color.r) < 0.001f && fabsf(result.g-color.g) < 0.001f && fabsf(result.b-color.b) < 0.001f && result.a == color.a);
    }
}

static void TestColorPickerDrag(void) {
    PickerFixture picker("PickerDrag"); picker.Run();
    Clay_BoundingBox box = Clay_GetElementData(picker.Child("Spectrum")).boundingBox;
    picker.Run(PressAt(box.x + box.width / 3, box.y));
    CHECK(picker.changed && picker.color.g > 254 && picker.color.r < 1 && picker.color.a == 128);
    ClayWidgets_Input input = MakeInput(); input.pointerDown = true;
    input.mouseX = box.x + box.width + 30; input.mouseY = box.y - 30;
    picker.Run(input);
    CHECK(picker.changed && picker.color.r > 254 && picker.color.g < 1);
    picker.Run(ReleaseAt(input.mouseX, input.mouseY));
    picker.Run(PressAt(box.x + box.width / 3, box.y));
    picker.Run(ReleaseAt(box.x, box.y));
    ui.focusedId = picker.Child("Luminance").id;
    input = MakeInput(); input.keyHome = true; picker.Run(input);
    CHECK(picker.changed && picker.color.r == 0 && picker.color.g == 0 && picker.color.b == 0);
    input = MakeInput(); input.keyUp = true; picker.Run(input);
    CHECK(picker.changed && picker.color.g > 0 && fabsf(picker.color.r) < 0.001f);
    CHECK(g_errors.empty());
}

static void TestColorPickerNavigation(void) {
    PickerFixture picker("PickerNavigation"); picker.Run();
    ui.focusedId = picker.Child("Palette").id;
    ClayWidgets_Input input = MakeInput(); input.keyEnd = true; picker.Run(input);
    CHECK(!picker.changed); // arrows move the highlight; activation selects
    input = MakeInput(); input.keyEnter = true; picker.Run(input);
    CHECK(picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {255,255,255,128}));
    input = MakeInput(); input.keyHome = true; picker.Run(input);
    input = MakeInput(); input.keyDown = true; picker.Run(input);
    input = MakeInput(); input.keySpace = true; picker.Run(input);
    CHECK(picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {255,0,0,128}));
    input = MakeInput(); input.keyTab = true; picker.Run(input);
    CHECK(ui.focusedId == picker.Child("Spectrum").id);
    input.shiftDown = true; picker.Run(input);
    CHECK(ui.focusedId == picker.Child("Palette").id);
    picker.Click(picker.Child("Swatch", 9));
    CHECK(picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {255,255,0,128}));
    CHECK(ui.focusedId == picker.Child("Palette").id);
    ui.focusedId = picker.Child("Channel", 0).id;
    input = MakeInput(); input.keyDown = true; picker.Run(input);
    CHECK(picker.changed && picker.color.r == 254);
    ui.focusedId = picker.Child("Channel", 3).id;
    input = MakeInput(); input.keyEnd = true; picker.Run(input);
    CHECK(picker.changed && picker.color.a == 255);
    picker.options.showAlpha = false;
    ui.focusedId = picker.Child("Hex").id;
    input = MakeInput(); input.keyTab = true;
    picker.Run(); picker.Run(input);
    CHECK(ui.focusedId == picker.Child("Palette").id && picker.color.a == 255);
}

static void TestColorPickerDisabled(void) {
    PickerFixture picker("PickerDisabled"); picker.Run();
    picker.color.r = -10; picker.color.g = 300; picker.Run();
    CHECK(picker.changed && picker.color.r == 0 && picker.color.g == 255);
    Clay_BoundingBox box = Clay_GetElementData(picker.Child("Spectrum")).boundingBox;
    picker.Run(PressAt(box.x + box.width / 2, box.y + 5));
    picker.options.disabled = true;
    Clay_Color before = picker.color;
    ClayWidgets_Input input = MakeInput(); input.pointerDown = true;
    input.mouseX = box.x; input.mouseY = box.y;
    picker.Run(input);
    CHECK(!picker.changed && SameColor(picker.color, before) && ui.activeId == 0 && ui.focusCount == 0);
    picker.options.disabled = false;
    Frame(MakeInput(), [&]() { ClayWidgets_BeginDisabled(&ui); picker.Draw(); ClayWidgets_EndDisabled(&ui); });
    CHECK(!picker.changed && SameColor(picker.color, before) && ui.focusCount == 0);
    CHECK(!ClayWidgets_ColorPicker(nullptr, picker.id, &picker.color, picker.options));
    CHECK(!ClayWidgets_ColorPicker(&ui, picker.id, nullptr, picker.options));
}

static void TestColorPickerDialog(void) {
    PickerFixture picker("PickerDialog"); picker.options.inlinePanel = false;
    picker.color = CLAY__INIT(Clay_Color) {80,140,220,128};
    Clay_Color original = picker.color;
    picker.Run(); picker.Open();
    Clay_ElementId panel = picker.Child("Panel");
    picker.Click(ClayWidgets__ChildId(panel, CLAY_STRING("Swatch"), 8));
    CHECK(!picker.changed && SameColor(picker.color, original));
    picker.Click(picker.Child("Cancel"));
    CHECK(!picker.changed && SameColor(picker.color, original));
    picker.Run(); picker.Open();
    picker.Click(ClayWidgets__ChildId(panel, CLAY_STRING("Swatch"), 8));
    picker.Click(picker.Child("OK"));
    CHECK(picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {255,0,0,128}));
    picker.Run(); CHECK(!picker.changed);
    picker.Open(); picker.Click(ClayWidgets__ChildId(panel, CLAY_STRING("Swatch"), 9));
    ClayWidgets_Input input = MakeInput(); input.keyEscape = true; picker.Run(input);
    CHECK(!picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {255,0,0,128}));
    CHECK(g_errors.empty());
}


static void TestColorPickerEntry(void) {
    PickerFixture picker("PickerEntry"); picker.Run();
    auto type = [&](Clay_ElementId id, const char *text) {
        ui.focusedId = id.id;
        ClayWidgets_Input input = MakeInput(); input.keySelectAll = true; picker.Run(input);
        input = MakeInput(); input.textUtf8 = text; input.textUtf8Length = (int32_t)strlen(text); picker.Run(input);
    };
    type(picker.Child("Hex"), "#12345678");
    CHECK(picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {18,52,86,120}));
    type(picker.Child("Hex"), "#xyz");
    CHECK(!picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {18,52,86,120}));
    type(picker.Child("Hex"), "#ab"); CHECK(!picker.changed);
    type(picker.Child("Channel", 0), "200"); CHECK(picker.changed && picker.color.r == 200);
    picker.options.showAlpha = false; picker.Run();
    type(picker.Child("Hex"), "00FF00");
    CHECK(picker.changed && SameColor(picker.color, CLAY__INIT(Clay_Color) {0,255,0,120}));
    type(picker.Child("HSL", 0), "240");
    CHECK(picker.changed && picker.color.b > 254 && picker.color.g < 1 && picker.color.a == 120);
    type(picker.Child("HSL", 2), "0");
    CHECK(picker.changed && picker.color.b == 0);
    type(picker.Child("HSL", 2), "50");
    CHECK(picker.changed && picker.color.b > 254 && picker.color.g < 1);
    CHECK(g_errors.empty());
}
