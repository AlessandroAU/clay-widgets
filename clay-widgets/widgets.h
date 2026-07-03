#ifndef CLAY_WIDGETS_H
#define CLAY_WIDGETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clay.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLAY_WIDGETS_TEXT_MAX_BYTES
#define CLAY_WIDGETS_TEXT_MAX_BYTES 2048
#endif

#ifndef CLAY_WIDGETS_MAX_FOCUSABLES
#define CLAY_WIDGETS_MAX_FOCUSABLES 128
#endif

typedef struct ClayWidgets_Input {
    float mouseX;
    float mouseY;
    bool pointerDown;
    bool pointerPressed;
    bool pointerReleased;
    float scrollX;
    float scrollY;
    float deltaTime;

    const char *textUtf8;
    int32_t textUtf8Length;

    bool keyBackspace;
    bool keyDelete;
    bool keyHome;
    bool keyEnd;
    bool keyLeft;
    bool keyRight;
    bool keyUp;
    bool keyDown;
    bool keyEnter;
    bool keyEscape;
    bool keyTab;
    bool keySelectAll;
    bool shiftDown;
} ClayWidgets_Input;

typedef Clay_Dimensions (*ClayWidgets_MeasureTextFunction)(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

typedef struct ClayWidgets_Spacing {
    uint16_t xs;
    uint16_t sm;
    uint16_t md;
    uint16_t lg;
} ClayWidgets_Spacing;

typedef struct ClayWidgets_Theme {
    Clay_Color textColor;
    Clay_Color textMutedColor;
    Clay_Color surfaceColor;
    Clay_Color surfaceAltColor;
    Clay_Color accentColor;
    Clay_Color accentMutedColor;
    Clay_Color borderColor;
    Clay_Color hoverColor;
    Clay_Color pressedColor;
    Clay_Color focusRingColor;

    uint16_t radiusSm;
    uint16_t radiusMd;

    uint16_t fontBody;
    uint16_t fontHeading;
    uint16_t fontMono;

    uint16_t fontSizeBody;
    uint16_t fontSizeHeading;
    uint16_t fontSizeSmall;

    ClayWidgets_Spacing spacing;
} ClayWidgets_Theme;

typedef struct ClayWidgets_Context {
    ClayWidgets_Input input;
    ClayWidgets_Theme theme;
    ClayWidgets_MeasureTextFunction measureText;
    void *measureTextUserData;

    uint32_t activeId;
    uint32_t focusedId;
    uint32_t focusOrder[CLAY_WIDGETS_MAX_FOCUSABLES];
    int32_t focusCount;
    uint32_t textInputId;
    uint32_t lastHoveredTextInputId;
    int32_t textCursor;
    int32_t textSelectionAnchor;
    float textScrollX;
    bool textPointerSelecting;
    float caretBlinkTime;
    float elapsedTime;
    uint32_t lastClickId;
    float lastClickTime;
    float lastClickX;
    float lastClickY;
    bool clickConsumed;

    uint32_t scrollBarDragContainerId;
    float scrollBarDragStartMouseY;
    float scrollBarDragStartScrollY;

    uint32_t openComboId;
    int32_t comboHighlightIndex;
} ClayWidgets_Context;

typedef struct ClayWidgets_SliderOptions {
    float minValue;
    float maxValue;
    float step;
} ClayWidgets_SliderOptions;

typedef struct ClayWidgets_TextInputOptions {
    const char *placeholder;
    bool clearOnEnter;
} ClayWidgets_TextInputOptions;

#include "core.h"
#include "themes.h"
#include "text.h"
#include "button.h"
#include "checkbox.h"
#include "radio.h"
#include "slider.h"
#include "progress-bar.h"
#include "text-input.h"
#include "combo.h"
#include "scroll-bar.h"

#ifdef __cplusplus
}
#endif

#endif