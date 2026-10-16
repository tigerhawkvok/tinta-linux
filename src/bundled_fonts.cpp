#include "app.h"

// Fonts Tinta ships, for text formats. On Windows the exe carries
// Monaspace Argon (the default code face) as RCDATA (resources/tinta.rc)
// and hands DirectWrite a private collection of it: nothing is installed,
// and GDI's private fonts (AddFontMemResourceEx) are invisible to
// DirectWrite. An installed copy of a family wins. On Linux fontconfig
// already has the bundled fonts (dwrite_pango.cpp), so this is a no-op.

#ifdef _WIN32
#include <dwrite_3.h>

#include <cwctype>
#include <map>
#include <set>
#include <string>

namespace {

const wchar_t* const kFontResources[] = {
    L"TINTA_FONT_ARGON_REGULAR", L"TINTA_FONT_ARGON_BOLD",
    L"TINTA_FONT_ARGON_ITALIC", L"TINTA_FONT_ARGON_BOLDITALIC",
};

std::wstring lower(std::wstring text) {
    for (auto& c : text) c = (wchar_t)std::towlower(c);
    return text;
}

struct Bundled {
    IDWriteFontCollection* collection = nullptr;
    std::set<std::wstring> families;            // lowercase
    std::map<std::wstring, bool> installed;     // lowercase family -> in the system
};

// One per DirectWrite factory: font files belong to the factory that
// loaded them. Process lifetime, like the factories.
Bundled& bundledFor(IDWriteFactory* factory) {
    static std::map<IDWriteFactory*, Bundled> all;
    auto found = all.find(factory);
    if (found != all.end()) return found->second;
    Bundled& bundled = all[factory];
    IDWriteFactory5* factory5 = nullptr;
    IDWriteInMemoryFontFileLoader* loader = nullptr;
    IDWriteFontSetBuilder1* builder = nullptr;
    IDWriteFontSet* set = nullptr;
    if (FAILED(factory->QueryInterface(__uuidof(IDWriteFactory5), reinterpret_cast<void**>(&factory5))) ||
        FAILED(factory5->CreateInMemoryFontFileLoader(&loader)) ||
        FAILED(factory5->RegisterFontFileLoader(loader)) ||
        FAILED(factory5->CreateFontSetBuilder(&builder))) {
        if (builder) builder->Release();
        if (factory5) factory5->Release();
        return bundled;  // before Windows 10 1703: system fonts only
    }
    for (const wchar_t* name : kFontResources) {
        HRSRC resource = FindResourceW(nullptr, name, MAKEINTRESOURCEW(10) /* RT_RCDATA */);
        HGLOBAL handle = resource ? LoadResource(nullptr, resource) : nullptr;
        const void* data = handle ? LockResource(handle) : nullptr;
        DWORD size = resource ? SizeofResource(nullptr, resource) : 0;
        if (!data || !size) continue;
        IDWriteFontFile* file = nullptr;
        if (SUCCEEDED(loader->CreateInMemoryFontFileReference(factory5, data, size, nullptr, &file))) {
            builder->AddFontFile(file);
            file->Release();
        }
    }
    IDWriteFontCollection1* collection = nullptr;
    if (SUCCEEDED(builder->CreateFontSet(&set)) &&
        SUCCEEDED(factory5->CreateFontCollectionFromFontSet(set, &collection))) {
        bundled.collection = collection;
        for (UINT32 i = 0; i < bundled.collection->GetFontFamilyCount(); i++) {
            IDWriteFontFamily* family = nullptr;
            IDWriteLocalizedStrings* names = nullptr;
            if (SUCCEEDED(bundled.collection->GetFontFamily(i, &family)) &&
                SUCCEEDED(family->GetFamilyNames(&names))) {
                for (UINT32 n = 0; n < names->GetCount(); n++) {
                    UINT32 length = 0;
                    if (FAILED(names->GetStringLength(n, &length))) continue;
                    std::wstring text(length + 1, L'\0');
                    if (SUCCEEDED(names->GetString(n, &text[0], length + 1))) {
                        text.resize(length);
                        bundled.families.insert(lower(text));
                    }
                }
            }
            if (names) names->Release();
            if (family) family->Release();
        }
    }
    if (set) set->Release();
    builder->Release();
    factory5->Release();
    // The loader stays registered: its in-memory files back the collection
    return bundled;
}

bool installedInSystem(IDWriteFactory* factory, const std::wstring& family) {
    IDWriteFontCollection* system = nullptr;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(factory->GetSystemFontCollection(&system, FALSE)) && system) {
        system->FindFamilyName(family.c_str(), &index, &exists);
        system->Release();
    }
    return exists != FALSE;
}

}  // namespace

IDWriteFontCollection* fontCollectionFor(const App& app, const wchar_t* family) {
    if (!family || !*family || !app.dwriteFactory) return nullptr;
    Bundled& bundled = bundledFor(app.dwriteFactory);
    if (!bundled.collection) return nullptr;
    std::wstring key = lower(family);
    if (!bundled.families.count(key)) return nullptr;
    auto known = bundled.installed.find(key);
    if (known == bundled.installed.end()) {
        known = bundled.installed.emplace(key, installedInSystem(app.dwriteFactory, family)).first;
    }
    return known->second ? nullptr : bundled.collection;
}

#else

IDWriteFontCollection* fontCollectionFor(const App&, const wchar_t*) { return nullptr; }

#endif
