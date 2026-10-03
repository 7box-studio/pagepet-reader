# PagePet Reader development priorities

The firmware's core job on Xteink X3 is to let a person return to a book quickly and read comfortably using physical buttons. PagePet is an optional companion that progresses with reading and requires no care.

These priorities describe the intended order, not release dates or claims of completed hardware support.

## 1. Resume the last book

The Home screen now shows the saved book in a visible Resume row, while Back opens it in one action. Verify that this path is discoverable without instructions.

Done when the book can be found and opened with buttons, reading resumes at the saved position after sleep or restart, and missing-book or missing-card cases have clear paths back to Home.

## 2. Adjust text while reading

The reading screen now has a small button-operated text-size screen with Small and Standard sizes. Consider line spacing and margins after device feedback. Back returns to the book without losing position.

Done when changing a setting rebuilds the page index without losing the reading position or exhausting X3 memory. Verify the entire button path on hardware.

## 3. Recover from card and cache errors

The error screen now shows a clear retry action and a Home path. A damaged page index is rebuilt on retry while the last saved reading position survives.

Done when a reader can return to the book with buttons after card removal, interrupted cache writing, and restart. A PagePet save error must not block reading.

## 4. EPUB and book import

Measure first open, cached reopen, page turns, a jump near the end, free heap, largest free block, and cache size on a physical X3 with representative books. Use the results to choose native EPUB reading or conversion during transfer. Keep import controls out of the main reading path.

Done when the selected approach preserves ordinary page-turn responsiveness and position recovery.

## PagePet rule

PagePet progresses through reading without daily tasks, page popups, or mandatory care. Its screen is entered voluntarily from Home. Reading and position saving continue if companion state fails.

## Release gate

Test physical X3 units with both known display controllers: buttons, sleep/wake, missing card, interrupted writes, first and cached open, page-turn timing, free heap, and largest free block. A successful build alone does not establish release readiness.
