import pathlib
import sys
import zipfile
import struct
import zlib

root = pathlib.Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
container = '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'

def epub(name, chapters, compression=zipfile.ZIP_DEFLATED, encrypted=False, css=None, images=None):
    manifest = ''.join(f'<item id="c{i}" href="c{i}.xhtml" media-type="application/xhtml+xml"/>' for i in range(len(chapters)))
    if css:
        manifest += '<item id="css" href="style.css" media-type="text/css"/>'
    spine = ''.join(f'<itemref idref="c{i}"/>' for i in range(len(chapters)))
    package = f'<package xmlns="http://www.idpf.org/2007/opf" version="3.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Host fixture</dc:title><dc:language>ru</dc:language></metadata><manifest>{manifest}</manifest><spine>{spine}</spine></package>'
    with zipfile.ZipFile(root / name, 'w', compression=compression) as z:
        z.writestr('mimetype', 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
        z.writestr('META-INF/container.xml', container)
        z.writestr('OPS/book.opf', package)
        for i, chapter in enumerate(chapters):
            z.writestr(f'OPS/c{i}.xhtml', chapter)
        if css:
            z.writestr('OPS/style.css', css)
        for path, data in (images or {}).items():
            z.writestr('OPS/' + path, data)
        if encrypted:
            z.writestr('META-INF/encryption.xml', '<encryption xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><EncryptedData xmlns="http://www.w3.org/2001/04/xmlenc#"><EncryptionMethod Algorithm="http://www.w3.org/2001/04/xmlenc#aes256-cbc"/></EncryptedData></encryption>')

def xhtml(text):
    return f'<html xmlns="http://www.w3.org/1999/xhtml"><body>{text}</body></html>'

empty = xhtml('')
long = xhtml('<h1>Первая глава</h1>' + '<p>Русский текст &amp; English text for pagination and resume. </p>' * 160)
second = xhtml('<p>Second chapter with <sup><a href="c1.xhtml#note">1</a></sup>.</p>')
for name, compression in [('book.epub', zipfile.ZIP_DEFLATED), ('stored.EPUB', zipfile.ZIP_STORED)]:
    epub(name, [empty, long, empty, second, empty], compression)
epub('broken.epub', [xhtml('<p>Good chapter</p>'), '<html><body><p>Unclosed'])
epub('encrypted.epub', [second], encrypted=True)
epub('large.epub', [xhtml('<p>Large chapter with bounded working memory.</p>' * 18000)])
epub('many.epub', [xhtml('<p>Chapter</p>')] * 1700)
epub('too-many.epub', [xhtml('<p>Chapter</p>')] * 3000)

def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 8, 8, 8, 2, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress((b'\x00' + b'\x00\x00\x00' * 8) * 8)) + chunk(b'IEND', b'')
epub('rich.epub', [xhtml('<p>Illustration &mdash; <a href="#note"><sup>1</sup></a></p><img src="image.png"/><p id="note">Note text</p>')], css='p { text-indent: 1em; }', images={'image.png': png})
# Same-size, stored containers test CRC-based invalidation without relying on FAT timestamps.
epub('replace.epub', [xhtml('<p>' + 'AAAA ' * 20 + '</p>')], zipfile.ZIP_STORED)
epub('replace-new.epub', [xhtml('<p>' + 'BBBB ' * 20 + '</p>')], zipfile.ZIP_STORED)
(root / 'invalid.epub').write_bytes(b'not a zip archive')
(root / 'plain.txt').write_text('Plain UTF-8 text.\n\n' * 1000)
