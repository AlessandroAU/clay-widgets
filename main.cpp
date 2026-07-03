#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CLAY_IMPLEMENTATION
#include "clay.h"

#define CLAY_WIDGETS_IMPLEMENTATION
#include "clay-widgets.h"

#include "raylib.h"

namespace {

static Clay_String ClayStringFromCString(const char *text) {
    Clay_String s = {};
    if (!text) {
        s.chars = "";
        s.length = 0;
        s.isStaticallyAllocated = true;
        return s;
    }
    s.chars = text;
    s.length = static_cast<int32_t>(std::strlen(text));
    s.isStaticallyAllocated = false;
    return s;
}

static Color ToRaylibColor(Clay_Color c) {
    auto clampByte = [](float v) -> unsigned char {
        if (v < 0.0f) return 0;
        if (v > 255.0f) return 255;
        return static_cast<unsigned char>(std::round(v));
    };
    return Color{clampByte(c.r), clampByte(c.g), clampByte(c.b), clampByte(c.a)};
}

static Color MixOverlay(Color base, Color overlay) {
    float a = static_cast<float>(overlay.a) / 255.0f;
    Color out = {
        static_cast<unsigned char>(base.r + (overlay.r - base.r) * a),
        static_cast<unsigned char>(base.g + (overlay.g - base.g) * a),
        static_cast<unsigned char>(base.b + (overlay.b - base.b) * a),
        base.a,
    };
    return out;
}

static void AppendUtf8FromCodepoint(char *buffer, int &length, int maxLength, int codepoint) {
    if (maxLength <= 0 || length >= maxLength) {
        return;
    }

    auto pushByte = [&](unsigned char b) {
        if (length < maxLength) {
            buffer[length++] = static_cast<char>(b);
        }
    };

    if (codepoint <= 0x7F) {
        pushByte(static_cast<unsigned char>(codepoint));
        return;
    }
    if (codepoint <= 0x7FF) {
        pushByte(static_cast<unsigned char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if (codepoint <= 0xFFFF) {
        pushByte(static_cast<unsigned char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F)));
        pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    pushByte(static_cast<unsigned char>(0xF0 | ((codepoint >> 18) & 0x07)));
    pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 12) & 0x3F)));
    pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F)));
    pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
}

static Clay_Dimensions MeasureTextRaylib(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Font *fonts = static_cast<Font *>(userData);
    Font font = fonts ? fonts[config->fontId] : GetFontDefault();

    std::string tmp;
    tmp.assign(text.chars, text.length);

    float fontSize = static_cast<float>(config->fontSize);
    float spacing = static_cast<float>(config->letterSpacing);
    Vector2 measured = MeasureTextEx(font, tmp.c_str(), fontSize, spacing);

    Clay_Dimensions out = {};
    out.width = measured.x;
    out.height = measured.y;
    return out;
}

static void HandleClayError(Clay_ErrorData errorData) {
    std::string msg(errorData.errorText.chars, errorData.errorText.length);
    TraceLog(LOG_ERROR, "Clay error (%d): %s", static_cast<int>(errorData.errorType), msg.c_str());
}

static Rectangle IntersectRects(Rectangle a, Rectangle b) {
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.width, b.x + b.width);
    float y2 = std::min(a.y + a.height, b.y + b.height);
    if (x2 < x1 || y2 < y1) {
        return Rectangle{x1, y1, 0, 0};
    }
    return Rectangle{x1, y1, x2 - x1, y2 - y1};
}

static void RenderClayCommands(Clay_RenderCommandArray commands, Font *fonts) {
    std::vector<Rectangle> scissorStack;
    std::vector<Color> overlayStack;
    bool scissorEnabled = false;
    const int roundedCornerSegments = 24;

    auto reapplyScissor = [&]() {
        if (scissorEnabled) {
            EndScissorMode();
            scissorEnabled = false;
        }
        if (!scissorStack.empty()) {
            Rectangle r = scissorStack.back();
            BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height));
            scissorEnabled = true;
        }
    };

    auto applyOverlay = [&](Color c) -> Color {
        Color out = c;
        for (Color overlay : overlayStack) {
            out = MixOverlay(out, overlay);
        }
        return out;
    };

    for (int i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        Rectangle rect = {
            cmd->boundingBox.x,
            cmd->boundingBox.y,
            cmd->boundingBox.width,
            cmd->boundingBox.height,
        };

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Color color = applyOverlay(ToRaylibColor(cmd->renderData.rectangle.backgroundColor));
                DrawRectangleRounded(rect, cmd->renderData.rectangle.cornerRadius.topLeft / std::max(1.0f, std::min(rect.width, rect.height)), roundedCornerSegments, color);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                const Clay_TextRenderData &text = cmd->renderData.text;
                Font font = fonts[text.fontId];
                std::string tmp(text.stringContents.chars, text.stringContents.length);
                Color color = applyOverlay(ToRaylibColor(text.textColor));
                DrawTextEx(font, tmp.c_str(), Vector2{rect.x, rect.y}, static_cast<float>(text.fontSize), static_cast<float>(text.letterSpacing), color);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                const Clay_BorderRenderData &b = cmd->renderData.border;
                Color c = applyOverlay(ToRaylibColor(b.color));
                if (b.width.top > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), static_cast<int>(rect.width), b.width.top, c);
                if (b.width.bottom > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y + rect.height - b.width.bottom), static_cast<int>(rect.width), b.width.bottom, c);
                if (b.width.left > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), b.width.left, static_cast<int>(rect.height), c);
                if (b.width.right > 0) DrawRectangle(static_cast<int>(rect.x + rect.width - b.width.right), static_cast<int>(rect.y), b.width.right, static_cast<int>(rect.height), c);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                if (scissorStack.empty()) {
                    scissorStack.push_back(rect);
                } else {
                    scissorStack.push_back(IntersectRects(scissorStack.back(), rect));
                }
                reapplyScissor();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                if (!scissorStack.empty()) {
                    scissorStack.pop_back();
                }
                reapplyScissor();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START: {
                overlayStack.push_back(ToRaylibColor(cmd->renderData.overlayColor.color));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END: {
                if (!overlayStack.empty()) {
                    overlayStack.pop_back();
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            case CLAY_RENDER_COMMAND_TYPE_NONE:
            default:
                break;
        }
    }

    if (scissorEnabled) {
        EndScissorMode();
    }
}

} // namespace

int main() {
    const int screenWidth = 1080;
    const int screenHeight = 720;
    const char *fontPath = "assets/fonts/Roboto-Regular.ttf";

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "clay-widgets demo");
    SetTargetFPS(60);

    Font fonts[8] = {};
    fonts[0] = GetFontDefault();
    bool customFontLoaded = false;
    if (FileExists(fontPath)) {
        Font roboto = LoadFontEx(fontPath, 64, nullptr, 0);
        if (roboto.texture.id != 0) {
            fonts[0] = roboto;
            customFontLoaded = true;
        } else {
            TraceLog(LOG_WARNING, "Failed to load font '%s'. Using default font.", fontPath);
        }
    } else {
        TraceLog(LOG_WARNING, "Font file not found: %s. Using default font.", fontPath);
    }

    if (fonts[0].texture.id != 0) {
        SetTextureFilter(fonts[0].texture, TEXTURE_FILTER_BILINEAR);
    }

    uint32_t clayMemorySize = Clay_MinMemorySize();
    void *clayMemory = std::malloc(clayMemorySize);
    if (!clayMemory) {
        TraceLog(LOG_ERROR, "Failed to allocate Clay arena");
        CloseWindow();
        return 1;
    }

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, clayMemory);
    Clay_Initialize(arena, Clay_Dimensions{static_cast<float>(screenWidth), static_cast<float>(screenHeight)}, Clay_ErrorHandler{HandleClayError, 0});
    Clay_SetMeasureTextFunction(MeasureTextRaylib, fonts);

    ClayWidgets_Context ui = {};
    ClayWidgets_Theme theme = ClayWidgets_DefaultTheme();
    ClayWidgets_Init(&ui, theme);
    ClayWidgets_SetMeasureTextFunction(&ui, MeasureTextRaylib, fonts);

    bool featureA = true;
    bool featureB = false;
    bool notificationsEnabled = true;
    int32_t selectedProfile = 2;
    int32_t selectedTheme = CLAY_WIDGETS_THEME_PRESET_SLATE;
    float masterVolume = 0.35f;
    float uiScale = 1.0f;
    char nameBuffer[128] = "Clay User";
    char projectBuffer[128] = "clay-widgets";
    int clickCount = 0;
    int applyCount = 0;
    char statusLine[128] = "Ready";

    static const Clay_String comboItems[] = {
        CLAY_STRING("Debug"),
        CLAY_STRING("Release"),
        CLAY_STRING("Release with Debug Info"),
        CLAY_STRING("Minimum Size"),
    };
    static const int32_t comboItemCount = (int32_t)(sizeof(comboItems) / sizeof(comboItems[0]));
    int32_t selectedBuildConfig = 1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        char frameUtf8[64] = {};
        int frameUtf8Len = 0;
        for (;;) {
            int codepoint = GetCharPressed();
            if (codepoint == 0) {
                break;
            }
            AppendUtf8FromCodepoint(frameUtf8, frameUtf8Len, static_cast<int>(sizeof(frameUtf8) - 1), codepoint);
        }
        frameUtf8[frameUtf8Len] = '\0';

        ClayWidgets_Input input = {};
        input.mouseX = static_cast<float>(GetMouseX());
        input.mouseY = static_cast<float>(GetMouseY());
        input.pointerDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        input.pointerPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        input.pointerReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        Vector2 wheel = GetMouseWheelMoveV();
        input.scrollX = wheel.x;
        input.scrollY = wheel.y;
        input.deltaTime = dt;
        input.textUtf8 = frameUtf8;
        input.textUtf8Length = frameUtf8Len;
        input.keyBackspace = IsKeyPressed(KEY_BACKSPACE);
        input.keyDelete = IsKeyPressed(KEY_DELETE);
        input.keyHome = IsKeyPressed(KEY_HOME);
        input.keyEnd = IsKeyPressed(KEY_END);
        input.keyLeft = IsKeyPressed(KEY_LEFT);
        input.keyRight = IsKeyPressed(KEY_RIGHT);
        input.keyUp = IsKeyPressed(KEY_UP);
        input.keyDown = IsKeyPressed(KEY_DOWN);
        input.keyEnter = IsKeyPressed(KEY_ENTER);
        input.keyEscape = IsKeyPressed(KEY_ESCAPE);
        input.keyTab = IsKeyPressed(KEY_TAB);
        input.keySelectAll = (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A);
        input.shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        ui.theme = ClayWidgets_ThemeFromPreset((ClayWidgets_ThemePreset)selectedTheme);

        ClayWidgets_BeginFrame(
            &ui,
            input,
            Clay_Dimensions{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
            true
        );

        bool compactLayout = GetScreenWidth() < 1120;
        Clay_SizingAxis leftPanelWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.58f);
        Clay_SizingAxis rightPanelWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.42f);
        Clay_SizingAxis leftPanelHeight = compactLayout ? CLAY_SIZING_PERCENT(0.64f) : CLAY_SIZING_GROW(0);
        Clay_SizingAxis rightPanelHeight = compactLayout ? CLAY_SIZING_PERCENT(0.36f) : CLAY_SIZING_GROW(0);

        CLAY(CLAY_ID("Root"), {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                .padding = CLAY_PADDING_ALL(16),
                .childGap = 14,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = ui.theme.surfaceColor,
        }) {
            ClayWidgets_Heading(&ui, CLAY_STRING("clay-widgets demo"));
            ClayWidgets_Label(&ui, CLAY_STRING("A starter UI kit on top of Clay + raylib"));
            ClayWidgets_Separator(&ui);

            CLAY(CLAY_ID("MainColumns"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .childGap = 16,
                    .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                },
            }) {
                CLAY(CLAY_ID("LeftPanel"), {
                    .layout = {
                        .sizing = { .width = leftPanelWidth, .height = leftPanelHeight },
                        .padding = CLAY_PADDING_ALL(12),
                        .childGap = 12,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .backgroundColor = ui.theme.surfaceAltColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(10),
                    .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
                }) {
                    ClayWidgets_Label(&ui, CLAY_STRING("Keyboard: Tab moves focus, Enter activates the focused control."));

                    CLAY(CLAY_ID("ActionRow"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                            .childGap = 10,
                            .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                        },
                    }) {
                        if (ClayWidgets_Button(&ui, CLAY_ID("PrimaryButton"), CLAY_STRING("Apply Changes"))) {
                            clickCount += 1;
                            applyCount += 1;
                            std::snprintf(statusLine, sizeof(statusLine), "Applied preset %d", selectedProfile);
                        }

                        if (ClayWidgets_Button(&ui, CLAY_ID("ResetButton"), CLAY_STRING("Reset Form"))) {
                            featureA = true;
                            featureB = false;
                            notificationsEnabled = true;
                            selectedProfile = 2;
                            selectedTheme = CLAY_WIDGETS_THEME_PRESET_SLATE;
                            masterVolume = 0.35f;
                            uiScale = 1.0f;
                            selectedBuildConfig = 1;
                            std::strncpy(nameBuffer, "Clay User", sizeof(nameBuffer));
                            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
                            std::strncpy(projectBuffer, "clay-widgets", sizeof(projectBuffer));
                            projectBuffer[sizeof(projectBuffer) - 1] = '\0';
                            std::strncpy(statusLine, "Form reset", sizeof(statusLine));
                            statusLine[sizeof(statusLine) - 1] = '\0';
                        }
                    }

                    ClayWidgets_Separator(&ui);
                    ClayWidgets_Label(&ui, CLAY_STRING("Project Settings"));

                    ClayWidgets_TextInput(
                        &ui,
                        CLAY_ID("NameInput"),
                        CLAY_STRING("Display name"),
                        nameBuffer,
                        static_cast<int32_t>(sizeof(nameBuffer)),
                        ClayWidgets_TextInputOptions{"Type your name", false}
                    );

                    ClayWidgets_TextInput(
                        &ui,
                        CLAY_ID("ProjectInput"),
                        CLAY_STRING("Project slug"),
                        projectBuffer,
                        static_cast<int32_t>(sizeof(projectBuffer)),
                        ClayWidgets_TextInputOptions{"workspace identifier", false}
                    );

                    ClayWidgets_Combo(
                        &ui,
                        CLAY_ID("BuildConfigCombo"),
                        CLAY_STRING("Build configuration"),
                        comboItems,
                        comboItemCount,
                        &selectedBuildConfig
                    );

                    if (ClayWidgets_Button(&ui, CLAY_ID("PingButton"), CLAY_STRING("Increment Counter"))) {
                        clickCount += 1;
                        std::strncpy(statusLine, "Counter incremented", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }

                    ClayWidgets_Checkbox(&ui, CLAY_ID("CheckboxA"), CLAY_STRING("Enable particles"), &featureA);
                    ClayWidgets_Checkbox(&ui, CLAY_ID("CheckboxB"), CLAY_STRING("Enable shadows"), &featureB);
                    ClayWidgets_Checkbox(&ui, CLAY_ID("CheckboxNotifications"), CLAY_STRING("Enable notifications"), &notificationsEnabled);

                    ClayWidgets_Label(&ui, CLAY_STRING("Profile"));
                    ClayWidgets_Radio(&ui, CLAY_ID("Profile1"), CLAY_STRING("Minimal"), 1, &selectedProfile);
                    ClayWidgets_Radio(&ui, CLAY_ID("Profile2"), CLAY_STRING("Balanced"), 2, &selectedProfile);
                    ClayWidgets_Radio(&ui, CLAY_ID("Profile3"), CLAY_STRING("Quality"), 3, &selectedProfile);

                    ClayWidgets_Label(&ui, CLAY_STRING("Theme"));
                    ClayWidgets_Radio(&ui, CLAY_ID("Theme1"), CLAY_STRING("Slate"), CLAY_WIDGETS_THEME_PRESET_SLATE, &selectedTheme);
                    ClayWidgets_Radio(&ui, CLAY_ID("Theme2"), CLAY_STRING("Sand"), CLAY_WIDGETS_THEME_PRESET_SAND, &selectedTheme);
                    ClayWidgets_Radio(&ui, CLAY_ID("Theme3"), CLAY_STRING("Forest"), CLAY_WIDGETS_THEME_PRESET_FOREST, &selectedTheme);

                    ClayWidgets_Label(&ui, CLAY_STRING("Master Volume"));
                    masterVolume = ClayWidgets_Slider(
                        &ui,
                        CLAY_ID("MasterVolume"),
                        masterVolume,
                        ClayWidgets_SliderOptions{0.0f, 1.0f, 0.01f}
                    );

                    ClayWidgets_ProgressBar(
                        &ui,
                        CLAY_ID("Progress"),
                        masterVolume,
                        CLAY_STRING("Volume level")
                    );

                    ClayWidgets_Label(&ui, CLAY_STRING("UI Scale"));
                    uiScale = ClayWidgets_Slider(
                        &ui,
                        CLAY_ID("UiScale"),
                        uiScale,
                        ClayWidgets_SliderOptions{0.75f, 1.5f, 0.05f}
                    );

                    ClayWidgets_ProgressBar(
                        &ui,
                        CLAY_ID("ApplyProgress"),
                        uiScale / 1.5f,
                        CLAY_STRING("Scale calibration")
                    );

                    ClayWidgets_ScrollBar(&ui, CLAY_ID("LeftPanel"));
                }

                CLAY(CLAY_ID("RightPanel"), {
                    .layout = {
                        .sizing = { .width = rightPanelWidth, .height = rightPanelHeight },
                        .padding = CLAY_PADDING_ALL(12),
                        .childGap = 8,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .backgroundColor = ui.theme.surfaceAltColor,
                    .cornerRadius = CLAY_CORNER_RADIUS(10),
                }) {
                    ClayWidgets_Label(&ui, CLAY_STRING("Live State"));

                    CLAY(CLAY_ID("RightPanelScrollView"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                            .childGap = 8,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
                    }) {
                        char stateLines[14][256] = {};
                        int stateLineCount = 0;
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Button clicks: %d", clickCount);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Apply count: %d", applyCount);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Feature A: %s", featureA ? "ON" : "OFF");
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Feature B: %s", featureB ? "ON" : "OFF");
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Notifications: %s", notificationsEnabled ? "ON" : "OFF");
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Profile: %d", selectedProfile);

                        const char *themeName = "Slate";
                        if (selectedTheme == CLAY_WIDGETS_THEME_PRESET_SAND) {
                            themeName = "Sand";
                        } else if (selectedTheme == CLAY_WIDGETS_THEME_PRESET_FOREST) {
                            themeName = "Forest";
                        }
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Theme: %s", themeName);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Volume: %.2f", masterVolume);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "UI Scale: %.2f", uiScale);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Name: %s", nameBuffer);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Project: %s", projectBuffer);

                        const char *buildConfigName = (selectedBuildConfig >= 0 && selectedBuildConfig < comboItemCount)
                            ? comboItems[selectedBuildConfig].chars : "(none)";
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Build config: %s", buildConfigName);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Focused widget id: %u", ui.focusedId);
                        std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Status: %s", statusLine);

                        for (int i = 0; i < stateLineCount; ++i) {
                            ClayWidgets_Label(&ui, ClayStringFromCString(stateLines[i]));
                        }

                        ClayWidgets_ScrollBar(&ui, CLAY_ID("RightPanelScrollView"));
                    }
                }
            }
        }

        Clay_RenderCommandArray commands = ClayWidgets_EndFrame(dt);

        BeginDrawing();
        ClearBackground(Color{18, 20, 25, 255});
        RenderClayCommands(commands, fonts);
        EndDrawing();
    }

    if (customFontLoaded) {
        UnloadFont(fonts[0]);
    }
    std::free(clayMemory);
    CloseWindow();
    return 0;
}
