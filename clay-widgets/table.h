#ifndef CLAY_WIDGETS_TABLE_H
#define CLAY_WIDGETS_TABLE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before table.h"
#endif

// A simple data table / grid. Define columns (title + a Clay sizing per column),
// call BeginTable to open the framed table and draw the header, then emit one
// TableRow per record, then EndTable. Rows are zebra-striped, hover-highlighted
// and selectable (TableRow returns true on click). Up to 12 columns.
//
//   ClayWidgets_TableColumn cols[] = {
//       { CLAY_STRING("Name"), CLAY_SIZING_GROW(0) },
//       { CLAY_STRING("Size"), CLAY_SIZING_FIXED(90) },
//   };
//   ClayWidgets_BeginTable(&ui, CLAY_ID("Files"), cols, 2);
//   for (int r = 0; r < n; r++) {
//       Clay_String cells[] = { rows[r].name, rows[r].size };
//       if (ClayWidgets_TableRow(&ui, rowId(r), cells, 2, r, r == selectedRow)) selectedRow = r;
//   }
//   ClayWidgets_EndTable(&ui, CLAY_ID("Files"));
typedef struct ClayWidgets_TableColumn {
    Clay_String title;
    Clay_SizingAxis width;
} ClayWidgets_TableColumn;

void ClayWidgets_BeginTable(ClayWidgets_Context *ctx, Clay_ElementId id, const ClayWidgets_TableColumn *columns, int32_t columnCount);
bool ClayWidgets_TableRow(ClayWidgets_Context *ctx, Clay_ElementId rowId, const Clay_String *cells, int32_t cellCount, int32_t rowIndex, bool selected);
void ClayWidgets_EndTable(ClayWidgets_Context *ctx, Clay_ElementId id);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

void ClayWidgets_BeginTable(ClayWidgets_Context *ctx, Clay_ElementId id, const ClayWidgets_TableColumn *columns, int32_t columnCount) {
    if (!ctx || !columns || columnCount <= 0) {
        return;
    }
    int32_t maxCols = (int32_t)(sizeof(ctx->tableColWidths) / sizeof(ctx->tableColWidths[0]));
    ctx->tableColCount = columnCount < maxCols ? columnCount : maxCols;
    for (int32_t i = 0; i < ctx->tableColCount; ++i) {
        ctx->tableColWidths[i] = columns[i].width;
    }

    Clay__OpenElementWithId(id);
    Clay__ConfigureOpenElement(CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
        .clip = { .horizontal = true, .vertical = false, .childOffset = { 0, 0 } },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    });

    // Header row: a darker strip with muted column titles, divided from the body.
    CLAY_AUTO_ID({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = 0,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = ctx->theme.surfaceColor,
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 0, .right = 0, .top = 0, .bottom = 1 },
        },
    }) {
        for (int32_t i = 0; i < ctx->tableColCount; ++i) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = ctx->tableColWidths[i], .height = CLAY_SIZING_FIT(0, 0) },
                    .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                },
            }) {
                CLAY_TEXT(columns[i].title, {
                    .textColor = ctx->theme.textMutedColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeSmall,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                });
            }
        }
    }
}

bool ClayWidgets_TableRow(ClayWidgets_Context *ctx, Clay_ElementId rowId, const Clay_String *cells, int32_t cellCount, int32_t rowIndex, bool selected) {
    if (!ctx || !cells || cellCount <= 0) {
        return false;
    }

    bool over = Clay_PointerOver(rowId);
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);

    Clay_Color transparent = { 0, 0, 0, 0 };
    Clay_Color rowBg = transparent;
    if (selected) {
        rowBg = ctx->theme.accentMutedColor;
    } else if (over) {
        rowBg = ctx->theme.hoverColor;
    } else if ((rowIndex & 1) != 0) {
        rowBg = ctx->theme.surfaceColor; // zebra stripe
    }

    int32_t count = cellCount < ctx->tableColCount ? cellCount : ctx->tableColCount;

    CLAY(rowId, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = 0,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = rowBg,
    }) {
        for (int32_t i = 0; i < count; ++i) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = ctx->tableColWidths[i], .height = CLAY_SIZING_FIT(0, 0) },
                    .padding = CLAY_PADDING_ALL(ctx->theme.spacing.sm),
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                },
            }) {
                CLAY_TEXT(cells[i], {
                    .textColor = ctx->theme.textColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeBody,
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                });
            }
        }
    }

    return clicked;
}

void ClayWidgets_EndTable(ClayWidgets_Context *ctx, Clay_ElementId id) {
    (void)id;
    if (!ctx) {
        return;
    }
    Clay__CloseElement(); // table container
}

#endif

#endif
