#ifndef TINTA_NOTICES_H
#define TINTA_NOTICES_H

#include <windows.h>

#include <string>

struct App;

// packaging/THIRD-PARTY-NOTICES.md, built into the binary: the licenses of
// everything Tinta ships (packaging/notices/generate_notices.py)
std::string thirdPartyNotices();
// Writes them to the temp folder (nothing lands beside a portable exe) and
// opens them as a document tab; false when the file cannot be written
bool openThirdPartyNotices(App& app, HWND hwnd);

#endif
