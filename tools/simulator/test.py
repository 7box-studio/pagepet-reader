#!/usr/bin/env python3
"""End-to-end button tests against the native build of src/main.cpp."""
from pathlib import Path
import re
import os
import tempfile

from client import Backend, save_png
from demo import ensure_demo


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
    print("Simulator application checks passed: front/side buttons, battery/charging/failures, EPUB/TXT, settings, resume, sleep, SD retry and PagePet.")


if __name__ == "__main__":
    main()
