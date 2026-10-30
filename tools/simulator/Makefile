# A native build of the firmware UI and reading engine; no ESP32 toolchain.
.DEFAULT_GOAL := all
SIM_BUILD ?= .cache/simulator
CXX ?= c++
CC ?= cc
SDK := third_party/freeink-sdk/libs/book/FreeInkBook
FONT := third_party/freeink-sdk/libs/font/FreeInkFont
UI := third_party/freeink-sdk/libs/ui/FreeInkUI
PET := third_party/pagepet
CPPFLAGS := -DPAGEPET_SIMULATOR=1 -DFREEINK_BOOK_SMALL=1 \
  -Itools/simulator/stubs -Itest/host/stubs -Isrc -Iinclude \
  -I$(SDK)/include -I$(SDK)/third_party/expat -I$(SDK)/third_party/miniz \
  -I$(SDK)/third_party/libunibreak -I$(SDK)/third_party/pngle -I$(SDK)/third_party/tjpgd \
  -I$(FONT)/include -I$(FONT)/third_party/stb -I$(UI)/include -I$(PET)/include
CXXFLAGS := -O0 -g -std=c++17 -Wall -Wextra -Werror -MMD -MP
CFLAGS := -O1 -g -std=c99 -MMD -MP
SOURCES := tools/simulator/runner.cpp src/BookFormat.cpp src/Storage.cpp src/TextReader.cpp src/ReaderChrome.cpp src/LibraryBrowser.cpp \
  $(SDK)/src/BookCatalog.cpp $(SDK)/src/epub/ZipCatalog.cpp $(SDK)/src/epub/XmlSax.cpp \
  $(SDK)/src/epub/PackageParsers.cpp $(SDK)/src/epub/ImageProbe.cpp $(SDK)/src/text/EntityFilter.cpp \
  $(SDK)/src/text/Hyphenator.cpp $(SDK)/src/css/Css.cpp $(SDK)/src/layout/ChapterLayout.cpp \
  $(SDK)/src/cache/PageCache.cpp $(SDK)/src/render/PageRenderer.cpp $(SDK)/src/render/ImageRenderer.cpp \
  $(FONT)/src/TtfFont.cpp $(UI)/src/FreeInkUI.cpp \
  $(PET)/src/companion_engine.cpp $(PET)/src/progression.cpp $(PET)/src/state_codec.cpp
VENDORS := miniz_impl expat_xmlparse expat_xmlrole expat_xmltok unibreak_impl pngle_impl tjpgd_impl
OBJECTS := $(addprefix $(SIM_BUILD)/obj/,$(SOURCES:.cpp=.o)) \
  $(addprefix $(SIM_BUILD)/obj/$(SDK)/src/vendor/,$(addsuffix .o,$(VENDORS)))

.PHONY: all
all: $(SIM_BUILD)/pagepet-simulator

$(SIM_BUILD)/pagepet-simulator: $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@

$(SIM_BUILD)/obj/%.o: %.cpp tools/simulator/Makefile
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(SIM_BUILD)/obj/%.o: %.c tools/simulator/Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

-include $(OBJECTS:.o=.d)
