# Ecstatica — root Makefile
#
# Builds the C99 port using CMake, then stages the binary into
# data/e1 or data/e2 for a native run.
#
# Usage:
#   make            — build ecstatica binary
#   make run        — alias for `make e2`
#   make e2         — build + copy to data/e2 + run
#   make e1         — build + copy to data/e1/W + run
#   make e2-viewer  — same as e2, but opens the model/animation viewer
#   make e1-viewer  — same as e1, but opens the model/animation viewer
#   make clean      — remove build artifacts
#   make dump       — regenerate wdump text for E2WIN95P.EXE

TARGET   = ecstatica
SRCDIR   = src
BUILDDIR = build
BUILT    = $(BUILDDIR)/bin/$(TARGET)

E2_DIR   		= data/e2
E1_DIR   		= data/e1/W
E1_DIR_DOS   	= data/e1
E1_DIR_ORIG		= data/e1-dos

.PHONY: all run e1 e2 e1-viewer e2-viewer build clean dump

all: build

build:
	cmake -S $(SRCDIR) -B $(BUILDDIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILDDIR)

$(BUILT): build

run: e2

e2: $(BUILT)
	cp $(BUILT) $(E2_DIR)/
	chmod +x $(E2_DIR)/$(TARGET)
	cd $(E2_DIR) && ./$(TARGET)

e1: $(BUILT)
	mkdir -p $(E1_DIR)
	cp $(BUILT) $(E1_DIR)/
	chmod +x $(E1_DIR)/$(TARGET)
	cd $(E1_DIR) && ./$(TARGET)

e2-viewer: $(BUILT)
	cp $(BUILT) $(E2_DIR)/
	chmod +x $(E2_DIR)/$(TARGET)
	cd $(E2_DIR) && ./$(TARGET) --viewer

e1-viewer: $(BUILT)
	mkdir -p $(E1_DIR)
	cp $(BUILT) $(E1_DIR)/
	chmod +x $(E1_DIR)/$(TARGET)
	cd $(E1_DIR) && ./$(TARGET) --viewer

e1-dos-bundle: $(BUILT)
	mkdir -p $(E1_DIR_DOS)
	cp $(BUILT) $(E1_DIR_DOS)/
	chmod +x $(E1_DIR_DOS)/$(TARGET)
	cd $(E1_DIR_DOS) && ./$(TARGET)

e1-dos: $(BUILT)
	mkdir -p $(E1_DIR_ORIG)
	cp $(BUILT) $(E1_DIR_ORIG)/
	chmod +x $(E1_DIR_ORIG)/$(TARGET)
	cd $(E1_DIR_ORIG) && ./$(TARGET)

clean:
	rm -rf $(BUILDDIR)

dump:
	wdump -Dx $(E2_DIR)/E2WIN95P.EXE > decomp/E2WIN95P.EXE.orig.txt
