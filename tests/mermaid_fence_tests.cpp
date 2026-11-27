// Mermaid fences in a Markdown document, parsed the way the viewer parses
// them: each fence must render as a diagram or fall back to a code block
// exactly as tests/fixtures/mermaid-preamble.md says.
#include "document.h"
#include "mermaid.h"
#include "mermaid_ext.h"

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

mermaidext::Size fakeMeasure(const std::string& text,
                             const mermaidext::TextStyle& style, float) {
    return {7.0f * style.scale * static_cast<float>(text.size()), 16.0f * style.scale};
}

// The decision layoutCodeBlock (src/render.cpp) makes before drawing
bool rendersAsDiagram(const std::string& code) {
    mermaidext::Kind kind = mermaidext::detectKind(code);
    if (kind == mermaidext::Kind::Flowchart) {
        auto parsed = mermaid::parse(code);
        return parsed.success && !parsed.diagram.nodes.empty();
    }
    if (kind == mermaidext::Kind::None) return false;
    auto built = mermaidext::build(kind, code, fakeMeasure, 1.0f);
    return built.ok && !built.prims.empty();
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

} // namespace

int main() {
    std::ifstream file(TINTA_MERMAID_PREAMBLE_FIXTURE, std::ios::binary);
    check(static_cast<bool>(file), "fixture opens");
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    qmd::MarkdownParser parser;
    auto document = parseDocument(parser, text, "mermaid-preamble.md");
    check(document.success && document.root, "fixture parses");
    if (!document.root) return 1;

    std::vector<std::string> fences;
    collectMermaidFences(document.root, fences);
    const std::vector<bool> expected = {true, true, true, true, true, true, true, true, false};
    check(fences.size() == expected.size(),
          "fixture has " + std::to_string(expected.size()) + " Mermaid fences, found " +
              std::to_string(fences.size()));

    for (size_t i = 0; i < std::min(fences.size(), expected.size()); i++) {
        bool rendered = rendersAsDiagram(fences[i]);
        check(rendered == expected[i],
              "fence " + std::to_string(i + 1) +
                  (expected[i] ? " renders as a diagram" : " stays a code block"));
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All Mermaid fence tests passed\n";
    return 0;
}
