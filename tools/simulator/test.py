#!/usr/bin/env python3
"""End-to-end button tests against the native build of src/main.cpp."""
from pathlib import Path
import re
import os
import tempfile
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile

from client import Backend, save_png
from demo import ensure_demo


def test_footnotes(binary):
    with tempfile.TemporaryDirectory(prefix="pagepet-footnotes-") as folder:
        card = Path(folder)
        with ZipFile(card / "notes.epub", "w", compression=ZIP_DEFLATED) as archive:
            archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
            archive.writestr("META-INF/container.xml",
                             '<container><rootfiles><rootfile full-path="OPS/book.opf"/></rootfiles></container>')
            archive.writestr("OPS/book.opf", '<package version="3.0"><metadata/>'
                             '<manifest><item id="main" href="main.xhtml" media-type="application/xhtml+xml"/>'
                             '<item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/>'
                             '</manifest><spine><itemref idref="main"/></spine></package>')
            archive.writestr("OPS/main.xhtml", '<html><body><p>Read this '
                             '<a href="#local"><sup>1</sup></a> '
                             '<sup><a href="notes.xhtml#remote">2</a></sup> '
                             '<a href="notes.xhtml#absent"><sup>3</sup></a></p>'
                             + '<p>Body text and more words.</p>' * 85
                             + '<p id="local">The local footnote.</p></body></html>')
            archive.writestr("OPS/notes.xhtml", '<html><body><p id="remote">The long footnote.</p>'
                             + '<p>This explanation spans more than one page.</p>' * 90 + '</body></html>')

        backend = Backend(binary, card)
        try:
            backend.ready()

            def key(command):
                backend.send(command)
                return backend.ready()[0]

            key("ok")
            source = key("ok")
            assert source["screen"] == "reading"
            assert key("ok")["screen"] == "reading-menu"
            assert key("down")["screen"] == "reading-menu"
            key("down")
            assert key("ok")["screen"] == "reading-menu"  # missing anchor in another document
            key("up")
            remote = key("ok")
            assert remote["screen"] == "note" and remote["pages"] > 1
            assert key("up")["page"] == remote["page"]
            key("advance")
            turned = key("down")
            assert turned["page"] == remote["page"] + 1 and turned["petPages"] == source["petPages"]
            restored = key("back")
            assert restored["screen"] == "reading"
            assert (restored["page"], restored["character"]) == (source["page"], source["character"])
            key("ok")
            local = key("ok")
            assert local["screen"] == "note" and local["page"] > source["page"], local
            assert key("power")["sleeping"]
            key("power")
            restored = key("back")
            assert restored["screen"] == "reading" and restored["character"] == source["character"]
            key("ok")
            for _ in range(3):
                key("down")
            assert key("ok")["screen"] == "settings"
        finally:
            backend.close()


def test_contents(binary):
    with tempfile.TemporaryDirectory(prefix="pagepet-contents-") as folder:
        card = Path(folder)
        with ZipFile(card / "contents.epub", "w", compression=ZIP_DEFLATED) as archive:
            archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
            archive.writestr("META-INF/container.xml",
                             '<container><rootfiles><rootfile full-path="OPS/book.opf"/></rootfiles></container>')
            archive.writestr("OPS/book.opf", '<package version="3.0"><metadata/><manifest>'
                             '<item id="main" href="main.xhtml" media-type="application/xhtml+xml"/>'
                             '<item id="next" href="next.xhtml" media-type="application/xhtml+xml"/>'
                             '<item id="nav" href="nav.xhtml" properties="nav" media-type="application/xhtml+xml"/>'
                             '</manifest><spine><itemref idref="main"/><itemref idref="next"/></spine></package>')
            archive.writestr("OPS/nav.xhtml",
                             '<html xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol>'
                             '<li><a href="main.xhtml">Start</a><ol>'
                             + ''.join(f'<li><a href="main.xhtml#s{i}">Section {i}</a></li>' for i in range(7))
                             + '</ol></li><li><a href="next.xhtml#missing">Missing</a></li>'
                             '<li><a href="next.xhtml#target">Target</a></li></ol></nav></body></html>')
            archive.writestr("OPS/main.xhtml", '<html><body>'
                             + ''.join(f'<p id="s{i}">Section {i}.</p>' for i in range(7)) + '</body></html>')
            archive.writestr("OPS/next.xhtml", '<html><body>' + '<p>Before the target section.</p>' * 90
                             + '<p id="target">Target section.</p></body></html>')

        backend = Backend(binary, card)
        try:
            backend.ready()

            def key(command):
                backend.send(command)
                return backend.ready()[0]

            key("ok")
            source = key("ok")
            assert key("ok")["screen"] == "reading-menu"
            assert key("ok")["screen"] == "contents"
            for _ in range(8):
                assert key("down")["screen"] == "contents"
            missing = key("ok")
            assert missing["screen"] == "contents" and missing["chapter"] == source["chapter"]
            assert key("back")["character"] == source["character"]
            key("ok")
            key("ok")
            for _ in range(9):
                key("down")
            target = key("ok")
            assert target["screen"] == "reading" and target["chapter"] == 1 and target["page"] > 0, target
            assert target["petPages"] == source["petPages"]
            key("restart")
            resumed = key("back")
            assert resumed["screen"] == "reading"
            assert (resumed["chapter"], resumed["character"]) == (target["chapter"], target["character"])
        finally:
            backend.close()


def main():
    binary = Path(".cache/simulator/pagepet-simulator").resolve()
    with tempfile.TemporaryDirectory(prefix="pagepet-simulator-") as folder:
        card = Path(folder)
        ensure_demo(card)
        backend = Backend(binary, card)
        try:
            state, _ = backend.ready()
            assert state["screen"] == "home" and state["selection"] == 1
            assert state["libraryCount"] == 0  # Home does not scan the card tree
            assert state["batteryKnown"] and state["battery"] == 82 and state["batteryReads"] == 1

            def key(command):
                backend.send(command)
                return backend.ready()

            state, _ = key("left")
            assert state["selection"] == 3  # language row
            state, _ = key("ok")
            assert state["language"] == 1
            key("right")
            state, _ = key("ok")
            assert state["screen"] == "library" and state["libraryCount"] == 3
            assert state["libraryRows"][0] == {"name": "Books", "folder": True}
            key("down")
            state, _ = key("ok")
            assert state["screen"] == "reading" and state["book"] == "/01-demo.epub"
            assert state["chapters"] == 2 and state["pages"] > 3
            assert state["batteryReads"] == 1
            builds, reads = state["builds"], state["sourceReads"]
            key("down")
            key("advance")
            state, reading = key("down")
            assert state["page"] == 2 and state["petPages"] == 1
            assert state["batteryReads"] == 2  # 30s elapsed; only the next render polls
            assert state["builds"] == builds and state["sourceReads"] == reads
            assert state["character"] > 0
            save_png(reading, Path("/tmp/pagepet-simulator-reading.png"))
            state, _ = key("left")
            assert state["page"] == 1
            state, _ = key("right")
            assert state["page"] == 2
            state, low = key("battery 5 0")
            assert state["battery"] == 5 and not state["charging"]
            assert state["sourceReads"] == reads
            save_png(low, Path("/tmp/pagepet-chrome-low.png"))
            state, charged = key("battery 100 1")
            assert state["battery"] == 100 and state["charging"]
            assert low != charged
            save_png(charged, Path("/tmp/pagepet-chrome-charging.png"))
            state, _ = key("battery-fail")
            assert state["batteryKnown"] and state["battery"] == 100 and not state["charging"]
            state, _ = key("battery 150 0")
            assert state["battery"] == 100
            state, _ = key("battery 0 0")
            assert state["batteryKnown"] and state["battery"] == 0

            key("card-off")
            state, _ = key("down")
            assert state["screen"] == "error"
            key("card-on")
            state, _ = key("ok")
            assert state["screen"] == "reading"  # real retry/remount path

            before = state["character"]
            state, _ = key("ok")
            assert state["screen"] == "settings"
            key("left")
            state, _ = key("ok")
            assert state["screen"] == "reading" and state["textSize"] == 0
            assert 0 < state["character"] <= before
            character = state["character"]
            state, _ = key("power")
            assert state["sleeping"]
            state, _ = key("power")  # exec/reset + actual state loading
            assert state["screen"] == "home" and not state["sleeping"]
            assert state["batteryKnown"] and state["battery"] == 0
            assert state["language"] == 1 and state["textSize"] == 0
            state, _ = key("back")
            assert state["screen"] == "reading" and state["character"] == character

            key("back")
            key("back")
            key("down")
            key("down")
            state, pet = key("ok")
            assert state["screen"] == "companion" and state["petPages"] == 1
            save_png(pet, Path("/tmp/pagepet-simulator-pet.png"))
            # Guard the real bug found by the simulator: the flash sprite
            # must be upright, centered, and above the labels in portrait.
            sprite = bytes(int(value, 16) for value in re.findall(
                r"0x([0-9a-fA-F]{2})", Path("include/DragonSprite.h").read_text()))
            bitmap = pet.split(b"\n", 2)[2]
            for row in range(256):
                start = (70 + row) * 66 + 17  # x=(528-256)/2=136
                assert bitmap[start:start + 32] == bytes(
                    byte ^ 255 for byte in sprite[row * 32:(row + 1) * 32])
            key("back")
            key("up")
            state, _ = key("ok")
            assert state["screen"] == "library"
            key("down")
            state, _ = key("ok")
            assert state["screen"] == "reading" and state["book"] == "/02-demo.txt"
            state, _ = key("restart")
            assert state["screen"] == "home"
            state, _ = key("back")
            assert state["screen"] == "reading" and state["book"] == "/02-demo.txt"
            assert (card / ".pagepet/state.dat").is_file()
            assert (card / ".pagepet/settings.dat").is_file()
            assert (card / ".pagepet/pet.dat").is_file()
        finally:
            backend.close()

    with tempfile.TemporaryDirectory(prefix="pagepet-empty-card-") as folder:
        backend = Backend(binary, Path(folder), env=dict(os.environ, PAGEPET_SIM_BATTERY="unknown"))
        try:
            state, _ = backend.ready()
            assert state["screen"] == "home" and state["bookCount"] == 0
            assert not state["batteryKnown"]
            backend.send("battery 0 0")
            state, _ = backend.ready()
            assert state["batteryKnown"] and state["battery"] == 0
            backend.send("ok")
            state, _ = backend.ready()
            assert state["screen"] == "library" and state["libraryCount"] == 0
            backend.send("back")
            backend.ready()
            backend.send("down")
            backend.ready()
            backend.send("ok")
            state, _ = backend.ready()
            assert state["screen"] == "companion"  # empty library is navigable
        finally:
            backend.close()
    test_footnotes(binary)
    test_contents(binary)
    print("Simulator application checks passed: EPUB contents and footnotes, front/side buttons, battery/charging/failures, EPUB/TXT, settings, resume, sleep, SD retry and PagePet.")


if __name__ == "__main__":
    main()
