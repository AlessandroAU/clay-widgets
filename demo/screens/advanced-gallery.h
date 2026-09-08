#ifndef CLAY_WIDGETS_ADVANCED_GALLERY_H
#define CLAY_WIDGETS_ADVANCED_GALLERY_H
#include <string>
#include <vector>
namespace {
static Clay_String AdvancedItem(int32_t row, void *data) {
    const auto &items = *static_cast<std::vector<std::string> *>(data);
    return ClayStringFromCString(items[row].c_str());
}
static Clay_String AdvancedCell(int32_t row, int32_t column, void *data) {
    return column == 0 ? AdvancedItem(row,data) : (row%2 ? CLAY_STRING("Ready") : CLAY_STRING("Queued"));
}
static int AdvancedCompare(int32_t a,int32_t b,int32_t column,void *) {
    return column == 0 ? a-b : (a%2)-(b%2);
}
static void DrawAdvancedGallery(ClayWidgets_Context &ui) {
    static std::vector<std::string> items;
    static int32_t order[10000]; static bool selection[10000];
    static ClayWidgets_TableState table={};
    static ClayWidgets_TextHistory history={};
    static char notes[512]="Try editing this text";
    static char query[64]="";
    static int32_t picked=0;
    static bool readOnly=false;
    static float ratio=0.5f;
    static Clay_Dimensions panelSize={260,90};
    if(items.empty()) {
        items.reserve(10000);
        for(int i=0;i<10000;++i) {items.push_back("Record "+std::to_string(i));order[i]=i;}
    }
    ClayWidgets_BeginCard(&ui,CLAY_ID("AdvancedGalleryCard"),CLAY_STRING("Editors & data"));
    ClayWidgets_EditResult edit={};
    ClayWidgets_TextInputOptions opts={};opts.history=&history;opts.readOnly=readOnly;opts.result=&edit;
    CLAY_AUTO_ID({ .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
        .childGap = ui.theme.spacing.sm, .layoutDirection = CLAY_TOP_TO_BOTTOM,
    } }) {
        CLAY_AUTO_ID({ .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
        } }) {
            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) } } }) {
                ClayWidgets_Label(&ui,CLAY_STRING("Text editor"));
            }
            ClayWidgets_Checkbox(&ui,CLAY_ID("AdvancedReadOnly"),CLAY_STRING("Read-only"),&readOnly);
        }
        opts.readOnly = readOnly;
        ClayWidgets_TextInput(&ui,CLAY_ID("AdvancedEditor"),CLAY_STRING(""),notes,sizeof(notes),opts);
        MutedLabel(ui,CLAY_STRING("Ctrl+Z to undo, Ctrl+Y to redo."));
    }
    ClayWidgets_SearchableCombo(&ui,CLAY_ID("AdvancedSearch"),CLAY_STRING("Build configuration"),kBuildConfigNames,4,&picked,query,sizeof(query));
    CLAY_AUTO_ID({ .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0) },
        .childGap = ui.theme.spacing.sm, .layoutDirection = CLAY_TOP_TO_BOTTOM,
    } }) {
        ClayWidgets_Label(&ui,CLAY_STRING("Records"));
        MutedLabel(ui,CLAY_STRING("10,000 rows. Click a header to sort; drag a divider to resize."));
        ClayWidgets_TableColumn columns[]={{CLAY_STRING("Record"),CLAY_SIZING_GROW(0)},{CLAY_STRING("Status"),CLAY_SIZING_FIXED(110)}};
        ClayWidgets_DataTable(&ui,CLAY_ID("AdvancedTable"),columns,2,10000,AdvancedCell,AdvancedCompare,&items,order,selection,&table,{140,28,60,false});
        MutedLabel(ui,CLAY_STRING("Shift selects a range; Ctrl toggles individual rows."));
    }
    ClayWidgets_Label(&ui,CLAY_STRING("Drag the split divider or focus it and use arrows"));
    ClayWidgets_SplitOptions split={};split.sizing={CLAY_SIZING_GROW(0),CLAY_SIZING_FIXED(90)};split.minFirst=50;split.minSecond=50;
    if(ClayWidgets_BeginSplit(&ui,CLAY_ID("AdvancedSplit"),&ratio,split)) {
        ClayWidgets_Label(&ui,CLAY_STRING("First pane"));
        ClayWidgets_NextSplit(&ui,CLAY_ID("AdvancedSplit"),false);
        ClayWidgets_Label(&ui,CLAY_STRING("Second pane"));
        ClayWidgets_EndSplit(&ui);
    }
    if(ClayWidgets_BeginResizablePanel(&ui,CLAY_ID("AdvancedPanel"),&panelSize,{140,60,360,180})) {
        ClayWidgets_Label(&ui,CLAY_STRING("Resize using the lower-right corner"));
        ClayWidgets_EndResizablePanel(&ui);
    }
    ClayWidgets_EndCard(&ui,CLAY_ID("AdvancedGalleryCard"));
}
}
#endif
