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
    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);

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

    float denominator = (maxValue - minValue);
    float t = denominator > 0.0f ? (clamped - minValue) / denominator : 0.0f;
    t = ClayWidgets__Clamp(t, 0.0f, 1.0f);

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
            .backgroundColor = ClayWidgets__MixColor(ctx->theme.accentMutedColor, ctx->theme.accentColor, t),
            .cornerRadius = CLAY_CORNER_RADIUS(8),
        }) {}
    }

    return clamped;
}

#endif

#endif