#ifndef CLAY_WIDGETS_SLIDER_H
#define CLAY_WIDGETS_SLIDER_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before slider.h"
#endif

float ClayWidgets_Slider(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    float value,
    ClayWidgets_SliderOptions options
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

float ClayWidgets_Slider(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    float value,
    ClayWidgets_SliderOptions options
) {
    if (!ctx) {
        return value;
    }

    float minValue = options.minValue;
    float maxValue = options.maxValue;
    float step = options.step;

    if (maxValue < minValue) {
        float temp = minValue;
        minValue = maxValue;
        maxValue = temp;
    }

    float clamped = ClayWidgets__Clamp(value, minValue, maxValue);
    bool over = options.disabled ? false : Clay_PointerOver(id);
    bool focused = options.disabled ? false : ClayWidgets__RegisterFocusable(ctx, id, over);

    if (!options.disabled) {
        if (ctx->input.pointerPressed && over) {
            ctx->activeId = id.id;
        }
        if (!ctx->input.pointerDown && ctx->activeId == id.id) {
            ctx->activeId = 0;
        }

        if (ctx->activeId == id.id && ctx->input.pointerDown) {
            Clay_ElementData data = Clay_GetElementData(id);
            if (data.found && data.boundingBox.width > 0.0f) {
                float localX = ctx->input.mouseX - data.boundingBox.x;
                float ratio = ClayWidgets__Clamp(localX / data.boundingBox.width, 0.0f, 1.0f);
                clamped = minValue + (maxValue - minValue) * ratio;

                if (step > 0.0f) {
                    float steps = (clamped - minValue) / step;
                    int32_t rounded = (int32_t)(steps + (steps >= 0.0f ? 0.5f : -0.5f));
                    clamped = minValue + ((float)rounded * step);
                }
                clamped = ClayWidgets__Clamp(clamped, minValue, maxValue);
            }
        }

        // Keyboard adjustment while focused: arrows step the value (a
        // hundredth of the range when no step is set), Home/End jump to the
        // ends. Keeps a Tab-focusable slider actually operable by keyboard.
        if (focused) {
            float keyStep = step > 0.0f ? step : (maxValue - minValue) / 100.0f;
            if (ctx->input.keyLeft) {
                clamped = ClayWidgets__Clamp(clamped - keyStep, minValue, maxValue);
            }
            if (ctx->input.keyRight) {
                clamped = ClayWidgets__Clamp(clamped + keyStep, minValue, maxValue);
            }
            if (ctx->input.keyHome) {
                clamped = minValue;
            }
            if (ctx->input.keyEnd) {
                clamped = maxValue;
            }
        }
    }

    float denominator = (maxValue - minValue);
    float t = denominator > 0.0f ? (clamped - minValue) / denominator : 0.0f;
    t = ClayWidgets__Clamp(t, 0.0f, 1.0f);

    Clay_Color fillColor = ClayWidgets__MixColor(ctx->theme.accentMutedColor, ctx->theme.accentColor, t);
    if (options.disabled) {
        fillColor = ClayWidgets__MixColor(fillColor, ctx->theme.surfaceColor, ctx->theme.disabledMix);
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(18) },
            .padding = CLAY_PADDING_ALL(1),
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(9),
        .border = {
            .color = (focused || over) ? ctx->theme.focusRingColor : ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_PERCENT(t),
                    .height = CLAY_SIZING_GROW(0),
                },
            },
            .backgroundColor = fillColor,
            .cornerRadius = CLAY_CORNER_RADIUS(8),
        }) {}

        // Live value, centered over the whole track. Floating so it overlays the
        // fill without affecting layout, and pointer-passthrough so it never
        // steals a drag from the slider underneath it.
        if (options.showValue) {
            int32_t decimals = options.valueDecimals;
            if (decimals <= 0) {
                if (step > 0.0f) {
                    if (step >= 1.0f) decimals = 0;
                    else if (step >= 0.1f) decimals = 1;
                    else if (step >= 0.01f) decimals = 2;
                    else decimals = 3;
                } else {
                    float range = maxValue - minValue;
                    if (range <= 1.0f) decimals = 2;
                    else if (range <= 10.0f) decimals = 1;
                    else decimals = 0;
                }
            }
            Clay_String valueText = ClayWidgets__ScratchFloat(ctx, clamped, decimals);
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
                },
                .floating = {
                    .parentId = id.id,
                    .zIndex = 1,
                    .attachPoints = {
                        .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                        .parent = CLAY_ATTACH_POINT_CENTER_CENTER,
                    },
                    .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                    .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
                },
            }) {
                CLAY_TEXT(valueText, {
                    .textColor = options.disabled ? ctx->theme.textMutedColor : ctx->theme.textColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeSmall,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                });
            }
        }
    }

    return clamped;
}

#endif

#endif