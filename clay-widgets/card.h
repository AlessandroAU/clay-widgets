#ifndef CLAY_WIDGETS_CARD_H
#define CLAY_WIDGETS_CARD_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before card.h"
#endif

// A card / group box: a bordered, padded surface with an optional title header
// and a separator, used to group related controls. Begin/End wrap the body:
//
//   ClayWidgets_BeginCard(&ui, CLAY_ID("Network"), CLAY_STRING("Network"));
//   ClayWidgets_Checkbox(&ui, CLAY_ID("Wifi"), CLAY_STRING("Wi-Fi"), &wifi);
//   ClayWidgets_Slider(&ui, CLAY_ID("Bw"), bw, opts);
//   ClayWidgets_EndCard(&ui, CLAY_ID("Network"));
//
// Pass an empty title for an untitled panel (no header row or separator).
// BeginCard grows to its parent's width and fits its content's height;
// BeginCardEx takes explicit sizing.
void ClayWidgets_BeginCardEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, Clay_SizingAxis width, Clay_SizingAxis height);
void ClayWidgets_BeginCard(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title);
void ClayWidgets_EndCard(ClayWidgets_Context *ctx, Clay_ElementId id);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

void ClayWidgets_BeginCardEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title, Clay_SizingAxis width, Clay_SizingAxis height) {
    if (!ctx) {
        return;
    }

    ClayWidgets__BeginElement(id, CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = width, .height = height },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.lg),
            .childGap = ctx->theme.spacing.md,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusMd),
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    });

    if (title.length > 0 && title.chars) {
        // A section header: the heading font at a size between body and page
        // heading, followed by a divider that separates it from the body.
        CLAY_TEXT(title, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontHeading,
            .fontSize = (uint16_t)(ctx->theme.fontSizeBody + 2),
        });
        ClayWidgets_Separator(ctx);
    }
}

void ClayWidgets_BeginCard(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title) {
    ClayWidgets_BeginCardEx(ctx, id, title, CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0, 0));
}

void ClayWidgets_EndCard(ClayWidgets_Context *ctx, Clay_ElementId id) {
    (void)id;
    if (!ctx) {
        return;
    }
    ClayWidgets__EndElement(); // card surface
}

#endif

#endif
