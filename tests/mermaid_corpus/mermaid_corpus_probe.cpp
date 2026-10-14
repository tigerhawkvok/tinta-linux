// Runs Mermaid sources through Tinta's native parsers and layout the way
// render.cpp does (detectKind, then mermaid::parse + layout or
// mermaidext::build) and prints one TSV row per diagram:
//   path  index  kind  status  detail  header
// status: ok | unrecognized | fail | crash | timeout
// .mmd files are one diagram; .md files contribute every fence whose info
// string's first word is in the accepted set (default: mermaid).
#include "mermaid.h"
#include "mermaid_ext.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static const char* kindName(mermaidext::Kind k) {
    using K = mermaidext::Kind;
    switch (k) {
        case K::None: return "none";
        case K::Flowchart: return "flowchart";
        case K::Sequence: return "sequence";
        case K::Class: return "class";
        case K::State: return "state";
        case K::Er: return "er";
        case K::Pie: return "pie";
        case K::Git: return "gitgraph";
        case K::Gantt: return "gantt";
        case K::Mindmap: return "mindmap";
        case K::Timeline: return "timeline";
        case K::Journey: return "journey";
        case K::Quadrant: return "quadrant";
        case K::XyChart: return "xychart";
        case K::Packet: return "packet";
        case K::Kanban: return "kanban";
        case K::Treemap: return "treemap";
        case K::Radar: return "radar";
        case K::Sankey: return "sankey";
        case K::Block: return "block";
        case K::Requirement: return "requirement";
        case K::C4: return "c4";
        case K::Architecture: return "architecture";
    }
    return "?";
}

static size_t utf8Chars(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) n += (c & 0xC0) != 0x80;
    return n;
}

static mermaidext::Size measure(const std::string& text,
                                const mermaidext::TextStyle& style,
                                float wrap) {
    float widest = 0.0f;
    int lines = 0;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        float w = 7.0f * style.scale * static_cast<float>(utf8Chars(line));
        int rows = 1;
        if (wrap > 0.0f && w > wrap) {
            rows = static_cast<int>(std::ceil(w / wrap));
            w = wrap;
        }
        widest = std::max(widest, w);
        lines += rows;
    }
    if (lines == 0) lines = 1;
    return {widest, 16.0f * style.scale * static_cast<float>(lines)};
}

static std::string oneLine(std::string s) {
    for (char& c : s) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    if (s.size() > 120) s = s.substr(0, 120);
    return s;
}

static std::string headerLine(const std::string& src) {
    std::stringstream ss(src);
    std::string line;
    while (std::getline(ss, line)) {
        size_t a = line.find_first_not_of(" \t\r");
        if (a == std::string::npos) continue;
        return oneLine(line.substr(a));
    }
    return "";
}

// Mirrors render.cpp: layoutMermaidDiagram / layoutMermaidExtDiagram
static void probeOne(const std::string& src, std::string& kind,
                     std::string& status, std::string& detail) {
    mermaidext::Kind k = mermaidext::detectKind(src);
    kind = kindName(k);
    if (k == mermaidext::Kind::None) {
        status = "unrecognized";
        return;
    }
    if (k == mermaidext::Kind::Flowchart) {
        auto parsed = mermaid::parse(src);
        if (!parsed.success) {
            status = "fail";
            detail = "line " + std::to_string(parsed.errorLine) + ": " +
                     parsed.error;
            return;
        }
        if (parsed.diagram.nodes.empty()) {
            status = "fail";
            detail = "no nodes";
            return;
        }
        std::vector<mermaid::Size> sizes(parsed.diagram.nodes.size(),
                                         {120.0f, 52.0f});
        mermaid::layout(parsed.diagram, sizes, 40.0f, 50.0f);
        status = "ok";
        return;
    }
    auto built = mermaidext::build(k, src, measure, 1.0f);
    if (!built.ok || built.prims.empty()) {
        status = "fail";
        detail = built.ok ? "no primitives" : built.error;
        return;
    }
    status = "ok";
}

static void runIsolated(const std::string& path, int index,
                        const std::string& src) {
    int fds[2];
    if (pipe(fds) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        alarm(5);
        std::string kind, status, detail;
        probeOne(src, kind, status, detail);
        std::string out = kind + "\t" + status + "\t" + oneLine(detail);
        ssize_t ignored = write(fds[1], out.data(), out.size());
        (void)ignored;
        _exit(0);
    }
    close(fds[1]);
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0) out.append(buf, n);
    close(fds[0]);
    int ws = 0;
    waitpid(pid, &ws, 0);
    if (WIFSIGNALED(ws)) {
        std::string kind = kindName(mermaidext::detectKind(src));
        out = kind + (WTERMSIG(ws) == SIGALRM ? "\ttimeout\t" : "\tcrash\tsignal " +
                      std::to_string(WTERMSIG(ws)));
    }
    std::cout << path << "\t" << index << "\t" << out << "\t"
              << headerLine(mermaid::stripPreamble(src)) << "\n";
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static void probeMarkdown(const std::string& path, const std::string& text,
                          const std::vector<std::string>& accepted) {
    std::stringstream ss(text);
    std::string line;
    int index = 0;
    bool inFence = false, capture = false;
    char fenceChar = 0;
    size_t fenceLen = 0;
    std::string body;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t indent = line.find_first_not_of(' ');
        if (indent == std::string::npos) indent = line.size();
        std::string t = indent <= 3 ? line.substr(indent) : std::string();
        if (!inFence) {
            if (t.size() >= 3 && (t[0] == '`' || t[0] == '~')) {
                size_t run = t.find_first_not_of(t[0]);
                if (run == std::string::npos) run = t.size();
                if (run >= 3) {
                    inFence = true;
                    fenceChar = t[0];
                    fenceLen = run;
                    std::string info = t.substr(run);
                    size_t a = info.find_first_not_of(" \t");
                    info = a == std::string::npos ? "" : info.substr(a);
                    std::string word = info.substr(0, info.find_first_of(" \t{"));
                    std::transform(word.begin(), word.end(), word.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
                    capture = std::find(accepted.begin(), accepted.end(), word) !=
                              accepted.end();
                    body.clear();
                }
            }
        } else {
            size_t run = t.find_first_not_of(fenceChar);
            if (run == std::string::npos) run = t.size();
            if (!t.empty() && t[0] == fenceChar && run >= fenceLen &&
                t.find_first_not_of(" \t", run) == std::string::npos) {
                if (capture) runIsolated(path, index++, body);
                inFence = false;
                continue;
            }
            if (capture) body += line + "\n";
        }
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> accepted = {"mermaid"};
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--accept" && i + 1 < argc) {
            accepted.push_back(argv[++i]);
        } else if (a == "-") {
            std::string p;
            while (std::getline(std::cin, p)) files.push_back(p);
        } else {
            files.push_back(a);
        }
    }
    for (const auto& path : files) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream buf;
        buf << in.rdbuf();
        if (endsWith(path, ".md")) {
            probeMarkdown(path, buf.str(), accepted);
        } else {
            runIsolated(path, 0, buf.str());
        }
    }
    return 0;
}
