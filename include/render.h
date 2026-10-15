#ifndef TINTA_RENDER_H
#define TINTA_RENDER_H

#include "app.h"
#include "mermaid_ext.h"

// Full synchronous layout of the whole document
void layoutDocument(App& app);

// Lays out from the top through ~2 viewports past the current scroll, then
// returns so the first frame can present. If blocks remain, layoutComplete is
// false and the caller posts WM_APP_LAYOUT_CHUNK to continue.
void layoutDocumentViewportFirst(App& app);

// Continues an incomplete layout for at most budgetUs. Returns true when done.
bool layoutDocumentContinue(App& app, int64_t budgetUs);

// Synchronously finishes any incomplete/dirty layout (search, TOC, End key)
void ensureLayoutComplete(App& app);

// UI-thread completion for WM_APP_IMAGE_READY: takes ownership of the
// AsyncImageResult, updates the image cache, and triggers a reflow
void completeAsyncImage(App& app, void* asyncResult);
void discardAsyncImage(void* asyncResult);

// Theme color for a diagram primitive's role (shared with the exporters)
D2D1_COLOR_F resolveDiagramRole(const App& app, const mermaidext::Prim& prim,
                                mermaidext::Role role);

#ifdef TINTA_MERMAN
// A Mermaid diagram through merman in the active theme, for the exports
// (docs/linux-port/PLAN.md §6): the SVG (Mermaid's own "parity" SVG for
// browsers, else the resvg-safe one) and its size in CSS pixels. False when
// merman rejects the source; the native renderer is then the fallback.
#include <string>
bool mermanDiagramSvg(App& app, const std::string& source, bool forBrowsers, std::string& svg, float& width,
                      float& height);
#ifdef TINTA_MERMAN_PNG
// merman's own PNG at `scale` pixels per CSS pixel on `background`
// (Windows: Direct2D cannot draw the SVG's text)
bool mermanDiagramPng(App& app, const std::string& source, float scale, D2D1_COLOR_F background, std::string& png,
                      float& width, float& height);
#endif
#endif

#endif // TINTA_RENDER_H
