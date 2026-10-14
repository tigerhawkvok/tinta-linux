#include "merman_engine.h"

#include "merman.h"

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

private:
    MermanNativeApi api_{};
    MermanNativeEngineToken engine_ = 0;
    std::string error_;
};

}  // namespace

std::string optionsJson(const Theme& theme) {
    std::string json = "{\"svg\":{\"pipeline\":\"resvg-safe\"},"
                       "\"site_config\":{\"themeCSS\":\"* { filter: none !important; }\"},"
                       "\"presentation\":{\"theme\":{\"appearance\":";
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

Diagram renderSvg(std::string_view source, const Theme& theme) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    static Engine engine;
    return engine.svg(source, optionsJson(theme));
}

}  // namespace merman_engine
