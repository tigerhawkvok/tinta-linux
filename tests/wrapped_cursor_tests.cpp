#include "d2d_init.h"
#include "editor.h"
#include "input.h"
#include "render.h"
#include "search.h"
#include "settings.h"
#include "utils.h"
#include <cmath>
#include <iostream>
#include <memory>

namespace {
int failures = 0, sequences = 0;
void check(bool ok, const char* message) {
    if (!ok) { if (failures < 25) std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void key(App& app, unsigned vk, bool shift = false, bool ctrl = false) {
    BYTE original[256], pressed[256];
    GetKeyboardState(original);
    memcpy(pressed,original,sizeof pressed);
    pressed[VK_CONTROL] = ctrl ? 0x80 : 0;
    pressed[VK_SHIFT] = shift ? 0x80 : 0;
    pressed[VK_MENU] = 0;
    SetKeyboardState(pressed);
    handleKeyDown(app,app.hwnd,vk);
    SetKeyboardState(original);
}
void place(App& app, size_t pos) {
    app.editorCursorPos=pos;
    app.editorCaretUpstreamPos=std::wstring::npos;
    app.editorDesiredCol=-1;
    app.editorHasSelection=false;
    app.editorSelStart=app.editorSelEnd=pos;
}
int caretRow(App& app) {
    D2D1_POINT_2F point{};
    check(editorCaretPoint(app,point),"caret geometry is available");
    float height=app.editorTextFormat->GetFontSize()*1.5f;
    return (int)std::floor((point.y+app.editorScrollY-chromeTopHeight(app)-dpi(app,8))/height+0.001f);
}
void paint(App& app) {
    app.cursorBlinkOn=true;
    app.renderTarget->BeginDraw();
    renderEditor(app,editorPaneWidth(app));
    check(SUCCEEDED(app.renderTarget->EndDraw()),"source renders with selection and caret");
}
void clusterBoundary(const App& app) {
    size_t pos=app.editorCursorPos;
    if (pos>0 && pos<app.editorText.size()) {
        wchar_t next=app.editorText[pos], previous=app.editorText[pos-1];
        check(!(previous>=0xd800 && previous<=0xdbff && next>=0xdc00 && next<=0xdfff),
              "vertical movement never splits an emoji surrogate pair");
        check(next!=0x0301,"vertical movement never splits a combining sequence");
    }
}
void paragraph(App& app, const wchar_t* marker, bool shift) {
    ++sequences;
    size_t start=app.editorText.find(marker), end=app.editorText.find(L'\n',start);
    check(start!=std::wstring::npos && end!=std::wstring::npos,"fixture paragraph exists");
    if (start==std::wstring::npos || end==std::wstring::npos) return;
    place(app,start); app.editorScrollY=app.editorScrollX=0;
    int row=caretRow(app), steps=0;
    while (app.editorCursorPos<=end && steps<100) {
        size_t before=app.editorCursorPos;
        key(app,VK_DOWN,shift); ++steps;
        check(app.editorCursorPos>before,"Down advances the insertion position from column zero");
        check(caretRow(app)==++row,"Down moves exactly one visual row");
        check(!shift || (app.editorSelStart==start && app.editorSelEnd==app.editorCursorPos),
              "Shift+Down retains its anchor and advances its endpoint");
        clusterBoundary(app);
        if (app.editorCursorPos==before) break;
    }
    check(app.editorCursorPos==end+1,"Down exits the wrapped paragraph at the next logical line");
    paint(app);
    D2D1_POINT_2F point{}; editorCaretPoint(app,point);
    check(point.y>=chromeTopHeight(app) && point.y<app.height,"scrolling keeps the caret visible");
    for (int n=0;n<steps;++n) {
        key(app,VK_UP,shift);
        check(caretRow(app)==--row,"Up moves exactly one visual row without skipping");
        clusterBoundary(app);
    }
    check(app.editorCursorPos==start,"Up reverses the traversal to paragraph start");
    check(!app.editorHasSelection,"reversing Shift selection back to its anchor clears it");
}
void edgeClicks(App& app) {
    // No whitespace: the right edge is an ambiguous insertion position shared
    // with the next row. A click must stay on the row that was clicked.
    auto source=app.editorText;
    app.editorText=std::wstring(500,L'x')+L"\nshort\n\nend";
    app.editorWordWrap=true; rebuildLineStarts(app);
    app.editorScrollY=app.editorScrollX=0; place(app,0);
    float height=app.editorTextFormat->GetFontSize()*1.5f;
    float textX=dpi(app,48)+editorGutterWidth(app);
    float width=editorPaneWidth(app)-textX-dpi(app,16);
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    app.dwriteFactory->CreateTextLayout(app.editorText.data(),500,app.editorTextFormat,
        width,1e7f,layout.GetAddressOf());
    layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    UINT32 count=0; layout->GetLineMetrics(nullptr,0,&count);
    std::vector<DWRITE_LINE_METRICS> rows(count);
    layout->GetLineMetrics(rows.data(),count,&count);
    float x=0,y=0; DWRITE_HIT_TEST_METRICS hit{};
    layout->HitTestTextPosition(rows[0].length-1,TRUE,&x,&y,&hit);
    app.lastClickTime={}; app.clickCount=0;
    handleEditorMouseDown(app,app.hwnd,(int)(textX+x),(int)(chromeTopHeight(app)+dpi(app,8)+height/2));
    handleEditorMouseUp(app,app.hwnd,0,0);
    check(app.editorCursorPos==rows[0].length && caretRow(app)==0,"right-edge click stays at end of clicked row");
    key(app,VK_SHIFT,true);
    check(caretRow(app)==0,"pressing Shift preserves the wrap side");
    key(app,VK_DOWN,true);
    check(caretRow(app)==1,"Down from a trailing wrap edge visits the very next row");
    key(app,VK_UP,true);
    check(caretRow(app)==0,"Up returns to the trailing edge without skipping");
    key(app,VK_LEFT);
    key(app,VK_RIGHT);
    check(caretRow(app)==1,"Right across a wrap boundary enters its next row");
    // Clicking/dragging to the left edge of a continuation row preserves it.
    app.lastClickTime={}; app.clickCount=0;
    handleEditorMouseDown(app,app.hwnd,(int)textX,(int)(chromeTopHeight(app)+dpi(app,8)+height*1.5f));
    check(caretRow(app)==1,"left-edge click stays at start of continuation row");
    handleEditorMouseMove(app,app.hwnd,(int)textX,(int)(chromeTopHeight(app)+dpi(app,8)+height*2.5f));
    handleEditorMouseUp(app,app.hwnd,0,0);
    check(caretRow(app)==2 && app.editorHasSelection,"dragging selects through the next wrapped row");
    // Preferred x survives short lines; Up/Down do not cycle at blank lines/EOF.
    place(app,500); int row=caretRow(app);
    key(app,VK_DOWN); check(caretRow(app)==row+1,"Down reaches a short logical line");
    key(app,VK_DOWN); check(caretRow(app)==row+2,"Down reaches an empty line");
    key(app,VK_UP); key(app,VK_UP);
    check(app.editorCursorPos==500 && caretRow(app)==row,"Up restores preferred x at a long line end");
    key(app,VK_END,false,true); size_t last=app.editorCursorPos;
    key(app,VK_DOWN); check(app.editorCursorPos==last,"Down at EOF remains stable");
    key(app,VK_HOME,false,true); key(app,VK_UP);
    check(app.editorCursorPos==0,"Up at BOF remains stable");
    key(app,VK_DOWN); size_t insert=app.editorCursorPos;
    auto before=app.editorText;
    handleCharInput(app,app.hwnd,L'Q');
    check(app.editorText==before.substr(0,insert)+L"Q"+before.substr(insert),"typing inserts at the reached wrap boundary");
    key(app,'Z',false,true);
    check(app.editorText==before && app.editorCursorPos==insert,"Undo restores text and the insertion position");
    app.editorText=source; rebuildLineStarts(app); app.editorDirty=false;
}
// The Read pill floats over the source's bottom-left corner: the caret
// line and the end of a long note must always clear it (#250)
void readPillClearance(App& app) {
    auto source=app.editorText;
    std::wstring text;
    for (int n=1;n<=120;++n) text+=L"Line "+std::to_wstring(n)+L" of a long note\n\n";
    app.editorText=text+L"Last line";
    const float pillTop=editorReadingButtonRect(app).top;
    const float height=app.editorTextFormat->GetFontSize()*1.5f;
    for (bool wrap : {true,false}) {
        ++sequences;
        app.editorWordWrap=wrap; rebuildLineStarts(app);
        app.editorScrollY=app.editorScrollX=0; place(app,0); paint(app);
        bool clear=true;
        for (size_t line=1;line<app.editorLineStarts.size();++line) {
            key(app,VK_DOWN);
            D2D1_POINT_2F point{};
            clear=clear && editorCaretPoint(app,point) && point.y+height<=pillTop;
        }
        check(clear,"walking down a long note keeps the caret line above the Read pill");
        app.editorScrollY=0; place(app,0); paint(app);
        for (int n=0;n<200;++n) handleEditorMouseWheel(app,app.hwnd,-3.0f);
        place(app,app.editorText.size());
        D2D1_POINT_2F point{};
        check(editorCaretPoint(app,point) && point.y+height<=pillTop,
              "scrolled to its end, the last line rises above the Read pill");
    }
    // The reading view floats the same pill, labelled Edit
    const float zoom=app.zoomFactor;
    setEditorReadingPreview(app,true);
    for (float level : {0.5f,1.0f,3.0f}) {
        ++sequences;
        app.zoomFactor=level; updateTextFormats(app); layoutDocument(app);
        float lastBottom=0.0f;
        for (const auto& rect : app.textRects) lastBottom=std::max(lastBottom,rect.rect.bottom);
        app.scrollY=std::max(0.0f,app.contentHeight-app.height);
        check(app.contentHeight>app.height,"the long note scrolls in the reading view");
        check(!app.textRects.empty() && lastBottom-app.scrollY<=editorReadingButtonRect(app).top,
              "scrolled to its end, the reading view's last line rises above the Edit pill");
    }
    setEditorReadingPreview(app,false);
    app.zoomFactor=zoom; updateTextFormats(app); app.scrollY=0;
    app.editorText=source; rebuildLineStarts(app); app.editorDirty=false;
    editorReparse(app); ensureLayoutComplete(app);
}
}
// Every match Enter steps to lands inside the source pane, with word wrap
// on and off, however deep into a long line it sits (#260)
void searchMatchesOnScreen(App& app) {
    if (app.editMode) exitEditMode(app);  // the cursor fixture is still open
    app.currentFile = TINTA_SEARCH_WRAP_FIXTURE;
    enterEditMode(app); editorReparse(app);
    check(app.editorText.find(L"latin number 8") != std::wstring::npos, "search fixture opens in the editor");
    for (int theme : {0,5}) for (float scale : {1.0f,1.5f}) {
        applyTheme(app,theme); app.contentScale=scale; updateTextFormats(app);
        for (int width : {650,1050}) for (bool preview : {true,false}) for (bool wrap : {true,false}) {
            app.width=width; app.editorShowPreview=preview;
            app.editorWordWrap=wrap; rebuildLineStarts(app);
            app.editorScrollY=app.editorScrollX=0; place(app,0); paint(app);
            openSearchInput(app);
            for (wchar_t ch : std::wstring(L"latin")) searchInputChar(app,app.hwnd,ch);
            check(app.editorSearchMatches.size()==17, "every latin in the fixture is a match");
            const float lineHeight=app.editorTextFormat->GetFontSize()*1.5f;
            for (size_t step=0; step<app.editorSearchMatches.size(); ++step) {
                if (step) key(app,VK_RETURN);
                paint(app);
                check(app.editorSearchCurrentIndex==(int)step, "Enter steps to the next match");
                D2D1_POINT_2F point{};
                check(editorCaretPoint(app,point), "the caret sits on the match");
                bool vertical = point.y>=chromeTopHeight(app) && point.y+lineHeight<=app.height;
                bool horizontal = point.x>=0 && point.x<=editorPaneWidth(app);
                if (!vertical || !horizontal) {
                    std::cerr << "  match " << step+1 << " at (" << point.x << ", " << point.y << ") wrap " << wrap
                              << " width " << width << " scale " << scale << " preview " << preview << '\n';
                }
                check(vertical, "the current match is on screen vertically");
                check(horizontal, "the current match is on screen horizontally");
            }
            closeSearchInput(app);
        }
    }
    exitEditMode(app);
}

// Rows each logical line actually wraps into at the current editor font,
// against the rows the editor reserved for it
bool rowsMatchFont(App& app, std::string& where) {
    const float textX=dpi(app,48)+editorGutterWidth(app);
    const float width=std::max(10.0f,editorPaneWidth(app)-textX-dpi(app,16));
    bool ok=app.editorRowStarts.size()==app.editorLineStarts.size()+1;
    for (size_t line=0; ok && line<app.editorLineStarts.size(); ++line) {
        size_t start=app.editorLineStarts[line];
        size_t end=line+1<app.editorLineStarts.size() ? app.editorLineStarts[line+1]-1 : app.editorText.size();
        UINT32 rows=1;
        if (end>start) {
            Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
            app.dwriteFactory->CreateTextLayout(app.editorText.data()+start,(UINT32)(end-start),
                app.editorTextFormat,width,1e7f,layout.GetAddressOf());
            layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            Microsoft::WRL::ComPtr<IDWriteTextLayout2> layout2;
            if (app.fontFallback && SUCCEEDED(layout.As(&layout2))) layout2->SetFontFallback(app.fontFallback);
            DWRITE_TEXT_METRICS metrics{};
            layout->GetMetrics(&metrics);
            rows=metrics.lineCount;
        }
        size_t reserved=app.editorRowStarts[line+1]-app.editorRowStarts[line];
        if (reserved!=rows) {
            where="line "+std::to_string(line+1)+": "+std::to_string(rows)+" rows drawn in "+
                  std::to_string(reserved)+" reserved";
            ok=false;
        }
    }
    return ok;
}

// #266: Ctrl+wheel over the source zooms its text. With word wrap on, the
// rows reserved per line were kept from the old font size, so a line that
// now wraps onto more rows was drawn over the next (and one that needs
// fewer left a gap)
void zoomKeepsWrappedRows(App& app) {
    if (app.editMode) exitEditMode(app);
    app.currentFile=TINTA_ZOOM_WRAP_FIXTURE;
    enterEditMode(app); editorReparse(app);
    check(app.editorText.find(L"\x6eda\x8f6e")!=std::wstring::npos,"zoom fixture opens in the editor");
    const float zoom=app.zoomFactor;
    BYTE original[256], pressed[256];
    GetKeyboardState(original);
    memcpy(pressed,original,sizeof pressed);
    pressed[VK_CONTROL]=0x80;
    for (int width : {650,1050}) for (bool preview : {true,false}) {
        ++sequences;
        app.width=width; app.editorShowPreview=preview;
        app.editorWordWrap=true; app.zoomFactor=1.0f; updateTextFormats(app);
        rebuildLineStarts(app); app.editorScrollY=app.editorScrollX=0; place(app,0); paint(app);
        std::string where;
        check(rowsMatchFont(app,where),"wrapped rows match the font before zooming");
        // Up in steps to 2.0x, then down to 0.6x; each tick as the user
        // makes it, then the zoom timer's catch-up, then a paint
        for (int step=0; step<24; ++step) {
            const short notches=step<10 ? WHEEL_DELTA : -WHEEL_DELTA;
            app.mouseX=(int)(editorPaneWidth(app)/2); app.mouseY=app.height/2;
            SetKeyboardState(pressed);
            handleMouseWheel(app,app.hwnd,MAKEWPARAM(0,notches),0);
            SetKeyboardState(original);
            if (app.zoomFactor!=app.appliedZoomFactor) updateTextFormats(app);  // TIMER_ZOOM_APPLY
            app.zoomApplyPending=false;
            paint(app);
            if (!rowsMatchFont(app,where)) {
                std::cerr << "  zoom " << app.zoomFactor << " width " << width << " preview " << preview
                          << ": " << where << '\n';
                check(false,"after a zoom every wrapped line keeps exactly the rows it is drawn in");
                break;
            }
        }
    }
    SetKeyboardState(original);
    app.zoomFactor=zoom; updateTextFormats(app);
    exitEditMode(app);
}

int runWrappedCursorTests() {
    auto state=std::make_unique<App>(); App& app=*state;
    app.hwnd=CreateWindowExW(0,L"STATIC",L"Wrapped cursor tests",WS_POPUP,0,0,1050,900,nullptr,nullptr,nullptr,nullptr);
    if (!app.hwnd || !initD2D(app) || !createRenderTarget(app)) return 2;
    app.width=1050; app.height=900; updateTextFormats(app);
    Settings settings; settings.keyProfile="windows"; applyKeymap(app,settings);
    app.currentFile=TINTA_WRAP_FIXTURE; enterEditMode(app); editorReparse(app); ensureLayoutComplete(app);
    check(app.tableCellRects.size()==6 && app.codeBlocks.size()==1,"mixed fixture retains its table and code");
    auto original=app.editorText;
    for (int theme : {0,5}) for (float scale : {1.0f,1.5f}) {
        applyTheme(app,theme); app.contentScale=scale; updateTextFormats(app);
        for (int width : {650,1050}) for (bool preview : {true,false}) {
            app.width=width; app.editorShowPreview=preview;
            for (bool wrap : {true,false}) for (bool shift : {false,true}) {
                app.editorWordWrap=wrap; rebuildLineStarts(app);
                for (const wchar_t* marker : {L"A long English",L"\x4e2d\x6587\x6bb5\x843d",L"Unicode clusters:"})
                    paragraph(app,marker,shift);
            }
            edgeClicks(app);
            readPillClearance(app);
        }
    }
    check(app.editorText==original,"navigation leaves the mixed Markdown unchanged");
    searchMatchesOnScreen(app);
    zoomKeepsWrappedRows(app);
    DestroyWindow(app.hwnd); app.hwnd=nullptr; state.reset(); CoUninitialize();
    std::cout << "Wrapped cursor: " << sequences << " sequences, " << failures << " failures\n";
    return failures ? 1 : 0;
}
