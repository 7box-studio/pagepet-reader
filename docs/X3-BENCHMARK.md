# Xteink X3 benchmark checklist

This checklist is the release gate for the reading path and the decision about EPUB support. Run it on physical Xteink X3 units; a PlatformIO build cannot replace these measurements.

## Hardware matrix

Run the same card and books on both known X3 panel-controller variants:

- UC8253
- UC8279d

Record the board identifier, firmware commit, card model, card filesystem, and battery level before each run.

## Book fixtures

Keep the fixture set on the card outside the firmware repository:

1. UTF-8 plain text, 100–300 KB.
2. UTF-8 plain text, 1–5 MB.
3. A long book with enough pages to test a jump near the end.
4. A representative DRM-free EPUB with several chapters, plus a large single-chapter EPUB and an illustrated EPUB.

Use the same files for cold and warm runs. Remove only the relevant directory under `/.pagepet/cache/` for a cold run; keep it for a warm run.

## Measurements

For every fixture and controller, record:

| Run | What to record |
| --- | --- |
| Cold open | Time from Confirm on a selected book in Library (or Continue reading on Home) to the first rendered page, free heap, largest free block, and cache bytes written. |
| Warm reopen | Time from opening an indexed book to the first rendered page, free heap, and largest free block. |
| Page turns | Time and free heap for 10 ordinary forward turns, 10 backward turns, and a chapter or page-boundary turn. |
| Near-end jump | Time to reach a page near the end and whether the page remains responsive. |
| Recovery | Card removal during cache creation, interrupted power, retry result, and saved position after restart. |

The firmware logs page timing in this form at 115200 baud:

```text
[reader] chapter 2/18 page 12/34 render=123ms free=... largest=...
```

Copy the largest observed `render`, the lowest `free`, and the lowest `largest` values into the test notes. Measure cache size on the card after the build and after a warm reopen.

## Button path to verify

Home → Library → Up/Down → Confirm enters folders or opens a book. Back from reading returns to its folder with the book selected; Back from a folder selects that folder in its parent, and Back from the root returns Home. Continue reading on Home (or Back as a shortcut) resumes the saved full path. In Reading, Confirm opens text settings; Up/Down changes the pending size; Confirm rebuilds pages; Back returns to the book. Error screen: Confirm retries, Back returns Home. Power enters sleep; releasing the power button after wake must not trigger a second sleep.

Also test more than eight entries and more than fourteen books in one folder, nested Russian names, empty folders, a removed/renamed folder, card removal while crossing a list-page boundary, and resume after sleep from a nested EPUB. Check that a cold boot does not scan the entire folder tree and that repeatedly opening/closing folders does not grow heap use.

Test every path on both controller variants. Record any difference before changing the UI or refresh mode.

## EPUB decision

Native EPUB reading is implemented experimentally using an SD-backed catalog and lazy chapter caches; no physical-device performance results are recorded yet. Compare it against TXT before considering it validated. Test compressed and stored chapters, first entry into an uncached chapter, backward chapter turns, empty front/trailing chapters, and a warm resume directly into a late chapter. Verify an unchanged warm cache causes no build refresh.

The logs also report catalog build time and table/parse arena peaks, and chapter layout time and arena/index peaks. The build buffer is 112 KiB (EPUB layout 60 KiB + parse 52 KiB); catalog building uses the same buffer for tables and borrows the framebuffer for parsing. The resident catalog is capped at 64 KiB, and the current index/page budgets are 32/20 KiB. Image pages may temporarily request another 96 KiB. These numbers exclude system, XML-parser and image-library heap use: record actual free heap and largest block on the device, including illustrated pages. A chapter remains subject to the SDK page cap and the 32 KiB index budget; a pathological book should return a recoverable error.

Repeat interrupted-write/retry checks for both catalog and page caches, replace an EPUB under the same filename, and verify legacy TXT saves plus EPUB chapter/character positions after sleep and text-size changes. Keep native reading only if first open, warm reopen, page turns, memory headroom, and recovery remain acceptable on both controllers. Otherwise convert EPUB during transfer and keep the on-device reader small.
