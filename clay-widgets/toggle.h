#ifndef CLAY_WIDGETS_TOGGLE_H
#define CLAY_WIDGETS_TOGGLE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before toggle.h"
#endif

// An on/off switch. Behaves like a checkbox (click or focus+Enter flips the
// bound bool) but reads as a sliding pill: the track fills with the accent
// color when on and a round knob slides from left (off) to right (on). Pass an
// empty label for a compact, label-less switch. Returns true on the frame the
// value changes.
bool ClayWidgets_Toggle(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_Toggle(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String text, bool *value) {
    if (!ctx || !value) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);
    if (!clicked && ClayWidgets__ActivateFocused(ctx, id)) {
        clicked = true;
    }
    if (clicked) {
        *value = !(*value);
    }

    const float trackWidth = 40.0f;
    const float trackHeight = 22.0f;
    const float knobSize = 18.0f;
    const float trackPad = 2.0f;

    // Track fills with the accent when on; the knob slides from left (off) to
    // right (on). The Clay color transition eases the track fill; the knob's
    // horizontal position is a Route B eased scalar (0 = off, 1 = on) driven into
    // the track's left padding, since a slide isn't a property of one element.
    Clay_Color trackColor = *value ? ctx->theme.accentColor : ctx->theme.surfaceAltColor;
    float knobT = ClayWidgets__AnimTo(ctx, id.id, *value ? 1.0f : 0.0f, 18.0f);
    float knobTravel = trackWidth - 2.0f * trackPad - knobSize;
    uint16_t knobLeftPad = (uint16_t)(trackPad + knobT * knobTravel + 0.5f);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_FIXED(trackWidth),
                    .height = CLAY_SIZING_FIXED(trackHeight),
                },
                .padding = { .left = knobLeftPad, .right = (uint16_t)trackPad, .top = (uint16_t)trackPad, .bottom = (uint16_t)trackPad },
                .childAlignment = {
                    .x = CLAY_ALIGN_X_LEFT,
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor = trackColor,
            .cornerRadius = CLAY_CORNER_RADIUS(trackHeight * 0.5f),
            .border = {
                .color = (focused || over) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
            },
            .transition = ClayWidgets__ColorTransition(ctx),
        }) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIXED(knobSize),
                        .height = CLAY_SIZING_FIXED(knobSize),
                    },
                },
                .backgroundColor = (Clay_Color){248, 250, 252, 255},
                .cornerRadius = CLAY_CORNER_RADIUS(knobSize * 0.5f),
                .border = {
                    .color = ctx->theme.borderColor,
                    .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
                },
            }) {}
        }

        if (text.length > 0 && text.chars) {
            CLAY_TEXT(text, {
                .textColor = ctx->theme.textColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }
    }

    return clicked;
}

#endif

#endif
