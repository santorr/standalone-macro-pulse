#pragma once
#include <windows.h>
namespace pulse {
inline int messageBox(HWND owner, const wchar_t* text, const wchar_t* title, UINT flags) {
    return MessageBoxExW(owner, text, title, flags, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
}
}
