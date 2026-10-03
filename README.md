<p align="center">
  <img src="assets/pagepet-reader-mascot.png" alt="PagePet dragon with an open book" width="180">
</p>

<h1 align="center">PagePet Reader</h1>

<p align="center"><strong>Open firmware for Xteink X3.</strong> Focused reading, physical buttons, and an optional companion.</p>

<p align="center"><a href="README.ru.md">Русский</a></p>

PagePet Reader is firmware for the **Xteink X3**. The primary experience is returning to a book and reading comfortably with physical buttons. PagePet develops quietly as you read. It never interrupts a page or asks for attention.

## Current state

This is an early development build. It currently provides:

- Local UTF-8 `.txt` reading from microSD with a page index stored on the card.
- Saved reading position and a visible Resume row for the last book.
- A button-only book list, PagePet screen, and English/Russian interface switch.
- Two text sizes from the reading screen; the current position is retained when pages are rebuilt.
- Sleep through the power button.
- Retry and return-home actions on book, page, and cache errors.

The interface defaults to **English**. To switch to Russian, select **Language: English** on the home screen with Up/Down and press Confirm. The choice is saved on microSD. Select **Язык: Русский** the same way to switch back.

Hardware reading, sleep, recovery, and both known X3 display-controller variants still need device validation. EPUB and other formats are not implemented yet. This build is not a stable daily-use release. See the [development priorities](docs/ROADMAP.md).

The measurement procedure for future hardware runs is in the [X3 benchmark checklist](docs/X3-BENCHMARK.md).

## Button controls

| Screen | Controls |
| --- | --- |
| Home | The Resume row shows the saved book. Back resumes it; Up/Down selects a book, PagePet, or language. Confirm opens or switches. |
| Reading | Side buttons: previous/next page. Confirm: reading settings. Back: return to the book list. |
| PagePet | Back: return to the book list. |
| Error | Confirm: retry the failed book or page. Back: return Home. |
| Any screen | Power button: sleep. |

Place `.txt` files in the microSD root. The current list shows up to 14 books; folders are not browsed yet.

## Build

Git and PlatformIO are required. The hardware SDK and companion engine are pinned as submodules:

```sh
git clone --recurse-submodules git@github.com:7box-studio/pagepet-reader.git
cd pagepet-reader
pio run -e x3
```

The firmware uses [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) for X3 hardware and book layout, and [PagePet](https://github.com/7box-studio/pagepet) for the companion. The two embedded reader sizes are generated from DejaVu Sans; its license is in `licenses/DejaVuSans-LICENSE.txt`.

## Community and license

Updates and discussion: [t.me/pagepet_reader](https://t.me/pagepet_reader).

For project rules and contribution guidance, see the [Code of Conduct](CODE_OF_CONDUCT.md), [Contributing guide](CONTRIBUTING.md), and [Security policy](SECURITY.md). Bug reports and focused feature requests use the [issue templates](.github/ISSUE_TEMPLATE/); pull requests use the [pull request template](.github/PULL_REQUEST_TEMPLATE.md).

The firmware source is available under the [MIT license](LICENSE). Dependencies and the font retain their own licenses.
