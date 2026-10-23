#include "admonition.h"
#include "document.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    failures++;
}

// Admonitions in every dialect (admonition.h): the kind, keyword, title,
// fold and body each one parses to, nesting, offsets, and what stays text
std::string plainText(const qmd::ElementPtr& e) {
    if (!e) return {};
    if (e->type == qmd::ElementType::Text || e->type == qmd::ElementType::Code) {
        std::string out = e->text;
        for (const auto& c : e->children) out += plainText(c);
        return out;
    }
    if (e->type == qmd::ElementType::SoftBreak || e->type == qmd::ElementType::HardBreak) return " ";
    std::string out;
    for (const auto& c : e->children) {
        if (!out.empty() && c->type != qmd::ElementType::Text && c->type != qmd::ElementType::SoftBreak &&
            e->type != qmd::ElementType::Paragraph && e->type != qmd::ElementType::Emphasis &&
            e->type != qmd::ElementType::Strong) {
            out += " ";
        }
        out += plainText(c);
    }
    return out;
}

const qmd::ElementPtr* findBox(const qmd::ElementPtr& e) {
    if (!e) return nullptr;
    for (const auto& c : e->children) {
        if (c->type == qmd::ElementType::BlockQuote && c->alertKind != 0) return &c;
        if (const auto* inner = findBox(c)) return inner;
    }
    return nullptr;
}

int countBoxes(const qmd::ElementPtr& e) {
    int n = 0;
    for (const auto& c : e->children) n += (c->type == qmd::ElementType::BlockQuote && c->alertKind != 0) + countBoxes(c);
    return n;
}

void admonitionTests(qmd::MarkdownParser& parser) {
    struct Case {
        const char* name;
        std::string source;
        int kind;
        const char* keyword;
        const char* title;
        int fold;
        const char* body;
    };
    const Case cases[] = {
        {"GitHub alert", "> [!WARNING]\n> Mind the gap.\n", qmd::AdmonitionWarning, "warning", "", 0,
         "Mind the gap."},
        {"GitHub caution", "> [!CAUTION]\n> Careful.\n", qmd::AdmonitionCaution, "caution", "", 0, "Careful."},
        {"Obsidian title and fold", "> [!faq]- Why *fold*?\n> Because.\n", qmd::AdmonitionQuestion, "faq",
         "Why fold?", 2, "Because."},
        {"Obsidian open fold", "> [!example]+ Shown\n> Body.\n", qmd::AdmonitionExample, "example", "Shown", 1,
         "Body."},
        {"Obsidian unknown type", "> [!recipe] Soup\n> Water.\n", qmd::AdmonitionNote, "recipe", "Soup", 0,
         "Water."},
        {"GitLab title", "> [!note] Trailing words\n", qmd::AdmonitionNote, "note", "Trailing words", 0, ""},
        {"Docusaurus", ":::tip[Fast **path**]\n\nUse it.\n\n:::\n", qmd::AdmonitionTip, "tip", "Fast path", 0,
         "Use it."},
        {"Docusaurus, old title, no blank lines", ":::danger Take care\nHot.\n:::\n", qmd::AdmonitionDanger,
         "danger", "Take care", 0, "Hot."},
        {"Docusaurus attributes", ":::info[Read me]{#id .class}\nText.\n:::\n", qmd::AdmonitionInfo, "info",
         "Read me", 0, "Text."},
        {"VitePress", "::: details Click me\nHidden.\n:::\n", qmd::AdmonitionDetails, "details", "Click me", 2,
         "Hidden."},
        {"VitePress warning", "::: warning\nCareful.\n:::\n", qmd::AdmonitionWarning, "warning", "", 0, "Careful."},
        {"MyST colon fence", ":::{important}\nRead.\n:::\n", qmd::AdmonitionImportant, "important", "", 0,
         "Read."},
        {"MyST backtick fence", "```{seealso}\nOther page.\n```\n", qmd::AdmonitionNote, "seealso", "", 0,
         "Other page."},
        {"MyST admonition with a class", "```{admonition} Custom\n:class: tip\nBody.\n```\n", qmd::AdmonitionTip,
         "admonition", "Custom", 0, "Body."},
        {"MyST dropdown", ":::{note} Folded\n:class: dropdown\nInside.\n:::\n", qmd::AdmonitionNote, "note",
         "Folded", 2, "Inside."},
        {"MkDocs", "!!! note \"Heads up\"\n    Indented body.\n", qmd::AdmonitionNote, "note", "Heads up", 0,
         "Indented body."},
        {"MkDocs folded", "??? bug\n    Crash.\n", qmd::AdmonitionBug, "bug", "", 2, "Crash."},
        {"MkDocs open fold", "???+ example \"Try\"\n    Run it.\n", qmd::AdmonitionExample, "example", "Try", 1,
         "Run it."},
        {"MkDocs alias", "!!! tldr\n    Short.\n", qmd::AdmonitionAbstract, "tldr", "", 0, "Short."},
        {"reStructuredText", ".. warning::\n   Indented.\n", qmd::AdmonitionWarning, "warning", "", 0, "Indented."},
        {"reStructuredText inline start", ".. tip:: Inline start\n   and more.\n", qmd::AdmonitionTip, "tip", "", 0,
         "Inline start and more."},
        {"reStructuredText admonition", ".. admonition:: My title\n\n   Body.\n", qmd::AdmonitionNote, "admonition",
         "My title", 0, "Body."},
        {"Hugo alert", "{{< alert \"warning\" >}}\nHot.\n{{< /alert >}}\n", qmd::AdmonitionWarning, "warning", "", 0,
         "Hot."},
        {"Hugo Docsy", "{{% alert title=\"Watch\" color=\"danger\" %}}\nOops.\n{{% /alert %}}\n",
         qmd::AdmonitionDanger, "danger", "Watch", 0, "Oops."},
        {"Hugo on one line", "{{< tip >}}One line{{< /tip >}}\n", qmd::AdmonitionTip, "tip", "", 0, "One line"},
        {"HTML", "<div class=\"admonition danger\">\n<p class=\"admonition-title\">Stop</p>\n\nDo not.\n\n</div>\n",
         qmd::AdmonitionDanger, "danger", "Stop", 0, "Do not."},
    };
    for (const auto& c : cases) {
        auto parsed = parseDocument(parser, c.source, "notes.md");
        const qmd::ElementPtr* boxPtr = parsed.success ? findBox(parsed.root) : nullptr;
        if (!boxPtr) {
            std::cerr << "  case: " << c.name << '\n';
            check(false, "the admonition parses to a box");
            continue;
        }
        const qmd::ElementPtr& box = *boxPtr;
        const std::string title = box->alertTitle ? plainText(box->alertTitle) : std::string();
        std::string body;
        for (const auto& child : box->children) body += (body.empty() ? "" : " ") + plainText(child);
        const bool ok = box->alertKind == c.kind && box->alertKeyword == c.keyword && title == c.title &&
                        box->alertFold == c.fold && body == c.body;
        if (!ok) {
            std::cerr << "  case: " << c.name << " -> kind " << box->alertKind << ", keyword '" << box->alertKeyword
                      << "', title '" << title << "', fold " << box->alertFold << ", body '" << body << "'\n";
        }
        check(ok, "an admonition dialect parses to its kind, keyword, title, fold and body");
        const size_t marker = c.source[0] == '>' ? c.source.find("[!") : 0;
        check(box->sourceOffset == marker, "the box starts at its marker");
    }

    // A title-less MkDocs box keeps no title bar
    {
        auto parsed = parseDocument(parser, "!!! info \"\"\n    Plain.\n", "notes.md");
        const auto* box = findBox(parsed.root);
        check(box && (*box)->alertNoTitle, "!!! info \"\" has no title bar");
    }
    // Offsets point at the source: the body and the title
    {
        const std::string src = "Intro.\n\n:::tip[Fast **path**]\n\nUse it.\n\n:::\n";
        auto parsed = parseDocument(parser, src, "notes.md");
        const auto* box = findBox(parsed.root);
        check(box && (*box)->sourceOffset == src.find(":::tip"), "the box's offset is its marker line");
        check(box && !(*box)->children.empty() && (*box)->children[0]->sourceOffset == src.find("Use it."),
              "the body's offsets point into the source");
        check(box && (*box)->alertTitle && (*box)->alertTitle->sourceOffset == src.find("Fast"),
              "the title's offsets point into the source");
    }
    // Nesting, in each style
    {
        auto parsed = parseDocument(parser, "::::info Parent\n\n:::tip Child\nInner.\n:::\n\nOuter.\n\n::::\n",
                                    "notes.md");
        const auto* outer = findBox(parsed.root);
        check(outer && (*outer)->alertKind == qmd::AdmonitionInfo && (*outer)->children.size() == 2 &&
                  (*outer)->children[0]->alertKind == qmd::AdmonitionTip &&
                  plainText((*outer)->children[0]) == "Inner." && plainText((*outer)->children[1]) == "Outer.",
              "Docusaurus nests by colon count");
    }
    {
        auto parsed = parseDocument(parser, "> [!note] Outer\n> > [!warning] Inner\n> > Deep.\n", "notes.md");
        const auto* outer = findBox(parsed.root);
        check(outer && (*outer)->alertKind == qmd::AdmonitionNote && !(*outer)->children.empty() &&
                  (*outer)->children[0]->alertKind == qmd::AdmonitionWarning,
              "Obsidian nests with > >");
    }
    {
        auto parsed = parseDocument(parser, "!!! note\n    Outer.\n\n    !!! tip\n        Inner.\n\nAfter.\n",
                                    "notes.md");
        const auto* outer = findBox(parsed.root);
        check(outer && (*outer)->alertKind == qmd::AdmonitionNote && (*outer)->children.size() == 2 &&
                  (*outer)->children[1]->alertKind == qmd::AdmonitionTip &&
                  plainText((*outer)->children[1]) == "Inner.",
              "MkDocs nests by indentation");
        check(parsed.root->children.size() == 2 && plainText(parsed.root->children[1]) == "After.",
              "a dedent ends the MkDocs body");
    }
    // In a list item
    {
        auto parsed = parseDocument(parser, "- item\n\n  :::note\n  Inside.\n  :::\n", "notes.md");
        const auto* box = findBox(parsed.root);
        check(box && (*box)->parent && (*box)->parent->type == qmd::ElementType::ListItem &&
                  plainText(*box) == "Inside.",
              "a container in a list item stays in it");
    }
    // What stays as written
    {
        auto parsed = parseDocument(parser, "```\n:::note\nx\n:::\n!!! tip\n```\n", "notes.md");
        check(countBoxes(parsed.root) == 0 && !parsed.root->children.empty() &&
                  parsed.root->children[0]->type == qmd::ElementType::CodeBlock &&
                  plainText(parsed.root->children[0]).find(":::note") != std::string::npos,
              "markers inside fenced code stay code");
    }
    check(countBoxes(parseDocument(parser, ":::foo\nx\n:::\n", "notes.md").root) == 0,
          "an unknown ::: type stays text");
    check(countBoxes(parseDocument(parser, ".. code-block:: python\n   x = 1\n", "notes.md").root) == 0,
          "other reStructuredText directives stay text");
    check(countBoxes(parseDocument(parser, "> [!note]x\n", "notes.md").root) == 0,
          "[!note] must end at a space or the line's end");
    check(countBoxes(parseDocument(parser, "Text with :::note in it.\n", "notes.md").root) == 0,
          "::: mid-line is text");
    // Empty and unclosed
    {
        auto parsed = parseDocument(parser, ":::note[Empty]\n:::\n\nAfter.\n", "notes.md");
        const auto* box = findBox(parsed.root);
        check(box && (*box)->children.empty() && parsed.root->children.size() == 2, "an empty box has no body");
    }
    {
        auto parsed = parseDocument(parser, ":::warning\nNo end.\n\nStill inside.\n", "notes.md");
        const auto* box = findBox(parsed.root);
        check(box && (*box)->children.size() == 2, "an unclosed container runs to the end");
    }
    // A callout with a title only
    {
        auto parsed = parseDocument(parser, "> [!tip] Just a title\n", "notes.md");
        const auto* box = findBox(parsed.root);
        check(box && (*box)->children.empty() && (*box)->alertTitle && plainText((*box)->alertTitle) == "Just a title",
              "a callout can be a title alone");
    }
    // Labels
    check(qmd::admonitionLabel("seealso") == "See also" && qmd::admonitionLabel("tldr") == "TL;DR" &&
              qmd::admonitionLabel("warning") == "Warning",
          "default titles read as words");
}

} // namespace

int main() {
    check(isSupportedDocumentPath("notes.md"), ".md is supported");
    check(isSupportedDocumentPath("notes.MARKDOWN"), ".markdown is case-insensitive");
    check(isSupportedDocumentPath(L"diagram.MMD"), ".mmd is case-insensitive");
    check(isMermaidDocumentPath("diagram.mmd"), ".mmd is detected as Mermaid");
    check(!isMermaidDocumentPath("notes.md"), ".md is not detected as Mermaid");
    check(!isSupportedDocumentPath("diagram.mmdd"), "similar extensions are rejected");
    check(isSupportedDocumentPath("notes.txt"), ".txt opens as a plain-text document");
    check(isSupportedDocumentPath("data.JSON"), ".json is case-insensitive");
    check(isPlainTextDocumentPath(L"config.yaml"), ".yaml is plain text");
    check(!isPlainTextDocumentPath("notes.md"), ".md is not plain text");
    check(isSupportedDropPath(L"notes.txt"), "existing .txt drag-and-drop remains supported");

    qmd::MarkdownParser parser;
    auto mermaid = parseDocument(
        parser, "flowchart LR\nA --> B\n", "diagram.mmd");
    check(mermaid.success, "Mermaid document is created");
    check(mermaid.root && mermaid.root->children.size() == 1,
          "Mermaid document has one diagram element");
    if (mermaid.root && mermaid.root->children.size() == 1) {
        check(mermaid.root->children[0]->type == qmd::ElementType::MermaidDiagram,
              ".mmd content becomes a Mermaid diagram element");
    }

    // Standalone PlantUML documents open as a plantuml code block so the
    // fence render paths (preview, HTML, DOCX) pick them up unchanged
    check(isPlantUmlDocumentPath("diagram.puml"), ".puml is detected as PlantUML");
    check(isPlantUmlDocumentPath(L"diagram.PUML"), ".PUML is case-insensitive");
    check(isPlantUmlDocumentPath(L"diagram.PlantUML"), ".PlantUML is case-insensitive");
    check(!isPlantUmlDocumentPath("notes.md"), ".md is not detected as PlantUML");
    check(!isPlainTextDocumentPath("diagram.puml"), ".puml is not plain text");
    check(isSupportedDocumentPath("diagram.puml"), ".puml is supported");
    check(isSupportedDocumentPath(L"diagram.plantuml"), ".plantuml is supported");
    check(!isSupportedDocumentPath("diagram.puml2"), "similar extensions are rejected");
    check(!isPlantUmlDocumentPath("diagram.puml2"), ".puml2 is not detected");
    check(isSupportedDropPath(L"diagram.puml"), ".puml drag-and-drop is supported");

    auto puml = parseDocument(parser, "@startuml\nA -> B\n@enduml\n", "diagram.puml");
    check(puml.success, "PlantUML document is created");
    check(puml.root && puml.root->children.size() == 1,
          "PlantUML document has one block");
    if (puml.root && puml.root->children.size() == 1) {
        const auto& block = puml.root->children[0];
        check(block->type == qmd::ElementType::CodeBlock,
              ".puml content becomes a code block");
        check(block->language == "plantuml",
              "code block carries the plantuml language tag");
        check(!block->children.empty() &&
                  block->children[0]->text == "@startuml\nA -> B\n@enduml\n",
              "PlantUML source is preserved verbatim");
    }
    auto plantuml = parseDocument(
        parser, "@startuml\nAlice -> Bob\n@enduml\n", L"diagram.plantuml");
    check(plantuml.success && plantuml.root &&
              plantuml.root->children.size() == 1 &&
              plantuml.root->children[0]->type == qmd::ElementType::CodeBlock &&
              plantuml.root->children[0]->language == "plantuml",
          ".plantuml parses through the wide-path overload");

    // Obsidian/Typora inline extensions
    auto ext = parseDocument(parser,
        "before ==mark 中文== mid x^2^ and H~2~O ~~gone~~ `==not this==`\n", "notes.md");
    check(ext.success, "extension test parses");
    if (ext.success && !ext.root->children.empty()) {
        const auto& para = ext.root->children[0];
        int highlights = 0, sups = 0, subs = 0, codeIntact = 0, strikes = 0;
        for (const auto& child : para->children) {
            if (child->type == qmd::ElementType::Highlight) {
                highlights++;
                check(!child->children.empty() &&
                      child->children[0]->text == "mark \xe4\xb8\xad\xe6\x96\x87",
                      "highlight content preserved incl. CJK");
            }
            if (child->type == qmd::ElementType::Superscript) {
                sups++;
                check(!child->children.empty() && child->children[0]->text == "2",
                      "superscript content preserved");
            }
            if (child->type == qmd::ElementType::Subscript) subs++;
            if (child->type == qmd::ElementType::Strikethrough) {
                strikes++;
                check(!child->children.empty() && child->children[0]->text == "gone",
                      "strikethrough content preserved");
            }
            if (child->type == qmd::ElementType::Code) {
                codeIntact++;
                check(!child->children.empty() &&
                      child->children[0]->text == "==not this==",
                      "code spans are not transformed");
            }
        }
        check(highlights == 1, "one ==highlight== parsed");
        check(sups == 1, "one ^sup^ parsed");
        check(subs == 1, "one ~sub~ parsed");
        check(strikes == 1, "one ~~strike~~ parsed");
        check(codeIntact == 1, "inline code untouched");
    }

    // GitHub alerts
    auto alert = parseDocument(parser, "> [!NOTE]\n> Body text here.\n", "notes.md");
    check(alert.success, "alert blockquote parses");
    if (alert.success && !alert.root->children.empty()) {
        const auto& quote = alert.root->children[0];
        check(quote->type == qmd::ElementType::BlockQuote, "alert stays a blockquote");
        check(quote->alertKind == 1, "[!NOTE] detected as alert kind 1");
        check(!quote->children.empty() &&
              !quote->children[0]->children.empty() &&
              quote->children[0]->children[0]->text == "Body text here.",
              "alert marker stripped, body preserved");
    }
    auto caution = parseDocument(parser, "> [!caution]\n> Careful.\n", "notes.md");
    check(caution.success && !caution.root->children.empty() &&
          caution.root->children[0]->alertKind == 5,
          "alert markers match case-insensitively");
    // Text after the marker is the callout's title (Obsidian, GitLab), not
    // a plain quote as on github.com
    auto titled = parseDocument(parser, "> [!NOTE] trailing words\n", "notes.md");
    check(titled.success && !titled.root->children.empty() &&
          titled.root->children[0]->alertKind == 1 && titled.root->children[0]->alertTitle,
          "marker with trailing text on the same line is a titled callout");
    admonitionTests(parser);

    // Inline <br> becomes a hard break (#45)
    auto br = parseDocument(parser, "line one<br>line two<BR/>line three<br />end\n", "notes.md");
    check(br.success, "inline br document parses");
    if (br.success && !br.root->children.empty()) {
        int hardBreaks = 0;
        int literalBr = 0;
        for (const auto& child : br.root->children[0]->children) {
            if (child->type == qmd::ElementType::HardBreak) hardBreaks++;
            if (child->type == qmd::ElementType::Text &&
                child->text.find("<br") != std::string::npos) literalBr++;
        }
        check(hardBreaks == 3, "all three <br> variants become hard breaks");
        check(literalBr == 0, "no literal <br> text remains");
    }

    // Exported documents from office/AI tools often wrap all visible content
    // in presentational <font> tags. They must not become literal table text.
    {
        auto styledTable = parseDocument(parser,
            "# 示例测试文档\n"
            "**<font style=\"color:#000000;\">文档版本</font>**：V1.0\n\n"
            "| **<font style=\"color:rgb(0, 0, 0);\">序号</font>** | "
            "**<font style=\"color:rgb(0, 0, 0);\">功能项</font>** | "
            "**<font style=\"color:rgb(0, 0, 0);\">测试状态</font>** | "
            "**<font style=\"color:rgb(0, 0, 0);\">备注</font>** |\n"
            "| --- | --- | --- | --- |\n"
            "| <font style=\"color:rgb(0, 0, 0);\">1</font> | "
            "<font style=\"color:#000000;\">检查输入框。</font> | "
            "<font style=\"color:#000000;\">正常</font> |  |\n"
            "| <font style=\"color:rgb(0, 0, 0);\">2</font> | "
            "<font style=\"color:#000000;\">提交示例数据。</font> | "
            "<font style=\"color:#000000;\">异常</font> | "
            "<font style=\"color:#000000;\">截图</font>![](https://example.test/error.png) |\n"
            "| <font style=\"color:rgb(0, 0, 0);\">3</font> | "
            "<font style=\"color:#000000;\">空状态占位。</font> | "
            "<font style=\"color:#000000;\">正常</font> | <br/> |\n",
            "sample.md");
        check(styledTable.success, "font-wrapped Chinese table parses");
        bool sawTable = false;
        bool sawLiteralFont = false;
        bool sawHeader = false;
        bool sawImage = false;
        bool sawHardBreak = false;
        std::function<void(const qmd::ElementPtr&)> walk =
            [&](const qmd::ElementPtr& node) {
            if (node->type == qmd::ElementType::Table) sawTable = true;
            if (node->type == qmd::ElementType::Image) sawImage = true;
            if (node->type == qmd::ElementType::HardBreak) sawHardBreak = true;
            if (node->type == qmd::ElementType::Text) {
                if (node->text.find("<font") != std::string::npos ||
                    node->text.find("</font>") != std::string::npos) {
                    sawLiteralFont = true;
                }
                if (node->text == "序号") sawHeader = true;
            }
            for (const auto& child : node->children) walk(child);
        };
        if (styledTable.root) walk(styledTable.root);
        check(sawTable, "font-wrapped content remains a Markdown table");
        check(sawHeader, "font-wrapped table header text is preserved");
        check(sawImage, "remote images inside table cells are preserved");
        check(sawHardBreak, "HTML line breaks inside table cells are preserved");
        check(!sawLiteralFont, "font tags are hidden from rendered text");
    }

    // YAML frontmatter is hidden instead of rendering as a setext heading (#61)
    auto fm = parseDocument(parser,
        "---\ntitle: 'Barometer'\naliases:\ntags: [Barometer]\n---\n\n"
        "[Link](https://example.com)\n", "notes.md");
    check(fm.success, "frontmatter document parses");
    if (fm.success) {
        bool sawHeading = false, sawTitleText = false;
        std::function<void(const qmd::ElementPtr&)> walk = [&](const qmd::ElementPtr& e) {
            if (e->type == qmd::ElementType::Heading) sawHeading = true;
            if (e->type == qmd::ElementType::Text &&
                e->text.find("title:") != std::string::npos) sawTitleText = true;
            for (const auto& ch : e->children) walk(ch);
        };
        walk(fm.root);
        check(!sawHeading, "frontmatter does not become a heading");
        check(!sawTitleText, "frontmatter keys are not rendered");
        bool sawLink = false;
        std::function<void(const qmd::ElementPtr&)> findLink = [&](const qmd::ElementPtr& e) {
            if (e->type == qmd::ElementType::Link) sawLink = true;
            for (const auto& ch : e->children) findLink(ch);
        };
        findLink(fm.root);
        check(sawLink, "content after frontmatter renders");
    }
    auto unclosed = parseDocument(parser, "---\ntitle: x\nno closing fence\n", "notes.md");
    check(unclosed.success && !unclosed.root->children.empty(),
          "unclosed fence renders as ordinary markdown");
    auto hrDoc = parseDocument(parser, "para\n\n---\n\nafter\n", "notes.md");
    bool hrSurvives = false;
    if (hrDoc.success) {
        for (const auto& ch : hrDoc.root->children) {
            if (ch->type == qmd::ElementType::HorizontalRule) hrSurvives = true;
        }
    }
    check(hrSurvives, "a mid-document --- is still a horizontal rule");

    auto markdown = parseDocument(parser, "# Heading\n", "notes.md");
    check(markdown.success, "Markdown document still parses");
    check(markdown.root && !markdown.root->children.empty() &&
          markdown.root->children[0]->type == qmd::ElementType::Heading,
          ".md content keeps Markdown parsing");

    // Plain-text .md references become fileref: links (#127)
    {
        auto refs = parseDocument(parser,
            "See docs/auth.md and ./setup.md plus C:\\notes\\a.md here.\n"
            "Not these: `code.md` or https://x.com/y.md or file.mdx or "
            "bare .md or v2.md.bak\n"
            "Sentence ends with index.md.\n",
            "notes.md");
        check(refs.success, "fileref test parses");
        std::vector<std::string> found;
        std::function<void(const qmd::ElementPtr&)> walk =
            [&](const qmd::ElementPtr& node) {
            if (node->type == qmd::ElementType::Link &&
                node->url.rfind("fileref:", 0) == 0) {
                found.push_back(node->url.substr(8));
            }
            if (node->type != qmd::ElementType::Code &&
                node->type != qmd::ElementType::CodeBlock) {
                for (const auto& child : node->children) walk(child);
            }
        };
        if (refs.root) walk(refs.root);
        check(found.size() == 4, "exactly the four real references detected");
        auto has = [&](const char* token) {
            for (const auto& item : found) {
                if (item == token) return true;
            }
            return false;
        };
        check(has("docs/auth.md"), "bare relative path detected");
        check(has("./setup.md"), "dot-relative path detected");
        check(has("C:\\notes\\a.md"), "absolute path detected");
        check(has("index.md"), "sentence-ending path detected sans period");
        check(!has("code.md"), "code spans are left alone");
        check(!has("y.md"), "URLs are left alone");
        check(!has("file.mdx"), "longer extensions are not references");
        check(!has(".md"), "a bare extension is not a reference");
        check(!has("v2.md"), "dotted archive names are not references");
    }

    // Plain-text documents become one highlighted code block
    {
        auto json = parseDocument(
            parser, "{\n  \"key\": true\n}\n", "data.json");
        check(json.success, "json document parses");
        check(json.root && json.root->children.size() == 1,
              "json document is a single block");
        if (json.root && json.root->children.size() == 1) {
            const auto& block = json.root->children[0];
            check(block->type == qmd::ElementType::CodeBlock,
                  "json content becomes a code block");
            check(block->language == "json",
                  "json block carries its language tag");
            check(!block->children.empty() &&
                      block->children[0]->text.find("\"key\"") !=
                          std::string::npos,
                  "json content is preserved verbatim");
        }
        auto txt = parseDocument(parser, "# not a heading\n", "notes.txt");
        check(txt.success && txt.root && !txt.root->children.empty() &&
                  txt.root->children[0]->type ==
                      qmd::ElementType::CodeBlock,
              ".txt content stays literal instead of parsing as markdown");
    }

    // Text-file references are detected like .md ones (#7)
    {
        auto refs = parseDocument(parser,
            "Check config.yaml and data/out.json plus error.log here.\n",
            "notes.md");
        check(refs.success, "text-file ref test parses");
        std::vector<std::string> found;
        std::function<void(const qmd::ElementPtr&)> walk =
            [&](const qmd::ElementPtr& node) {
            if (node->type == qmd::ElementType::Link &&
                node->url.rfind("fileref:", 0) == 0) {
                found.push_back(node->url.substr(8));
            }
            if (node->type != qmd::ElementType::Code &&
                node->type != qmd::ElementType::CodeBlock) {
                for (const auto& child : node->children) walk(child);
            }
        };
        if (refs.root) walk(refs.root);
        check(found.size() == 3, "all three text-file references detected");
    }

    // Emoji shortcodes become their emoji, code stays literal
    {
        auto emoji = parseDocument(parser,
            "Ship it :rocket: with :+1: but `:tada:` stays and "
            ":not_a_real_code: survives\n"
            "```\n:fire: in a fence\n```\n",
            "notes.md");
        check(emoji.success, "emoji test parses");
        std::string flat;
        std::string codeFlat;
        std::function<void(const qmd::ElementPtr&)> walk =
            [&](const qmd::ElementPtr& node) {
            if (node->type == qmd::ElementType::Code ||
                node->type == qmd::ElementType::CodeBlock) {
                codeFlat += node->text;
                for (const auto& child : node->children)
                    codeFlat += child->text;
                return;
            }
            flat += node->text;
            for (const auto& child : node->children) walk(child);
        };
        if (emoji.root) walk(emoji.root);
        check(flat.find("\xF0\x9F\x9A\x80") != std::string::npos,
              ":rocket: replaced with the emoji");
        check(flat.find("\xF0\x9F\x91\x8D") != std::string::npos,
              ":+1: replaced with the emoji");
        check(flat.find(":rocket:") == std::string::npos,
              "no literal :rocket: remains");
        check(flat.find(":not_a_real_code:") != std::string::npos,
              "unknown shortcodes stay literal");
        check(codeFlat.find(":tada:") != std::string::npos,
              "inline code shortcodes untouched");
        check(codeFlat.find(":fire:") != std::string::npos,
              "fenced code shortcodes untouched");
    }

    // md4c sends line breaks and code indents from its own string literals,
    // not the input: blocks opening with one keep in-source offsets, in order
    {
        const std::string md =
            "[A link](https://example.com)\nends the first line.\n\n"
            "**Bold**\\\nthen a hard break.\n\n"
            "- `code`\n  wraps in a tight item.\n\n"
            "> *Quoted*\n> wraps.\n\n"
            "```\n\nblank first line\n```\n\n"
            "```\n    indented first line\n```\n\n"
            "Last paragraph.\n";
        auto doc = parseDocument(parser, md, "notes.md");
        check(doc.success && doc.root && doc.root->children.size() == 7,
              "soft-break offset document parses into seven blocks");
        bool inSource = true;
        std::function<void(const qmd::ElementPtr&)> walk = [&](const qmd::ElementPtr& node) {
            if (node->sourceOffset != SIZE_MAX && node->sourceOffset >= md.size()) inSource = false;
            for (const auto& child : node->children) walk(child);
        };
        // The block's own offset, else its first descendant's (as layout's scroll anchors)
        std::function<size_t(const qmd::ElementPtr&)> firstIn = [&](const qmd::ElementPtr& node) {
            if (node->sourceOffset != SIZE_MAX) return node->sourceOffset;
            for (const auto& child : node->children) {
                const size_t offset = firstIn(child);
                if (offset != SIZE_MAX) return offset;
            }
            return SIZE_MAX;
        };
        if (doc.root) {
            walk(doc.root);
            size_t previous = 0;
            bool ordered = true;
            for (const auto& block : doc.root->children) {
                const size_t offset = firstIn(block);
                if (offset == SIZE_MAX || offset < previous) ordered = false;
                previous = offset;
            }
            check(ordered, "every block maps to its own place in the source, in order");
            if (doc.root->children.size() == 7) {
                // Each sits on its first rendered character's source: the
                // line break, the blank line, the indent
                check(doc.root->children[0]->sourceOffset == md.find("\nends the first line"),
                      "a paragraph opening with a soft break starts at that break");
                check(doc.root->children[4]->sourceOffset == md.find("\nblank first line"),
                      "a code block opening with a blank line starts at that line");
                check(doc.root->children[5]->sourceOffset == md.find("    indented first line"),
                      "a code block opening with an indent starts at that indent");
            }
        }
        check(inSource, "no source offset points outside the source");
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All document tests passed\n";
    return 0;
}
