#ifndef CLAY_WIDGETS_TEXT_H
#define CLAY_WIDGETS_TEXT_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before text.h"
#endif

void ClayWidgets_Label(ClayWidgets_Context *ctx, Clay_String text);
void ClayWidgets_Heading(ClayWidgets_Context *ctx, Clay_String text);
void ClayWidgets_Separator(ClayWidgets_Context *ctx);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

void ClayWidgets_Label(ClayWidgets_Context *ctx, Clay_String text) {
    if (!ctx) {
        return;
    }
    CLAY_TEXT(text, {
        .textColor = ctx->theme.textColor,
        .fontId = ctx->theme.fontBody,
        .fontSize = ctx->theme.fontSizeBody,
    });
}

void ClayWidgets_Heading(ClayWidgets_Context *ctx, Clay_String text) {
    if (!ctx) {
        return;
    }
    CLAY_TEXT(text, {
        .textColor = ctx->theme.textColor,
        .fontId = ctx->theme.fontHeading,
        .fontSize = ctx->theme.fontSizeHeading,
    });
}

void ClayWidgets_Separator(ClayWidgets_Context *ctx) {
    if (!ctx) {
        return;
    }
    // A classic separator is etched rather than drawn: a shadow line with a
    // highlight line under it, which is exactly a 2px bar whose bottom edge is
    // the highlight. Flat themes keep the single hairline.
    bool beveled = ClayWidgets__IsBeveled(ctx);
    CLAY_AUTO_ID({
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_FIXED(beveled ? 2.0f : 1.0f),
            },
        },
        .backgroundColor = beveled ? ctx->theme.edgeShadowColor : ctx->theme.borderColor,
        .border = {
            .color = ctx->theme.edgeHighlightColor,
            .width = { .bottom = (uint16_t)(beveled ? 1 : 0) },
        },
    }) {}
}

#endif

#endif