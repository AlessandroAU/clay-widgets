#include <cstdio>
#include <cstring>
#define CLAY_IMPLEMENTATION
#include "clay.h"
#define CLAY_WIDGETS_IMPLEMENTATION
#include "clay-widgets/widgets.h"
#include "backends/raylib/clay-raylib-renderer.h"
#include "assets/generated/embedded-font.h"

int main() {
    (void)RenderClayCommands;
    (void)HandleClayError;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320,200,"Backend regression tests");
    FontCache cache; cache.fallback=GetFontDefault();
    cache.ttfData=kEmbeddedRobotoTTF;cache.ttfSize=(int)kEmbeddedRobotoTTFSize;cache.haveEmbedded=true;
    int ascii[]={32,63,65}; int extended[]={32,63,65,0xE9};
    int failures=0,checks=0;
    auto check=[&](bool condition,const char *name){++checks;if(!condition){++failures;std::printf("FAIL: %s\n",name);}};
    check(FontCache_Register(cache,1,kEmbeddedRobotoTTF,(int)kEmbeddedRobotoTTFSize,ascii,3),"register ASCII face");
    check(FontCache_Register(cache,2,kEmbeddedRobotoTTF,(int)kEmbeddedRobotoTTFSize,extended,4),"register extended fallback");
    Font primary=*FontCache_Get(cache,16,1),fallback=*FontCache_Get(cache,16,2);
    check(primary.glyphCount==3 && fallback.glyphCount==4,"fontId selects independent atlases");
    Font selected=FontCache_Glyph(cache,1,16,0xE9);
    check(selected.texture.id==fallback.texture.id,"missing glyph selects fallback font");
    Clay_TextElementConfig config={};config.fontSize=16;config.fontId=1;
    auto measure=MeasureTextRaylib({3,"A\xC3\xA9","A\xC3\xA9"},&config,&cache);
    check(measure.width>0 && measure.height==16,"measure fallback text");
    BeginDrawing();ClearBackground(BLACK);
    auto drawn=FontCache_Text(cache,1,"A\xC3\xA9",3,16,0,{0,0},WHITE,true);
    EndDrawing();
    check(drawn.width==measure.width && drawn.height==measure.height,"draw and measurement agree");
    // Glyphs draw with their atlas padding, so padded rectangles must never
    // overlap. Hinting can round an outline a pixel taller than the requested
    // size (Roboto at 8 and 10 px), which the atlas rows have to allow for.
    bool separate=true,advances=true;
    for(int size=8;size<=16;++size){
        Font latin=ClayWidgets_BakeFont(kEmbeddedRobotoTTF,(int)kEmbeddedRobotoTTFSize,size);
        separate=separate && latin.glyphCount>150;
        for(int i=0;i<latin.glyphCount && separate;++i)for(int j=i+1;j<latin.glyphCount;++j){
            Rectangle a=latin.recs[i],b=latin.recs[j];float p=(float)latin.glyphPadding;
            if(a.width<=0||b.width<=0)continue;
            if(a.x-p<b.x+b.width+p && b.x-p<a.x+a.width+p && a.y-p<b.y+b.height+p && b.y-p<a.y+a.height+p){separate=false;break;}
        }
        int aIndex=GetGlyphIndex(latin,'a');
        advances=advances && latin.glyphs[aIndex].value=='a' && latin.glyphs[aIndex].advanceX>0;
        UnloadFont(latin);
    }
    check(separate,"padded glyphs do not overlap in the atlas");
    check(advances,"baked glyphs carry advances");
    char buffer[8]={};int length=0;AppendUtf8FromCodepoint(buffer,length,1,0xE9);check(length==0,"encoder rejects partial UTF8");
    AppendUtf8FromCodepoint(buffer,length,8,0xD800);check(length==0,"encoder rejects surrogate");
    AppendUtf8FromCodepoint(buffer,length,8,0x1F600);check(length==4,"encoder emits whole scalar");
    FontCache_Unload(cache);CloseWindow();
    std::printf("%d backend checks, %d failures\n",checks,failures);
    return failures?1:0;
}
