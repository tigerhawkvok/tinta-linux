// Linux desktop integration: what registerFileAssociation's registry keys
// do on Windows, for --register and the first-run "default viewer?" prompt.
// Per user, in the XDG folders:
//   applications/tinta.desktop     Exec is the AppImage ($APPIMAGE) or the binary
//   icons/hicolor/*/apps/tinta.png from the installation (or the source tree)
//   mime/packages/tinta-mermaid.xml text/vnd.mermaid for *.mmd (shared-mime-info has none)
//   mimeapps.list                  Tinta as the default for Markdown and Mermaid
#include "compat_internal.h"

#include <gio/gio.h>

#include <string>
#include <unistd.h>

namespace {

const char* const kMimeTypes[] = {"text/markdown", "text/x-markdown", "text/vnd.mermaid"};

std::string exePath() {
    char buffer[4096];
    ssize_t n = readlink("/proc/self/exe", buffer, sizeof buffer - 1);
    return n > 0 ? std::string(buffer, static_cast<size_t>(n)) : std::string();
}

// One quoted Exec= argument (Desktop Entry spec: reserved characters take a
// backslash, which the string escape rule doubles; % doubles)
std::string execArgument(const std::string& path) {
    std::string out = "\"";
    for (char c : path) {
        if (c == '"' || c == '`' || c == '$') out += "\\\\";
        if (c == '\\') out += "\\\\\\";
        if (c == '%') out += '%';
        out += c;
    }
    return out + "\"";
}

bool writeFile(const std::string& path, const std::string& text) {
    gchar* dir = g_path_get_dirname(path.c_str());
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    return g_file_set_contents(path.c_str(), text.data(), static_cast<gssize>(text.size()), nullptr);
}

void run(const char* tool, const std::string& argument) {
    gchar* found = g_find_program_in_path(tool);
    if (!found) return;  // optional: caches refresh on their own too
    const gchar* argv[] = {found, argument.c_str(), nullptr};
    g_spawn_sync(nullptr, const_cast<gchar**>(argv), nullptr,
                 static_cast<GSpawnFlags>(G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    g_free(found);
}

void copyIcons(const std::string& dataDir) {
    std::string exe = exePath();
    std::string installed = exe.substr(0, exe.find_last_of('/')) + "/../share/icons/hicolor/";
    for (int size : {16, 32, 48, 256}) {
        std::string dims = std::to_string(size) + "x" + std::to_string(size);
        std::string source = installed + dims + "/apps/tinta.png";
#ifdef TINTA_SOURCE_ICON_DIR
        if (!g_file_test(source.c_str(), G_FILE_TEST_EXISTS)) {
            source = std::string(TINTA_SOURCE_ICON_DIR) + "/tinta-" + std::to_string(size) + ".png";
        }
#endif
        gchar* bytes = nullptr;
        gsize length = 0;
        if (!g_file_get_contents(source.c_str(), &bytes, &length, nullptr)) continue;
        writeFile(dataDir + "/icons/hicolor/" + dims + "/apps/tinta.png", std::string(bytes, length));
        g_free(bytes);
    }
}

// Tinta first for each type, other entries untouched
bool setDefaults(const std::string& configDir) {
    std::string path = configDir + "/mimeapps.list";
    GKeyFile* file = g_key_file_new();
    g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_KEEP_COMMENTS, nullptr);
    for (const char* type : kMimeTypes) {
        g_key_file_set_string(file, "Default Applications", type, "tinta.desktop;");
        gsize count = 0;
        gchar** added = g_key_file_get_string_list(file, "Added Associations", type, &count, nullptr);
        std::string list = "tinta.desktop;";
        for (gsize i = 0; i < count; i++) {
            if (g_strcmp0(added[i], "tinta.desktop") != 0) list += std::string(added[i]) + ";";
        }
        g_strfreev(added);
        g_key_file_set_value(file, "Added Associations", type, list.c_str());
    }
    g_mkdir_with_parents(configDir.c_str(), 0755);
    bool ok = g_key_file_save_to_file(file, path.c_str(), nullptr);
    g_key_file_free(file);
    return ok;
}

}  // namespace

bool tintaRegisterDesktopApp() {
    const char* appimage = g_getenv("APPIMAGE");
    std::string exec = appimage && *appimage ? appimage : exePath();
    if (exec.empty()) return false;
    std::string data = g_get_user_data_dir();
    std::string entry =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Tinta\n"
        "GenericName=Markdown Viewer\n"
        "Comment=Fast Markdown and Mermaid viewer and editor\n"
        "Exec=" + execArgument(exec) + " %f\n"
        "Icon=tinta\n"
        "Terminal=false\n"
        "Categories=Office;Viewer;\n"
        "MimeType=text/markdown;text/x-markdown;text/vnd.mermaid;\n"
        "Keywords=markdown;md;mermaid;viewer;editor;\n"
        "StartupWMClass=tinta\n"
        "StartupNotify=true\n";
    if (!writeFile(data + "/applications/tinta.desktop", entry)) return false;
    copyIcons(data);
    writeFile(data + "/mime/packages/tinta-mermaid.xml",
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
              "  <mime-type type=\"text/vnd.mermaid\">\n"
              "    <comment>Mermaid diagram</comment>\n"
              "    <sub-class-of type=\"text/plain\"/>\n"
              "    <glob pattern=\"*.mmd\"/>\n"
              "    <glob pattern=\"*.mermaid\"/>\n"
              "  </mime-type>\n"
              "</mime-info>\n");
    run("update-mime-database", data + "/mime");
    run("update-desktop-database", data + "/applications");
    return setDefaults(g_get_user_config_dir());
}
