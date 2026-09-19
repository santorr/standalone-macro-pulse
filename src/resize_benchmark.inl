// Deterministic, isolated resize workload. Measures completed client drawing,
// not monitor presentation/FPS. Never starts input, reads user data or networks.
void App::runResizeBenchmark() {
    std::ofstream out(resizeReport);
    if (!out) { ++smokeExit; DestroyWindow(hwnd); return; }
    out << "dpi,page,frames,p50_ms,p95_ms,max_ms,layouts,positions,paints,child_paints,bitmap_allocations,column_writes,layout_ms,paint_ms,child_ms,gdi_delta\n";
    KillTimer(hwnd, 1); KillTimer(hwnd, 2); KillTimer(hwnd, 3);
    for (int scale : {96, 144, 192}) {
        dpi = scale; makeFonts();
        for (int tab = 0; tab < 4; ++tab) {
            page = tab; layout();
            std::vector<double> frames;
            RenderMetrics before{}; DWORD objects = 0;
            for (int frame = -8; frame < 64; ++frame) {
                if (frame == 0) { before = renderMetrics; objects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS); }
                const int step = frame < 0 ? -frame : frame % 32;
                const int edge = (frame / 32) % 2 ? 31 - step : step;
                auto began = std::chrono::steady_clock::now();
                SetWindowPos(hwnd, nullptr, 0, 0, s(1020 + edge * 15), s(710 + edge * 9), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                // Flush pending painting without invalidating another full frame.
                RedrawWindow(hwnd, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
                GdiFlush();
                // Service the test window so Windows does not replace it with a
                // hung-window ghost during a long, synchronous measurement.
                MSG pending{};
                while (PeekMessageW(&pending, hwnd, 0, 0, PM_REMOVE)) {
                    if (pending.message == WM_PAINT || pending.message == WM_NCPAINT) DispatchMessageW(&pending);
                }
                if (frame >= 0) frames.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
            }
            std::sort(frames.begin(), frames.end());
            out << scale << ',' << tab << ',' << frames.size() << ',' << frames[frames.size()/2] << ',' << frames[frames.size()*95/100] << ',' << frames.back() << ','
                << renderMetrics.layouts - before.layouts << ',' << renderMetrics.positions - before.positions << ',' << renderMetrics.paints - before.paints << ','
                << renderMetrics.childPaints - before.childPaints << ',' << renderMetrics.allocations - before.allocations << ',' << renderMetrics.columnWrites - before.columnWrites << ',' << renderMetrics.layoutMs - before.layoutMs << ',' << renderMetrics.paintMs - before.paintMs << ',' << renderMetrics.childMs - before.childMs << ','
                << static_cast<long>(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS)) - static_cast<long>(objects) << '\n';
            out.flush();
            if (renderMetrics.paints == before.paints) ++smokeExit;
        }
    }
    dirty = false; DestroyWindow(hwnd);
}
