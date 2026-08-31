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
#   make e2-scenes  — same, opening on the scripted-scene browser
#   make e1-scenes  — same, opening on the scripted-scene browser
#   make clean      — remove build artifacts
#   make dump       — regenerate wdump text for E2WIN95P.EXE
#
# DOS target (Open Watcom + DOS/4GW, run under DOSBox-X):
#   make dos        — build dos/ecstatic.exe
#   make dos-e1     — build + run the E1 DOS data in DOSBox-X
#   make dos-e2     — build + run the E2 data in DOSBox-X
#   make dos-clean  — remove DOS build artifacts
#
# Win9x target (Open Watcom, 32-bit PE, run under Wine):
#   make win9x      — build win9x/ecstatica.exe
#   make win9x-e1   — build + run the E1 Win95 data under Wine
#   make win9x-e2   — build + run the E2 data under Wine
#   make win9x-clean
#
# Set WATCOM if Open Watcom is not in ~/watcom. Both cross builds live in their
# own directories and are driven by Watcom's wmake, not by this file.

TARGET   = ecstatica
SRCDIR   = src
BUILDDIR = build
BUILT    = $(BUILDDIR)/bin/$(TARGET)

E2_DIR   		= data/e2
E1_DIR   		= data/e1/W
E1_DIR_DOS   	= data/e1
E1_DIR_ORIG		= data/e1-dos

# ── DOS ───────────────────────────────────────────────────────
WATCOM     ?= $(HOME)/watcom
# Open Watcom ships a host binary directory per platform: bino64 on macOS,
# binl64 on Linux. CI builds these targets on Linux runners.
ifeq ($(shell uname -s),Darwin)
WATCOM_BIN  = $(WATCOM)/bino64
else
WATCOM_BIN  = $(WATCOM)/binl64
endif
DOS_EXE     = dos/ecstatic.exe
DOSBOX     ?= dosbox-x
# DOS/4GW has to sit next to the executable for the stub to find it.
DOS4GW      = $(WATCOM)/binw/dos4gw.exe

WIN9X_EXE   = win9x/ecstatica.exe
WINE       ?= wine

.PHONY: all run e1 e2 e1-viewer e2-viewer e1-scenes e2-scenes build clean dump \
        dos dos-e1 dos-e2 dos-clean win9x win9x-e1 win9x-e2 win9x-clean

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

e2-scenes: $(BUILT)
	cp $(BUILT) $(E2_DIR)/
	chmod +x $(E2_DIR)/$(TARGET)
	cd $(E2_DIR) && ./$(TARGET) --scenes

e1-scenes: $(BUILT)
	mkdir -p $(E1_DIR)
	cp $(BUILT) $(E1_DIR)/
	chmod +x $(E1_DIR)/$(TARGET)
	cd $(E1_DIR) && ./$(TARGET) --scenes

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

# ── DOS build and run ─────────────────────────────────────────
#
# dos/Makefile is a wmake makefile, so it is invoked through Watcom's wmake
# with WATCOM/PATH/INCLUDE set the way the toolchain expects.

dos:
	@test -x $(WATCOM_BIN)/wmake || { \
	  echo "Open Watcom not found at $(WATCOM)."; \
	  echo "Install it or pass WATCOM=/path/to/watcom."; exit 1; }
	cd dos && WATCOM=$(WATCOM) PATH=$(WATCOM_BIN):$$PATH INCLUDE=$(WATCOM)/h \
	  $(WATCOM_BIN)/wmake

$(DOS_EXE): dos

# Stage the executable and DOS/4GW into a run directory, mount the game data as
# C: and that directory as D:, and start the game. The data directory is left
# untouched — nothing is copied into it.
define dos_run
	@test -d $(1) || { echo "No game data at $(1)."; exit 1; }
	@test -f $(DOS4GW) || { echo "DOS/4GW not found at $(DOS4GW)."; exit 1; }
	@command -v $(DOSBOX) >/dev/null || { \
	  echo "$(DOSBOX) not found. Install DOSBox-X or pass DOSBOX=<path>."; exit 1; }
	@mkdir -p $(BUILDDIR)/dosrun
	@cp $(DOS_EXE) $(DOS4GW) $(BUILDDIR)/dosrun/
	@printf '[dosbox]\nmemsize=32\n[autoexec]\nmount c %s\nmount d %s\nset PATH=D:\nc:\nd:ecstatic.exe\n' \
	  "$(abspath $(1))" "$(abspath $(BUILDDIR)/dosrun)" > $(BUILDDIR)/dosrun/dosbox.conf
	$(DOSBOX) -conf $(BUILDDIR)/dosrun/dosbox.conf -nomenu
endef

dos-e1: dos
	$(call dos_run,$(E1_DIR_DOS))

dos-e1-original: dos
	$(call dos_run,$(E1_DIR_ORIG))

dos-e2: dos
	$(call dos_run,$(E2_DIR))

dos-clean:
	cd dos && rm -f *.obj *.exe *.map link.lnk
	rm -rf $(BUILDDIR)/dosrun

# ── Win9x build and run ───────────────────────────────────────
#
# The same Watcom toolchain targeting Win32. Wine runs the result for testing;
# on a real Win9x machine the executable is simply copied next to the data.

win9x:
	@test -x $(WATCOM_BIN)/wmake || { \
	  echo "Open Watcom not found at $(WATCOM)."; \
	  echo "Install it or pass WATCOM=/path/to/watcom."; exit 1; }
	cd win9x && WATCOM=$(WATCOM) PATH=$(WATCOM_BIN):$$PATH \
	  $(WATCOM_BIN)/wmake

# Wine runs with the data directory as the working directory, matching how the
# game finds its archives everywhere else.
define win9x_run
	@test -d $(1) || { echo "No game data at $(1)."; exit 1; }
	@command -v $(WINE) >/dev/null || { \
	  echo "$(WINE) not found. Install Wine or pass WINE=<path>."; exit 1; }
	cd $(1) && WINEDEBUG=-all $(WINE) $(abspath $(WIN9X_EXE))
endef

win9x-e1: win9x
	$(call win9x_run,$(E1_DIR))

win9x-e2: win9x
	$(call win9x_run,$(E2_DIR))

win9x-clean:
	cd win9x && rm -f *.obj *.exe *.map link.lnk
