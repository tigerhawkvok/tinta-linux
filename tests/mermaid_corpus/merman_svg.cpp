// Writes merman_engine's SVG for one Mermaid source file, for visual checks.
// With --png SCALE it writes the PNG the Windows viewer would draw instead,
// and reports its pixel size against the layout size and the time taken.
// usage: merman_svg <in.mmd> <out.svg|out.png> [--dark] [--font FAMILY] [--png SCALE]
#include "merman_engine.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

double msSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

uint32_t bigEndian32(const std::string& bytes, size_t at) {
    return (uint32_t)(unsigned char)bytes[at] << 24 | (uint32_t)(unsigned char)bytes[at + 1] << 16 |
           (uint32_t)(unsigned char)bytes[at + 2] << 8 | (uint32_t)(unsigned char)bytes[at + 3];
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: merman_svg <in.mmd> <out.svg|out.png> [--dark] [--font FAMILY] [--png SCALE]\n";
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    merman_engine::Theme theme;
    float pngScale = 0.0f;
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--dark") {
            theme.dark = true;
            theme.roles = {{"canvas", "#1e1e2e"}, {"surface", "#313244"}, {"text", "#cdd6f4"},
                           {"border", "#89b4fa"}, {"line", "#a6adc8"}};
        } else if (arg == "--font" && i + 1 < argc) {
            theme.fontFamily = argv[++i];
        } else if (arg == "--png" && i + 1 < argc) {
            pngScale = std::strtof(argv[++i], nullptr);
        }
    }
    auto start = std::chrono::steady_clock::now();
    auto diagram = merman_engine::renderSvg(source, theme);
    double svgMs = msSince(start);
    if (!diagram.ok) {
        std::cerr << "merman: " << diagram.error << "\n";
        return 1;
    }
    if (pngScale <= 0.0f) {
        std::ofstream(argv[2], std::ios::binary) << diagram.svg;
        std::cout << diagram.width << " x " << diagram.height << "\n";
        return 0;
    }
    // Twice: the first call also loads the system fonts (once per process)
    std::string error;
    start = std::chrono::steady_clock::now();
    std::string png = merman_engine::renderPng(source, theme, pngScale, "", error);
    double firstMs = msSince(start);
    start = std::chrono::steady_clock::now();
    png = merman_engine::renderPng(source, theme, pngScale, "", error);
    double againMs = msSince(start);
    if (png.size() < 24) {
        std::cerr << "merman png: " << error << "\n";
        return 1;
    }
    std::ofstream(argv[2], std::ios::binary) << png;
    // IHDR: width and height follow the 8-byte signature and chunk header
    std::cout << "layout " << diagram.width * pngScale << " x " << diagram.height * pngScale << ", png "
              << bigEndian32(png, 16) << " x " << bigEndian32(png, 20) << "; svg " << svgMs << " ms, png "
              << firstMs << " ms first, " << againMs << " ms again\n";
    return 0;
}
