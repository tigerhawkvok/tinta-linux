// Keyboard state for the Linux <windows.h>.
#include <windows.h>

namespace {

// Bit 0x8000: down. Bit 0x0001: toggled on each press (Caps Lock style).
SHORT g_keyState[256] = {};

void setOne(int virtualKey, bool down) {
    auto& state = g_keyState[virtualKey & 0xFF];
    bool wasDown = (state & 0x8000) != 0;
    if (down && !wasDown) state ^= 0x0001;
    state = static_cast<SHORT>(down ? (state | 0x8000) : (state & ~0x8000));
}

}  // namespace

SHORT GetKeyState(int virtualKey) { return g_keyState[virtualKey & 0xFF]; }

// Side-specific keys also drive the generic key, as Windows reports them
void tintaSetKeyState(int virtualKey, bool down) {
    setOne(virtualKey, down);
    switch (virtualKey) {
        case VK_LSHIFT:
        case VK_RSHIFT:
            setOne(VK_SHIFT, (g_keyState[VK_LSHIFT] | g_keyState[VK_RSHIFT]) & 0x8000);
            break;
        case VK_LCONTROL:
        case VK_RCONTROL:
            setOne(VK_CONTROL, (g_keyState[VK_LCONTROL] | g_keyState[VK_RCONTROL]) & 0x8000);
            break;
        case VK_LMENU:
        case VK_RMENU:
            setOne(VK_MENU, (g_keyState[VK_LMENU] | g_keyState[VK_RMENU]) & 0x8000);
            break;
        default:
            break;
    }
}
