// Writes merman_engine's SVG for one Mermaid source file, for visual checks.
// usage: merman_svg <in.mmd> <out.svg> [--dark] [--font FAMILY]
#include "merman_engine.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: merman_svg <in.mmd> <out.svg> [--dark] [--font FAMILY]\n";
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    merman_engine::Theme theme;
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--dark") {
            theme.dark = true;
            theme.roles = {{"canvas", "#1e1e2e"}, {"surface", "#313244"}, {"text", "#cdd6f4"},
                           {"border", "#89b4fa"}, {"line", "#a6adc8"}};
        } else if (arg == "--font" && i + 1 < argc) {
            theme.fontFamily = argv[++i];
        }
    }
    auto diagram = merman_engine::renderSvg(source, theme);
    if (!diagram.ok) {
        std::cerr << "merman: " << diagram.error << "\n";
        return 1;
    }
    std::ofstream(argv[2], std::ios::binary) << diagram.svg;
    std::cout << diagram.width << " x " << diagram.height << "\n";
    return 0;
}
