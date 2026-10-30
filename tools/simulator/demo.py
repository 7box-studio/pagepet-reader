"""Small, original sample books for an isolated simulator SD folder."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile
from shutil import copyfile


def ensure_demo(card: Path) -> None:
    card.mkdir(parents=True, exist_ok=True)
    epub = card / "01-demo.epub"
    if not epub.exists():
        with ZipFile(epub, "w", compression=ZIP_DEFLATED) as archive:
            archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
            archive.writestr("META-INF/container.xml", (
                '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
                '<rootfiles><rootfile full-path="OPS/book.opf" '
                'media-type="application/oebps-package+xml"/></rootfiles></container>'))
            archive.writestr("OPS/book.opf", (
                '<package xmlns="http://www.idpf.org/2007/opf" version="3.0">'
                '<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
                '<dc:title>PagePet Simulator</dc:title><dc:language>ru</dc:language>'
                '</metadata><manifest>'
                '<item id="first" href="first.xhtml" media-type="application/xhtml+xml"/>'
                '<item id="second" href="second.xhtml" media-type="application/xhtml+xml"/>'
                '</manifest><spine><itemref idref="first"/><itemref idref="second"/></spine></package>'))
            for name, title, paragraph in (
                ("first", "Первая глава", "Это пробная книга PagePet. Боковые кнопки перелистывают страницы. "
                 "Подтверждение открывает настройки текста, а кнопка «Назад» возвращает к списку книг."),
                ("second", "Вторая глава", "Теперь вы читаете следующую главу EPUB. Страницы готовятся по мере "
                 "чтения и сохраняются на виртуальной карте. Можно вернуться назад и продолжить с прежнего места."),
            ):
                body = f"<h1>{title}</h1>" + "".join(f"<p>{paragraph}</p>" for _ in range(60))
                archive.writestr(f"OPS/{name}.xhtml",
                                f'<html xmlns="http://www.w3.org/1999/xhtml"><body>{body}</body></html>')
    plain = card / "02-demo.txt"
    if not plain.exists():
        plain.write_text("PagePet — пробная книга TXT\n\n" + (
            "Эта книга нужна для проверки обычного текстового чтения. "
            "Стрелки перелистывают страницы, Enter открывает настройки, Escape возвращает назад.\n\n"
        ) * 100, encoding="utf-8")
    for source, destination in ((epub, card / "Books/Проза/01-demo.epub"),
                                (plain, card / "Books/Заметки/02-demo.txt")):
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            copyfile(source, destination)
