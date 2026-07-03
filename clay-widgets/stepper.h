#ifndef CLAY_WIDGETS_STEPPER_H
#define CLAY_WIDGETS_STEPPER_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before stepper.h"
#endif

// A numeric stepper: [-] value [+], joined into one bordered control. Clicking
// the buttons steps the caller-owned int by options.step, clamped to
// [minValue, maxValue]; when focused, Up/Right increment and Down/Left
// decrement. Returns true on the frame the value changes.
//
//   ClayWidgets_Stepper(&ui, CLAY_ID("Count"), &count,
//                       ClayWidgets_StepperOptions{ 0, 99, 1 });
bool ClayWidgets_Stepper(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    int32_t *value,
    ClayWidgets_StepperOptions options
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_Stepper(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    int32_t *value,
    ClayWidgets_StepperOptions options
) {
    if (!ctx || !value) {
        return false;
    }

    int32_t minValue = options.minValue;
    int32_t maxValue = options.maxValue;
    if (maxValue < minValue) {
        int32_t tmp = minValue; minValue = maxValue; maxValue = tmp;
    }
    int32_t step = options.step > 0 ? options.step : 1;

    Clay_ElementId minusId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsStepperMinus"), id.id);
    Clay_ElementId plusId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsStepperPlus"), id.id);

    bool over = Clay_PointerOver(id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, over);
    bool changed = false;

    int32_t start = *value;
    if (ClayWidgets__ConsumeClick(ctx, Clay_PointerOver(minusId))) {
        *value -= step;
    }
    if (ClayWidgets__ConsumeClick(ctx, Clay_PointerOver(plusId))) {
        *value += step;
    }
    if (focused) {
        if (ctx->input.keyUp || ctx->input.keyRight) {
            *value += step;
        }
        if (ctx->input.keyDown || ctx->input.keyLeft) {
            *value -= step;
        }
    }
    *value = ClayWidgets__MaxI32(minValue, ClayWidgets__MinI32(*value, maxValue));
    changed = (*value != start);

    float r = (float)ctx->theme.radiusSm;
    bool minusOver = Clay_PointerOver(minusId);
    bool plusOver = Clay_PointerOver(plusId);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = 0,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .cornerRadius = CLAY_CORNER_RADIUS(r),
        .border = {
            .color = focused ? ctx->theme.focusRingColor : ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        // Minus button.
        CLAY(minusId, {
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIXED(34), .height = CLAY_SIZING_FIT(0, 0) },
                .padding = { .left = 0, .right = 0, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = minusOver ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
            .cornerRadius = { .topLeft = r, .topRight = 0.0f, .bottomLeft = r, .bottomRight = 0.0f },
        }) {
            CLAY_TEXT(CLAY_STRING("-"), {
                .textColor = ctx->theme.textColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }

        // Value display.
        CLAY_AUTO_ID({
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(50, 0), .height = CLAY_SIZING_GROW(0) },
                .padding = { .left = ctx->theme.spacing.sm, .right = ctx->theme.spacing.sm, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .border = {
                .color = ctx->theme.borderColor,
                .width = { .left = 1, .right = 1, .top = 0, .bottom = 0 },
            },
        }) {
            CLAY_TEXT(ClayWidgets__ScratchInt(ctx, *value), {
                .textColor = ctx->theme.textColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
                .textAlignment = CLAY_TEXT_ALIGN_CENTER,
            });
        }

        // Plus button.
        CLAY(plusId, {
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIXED(34), .height = CLAY_SIZING_FIT(0, 0) },
                .padding = { .left = 0, .right = 0, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = plusOver ? ctx->theme.hoverColor : ctx->theme.surfaceAltColor,
            .cornerRadius = { .topLeft = 0.0f, .topRight = r, .bottomLeft = 0.0f, .bottomRight = r },
        }) {
            CLAY_TEXT(CLAY_STRING("+"), {
                .textColor = ctx->theme.textColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }
    }

    return changed;
}

#endif

#endif
