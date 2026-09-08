// Additional behavioral regressions; included by test-widgets.cpp after its harness.
static bool RejectBang(const char *text, int32_t, void *) { return std::strchr(text,'!') == nullptr; }
static bool FailClipboard(const char *, void *) { return false; }
static void TestReviewRegressions() {
    char small[2]=""; int32_t length=0;
    CHECK(!ClayWidgets__InsertTextAtCaret(&ui,small,&length,2,"\xC3\xA9",2));
    CHECK(length==0 && small[0]==0);
    char enough[5]=""; length=0;
    CHECK(ClayWidgets__InsertTextAtCaret(&ui,enough,&length,5,"\xF0\x9F\x98\x80",4));
    CHECK(length==4);
    ui.textCursor=ui.textSelectionAnchor=0;
    char selected[3]="ab"; length=2; ui.textCursor=2;
    CHECK(!ClayWidgets__InsertTextAtCaret(&ui,selected,&length,3,"\xF0\x9F\x98\x80",4));
    CHECK(!strcmp(selected,"ab")); // rejected replacement must preserve the selection

    ResetUi(); auto field=CLAY_ID("SafeCut"); char buffer[4096]; memset(buffer,'a',3000); buffer[3000]=0;
    ClayWidgets_EditResult result={}; ClayWidgets_TextInputOptions options={}; options.result=&result;
    auto body=[&]{ClayWidgets_TextInput(&ui,field,CLAY_STRING(""),buffer,sizeof(buffer),options);};
    ui.focusedId=field.id; Frame(MakeInput(),body); ui.textSelectionAnchor=0;
    ClayWidgets_SetClipboardFunctions(&ui,nullptr,nullptr,nullptr);
    auto cut=MakeInput();cut.keyCut=true;Frame(cut,body);
    CHECK(strlen(buffer)==3000 && result.clipboardFailed);
    ClayWidgets_SetClipboardFunctions(&ui,TestGetClipboard,TestSetClipboard,nullptr);
    ClayWidgets_SetClipboardWriteFunction(&ui,FailClipboard);
    Frame(cut,body); CHECK(strlen(buffer)==3000 && result.clipboardFailed);
    ClayWidgets_SetClipboardWriteFunction(&ui,nullptr);
    Frame(cut,body); CHECK(buffer[0]==0 && g_clipboard.size()==3000 && result.changed);

    ResetUi(); auto combo=CLAY_ID("KeyboardComboRegression");
    Clay_String items[]={CLAY_STRING("One"),CLAY_STRING("Two")}; int32_t selection=0; bool changed=false;
    ui.focusedId=combo.id; auto enter=MakeInput();enter.keyEnter=true;
    auto comboBody=[&]{changed=ClayWidgets_Combo(&ui,combo,CLAY_STRING(""),items,2,&selection);};
    Frame(enter,comboBody);CHECK(ui.openComboId==combo.id && !changed);
    Frame(enter,comboBody);CHECK(!ui.openComboId && !changed);
    int32_t choice=1;auto radio=CLAY_ID("RadioChanged");ui.focusedId=radio.id;
    Frame(enter,[&]{CHECK(!ClayWidgets_Radio(&ui,radio,CLAY_STRING("Chosen"),1,&choice));});
    auto tab=CLAY_ID("TabChanged");ui.focusedId=tab.id;
    Frame(enter,[&]{CHECK(!ClayWidgets_Tab(&ui,tab,CLAY_STRING("Chosen"),1,&choice));});

    ResetUi(); int32_t value=INT32_MAX; auto stepper=CLAY_ID("OverflowStepper");ui.focusedId=stepper.id;
    auto right=MakeInput();right.keyRight=true;
    Frame(right,[&]{ClayWidgets_Stepper(&ui,stepper,&value,{INT32_MIN,INT32_MAX,1,false});});
    CHECK(value==INT32_MAX);
    value=INT32_MIN;right.keyRight=false;right.keyLeft=true;
    Frame(right,[&]{ClayWidgets_Stepper(&ui,stepper,&value,{INT32_MIN,INT32_MAX,INT32_MAX,false});});CHECK(value==INT32_MIN);

    ResetUi();auto button=CLAY_ID("RemovedCapture");
    auto buttonBody=[&]{ClayWidgets_Button(&ui,button,CLAY_STRING("Button"));};Frame(MakeInput(),buttonBody);
    auto box=Clay_GetElementData(button).boundingBox;Frame(PressAt(box.x+5,box.y+5),buttonBody);CHECK(ui.activeId==button.id);
    Frame(PressAt(box.x+5,box.y+5),[]{});CHECK(ui.activeId==0);
    Frame(ReleaseAt(box.x+5,box.y+5),[]{});CHECK(ui.activeId==0);
    bool unintended=false;
    Frame(MakeInput(),buttonBody);
    Frame(ReleaseAt(box.x+5,box.y+5),[&]{unintended=ClayWidgets_Button(&ui,button,CLAY_STRING("Button"));});
    CHECK(!unintended);

    ResetUi();bool open=true;auto modal=CLAY_ID("LayerRegressionModal");
    auto modalBody=[&]{if(ClayWidgets_BeginModal(&ui,modal,CLAY_STRING("Modal"),&open)){
        ClayWidgets_Combo(&ui,combo,CLAY_STRING(""),items,2,&selection);ClayWidgets_EndModal(&ui,modal);}};
    Frame(MakeInput(),modalBody);ui.focusedId=combo.id;ui.openComboId=combo.id;
    auto commands=Frame(MakeInput(),modalBody);
    auto dropdown=Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsComboDropdown"),combo.id);
    Clay_RenderCommand drop={},scrim={};
    CHECK(FindCommandById(commands,dropdown.id,&drop));CHECK(FindCommandById(commands,ClayWidgets__ModalScrimId(modal).id,&scrim));
    CHECK(drop.zIndex>scrim.zIndex); CHECK(ui.overlayDepth==0);
}

static int g_semantics=0,g_ime=0;
static void SemanticSink(const ClayWidgets_SemanticNode *node,void *) { if(node->role==CLAY_WIDGETS_ROLE_TEXT_FIELD) ++g_semantics; }
static void ImeSink(Clay_BoundingBox caret,const char *,int32_t,int32_t,void *) { CHECK(caret.width==1 && caret.height>0); ++g_ime; }
static void TestEditorFeatures() {
    static ClayWidgets_TextHistory history={}; history={};
    auto id=CLAY_ID("HistoryEditor");char buffer[128]="hello world";
    ClayWidgets_EditResult result={};ClayWidgets_TextInputOptions options={};options.history=&history;options.result=&result;options.validate=RejectBang;
    ui.focusedId=id.id;
    auto body=[&]{ClayWidgets_TextInput(&ui,id,CLAY_STRING("Name"),buffer,sizeof(buffer),options);};
    Frame(MakeInput(),body);
    auto type=MakeInput();type.textUtf8=".";type.textUtf8Length=1;Frame(type,body);CHECK(!strcmp(buffer,"hello world."));
    auto undo=MakeInput();undo.keyUndo=true;Frame(undo,body);CHECK(!strcmp(buffer,"hello world") && result.changed);
    auto redo=MakeInput();redo.keyRedo=true;Frame(redo,body);CHECK(!strcmp(buffer,"hello world.") && result.changed);
    type.textUtf8="!";Frame(type,body);CHECK(!strcmp(buffer,"hello world.") && result.rejected && !result.changed);
    Frame(undo,body); type.textUtf8="?";Frame(type,body);Frame(redo,body);CHECK(!strcmp(buffer,"hello world?"));
    auto word=MakeInput();word.controlDown=true;word.keyLeft=true;Frame(word,body);CHECK(ui.textCursor==6);
    options.readOnly=true;Frame(type,body);CHECK(!strcmp(buffer,"hello world?"));Frame(undo,body);CHECK(!strcmp(buffer,"hello world?"));
    options.readOnly=false;auto submit=MakeInput();submit.keyEnter=true;Frame(submit,body);CHECK(result.submitted && !result.changed);
    ClayWidgets_SetSemanticFunction(&ui,SemanticSink,nullptr);ClayWidgets_SetImeFunction(&ui,ImeSink,nullptr);g_semantics=g_ime=0;
    auto preedit=MakeInput();preedit.compositionUtf8="candidate";preedit.compositionLength=9;preedit.keyEnter=true;
    Frame(preedit,body);CHECK(!strcmp(buffer,"hello world?") && !result.submitted);CHECK(g_semantics==1 && g_ime==1);
    Frame(MakeInput(),[&]{CHECK(ClayWidgets_BeginDisabled(&ui));body();ClayWidgets_EndDisabled(&ui);});CHECK(ui.focusedId==0);
    ClayWidgets_RequestFocus(&ui,id);Frame(MakeInput(),body);CHECK(ui.focusedId==id.id);
    ResetUi();auto area=CLAY_ID("AreaHistory");char multiline[64]="a\nb";
    ClayWidgets_TextAreaOptions areaOptions={};areaOptions.history=&history;areaOptions.result=&result;areaOptions.validate=RejectBang;
    ui.focusedId=area.id;auto areaBody=[&]{ClayWidgets_TextArea(&ui,area,CLAY_STRING("Notes"),multiline,sizeof(multiline),areaOptions);};
    Frame(MakeInput(),areaBody);Frame(submit,areaBody);CHECK(!strcmp(multiline,"a\nb\n") && !result.submitted);
    Frame(undo,areaBody);CHECK(!strcmp(multiline,"a\nb"));
    submit.controlDown=true;Frame(submit,areaBody);CHECK(result.submitted && !strcmp(multiline,"a\nb"));
    areaOptions.readOnly=true;Frame(type,areaBody);CHECK(!strcmp(multiline,"a\nb"));
}

static int g_itemCalls=0;
static Clay_String VirtualItem(int32_t,void *) {++g_itemCalls;return CLAY_STRING("Item");}
static Clay_String TableCell(int32_t row,int32_t,void *) { static const Clay_String names[]={CLAY_STRING("C"),CLAY_STRING("A"),CLAY_STRING("B")};return names[row%3]; }
static int TableCompare(int32_t a,int32_t b,int32_t,void *) { static const int values[]={3,1,2}; return values[a]-values[b]; }
static void TestCollectionsAndPanels() {
    auto id=CLAY_ID("HugeVirtualList");int32_t selected=0;g_itemCalls=0;
    auto body=[&]{ClayWidgets_VirtualList(&ui,id,100000,VirtualItem,nullptr,&selected,{120,24,false});};
    Frame(MakeInput(),body);CHECK(g_itemCalls<20);
    ui.focusedId=id.id;auto end=MakeInput();end.keyEnd=true;g_itemCalls=0;Frame(end,body);CHECK(selected==99999 && g_itemCalls<20);
    auto scroll=Clay_GetScrollContainerData(id);CHECK(scroll.found && scroll.scrollPosition->y< -2000000);

    ResetUi();auto table=CLAY_ID("RichTable");int32_t order[]={0,1,2};bool selection[3]={};ClayWidgets_TableState state={};
    ClayWidgets_TableColumn columns[]={{CLAY_STRING("Name"),CLAY_SIZING_FIXED(120)}};
    auto tableBody=[&]{ClayWidgets_DataTable(&ui,table,columns,1,3,TableCell,TableCompare,nullptr,order,selection,&state,{100,24,40,false});};
    Frame(MakeInput(),tableBody);
    auto header=ClayWidgets__ChildId(table,CLAY_STRING("TableHeader"),0);ui.focusedId=header.id;
    auto enter=MakeInput();enter.keyEnter=true;Frame(enter,tableBody);CHECK(state.sortChanged);
    CHECK(order[0]==1 && order[1]==2 && order[2]==0); // first click sorts ascending
    Frame(enter,tableBody);CHECK(order[0]==0 && order[1]==2 && order[2]==1);
    ui.focusedId=table.id;auto down=MakeInput();down.keyDown=true;Frame(down,tableBody);CHECK(selection[2]);
    down.shiftDown=true;Frame(down,tableBody);CHECK(selection[2] && selection[1]);
    auto resize=ClayWidgets__ChildId(table,CLAY_STRING("TableResize"),0);auto box=Clay_GetElementData(resize).boundingBox;
    Frame(PressAt(box.x+2,box.y+2),tableBody);auto drag=PressAt(box.x+42,box.y+2);drag.pointerPressed=false;Frame(drag,tableBody);CHECK(state.widths[0]>150);
    Frame(ReleaseAt(box.x+42,box.y+2),tableBody);CHECK(ui.activeId==0);
    Frame(MakeInput(),[&]{CHECK(!ClayWidgets_BeginTable(&ui,CLAY_ID("BadTable"),nullptr,0));ClayWidgets_EndTable(&ui,CLAY_ID("BadTable"));ClayWidgets_Label(&ui,CLAY_STRING("Still in root"));});

    ResetUi();
    ClayWidgets_TableState fitted={};
    ClayWidgets_TableColumn fittedColumns[]={{CLAY_STRING("Name"),CLAY_SIZING_GROW(0)},
        {CLAY_STRING("Status"),CLAY_SIZING_FIXED(110)}};
    float containerWidth=400;
    auto fittedBody=[&]{
        CLAY(CLAY_ID("TableContainer"),{.layout={.sizing={CLAY_SIZING_FIXED(containerWidth),CLAY_SIZING_FIT(0)}}}) {
            ClayWidgets_DataTable(&ui,table,fittedColumns,2,3,TableCell,TableCompare,nullptr,order,selection,&fitted,{100,24,40,false});
        }
    };
    Frame(MakeInput(),fittedBody);Frame(MakeInput(),fittedBody);
    auto fittedHeader=ClayWidgets__ChildId(table,CLAY_STRING("TableHeaderRow"),0);
    CHECK(fabsf(fitted.widths[0]+fitted.widths[1]+CLAY_WIDGETS_SCROLLBAR_WIDTH+4
        -Clay_GetElementData(fittedHeader).boundingBox.width)<0.1f);
    auto statusHeader=ClayWidgets__ChildId(table,CLAY_STRING("TableHeader"),1);
    auto firstRow=ClayWidgets__ChildId(table,CLAY_STRING("DataRow"),order[0]);
    auto statusCell=ClayWidgets__ChildId(firstRow,CLAY_STRING("DataCell"),1);
    CHECK(fabsf(Clay_GetElementData(statusHeader).boundingBox.x-Clay_GetElementData(statusCell).boundingBox.x)<0.1f);
    float initialWidth=fitted.widths[0];containerWidth=500;
    Frame(MakeInput(),fittedBody);Frame(MakeInput(),fittedBody);
    CHECK(fabsf(fitted.widths[0]-initialWidth-100)<0.1f && fitted.widths[1]==110);
    box=Clay_GetElementData(resize).boundingBox;
    Frame(PressAt(box.x+2,box.y+2),fittedBody);
    drag=PressAt(box.x+22,box.y+2);drag.pointerPressed=false;Frame(drag,fittedBody);
    Frame(ReleaseAt(box.x+22,box.y+2),fittedBody);
    float draggedWidth=fitted.widths[0];containerWidth=600;
    Frame(MakeInput(),fittedBody);Frame(MakeInput(),fittedBody);
    CHECK(fabsf(draggedWidth-initialWidth-120)<0.1f && fitted.widths[0]==draggedWidth);

    ResetUi();float ratio=0.5f;auto split=CLAY_ID("SplitTest");
    ClayWidgets_SplitOptions opts={};opts.sizing={CLAY_SIZING_FIXED(400),CLAY_SIZING_FIXED(100)};opts.minFirst=40;opts.minSecond=40;
    auto splitBody=[&]{if(ClayWidgets_BeginSplit(&ui,split,&ratio,opts)){ClayWidgets_Label(&ui,CLAY_STRING("Left"));ClayWidgets_NextSplit(&ui,split,false);ClayWidgets_Label(&ui,CLAY_STRING("Right"));ClayWidgets_EndSplit(&ui);}};
    Frame(MakeInput(),splitBody);ui.focusedId=ClayWidgets__ChildId(split,CLAY_STRING("SplitDivider"),0).id;
    auto right=MakeInput();right.keyRight=true;Frame(right,splitBody);CHECK(ratio>0.5f);
    auto home=MakeInput();home.keyHome=true;Frame(home,splitBody);CHECK(ratio>=0.099f && ratio<=0.101f);
    Clay_Dimensions size={100,100};auto panel=CLAY_ID("ResizePanelTest");ui.focusedId=ClayWidgets__ChildId(panel,CLAY_STRING("PanelResize"),0).id;
    Frame(right,[&]{if(ClayWidgets_BeginResizablePanel(&ui,panel,&size,{80,80,105,200}))ClayWidgets_EndResizablePanel(&ui);});CHECK(size.width==105);
}

static void TestWheelMomentum() {
    auto a=CLAY_ID("MomentumA"),b=CLAY_ID("MomentumB");
    auto body=[&]{for(auto id:{a,b}) {
        ClayWidgets__BeginScrollElement(id,(Clay_ElementDeclaration){
            .layout={.sizing={CLAY_SIZING_FIXED(300),CLAY_SIZING_FIXED(100)}},
            .clip={.horizontal=true,.vertical=true}});
        CLAY_AUTO_ID({.layout={.sizing={CLAY_SIZING_FIXED(900),CLAY_SIZING_FIXED(1000)}}}) {}
        ClayWidgets__EndElement();
    }};
    auto position=[&](Clay_ElementId id){return *Clay_GetScrollContainerData(id).scrollPosition;};
    auto initialize=[&]{ResetUi();ui.animationsEnabled=true;Frame(MakeInput(),body);Frame(MakeInput(),body);
        for(auto id:{a,b}) *Clay_GetScrollContainerData(id).scrollPosition={0,0};
        Frame(MakeInput(),body);
    };
    initialize();auto wheel=MakeInput();wheel.mouseX=10;wheel.mouseY=10;wheel.scrollY=-6;wheel.scrollX=-3;
    Frame(wheel,body);auto first=position(a);
    CHECK(first.y<0 && first.y>-60 && first.x<0 && first.x>-30);
    Frame(MakeInput(),body);CHECK(position(a).y<first.y); // coasts even after the pointer leaves
    for(int i=0;i<90;++i)Frame(MakeInput(),body);
    CHECK(fabsf(position(a).y+60)<0.01f && fabsf(position(a).x+30)<0.01f);
    CHECK(position(b).y==0);
    Frame(wheel,body);auto before=position(a);
    wheel.scrollY=3;wheel.scrollX=0;Frame(wheel,body);CHECK(position(a).y>before.y);
    Frame(PressAt(10,10),body);before=position(a);Frame(MakeInput(),body);CHECK(position(a).y==before.y);
    wheel.scrollY=-6;Frame(wheel,body);before=position(a);
    wheel.mouseY=118;Frame(wheel,body);CHECK(position(a).y==before.y && position(b).y<0);
    Clay_GetScrollContainerData(b).scrollPosition->y=-200;
    Frame(MakeInput(),body);CHECK(position(b).y==-200); // external scroll cancels momentum
    wheel.scrollY=-10000;Frame(wheel,body);Frame(MakeInput(),body);CHECK(position(b).y==-900);
    initialize();wheel.mouseY=10;wheel.scrollY=-6;wheel.deltaTime=0;Frame(wheel,body);CHECK(position(a).y==0);
    Frame(MakeInput(),body);CHECK(position(a).y<0);
    float samples[2]={};
    for(int rate=0;rate<2;++rate) {
        initialize();int hz=rate ? 120 : 30;
        wheel.deltaTime=1.0f/hz;Frame(wheel,body);
        auto idle=MakeInput();idle.deltaTime=wheel.deltaTime;
        for(int i=1;i<hz/2;++i)Frame(idle,body);
        samples[rate]=position(a).y;
    }
    CHECK(fabsf(samples[0]-samples[1])<0.01f);
    initialize();ui.scrollMomentumTime=0;Frame(wheel,body);CHECK(position(a).y==-60);
    initialize();ui.animationsEnabled=false;Frame(wheel,body);CHECK(position(a).y==-60);
    initialize();Frame(wheel,body);auto key=MakeInput();key.keyDown=true;Frame(key,body);
    before=position(a);Frame(MakeInput(),body);CHECK(position(a).y==before.y);
    initialize();Frame(wheel,body);Frame(MakeInput(),[]{});Frame(MakeInput(),[]{});
    CHECK(ui.scrollMomentumRemaining[1]==0);
}

static void TestDraggableModal() {
    auto id=CLAY_ID("DragModal");bool open=true;
    ClayWidgets_ModalOptions options={true,360};
    auto body=[&]{if(ClayWidgets_BeginModalEx(&ui,id,CLAY_STRING("Move me"),&open,options)) {
        ClayWidgets_Button(&ui,CLAY_ID("DragModalBody"),CLAY_STRING("Body control"));
        ClayWidgets_EndModal(&ui,id);
    }};
    Frame(MakeInput(),body);Frame(MakeInput(),body);
    auto dialog=ClayWidgets__ModalDialogId(id);
    auto title=Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalTitle"),id.id);
    auto initial=Clay_GetElementData(dialog).boundingBox;
    auto titleBox=Clay_GetElementData(title).boundingBox;
    Frame(PressAt(titleBox.x+5,titleBox.y+5),body);
    auto move=PressAt(titleBox.x+65,titleBox.y+35);move.pointerPressed=false;
    Frame(move,body);
    auto moved=Clay_GetElementData(dialog).boundingBox;
    CHECK(fabsf(moved.x-initial.x-60)<0.1f && fabsf(moved.y-initial.y-30)<0.1f);
    CHECK(ui.activeId==title.id);
    move.mouseX=5000;move.mouseY=5000;Frame(move,body);
    moved=Clay_GetElementData(dialog).boundingBox;
    CHECK(moved.x>=0 && moved.y>=0 && moved.x+moved.width<=800 && moved.y+moved.height<=600);
    Frame(ReleaseAt(move.mouseX,move.mouseY),body);CHECK(ui.activeId==0 && open);
    auto escape=MakeInput();escape.keyEscape=true;Frame(escape,body);CHECK(!open);
    open=true;Frame(MakeInput(),body);Frame(MakeInput(),body);
    moved=Clay_GetElementData(dialog).boundingBox;
    CHECK(fabsf(moved.x-initial.x)<0.1f && fabsf(moved.y-initial.y)<0.1f);
    auto bodyBox=Clay_GetElementData(CLAY_ID("DragModalBody")).boundingBox;
    Frame(PressAt(bodyBox.x+5,bodyBox.y+5),body);
    CHECK(ui.activeId!=title.id);
    Frame(ReleaseAt(bodyBox.x+5,bodyBox.y+5),body);
    auto close=Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalClose"),id.id);
    auto closeBox=Clay_GetElementData(close).boundingBox;
    Frame(PressAt(closeBox.x+5,closeBox.y+5),body);CHECK(ui.activeId!=title.id);
    Frame(ReleaseAt(closeBox.x+5,closeBox.y+5),body);CHECK(!open);
    Frame(MakeInput(),body);
    options.draggable=false;open=true;Frame(MakeInput(),body);Frame(MakeInput(),body);
    titleBox=Clay_GetElementData(title).boundingBox;
    initial=Clay_GetElementData(dialog).boundingBox;
    Frame(PressAt(titleBox.x+5,titleBox.y+5),body);
    move=PressAt(titleBox.x+65,titleBox.y+35);move.pointerPressed=false;Frame(move,body);
    moved=Clay_GetElementData(dialog).boundingBox;
    CHECK(moved.x==initial.x && moved.y==initial.y);
    Frame(ReleaseAt(move.mouseX,move.mouseY),body);
    Frame(PressAt(1,1),body);CHECK(!open);
}

static void TestKeyboardMenu() {
    auto menu=CLAY_ID("KeyboardFile");auto first=CLAY_ID("KeyboardNew"),second=CLAY_ID("KeyboardOpen");bool selected=false;
    auto body=[&]{if(ClayWidgets_BeginMenu(&ui,menu,CLAY_STRING("File"))){ClayWidgets_MenuItem(&ui,first,CLAY_STRING("New"));selected=ClayWidgets_MenuItem(&ui,second,CLAY_STRING("Open"));ClayWidgets_EndMenu(&ui,menu);}};
    Frame(MakeInput(),body);ui.focusedId=menu.id;auto enter=MakeInput();enter.keyEnter=true;Frame(enter,body);CHECK(ui.openMenuId==menu.id && ui.focusedId==first.id);
    auto down=MakeInput();down.keyDown=true;Frame(down,body);CHECK(ui.focusedId==second.id);
    Frame(enter,body);CHECK(selected && ui.openMenuId==0 && ui.overlayDepth==0);
}

static void TestNestedComposition() {
    Clay_String choices[]={CLAY_STRING("One"),CLAY_STRING("Two"),CLAY_STRING("Three")};
    char query[32]="tw";int32_t picked=0;auto search=CLAY_ID("SearchRegression");bool changed=false;
    auto searchBody=[&]{changed=ClayWidgets_SearchableCombo(&ui,search,CLAY_STRING("Search"),choices,3,&picked,query,sizeof(query));};
    Frame(MakeInput(),searchBody);ui.focusedId=search.id;ui.openComboId=search.id;ui.comboHighlightIndex=0;
    auto enter=MakeInput();enter.keyEnter=true;Frame(enter,searchBody);CHECK(changed && picked==1);
    strcpy(query,"no matches");ui.openComboId=search.id;Frame(MakeInput(),searchBody);CHECK(!ui.openComboId && picked==1);
    ResetUi();
    ui.animationsEnabled=true;
    auto table=CLAY_ID("WheelTable");int32_t order[100];bool selection[100]={};for(int i=0;i<100;++i)order[i]=i;
    ClayWidgets_TableState state={};ClayWidgets_TableColumn columns[]={{CLAY_STRING("Name"),CLAY_SIZING_FIXED(120)}};
    auto body=[&]{ClayWidgets_DataTable(&ui,table,columns,1,100,TableCell,nullptr,nullptr,order,selection,&state,{100,24,40,false});};
    Frame(MakeInput(),body);
    auto row=ClayWidgets__ChildId(table,CLAY_STRING("DataRow"),0);
    auto cell=ClayWidgets__ChildId(row,CLAY_STRING("DataCell"),0);
    auto box=Clay_GetElementData(cell).boundingBox;
    auto hover=MakeInput();hover.mouseX=box.x+5;hover.mouseY=box.y+5;Frame(hover,body);
    hover.scrollY=-2;Frame(hover,body);
    auto content=ClayWidgets__ChildId(table,CLAY_STRING("TableBody"),0);
    CHECK(Clay_GetScrollContainerData(content).scrollPosition->y<0);
    auto header=ClayWidgets__ChildId(table,CLAY_STRING("TableHeaderRow"),0);
    CHECK(Clay_GetElementData(header).boundingBox.y==ceilf(ui.theme.radiusMd*0.3f)); // header stays inside the frame while rows scroll
    auto track=Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarTrack"),content.id);
    auto trackBox=Clay_GetElementData(track).boundingBox;
    auto contentBox=Clay_GetElementData(content).boundingBox;
    CHECK(fabsf(contentBox.x+contentBox.width-trackBox.x-trackBox.width)<0.1f);
    CHECK(trackBox.width==CLAY_WIDGETS_SCROLLBAR_WIDTH+4);
    CHECK(fabsf(trackBox.y-contentBox.y)<0.1f && fabsf(trackBox.height-contentBox.height)<0.1f);

    ResetUi();auto outer=CLAY_ID("OuterModal"),inner=CLAY_ID("InnerModal");bool outerOpen=true,innerOpen=false;
    auto bodyId=CLAY_ID("OuterBodyButton");
    auto dialogs=[&]{if(ClayWidgets_BeginModal(&ui,outer,CLAY_STRING("Outer"),&outerOpen)){
        ClayWidgets_Button(&ui,bodyId,CLAY_STRING("Open nested"));
        if(ClayWidgets_BeginModalEx(&ui,inner,CLAY_STRING("Inner"),&innerOpen,{true,320}))ClayWidgets_EndModal(&ui,inner);
        ClayWidgets_EndModal(&ui,outer);}};
    Frame(MakeInput(),dialogs);ui.focusedId=bodyId.id;innerOpen=true;Frame(MakeInput(),dialogs);
    CHECK(ui.focusedId==Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsModalClose"),inner.id).id);
    auto escape=MakeInput();escape.keyEscape=true;Frame(escape,dialogs);
    CHECK(!innerOpen && outerOpen && ui.focusedId==bodyId.id && ui.overlayDepth==0);
    Frame(MakeInput(),dialogs);
    CHECK(ui.focusedId==bodyId.id);
}
