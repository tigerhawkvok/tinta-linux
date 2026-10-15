#include "merman_engine.h"

#include "merman.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace merman_engine {
namespace {

MermanNativeSlice slice(const void* data, size_t length) {
    MermanNativeSlice s;
    s.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeSlice);
    s.data = static_cast<const uint8_t*>(data);
    s.len = length;
    return s;
}

std::string bufferText(const MermanNativeBuffer& buffer) {
    if (!buffer.data) return {};
    return std::string(reinterpret_cast<const char*>(buffer.data), buffer.len);
}

void appendJsonString(std::string& out, std::string_view text) {
    out += '"';
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char escaped[8];
                    std::snprintf(escaped, sizeof escaped, "\\u%04x", c);
                    out += escaped;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

// The "message" member of merman's failure JSON, unescaped enough to show
std::string failureMessage(const std::string& json) {
    size_t key = json.find("\"message\":\"");
    if (key == std::string::npos) return json;
    std::string message;
    for (size_t i = key + 11; i < json.size() && json[i] != '"'; i++) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            char next = json[++i];
            message += next == 'n' ? '\n' : next == 't' ? '\t' : next;
        } else {
            message += json[i];
        }
    }
    return message;
}

// Root viewBox "minX minY width height"
bool viewBoxSize(const std::string& svg, float& width, float& height) {
    size_t root = svg.find("<svg");
    if (root == std::string::npos) return false;
    size_t end = svg.find('>', root);
    size_t attr = svg.find("viewBox=\"", root);
    if (attr == std::string::npos || attr > end) return false;
    float minX, minY;
    return std::sscanf(svg.c_str() + attr + 9, "%f %f %f %f", &minX, &minY, &width, &height) == 4 &&
           width > 0.0f && height > 0.0f;
}

class Engine {
public:
    Engine() {
        MermanNativeApiRequest discovery{};
        discovery.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeApiRequest);
        discovery.expected_abi_version = MERMAN_NATIVE_ABI_VERSION;
        discovery.expected_minimum_prefix_layout_digest =
            slice(MERMAN_NATIVE_ABI_MINIMUM_PREFIX_LAYOUT_DIGEST,
                  std::strlen(MERMAN_NATIVE_ABI_MINIMUM_PREFIX_LAYOUT_DIGEST));
        api_.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeApi);
        if (merman_get_native_api(&discovery, &api_) != MERMAN_NATIVE_STATUS_OK) {
            error_ = "merman ABI discovery failed";
            return;
        }
        MermanNativeEngineConfig config{};
        config.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeEngineConfig);
        config.options_json = slice(nullptr, 0);
        MermanNativeResult result = MERMAN_NATIVE_RESULT_INIT;
        if (api_.engine_new(&config, &engine_, &result) != MERMAN_NATIVE_STATUS_OK) {
            error_ = failureMessage(bufferText(result.metadata_or_error_json));
            engine_ = 0;
        }
        api_.result_free(&result);
    }

    ~Engine() {
        if (engine_) api_.engine_try_close(engine_);
    }

    Diagram svg(std::string_view source, const std::string& options) {
        Diagram diagram;
        if (!engine_) {
            diagram.error = error_;
            return diagram;
        }
        MermanNativeOperationRequest request{};
        request.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeOperationRequest);
        request.operation = MERMAN_NATIVE_OPERATION_SVG;
        request.source = slice(source.data(), source.size());
        request.uri = slice(nullptr, 0);
        request.options_json = slice(options.data(), options.size());
        MermanNativeResult result = MERMAN_NATIVE_RESULT_INIT;
        if (api_.execute_collect(engine_, &request, &result) == MERMAN_NATIVE_STATUS_OK) {
            diagram.svg = bufferText(result.data);
            diagram.ok = viewBoxSize(diagram.svg, diagram.width, diagram.height);
            if (!diagram.ok) diagram.error = "merman SVG has no usable viewBox";
        } else {
            diagram.error = failureMessage(bufferText(result.metadata_or_error_json));
        }
        api_.result_free(&result);
        return diagram;
    }

    // PNG bytes (merman's resvg raster), or empty with `error` set
    std::string png(std::string_view source, const std::string& options, std::string& error) {
        if (!engine_) {
            error = error_;
            return {};
        }
        MermanNativeOperationRequest request{};
        request.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeOperationRequest);
        request.operation = MERMAN_NATIVE_OPERATION_PNG;
        request.source = slice(source.data(), source.size());
        request.uri = slice(nullptr, 0);
        request.options_json = slice(options.data(), options.size());
        MermanNativeResult result = MERMAN_NATIVE_RESULT_INIT;
        std::string bytes;
        if (api_.execute_collect(engine_, &request, &result) == MERMAN_NATIVE_STATUS_OK) {
            bytes = bufferText(result.data);
        } else {
            error = failureMessage(bufferText(result.metadata_or_error_json));
        }
        api_.result_free(&result);
        return bytes;
    }

private:
    MermanNativeApi api_{};
    MermanNativeEngineToken engine_ = 0;
    std::string error_;
};

}  // namespace

namespace {

std::string optionsJson(const Theme& theme, const char* pipeline) {
    std::string json = "{\"svg\":{\"pipeline\":\"";
    json += pipeline;
    json += "\"},\"site_config\":{\"themeCSS\":\"* { filter: none !important; }\"},";
#ifdef TINTA_MERMAN_MATH
    json += "\"environment\":{\"math_renderer\":\"ratex\"},";
#endif
    json += "\"presentation\":{\"theme\":{\"appearance\":";
    json += theme.dark ? "\"dark\"" : "\"light\"";
    if (!theme.fontFamily.empty()) {
        json += ",\"font_family\":";
        appendJsonString(json, theme.fontFamily);
    }
    if (theme.fontSize > 0.0f) {
        char size[32];
        std::snprintf(size, sizeof size, ",\"font_size\":\"%gpx\"", theme.fontSize);
        json += size;
    }
    if (!theme.roles.empty()) {
        json += ",\"roles\":{";
        for (size_t i = 0; i < theme.roles.size(); i++) {
            if (i) json += ',';
            appendJsonString(json, theme.roles[i].first);
            json += ':';
            appendJsonString(json, theme.roles[i].second);
        }
        json += '}';
    }
    if (!theme.seriesPalette.empty()) {
        json += ",\"series_palette\":[";
        for (size_t i = 0; i < theme.seriesPalette.size(); i++) {
            if (i) json += ',';
            appendJsonString(json, theme.seriesPalette[i]);
        }
        json += ']';
    }
    json += "}}}";
    return json;
}

// ---- Math labels -----------------------------------------------------------
// RaTeX draws each formula as an inline <svg> (sized in em) inside an HTML
// <foreignObject> label, which only a browser renders; the resvg-safe
// pipeline drops it, leaving the label empty or (sequence actors) showing
// a raw "$$...$$" text in its place. Each math-only label of the parity
// render is placed, by its absolute position, over the resvg-safe render,
// centred in the label box and painted in the theme's text colour. Labels
// mixing text and math keep the resvg-safe text without the formula.

std::string attribute(std::string_view tag, std::string_view name) {
    std::string key = " " + std::string(name) + "=\"";
    size_t at = tag.find(key);
    if (at == std::string_view::npos) return {};
    at += key.size();
    size_t end = tag.find('"', at);
    return end == std::string_view::npos ? std::string() : std::string(tag.substr(at, end - at));
}

float number(const std::string& text) {
    return text.empty() ? 0.0f : std::strtof(text.c_str(), nullptr);
}

// Sum of translate() functions; false for any other transform, whose
// formulas are then left out rather than misplaced
bool translation(const std::string& transform, float& dx, float& dy) {
    dx = dy = 0.0f;
    size_t at = 0;
    while (at < transform.size()) {
        if (std::isspace(static_cast<unsigned char>(transform[at]))) {
            at++;
            continue;
        }
        if (transform.compare(at, 10, "translate(") != 0) return false;
        size_t close = transform.find(')', at);
        if (close == std::string::npos) return false;
        std::string args = transform.substr(at + 10, close - at - 10);
        for (char& c : args) {
            if (c == ',') c = ' ';
        }
        float x = 0.0f, y = 0.0f;
        int read = std::sscanf(args.c_str(), "%f %f", &x, &y);
        if (read < 1) return false;
        dx += x;
        dy += read == 2 ? y : 0.0f;
        at = close + 1;
    }
    return true;
}

struct Formula {
    std::string viewBox, body;
    float widthEm = 0.0f, heightEm = 0.0f;
};

// The formulas of a math-only foreignObject label; empty when the label also
// carries plain text (it keeps the resvg-safe text fallback instead)
std::vector<Formula> formulas(std::string_view html) {
    std::vector<Formula> found;
    std::string rest;  // html with the formulas cut out
    size_t cursor = 0;
    for (size_t at = html.find("<svg"); at != std::string_view::npos; at = html.find("<svg", cursor)) {
        size_t open = html.find('>', at);
        size_t close = html.find("</svg>", at);
        if (open == std::string_view::npos || close == std::string_view::npos) return {};
        std::string_view tag = html.substr(at, open - at);
        std::string width = attribute(tag, "width");
        if (width.size() < 3 || width.compare(width.size() - 2, 2, "em") != 0) return {};
        Formula formula;
        formula.viewBox = attribute(tag, "viewBox");
        formula.widthEm = number(width);
        formula.heightEm = number(attribute(tag, "height"));
        formula.body = std::string(html.substr(open + 1, close - open - 1));
        found.push_back(std::move(formula));
        rest += html.substr(cursor, at - cursor);
        cursor = close + 6;
    }
    rest += html.substr(cursor);
    bool inTag = false;
    for (char c : rest) {
        if (c == '<') inTag = true;
        else if (c == '>') inTag = false;
        else if (!inTag && !std::isspace(static_cast<unsigned char>(c))) return {};
    }
    return found;
}

// Inline style on every formula shape: lifted into a node, the diagram's own
// stylesheet (.node path, .node rect: gradient fills, strokes) would
// otherwise restyle the glyphs, and CSS outranks presentation attributes
std::string pinGlyphStyle(const std::string& body, const std::string& colour) {
    const std::string style = " style=\"fill:" + colour + ";stroke:none;filter:none\"";
    std::string out;
    out.reserve(body.size() + 64);
    for (size_t i = 0; i < body.size(); i++) {
        out += body[i];
        if (body[i] != '<') continue;
        for (const char* shape : {"path", "rect", "polygon", "circle", "ellipse", "line", "polyline"}) {
            size_t n = std::strlen(shape);
            if (body.compare(i + 1, n, shape) == 0 && i + 1 + n < body.size() &&
                (body[i + 1 + n] == ' ' || body[i + 1 + n] == '/' || body[i + 1 + n] == '>')) {
                out.append(shape, n);
                out += style;
                i += n;
                break;
            }
        }
    }
    return out;
}

std::string themeTextColour(const Theme& theme) {
    for (const auto& role : theme.roles) {
        if (role.first == "text") return role.second;
    }
    return theme.dark ? "#e5e7eb" : "#333333";
}

struct PlacedLabel {
    float x, y, width, height;  // root user space
    bool edgeLabel;
    std::vector<Formula> parts;
};

// Every math-only foreignObject of `svg` with its absolute box. Tags are
// walked with a stack of accumulated translations; the HTML inside each
// foreignObject is read as a whole, never pushed.
std::vector<PlacedLabel> placedFormulas(const std::string& svg) {
    struct Frame {
        float x, y;
        bool placeable;
    };
    std::vector<Frame> stack;
    std::vector<PlacedLabel> placed;
    for (size_t at = svg.find('<'); at != std::string::npos;) {
        if (svg.compare(at, 4, "<!--") == 0 || svg.compare(at, 9, "<![CDATA[") == 0) {
            size_t end = svg.find(svg[at + 2] == '-' ? "-->" : "]]>", at);
            if (end == std::string::npos) break;
            at = svg.find('<', end);
            continue;
        }
        size_t end = svg.find('>', at);
        if (end == std::string::npos) break;
        if (svg[at + 1] == '?' || svg[at + 1] == '!') {
            at = svg.find('<', end);
            continue;
        }
        if (svg[at + 1] == '/') {
            if (!stack.empty()) stack.pop_back();
            at = svg.find('<', end);
            continue;
        }
        std::string_view tag(svg.data() + at, end - at);
        bool selfClosing = svg[end - 1] == '/';
        Frame frame = stack.empty() ? Frame{0.0f, 0.0f, true} : stack.back();
        float dx, dy;
        if (translation(attribute(tag, "transform"), dx, dy)) {
            frame.x += dx;
            frame.y += dy;
        } else {
            frame.placeable = false;
        }
        if (tag.compare(0, 14, "<foreignObject") == 0 && !selfClosing) {
            size_t close = svg.find("</foreignObject>", end);
            if (close == std::string::npos) break;
            std::string_view html(svg.data() + end + 1, close - end - 1);
            auto parts = formulas(html);
            float width = number(attribute(tag, "width"));
            float height = number(attribute(tag, "height"));
            if (frame.placeable && !parts.empty() && width > 0.0f && height > 0.0f) {
                placed.push_back({frame.x + number(attribute(tag, "x")),
                                  frame.y + number(attribute(tag, "y")), width, height,
                                  html.find("class=\"edgeLabel\"") != std::string_view::npos,
                                  std::move(parts)});
            }
            at = svg.find('<', close + 16);
            continue;
        }
        if (!selfClosing) stack.push_back(frame);
        at = svg.find('<', end);
    }
    return placed;
}

// Drops <text> elements whose whole content is $$...$$ formulas: resvg-safe
// leaves them where the browser would have shown the rendered math
std::string withoutRawMath(const std::string& svg) {
    std::string out;
    size_t cursor = 0;
    for (size_t at = svg.find("<text"); at != std::string::npos; at = svg.find("<text", cursor)) {
        size_t close = svg.find("</text>", at);
        if (close == std::string::npos) break;
        std::string content;
        bool inTag = false;
        for (size_t i = svg.find('>', at) + 1; i < close; i++) {
            if (svg[i] == '<') inTag = true;
            else if (svg[i] == '>') inTag = false;
            else if (!inTag) content += svg[i];
        }
        bool rawMath = false;
        size_t i = 0;
        while (i < content.size()) {
            if (std::isspace(static_cast<unsigned char>(content[i]))) {
                i++;
            } else if (content.compare(i, 2, "$$") == 0) {
                size_t end = content.find("$$", i + 2);
                if (end == std::string::npos) {
                    rawMath = false;
                    break;
                }
                rawMath = true;
                i = end + 2;
            } else {
                rawMath = false;
                break;
            }
        }
        out.append(svg, cursor, at - cursor);
        if (!rawMath) out.append(svg, at, close + 7 - at);
        cursor = close + 7;
    }
    out.append(svg, cursor, std::string::npos);
    return out;
}

// The math-only labels of `parity`, drawn over `safe` in a root group
std::string liftFormulas(const std::string& safe, const std::string& parity,
                         const std::string& colour) {
    std::vector<PlacedLabel> labels = placedFormulas(parity);
    if (labels.empty()) return safe;
    std::string lifted = "<g data-tinta-math-labels=\"1\">";
    for (const auto& label : labels) {
        float widestEm = 0.0f, totalEm = 0.0f;
        for (const auto& part : label.parts) {
            widestEm = std::max(widestEm, part.widthEm);
            totalEm += part.heightEm;
        }
        if (widestEm <= 0.0f || totalEm <= 0.0f) continue;
        // The label's font size: box heights fit the formulas exactly,
        // widths do not (node labels are a fixed CSS width)
        float px = label.height / totalEm;
        float y = label.y + (label.height - totalEm * px) / 2.0f;
        char buffer[256];
        // Edge labels sit on their line: the diagram's `.edgeLabel rect`
        // rule paints a bare rect in the theme's label background
        if (label.edgeLabel) {
            std::snprintf(buffer, sizeof buffer,
                          "<g class=\"edgeLabel\"><rect x=\"%g\" y=\"%g\" width=\"%g\" "
                          "height=\"%g\"/></g>",
                          label.x, label.y, label.width, label.height);
            lifted += buffer;
        }
        for (const auto& part : label.parts) {
            std::snprintf(buffer, sizeof buffer,
                          "<svg data-tinta-math=\"1\" x=\"%g\" y=\"%g\" width=\"%g\" "
                          "height=\"%g\" viewBox=\"%s\" overflow=\"visible\">",
                          label.x + (label.width - part.widthEm * px) / 2.0f, y,
                          part.widthEm * px, part.heightEm * px, part.viewBox.c_str());
            lifted += buffer;
            lifted += pinGlyphStyle(part.body, colour);
            lifted += "</svg>";
            y += part.heightEm * px;
        }
    }
    lifted += "</g>";
    std::string out = withoutRawMath(safe);
    size_t rootClose = out.rfind("</svg>");
    if (rootClose == std::string::npos) return safe;
    out.insert(rootClose, lifted);
    return out;
}

}  // namespace

std::string optionsJson(const Theme& theme) { return optionsJson(theme, "resvg-safe"); }

namespace {
// One engine for every caller, used one request at a time
std::mutex& engineMutex() {
    static std::mutex mutex;
    return mutex;
}
Engine& engine() {
    static Engine instance;
    return instance;
}
}  // namespace

Diagram renderSvg(std::string_view source, const Theme& theme) {
    std::lock_guard<std::mutex> lock(engineMutex());
    Diagram diagram = engine().svg(source, optionsJson(theme, "resvg-safe"));
#ifdef TINTA_MERMAN_MATH
    // Mermaid math is $$...$$ only
    if (diagram.ok && source.find("$$") != std::string_view::npos) {
        Diagram parity = engine().svg(source, optionsJson(theme, "parity"));
        if (parity.ok) {
            diagram.svg = liftFormulas(diagram.svg, parity.svg, themeTextColour(theme));
        }
    }
#endif
    return diagram;
}

Diagram renderParitySvg(std::string_view source, const Theme& theme) {
    std::lock_guard<std::mutex> lock(engineMutex());
    return engine().svg(source, optionsJson(theme, "parity"));
}

std::string renderPng(std::string_view source, const Theme& theme, float scale, const std::string& background,
                      std::string& error) {
    std::string options = optionsJson(theme, "resvg-safe");
    char raster[96];
    std::snprintf(raster, sizeof raster, "{\"raster\":{\"scale\":%g,\"background\":", scale);
    std::string group = raster;
    appendJsonString(group, background.empty() ? std::string("transparent") : background);
    group += "},";
    options.replace(0, 1, group);  // into the top-level object
    std::lock_guard<std::mutex> lock(engineMutex());
    return engine().png(source, options, error);
}

}  // namespace merman_engine
