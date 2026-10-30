#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
reader_test_dir=$(mktemp -d /tmp/pagepet-reader-tests.XXXXXX)
trap 'rm -rf "$reader_test_dir"' EXIT
sdk=third_party/freeink-sdk/libs/book/FreeInkBook
font=third_party/freeink-sdk/libs/font/FreeInkFont
includes=(-Itest/host/stubs -Isrc -Iinclude -I"$sdk/include" -I"$sdk/third_party/expat"
  -I"$sdk/third_party/miniz" -I"$sdk/third_party/libunibreak" -I"$sdk/third_party/pngle"
  -I"$sdk/third_party/tjpgd" -I"$font/include" -I"$font/third_party/stb"
  -Ithird_party/freeink-sdk/libs/ui/FreeInkUI/include)
objects=()
reader_sanitize_flags=()
if [[ ${PAGEPET_SANITIZE:-0} == 1 ]]; then
  reader_sanitize_flags=(-fsanitize=address,undefined)
fi
for vendor in miniz_impl expat_xmlparse expat_xmlrole expat_xmltok unibreak_impl pngle_impl tjpgd_impl; do
  cc -O1 -g -std=c99 "${reader_sanitize_flags[@]}" "${includes[@]}" -c "$sdk/src/vendor/$vendor.c" -o "$reader_test_dir/$vendor.o"
  objects+=("$reader_test_dir/$vendor.o")
done
sources=(src/BookFormat.cpp src/Storage.cpp src/TextReader.cpp test/host/test_reader.cpp
  "$sdk/src/BookCatalog.cpp" "$sdk/src/epub/ZipCatalog.cpp" "$sdk/src/epub/XmlSax.cpp"
  "$sdk/src/epub/PackageParsers.cpp" "$sdk/src/epub/ImageProbe.cpp" "$sdk/src/text/EntityFilter.cpp"
  "$sdk/src/text/Hyphenator.cpp" "$sdk/src/css/Css.cpp" "$sdk/src/layout/ChapterLayout.cpp"
  "$sdk/src/cache/PageCache.cpp" "$sdk/src/render/PageRenderer.cpp" "$sdk/src/render/ImageRenderer.cpp"
  "$font/src/TtfFont.cpp")
c++ -O0 -g -std=c++17 -Wall -Wextra -Werror -DFREEINK_BOOK_SMALL=1 "${reader_sanitize_flags[@]}" \
  "${includes[@]}" "${sources[@]}" "${objects[@]}" -o "$reader_test_dir/test_reader"
python3 test/host/fixtures.py "$reader_test_dir/card"
"$reader_test_dir/test_reader" "$reader_test_dir/card"
