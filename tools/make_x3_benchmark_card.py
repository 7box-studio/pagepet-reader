#!/usr/bin/env python3
"""Create a microSD benchmark fixture archive for PagePet Reader."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


def write_member(archive, name, data, compression=ZIP_DEFLATED):
    archive.writestr(ZipInfo(name), data, compress_type=compression)


def navigation_book(source, output):
    with ZipFile(source) as original, ZipFile(output, "w") as book:
        package = original.read("OPS/book.opf").decode()
        package = package.replace(
            "</manifest>",
            '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
            '<item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/></manifest>',
        )
        chapter = original.read("OPS/c1.xhtml").decode()
        heading_end = chapter.index("</h1>") + len("</h1>")
        chapter = chapter[:heading_end] + (
            '<p><a href="notes.xhtml#long"><sup>1</sup></a> '
            '<a href="notes.xhtml#missing"><sup>2</sup></a></p>'
        ) + chapter[heading_end:]
        paragraphs = chapter.split("<p>")
        for index in range(1, 11):
            position = index * 14
            paragraphs[position] = f'<p id="section-{index}">' + paragraphs[position]
        chapter = paragraphs[0] + "".join(
            part if part.startswith("<p id=") else "<p>" + part for part in paragraphs[1:]
        )
        write_member(book, "mimetype", "application/epub+zip", ZIP_STORED)
        write_member(book, "META-INF/container.xml", original.read("META-INF/container.xml"))
        write_member(book, "OPS/book.opf", package)
        for name in original.namelist():
            if name.endswith(".xhtml"):
                write_member(book, name, chapter if name == "OPS/c1.xhtml" else original.read(name))
        sections = "".join(
            f'<li><a href="c1.xhtml#section-{index}">Section {index}</a></li>' for index in range(1, 11)
        )
        write_member(book, "OPS/nav.xhtml",
                     '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">'
                     '<head><title>Contents</title></head><body><nav epub:type="toc"><ol>'
                     '<li><a href="c1.xhtml">Start</a><ol>' + sections + '</ol></li></ol></nav></body></html>')
        write_member(book, "OPS/notes.xhtml",
                     '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Notes</title></head>'
                     '<body><h1 id="long">Long note</h1>'
                     + '<p>This footnote continues across several pages.</p>' * 90 + '</body></html>')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="path for the ZIP archive")
    args = parser.parse_args()
    fixtures = Path(__file__).resolve().parents[1] / "test/host/fixtures.py"
    with tempfile.TemporaryDirectory(prefix="pagepet-x3-") as folder:
        root = Path(folder)
        subprocess.run([sys.executable, str(fixtures), folder], check=True)
        navigation = root / "navigation.epub"
        navigation_book(root / "book.epub", navigation)
        with ZipFile(args.output, "x") as archive:
            prefix = "Books/Проверка X3/"
            for name, fixture in (
                ("01-compressed.epub", "book.epub"),
                ("02-stored.epub", "stored.EPUB"),
                ("03-illustrated.epub", "rich.epub"),
                ("04-large-chapter.epub", "large.epub"),
                ("07-contents-and-notes.epub", navigation.name),
            ):
                write_member(archive, prefix + name, (root / fixture).read_bytes())
            line = "Страница для проверки чтения на Xteink X3. Позиция сохраняется после перезапуска.\n\n"
            write_member(archive, prefix + "05-text-small.txt", line * 1200)
            write_member(archive, prefix + "06-text-large.txt", line * 14000)


if __name__ == "__main__":
    main()
