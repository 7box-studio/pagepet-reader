#!/usr/bin/env python3
"""Folder navigation and bounded list paging through the real firmware UI."""
from pathlib import Path
import shutil
import tempfile

from client import Backend, save_png
from demo import ensure_demo


def main():
    binary = Path(".cache/simulator/pagepet-simulator").resolve()
    with tempfile.TemporaryDirectory(prefix="pagepet-folders-") as folder:
        card = Path(folder)
        nested = card / "A/Проза"
        nested.mkdir(parents=True)
        (card / "Empty").mkdir()
        (card / ".hidden").mkdir()
        (card / "ignored.pdf").write_text("not a supported book")
        (card / ".secret.txt").write_text("hidden")
        text = "Пробная книга для проверки папок и восстановления позиции.\n\n" * 120
        (nested / "книга один.TXT").write_text(text)
        for i in range(31):
            (card / f"T{i:02}.txt").write_text(text)
        backend = Backend(binary, card)
        state, _ = backend.ready()

        def key(command):
            nonlocal state
            backend.send(command)
            state, pbm = backend.ready()
            return pbm

        def selected():
            index = state["librarySelection"] - state["libraryFirst"]
            return state["libraryRows"][index]

        def enter(name):
            assert state["screen"] == "library"
            for _ in range(state["libraryCount"] + 1):
                if selected()["name"] == name:
                    return key("ok")
                key("down")
            raise AssertionError(f"Entry missing: {name}")

        try:
            assert state["screen"] == "home" and state["selection"] == 1
            assert state["libraryRows"] == []  # no recursive indexing at boot
            key("ok")
            assert state["screen"] == "library" and state["libraryPath"] == "/"
            assert state["libraryCount"] == 33
            assert len(state["libraryRows"]) == 8
            assert [e["name"] for e in state["libraryRows"][:2]] == ["A", "Empty"]
            key("up")  # wrap straight to the last page
            assert state["librarySelection"] == 32 and state["libraryFirst"] == 32
            assert len(state["libraryRows"]) == 1 and selected()["name"] == "T30.txt"
            save_png(key("snapshot"), Path("/tmp/pagepet-library-last.png"))
            key("ok")
            assert state["screen"] == "reading" and state["book"] == "/T30.txt"
            key("back")
            assert state["screen"] == "library" and state["librarySelection"] == 32

            enter("A")
            assert state["libraryPath"] == "/A"
            enter("Проза")
            assert state["libraryPath"] == "/A/Проза"
            save_png(key("snapshot"), Path("/tmp/pagepet-library-folder.png"))
            enter("книга один.TXT")
            assert state["screen"] == "reading" and state["book"] == "/A/Проза/книга один.TXT"
            key("down")
            position = state["character"]
            assert position > 0
            key("power")
            assert state["sleeping"]
            key("power")
            assert state["screen"] == "home" and state["selection"] == 0
            key("ok")  # visible Continue reading row, not just the Back shortcut
            assert state["screen"] == "reading" and state["character"] == position
            assert state["book"] == "/A/Проза/книга один.TXT"
            key("back")
            assert state["libraryPath"] == "/A/Проза" and selected()["name"] == "книга один.TXT"
            key("back")
            assert state["libraryPath"] == "/A" and selected()["name"] == "Проза"
            key("back")
            assert state["libraryPath"] == "/" and selected()["name"] == "A"
            enter("Empty")
            assert state["libraryCount"] == 0
            key("ok")  # empty folder's Confirm is harmless
            assert state["screen"] == "library"
            key("back")
            assert state["libraryPath"] == "/" and selected()["name"] == "Empty"

            # Offline directory traversal fails at a page boundary, then retries
            # that same folder after the card is returned.
            key("up")  # A, row 0
            key("card-off")
            for _ in range(8):
                key("down")
            assert state["screen"] == "error"
            key("card-on")
            key("ok")
            assert state["screen"] == "library" and state["libraryPath"] == "/"
            assert state["librarySelection"] == 8 and state["libraryFirst"] == 8

            # Selecting a folder that disappeared must offer an exit to Home;
            # reopening Library afterwards must not be trapped in that path.
            for _ in range(state["libraryCount"]):
                if selected()["name"] == "A":
                    break
                key("down")
            shutil.rmtree(card / "A")
            key("ok")
            assert state["screen"] == "error"
            key("back")
            assert state["screen"] == "home"
            while state["selection"] != 1:
                key("down")
            key("ok")
            assert state["screen"] == "library" and state["libraryPath"] == "/"
            assert state["libraryCount"] == 32

            # Reject an overlong nested path explicitly; never open a
            # truncated sibling or write a truncated saved position.
            long_name = "a" * 100
            (card / long_name / ("b" * 40)).mkdir(parents=True)
            key("back")
            key("ok")
            enter(long_name)
            assert state["libraryPath"] == "/" + long_name
            enter("b" * 40)
            assert state["screen"] == "error" and "long" in state["error"]
            key("back")
            assert state["screen"] == "home"
            key("ok")
            assert state["screen"] == "library" and state["libraryPath"] == "/" + long_name
            key("back")
            assert state["libraryPath"] == "/" and selected()["name"] == long_name
        finally:
            backend.close()
    with tempfile.TemporaryDirectory(prefix="pagepet-folder-epub-") as folder:
        card = Path(folder)
        ensure_demo(card)
        backend = Backend(binary, card)
        try:
            state, _ = backend.ready()

            def press(command):
                backend.send(command)
                return backend.ready()

            press("ok")  # Library
            press("ok")  # Books
            press("down")  # Проза (folders first, card order in this fixture)
            state, _ = press("ok")
            assert state["libraryPath"] == "/Books/Проза"
            state, _ = press("ok")
            assert state["screen"] == "reading" and state["book"] == "/Books/Проза/01-demo.epub"
            state, _ = press("down")
            character = state["character"]
            press("power")
            press("power")
            state, _ = press("ok")
            assert state["book"] == "/Books/Проза/01-demo.epub" and state["character"] == character
            state, _ = press("back")
            assert state["screen"] == "library" and state["libraryPath"] == "/Books/Проза"
            press("back")  # Books
            press("back")  # /
            press("down")  # root 01-demo.epub, same name as nested book
            state, _ = press("ok")
            assert state["book"] == "/01-demo.epub"
            assert len(list((card / ".pagepet/cache").iterdir())) == 2  # independent full-path identities
        finally:
            backend.close()
    print("Library checks passed: 33 entries, eight-row paging, nested UTF-8 paths, resume, parent selection, empty folders, nested EPUB, separate cache identities, SD errors and path limits.")


if __name__ == "__main__":
    main()
