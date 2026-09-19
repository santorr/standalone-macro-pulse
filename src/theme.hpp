#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>

namespace theme {
inline Gdiplus::Color color(COLORREF value, BYTE alpha = 255) { return {alpha, GetRValue(value), GetGValue(value), GetBValue(value)}; }
inline void rounded(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float radius) {
    float r = std::min(radius, std::min(w, h) / 2.f), d = r * 2;
    if (r <= 0) { path.AddRectangle(Gdiplus::RectF(x, y, w, h)); return; }
    path.AddArc(x, y, d, d, 180, 90); path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90); path.AddArc(x, y + h - d, d, d, 90, 90); path.CloseFigure();
}
inline void surface(HDC dc, float x, float y, float w, float h, float radius, COLORREF fill, COLORREF border = CLR_INVALID) {
    if (w <= 0 || h <= 0) return;
    Gdiplus::Graphics graphics(dc); graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path; rounded(path, x + .5f, y + .5f, w - 1, h - 1, radius);
    Gdiplus::SolidBrush brush(color(fill)); graphics.FillPath(&brush, &path);
    if (border != CLR_INVALID) { Gdiplus::Pen pen(color(border)); graphics.DrawPath(&pen, &path); }
}
inline void gradient(HDC dc, float x, float y, float w, float h, float radius, COLORREF a, COLORREF b, COLORREF border) {
    Gdiplus::Graphics graphics(dc); graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path; rounded(path, x + .5f, y + .5f, w - 1, h - 1, radius);
    Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(x, y), Gdiplus::PointF(x + w, y + h), color(a), color(b));
    graphics.FillPath(&brush, &path); Gdiplus::Pen pen(color(border)); graphics.DrawPath(&pen, &path);
}
inline void symbol(HDC dc, int kind, float x, float y, float size, COLORREF value) {
    Gdiplus::Graphics g(dc); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias); g.TranslateTransform(x, y); g.ScaleTransform(size / 24, size / 24);
    Gdiplus::Pen pen(color(value), 1.65f); pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound); pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush fill(color(value)); Gdiplus::GraphicsPath path;
    switch (kind) {
    case 0: rounded(path, 5, 2, 14, 20, 6); g.DrawPath(&pen, &path); g.DrawLine(&pen, 5.f, 10.f, 19.f, 10.f); g.DrawLine(&pen, 12.f, 2.f, 12.f, 10.f); break;
    case 1: {
        Gdiplus::PointF points[] = {{3, 7}, {12, 2}, {21, 7}, {12, 12}, {3, 7}}; g.DrawLines(&pen, points, 5);
        g.DrawLine(&pen, 3.f, 12.f, 12.f, 17.f); g.DrawLine(&pen, 12.f, 17.f, 21.f, 12.f);
        g.DrawLine(&pen, 3.f, 17.f, 12.f, 22.f); g.DrawLine(&pen, 12.f, 22.f, 21.f, 17.f); break;
    }
    case 2: g.DrawEllipse(&pen, 3.f, 4.f, 18.f, 18.f); g.DrawLine(&pen, 12.f, 8.f, 12.f, 13.f); g.DrawLine(&pen, 12.f, 13.f, 15.f, 15.f); g.DrawLine(&pen, 9.f, 1.f, 15.f, 1.f); break;
    case 3: rounded(path, 5, 5, 14, 14, 3); g.FillPath(&fill, &path); break;
    case 4: g.DrawEllipse(&pen, 5.f, 5.f, 14.f, 14.f); g.DrawEllipse(&pen, 10.f, 10.f, 4.f, 4.f); g.DrawLine(&pen, 12.f, 1.f, 12.f, 5.f); g.DrawLine(&pen, 12.f, 19.f, 12.f, 23.f); g.DrawLine(&pen, 1.f, 12.f, 5.f, 12.f); g.DrawLine(&pen, 19.f, 12.f, 23.f, 12.f); break;
    case 5: rounded(path, 2, 5, 20, 14, 3); g.DrawPath(&pen, &path); for (int a = 0; a < 4; ++a) g.DrawLine(&pen, 5.f + a * 4, 9.f, 5.f + a * 4, 10.f); g.DrawLine(&pen, 7.f, 15.f, 17.f, 15.f); break;
    case 6: { Gdiplus::PointF points[] = {{1, 13}, {6, 13}, {10, 4}, {14, 20}, {18, 11}, {23, 11}}; g.DrawLines(&pen, points, 6); break; }
    case 7: { Gdiplus::PointF points[] = {{7, 4}, {20, 12}, {7, 20}}; g.FillPolygon(&fill, points, 3); break; }
    case 8: g.DrawLine(&pen, 12.f, 5.f, 12.f, 19.f); g.DrawLine(&pen, 5.f, 12.f, 19.f, 12.f); break;
    case 9: case 10: { if (kind == 10) { g.TranslateTransform(24.f, 24.f); g.RotateTransform(180.f); } g.DrawLine(&pen, 12.f, 20.f, 12.f, 4.f); g.DrawLine(&pen, 6.f, 10.f, 12.f, 4.f); g.DrawLine(&pen, 18.f, 10.f, 12.f, 4.f); break; }
    case 11: rounded(path, 6, 6, 12, 16, 2); g.DrawPath(&pen, &path); g.DrawLine(&pen, 4.f, 6.f, 20.f, 6.f); g.DrawLine(&pen, 9.f, 2.f, 15.f, 2.f); g.DrawLine(&pen, 10.f, 10.f, 10.f, 18.f); g.DrawLine(&pen, 14.f, 10.f, 14.f, 18.f); break;
    }
}
}
