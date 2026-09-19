# Resize performance — 1.6.1

The 1.6.0 resize path repositioned all 51 controls, updated all four macro columns on every page, and synchronously repainted the window tree on every size notification. In the macro editor, a single size change could paint the parent five times. `WS_EX_COMPOSITED` added whole-tree composition on top of an already buffered parent, while every parent paint allocated a new device-dependent bitmap.

## Changes

- Batch changed positions with `DeferWindowPos`; skip inactive pages and unchanged geometry. Cache control handles/types and column widths.
- Invalidate for normal Windows paint processing instead of forcing synchronous full-tree redraws from layout.
- Replace whole-tree composition with a persistent top-down DIB for the parent and buffered native/custom controls.
- Cache complete button/combo images by their visual state, size and DPI. Hover, focus, capture text, shortcuts, selection and enabled state invalidate the cache.
- Reuse bitmap capacity as the window grows. Only copy the dirty rectangle. Avoid an intermediate blank parent erase.
- Keep run/stop state updates independent from geometry caching. Preserve list selection/scroll position and refresh combo/library item heights after DPI changes.

## Local comparison

Windows x64, MSVC Release builds, the same resize workload on the same machine. The baseline is 1.6.0 with measurement instrumentation. Each page has eight warm-up resizes followed by 64 samples; one repeated size produces no layout. These are **wall-clock resize plus completed client drawing times**, including native window management and `GdiFlush`. They are not presented FPS, input-to-display latency, or a guarantee for other PCs. Desktop composition, clipping, system load and Windows size limits affect timings and native paint counts. The main comparison uses 96 DPI; the additional 144/192 rows exercise scaled rendering on the current desktop.

| Page, 96 DPI | Before median | After median | Before p95 | After p95 | Median speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| Auto-clicker | 52.81 ms | 15.56 ms | 60.42 ms | 19.43 ms | 3.4× |
| Macro editor | 130.19 ms | 18.25 ms | 147.72 ms | 26.41 ms | 7.1× |
| Preferences | 48.32 ms | 7.89 ms | 55.13 ms | 11.76 ms | 6.1× |
| Library | 51.42 ms | 10.27 ms | 63.32 ms | 13.10 ms | 5.0× |

Across the 63 actual size changes in the 96-DPI macro-editor run, parent paints fell from **315 to 63**, control placements from **3,213 to 1,197**, and column updates from **252 to 63**. After warming the parent buffer, that editor run allocated **zero** new parent bitmaps instead of 315. No GDI-object growth was observed in those four 96-DPI runs.

Raw data: [before](performance/resize-before.csv), [after](performance/resize-after.csv). Detailed timing columns in the new harness can overlap because layout may cause native child work; do not sum them as exclusive CPU categories.

## Reproduce

Build Release, then run the isolated workload:

```powershell
pwsh -File scripts/build.ps1 -Configuration Release
Start-Process -FilePath ./build/Release/MacroPulse.exe -ArgumentList '--resize-benchmark','build/resize.csv' -WindowStyle Hidden -Wait
```

The harness opens its own test window without activation, uses temporary in-memory settings/library data, never runs input-producing actions, and performs no network requests. It exits after writing the CSV. Timings are diagnostic and are deliberately not a CI pass/fail threshold.

## Regression coverage

`render_regression` runs in Debug and Release CI. It compares cached and uncached control pixels after label, hover, focus, pressed/disabled, preset, shortcut and DPI changes. It compares a partial parent repaint with a fresh full frame, checks inactive-page visibility, verifies unchanged geometry avoids position/column work, preserves selection and scroll in a 200-row macro, and checks GDI resources after repeated resizes. A wait-only engine run verifies that geometry caching still enables emergency stop and disables editing while running, without injecting any input.
