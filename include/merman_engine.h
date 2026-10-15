#ifndef TINTA_MERMAN_ENGINE_H
#define TINTA_MERMAN_ENGINE_H

// Mermaid rendering through merman (https://github.com/Latias94/merman), the
// headless Rust port of Mermaid, via its C ABI. Platform-free: this produces
// SVG; each platform turns it into pixels (librsvg on Linux, merman's PNG on
// Windows, where Direct2D's SVG renderer has no <text>, <marker> or <style>).

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace merman_engine {

// Host theme in merman's semantic roles ("canvas", "surface", "text",
// "line", "note-background", ...; PLAN.md §6.2). Colours are CSS strings.
// A diagram's own frontmatter or %%{init}%% still overrides these.
struct Theme {
    bool dark = false;
    std::vector<std::pair<std::string, std::string>> roles;
    std::vector<std::string> seriesPalette;
    std::string fontFamily;
    float fontSize = 16.0f;
};

struct Diagram {
    bool ok = false;
    std::string svg;
    // CSS pixels from the root viewBox: Mermaid writes width="100%"
    float width = 0.0f;
    float height = 0.0f;
    std::string error;  // merman's message when !ok
};

// Renders on a lazily created engine shared by all callers (serialized).
Diagram renderSvg(std::string_view source, const Theme& theme);

// For browsers (the HTML export): Mermaid's own SVG, with HTML labels in
// foreignObject and math as browsers draw it
Diagram renderParitySvg(std::string_view source, const Theme& theme);

// merman's own raster (resvg; the "png" Cargo feature): for Windows, where
// Direct2D's SVG renderer draws no <text>. `scale` pixels per CSS pixel on
// `background` (a CSS colour; empty is transparent). Empty on failure, with
// `error` set. Math labels are not lifted here (they are in renderSvg).
std::string renderPng(std::string_view source, const Theme& theme, float scale, const std::string& background,
                      std::string& error);

// The per-request options document: resvg-safe SVG (no foreignObject), no
// drop shadows (100x faster through librsvg), the host theme
std::string optionsJson(const Theme& theme);

}  // namespace merman_engine

#endif  // TINTA_MERMAN_ENGINE_H
