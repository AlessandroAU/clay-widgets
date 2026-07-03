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
    CLAY_AUTO_ID({
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_FIXED(1),
            },
        },
        .backgroundColor = ctx->theme.borderColor,
    }) {}
}

#endif

#endif