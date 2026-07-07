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

#ifndef CLAY_WIDGETS_SCROLLBAR_WIDTH
#define CLAY_WIDGETS_SCROLLBAR_WIDTH 8
#endif

#ifndef CLAY_WIDGETS_MAX_ANIMS
#define CLAY_WIDGETS_MAX_ANIMS 64
#endif

#ifndef CLAY_WIDGETS_MAX_SCROLL_NESTING
#define CLAY_WIDGETS_MAX_SCROLL_NESTING 8
#endif

typedef struct ClayWidgets_Input {
    float mouseX;
    float mouseY;
    bool pointerDown;
    bool pointerPressed;
    bool pointerReleased;
    bool pointerRightPressed;
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

// One eased scalar for a widget effect that a Clay property transition can't
// express (e.g. a toggle knob's position). Keyed by element id; reclaimed when
// its id stops being requested. See ClayWidgets__AnimTo.
typedef struct ClayWidgets_AnimSlot {
    uint32_t id;
    uint32_t frame; // last frame this slot was requested; stale slots get reused
    float value;    // current eased value
} ClayWidgets_AnimSlot;

typedef struct ClayWidgets_Context {
    ClayWidgets_Input input;
    ClayWidgets_Theme theme;
    ClayWidgets_MeasureTextFunction measureText;
    void *measureTextUserData;
    Clay_Dimensions layoutDimensions;

    // When false, widgets emit no Clay transitions (colors/enter states snap
    // instantly) and ClayWidgets__AnimTo returns its target directly. Defaults to
    // true; turn off for deterministic screenshots or to honor a reduce-motion
    // preference. See ClayWidgets__ColorTransition and ClayWidgets__AnimTo.
    bool animationsEnabled;

    // Per-widget eased scalars (Route B animations) plus the frame counter used
    // to age out slots whose widget has disappeared.
    ClayWidgets_AnimSlot anims[CLAY_WIDGETS_MAX_ANIMS];
    uint32_t animFrame;

    // A hovered widget clip that Clay treats as a scroll container but that can't
    // consume a vertical wheel (a horizontally-clipped table, a single-line text
    // field). Recorded during layout; next BeginFrame forwards the swallowed
    // wheel straight to the enclosing scroll panel (wheelFallthroughPanelId) so
    // the panel scrolls even when the clip covers all of it. Box is last frame's
    // geometry, used to confirm the pointer is still over the clip.
    uint32_t wheelFallthroughId;
    uint32_t wheelFallthroughPanelId; // content id of the scroll panel to forward to (0 = none)
    Clay_BoundingBox wheelFallthroughBox;

    // Stack of open scroll-panel content ids, so a clip widget can find the
    // scroll panel it lives in. Pushed by BeginScrollPanel, popped by
    // EndScrollPanel, reset each frame.
    uint32_t scrollPanelStack[CLAY_WIDGETS_MAX_SCROLL_NESTING];
    int32_t scrollPanelDepth;

    uint32_t activeId;
    uint32_t focusedId;
    uint32_t focusOrder[CLAY_WIDGETS_MAX_FOCUSABLES];
    int32_t focusCount;
    uint32_t textInputId;
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

    uint32_t hoverTooltipId;
    float hoverTooltipTime;

    uint32_t openMenuId;

    uint32_t openContextMenuId;
    float contextMenuX;
    float contextMenuY;

    // Ring of small buffers for dynamic strings (e.g. a stepper's number). Clay
    // retains text by pointer until render, so these must outlive the layout;
    // reused across frames, reset at the start of each frame.
    char textScratch[8][24];
    uint32_t textScratchNext;

    // Active transient toast: message plus remaining seconds. Counted down by
    // ClayWidgets_ToastLayer each frame.
    char toastMessage[160];
    int32_t toastLength;
    float toastRemaining;
    int32_t toastVariant;

    // Column widths captured by BeginTable so TableRow can size its cells to
    // match the header without the caller passing them again.
    Clay_SizingAxis tableColWidths[12];
    int32_t tableColCount;
} ClayWidgets_Context;

typedef struct ClayWidgets_SliderOptions {
    float minValue;
    float maxValue;
    float step;
} ClayWidgets_SliderOptions;

typedef struct ClayWidgets_StepperOptions {
    int32_t minValue;
    int32_t maxValue;
    int32_t step;
} ClayWidgets_StepperOptions;

typedef struct ClayWidgets_TextInputOptions {
    const char *placeholder;
    bool clearOnEnter;
} ClayWidgets_TextInputOptions;

typedef struct ClayWidgets_ScrollPanelOptions {
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    uint16_t fadeMargin; // vertical inset where content clips before the panel edge (0 = theme default)
    uint16_t padding;    // horizontal content padding (0 = theme default)
    uint16_t childGap;   // gap between child widgets (0 = theme default)
} ClayWidgets_ScrollPanelOptions;

#include "core.h"
#include "themes.h"
#include "text.h"
#include "image.h"
#include "badge.h"
#include "card.h"
#include "collapsible.h"
#include "tree.h"
#include "button.h"
#include "checkbox.h"
#include "toggle.h"
#include "radio.h"
#include "tab.h"
#include "slider.h"
#include "progress-bar.h"
#include "text-input.h"
#include "combo.h"
#include "listbox.h"
#include "segmented.h"
#include "stepper.h"
#include "table.h"
#include "tooltip.h"
#include "modal.h"
#include "toast.h"
#include "menu.h"
#include "scroll-bar.h"
#include "scroll-panel.h"

#ifdef __cplusplus
}
#endif

#endif