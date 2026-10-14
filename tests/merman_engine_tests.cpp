// merman_engine against the Markdown fixtures, parsed the way the viewer
// parses them: which fences render, the error path, and the host theme.
#include "document.h"
#include "merman_engine.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    failures++;
}

void collectMermaidFences(const qmd::ElementPtr& element, std::vector<std::string>& out) {
    if (element->type == qmd::ElementType::CodeBlock) {
        std::string language = element->language;
        std::transform(language.begin(), language.end(), language.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (language == "mermaid") {
            std::string code;
            for (const auto& child : element->children) {
                if (child->type == qmd::ElementType::Text) code += child->text;
            }
            out.push_back(code);
        }
    }
    for (const auto& child : element->children) collectMermaidFences(child, out);
}

std::vector<std::string> fencesOf(const char* path) {
    std::ifstream file(path, std::ios::binary);
    check(static_cast<bool>(file), std::string(path) + " opens");
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    qmd::MarkdownParser parser;
    auto document = parseDocument(parser, text, path);
    std::vector<std::string> fences;
    if (document.root) collectMermaidFences(document.root, fences);
    return fences;
}

void checkFixture(const char* path, const std::vector<bool>& expected) {
    auto fences = fencesOf(path);
    check(fences.size() == expected.size(),
          std::string(path) + ": expected " + std::to_string(expected.size()) +
              " Mermaid fences, found " + std::to_string(fences.size()));
    merman_engine::Theme theme;
    for (size_t i = 0; i < std::min(fences.size(), expected.size()); i++) {
        auto diagram = merman_engine::renderSvg(fences[i], theme);
        std::string where = std::string(path) + " fence " + std::to_string(i + 1);
        check(diagram.ok == expected[i],
              where + (expected[i] ? " renders" : " is rejected") +
                  (diagram.ok ? "" : " (merman: " + diagram.error + ")"));
        if (diagram.ok) {
            check(diagram.width > 0.0f && diagram.height > 0.0f, where + " has a size");
            check(diagram.svg.find("<foreignObject") == std::string::npos,
                  where + " has no foreignObject (resvg-safe)");
        } else {
            check(!diagram.error.empty(), where + " explains the failure");
        }
    }
}

void testTheme() {
    merman_engine::Theme theme;
    theme.dark = true;
    theme.roles = {{"surface", "#123456"}, {"text", "#fedcba"}, {"line", "#0a0b0c"}};
    theme.seriesPalette = {"#111111", "#222222"};
    theme.fontFamily = "Noto \"Sans\"";
    theme.fontSize = 15.0f;
    std::string json = merman_engine::optionsJson(theme);
    check(json.find("\"font_family\":\"Noto \\\"Sans\\\"\"") != std::string::npos,
          "font family is JSON-escaped");
    auto diagram = merman_engine::renderSvg("flowchart LR\n  A[Start] --> B[End]\n", theme);
    check(diagram.ok, "themed flowchart renders" + (diagram.ok ? "" : ": " + diagram.error));
    std::string svg = diagram.svg;
    std::transform(svg.begin(), svg.end(), svg.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    check(svg.find("#123456") != std::string::npos, "surface role reaches the SVG");
    check(svg.find("#0a0b0c") != std::string::npos, "line role reaches the SVG");
    check(svg.find("filter:url") == std::string::npos ||
              svg.find("filter: none !important") != std::string::npos,
          "drop shadows are switched off");
}

}  // namespace

int main() {
    checkFixture(TINTA_MERMAN_FIXTURE, {true, true, true, true, false});
    checkFixture(TINTA_MERMAID_PREAMBLE_FIXTURE,
                 {true, true, true, true, true, true, true, true, false});
    testTheme();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All merman engine tests passed\n";
    return 0;
}
