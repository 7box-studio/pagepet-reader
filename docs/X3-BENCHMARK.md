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
4. A representative compressed book or transferred EPUB once the import path exists.

Use the same files for cold and warm runs. Remove only the relevant directory under `/.pagepet/cache/` for a cold run; keep it for a warm run.

## Measurements

For every fixture and controller, record:

| Run | What to record |
| --- | --- |
| Cold open | Time from Confirm on Home to the first rendered page, free heap, largest free block, and cache bytes written. |
| Warm reopen | Time from opening an indexed book to the first rendered page, free heap, and largest free block. |
| Page turns | Time and free heap for 10 ordinary forward turns, 10 backward turns, and a chapter or page-boundary turn. |
| Near-end jump | Time to reach a page near the end and whether the page remains responsive. |
| Recovery | Card removal during cache creation, interrupted power, retry result, and saved position after restart. |

The firmware logs page timing in this form at 115200 baud:

```text
[reader] page 12/340 display=123ms free=... largest=...
```

Copy the largest observed `display`, the lowest `free`, and the lowest `largest` values into the test notes. Measure cache size on the card after the build and after a warm reopen.

## Button path to verify

Home → Up/Down → Confirm opens the selected book. Back returns to Home. In Reading, Confirm opens text settings; Up/Down changes the pending size; Confirm rebuilds pages; Back returns to the book. Error screen: Confirm retries, Back returns Home. Power enters sleep; releasing the power button after wake must not trigger a second sleep.

Test every path on both controller variants. Record any difference before changing the UI or refresh mode.

## EPUB decision

Do not add native EPUB parsing based on binary size alone. Compare the measurements above with the current TXT reading path. Choose native reading only if first open, warm reopen, page turns, memory headroom, and recovery remain acceptable on both controllers. Otherwise convert EPUB during transfer and keep the on-device reader small.
