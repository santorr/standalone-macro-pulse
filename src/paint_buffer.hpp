#pragma once
#include <windows.h>
#include <algorithm>

namespace pulse {
// Reuse a directly addressable bitmap rather than converting a new device-
// dependent bitmap for every GDI+ primitive and every resize frame.
class PaintBuffer {
public:
    ~PaintBuffer() { reset(); }
    PaintBuffer() = default;
    PaintBuffer(const PaintBuffer&) = delete;
    PaintBuffer& operator=(const PaintBuffer&) = delete;
    bool ensure(HDC target, int width, int height, int blockWidth = 256, int blockHeight = 128) {
        if (width <= 0 || height <= 0) return false;
        if (dc_ && width <= width_ && height <= height_) return true;
        int w = ((std::max(width, width_) + blockWidth - 1) / blockWidth) * blockWidth;
        int h = ((std::max(height, height_) + blockHeight - 1) / blockHeight) * blockHeight;
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        auto bitmap = CreateDIBSection(target, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap) return false;
        if (!dc_) dc_ = CreateCompatibleDC(target);
        if (!dc_) { DeleteObject(bitmap); return false; }
        auto previous = SelectObject(dc_, bitmap);
        if (!previous || previous == HGDI_ERROR) { DeleteObject(bitmap); return false; }
        if (bitmap_) DeleteObject(bitmap_); else original_ = previous;
        bitmap_ = bitmap; pixels_ = static_cast<const DWORD*>(pixels); width_ = w; height_ = h; ++allocations_;
        return true;
    }
    const DWORD* row(int y) const { return pixels_ + size_t(y) * width_; }
    HDC dc() const { return dc_; }
    size_t allocations() const { return allocations_; }
    void reset() {
        if (dc_) { SelectObject(dc_, original_); DeleteObject(bitmap_); DeleteDC(dc_); }
        dc_ = nullptr; bitmap_ = nullptr; original_ = nullptr; pixels_ = nullptr; width_ = height_ = 0;
    }
private:
    const DWORD* pixels_ = nullptr;
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ original_ = nullptr;
    int width_ = 0, height_ = 0;
    size_t allocations_ = 0;
};
}
