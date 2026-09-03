#------------------------------------------------------------------------------
# Ecstatica — PlayStation Portable build
#------------------------------------------------------------------------------
#
#   make                build EBOOT.PBP
#   make release        stage build/ as a PSP/GAME folder
#   make install        release, then copy onto a Memory Stick
#   make clean
#
# Common overrides:
#   PSPSDK=<path>   PSP SDK path        (default: `psp-config --pspsdk-path`)
#   GAME_DATA=<dir> data to ship        (default ../data/e2)
#   MSTICK=<path>   Memory Stick mount  (default /Volumes/PSP on macOS)
#
# Like dos/ and pocket/, this compiles ../src directly instead of going through
# CMake — the toolchain and the target are both fixed, so there is nothing to
# configure. Objects land in obj/, never beside the sources.
#
# Toolchain: pspdev/psptoolchain (psp-gcc, newlib, pspsdk). On macOS:
#   brew install pspdev/pspdev/pspdev     # or build psptoolchain from source
#   export PSPDEV=/usr/local/pspdev; export PATH=$PSPDEV/bin:$PATH

TARGET      = ecstatica
SRC_DIR    := ../src
OBJ        := obj

PSPSDK := $(shell psp-config --pspsdk-path 2>/dev/null)
ifeq ($(PSPSDK),)
$(error psp-config not found — install the PSP toolchain and put $$PSPDEV/bin on PATH)
endif

ENGINE_SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/tools/*.c) \
               $(SRC_DIR)/platforms/psp.c
OBJS        := $(patsubst $(SRC_DIR)/%.c,$(OBJ)/%.o,$(ENGINE_SRCS))

INCDIR   = $(SRC_DIR)
LIBDIR   =
LIBS     = -lpspgu -lpspge -lpspdisplay -lpspctrl -lpspaudio -lpsppower -lm

# -G0 is mandatory for PSP libraries: the small-data area is not shared between
# the SDK's objects and ours, and mixing them mislinks silently.
#
# -fsigned-char: `char` is unsigned on MIPS but signed on the x86 the original
# Watcom binary targeted, and the engine keeps signed data in plain char arrays
# (shade_tab, shadow_tab, the bitmap planes).
CFLAGS  = -O2 -G0 -Wall -Wextra -fwrapv -fsigned-char \
          -Wno-unused-parameter -Wno-missing-field-initializers \
          -Wno-sign-compare -Wno-unused-but-set-variable -Wno-char-subscripts
ASFLAGS = $(CFLAGS)

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = Ecstatica

include $(PSPSDK)/lib/build.mak

# build.mak has its own %.o: %.c rule that would write objects next to the
# sources in ../src. This more specific pattern wins for everything in OBJS.
$(OBJ)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Packaging ────────────────────────────────────────────────────────
# A PSP game is a folder under PSP/GAME containing EBOOT.PBP. The engine reads
# every asset relative to the folder the EBOOT was launched from, so the game
# data goes in beside it — exactly the layout the desktop builds run from.
GAME_DATA ?= ../data/e1
STAGE     := build/PSP/GAME/ECSTATICA

release: EBOOT.PBP
	@test -d "$(GAME_DATA)" || { \
	  echo "no game data at $(GAME_DATA) — set GAME_DATA=<dir>"; exit 1; }
	@rm -rf build
	@mkdir -p "$(STAGE)"
	@cp EBOOT.PBP "$(STAGE)/"
	@cp -R "$(GAME_DATA)/" "$(STAGE)/"
	@# The desktop binaries get staged into the data directories by the root
	@# Makefile; they have no business on a Memory Stick.
	@rm -f "$(STAGE)/ecstatica" "$(STAGE)/ecstatica_debug.log"
	@echo "Ready: build/ ($$(du -sh build | cut -f1))"

MSTICK ?= /Volumes/PSP
CARD   := $(MSTICK)/PSP/GAME/ECSTATICA

# Push the game onto a Memory Stick. The EBOOT is always refreshed, but the
# game data — hundreds of MB that never change between code builds — is copied
# only when the card does not already carry a database (CODE/ECSTATIC.FAN is
# the same marker the engine probes for). So the first install is slow and
# every reinstall after that is a one-second EBOOT swap. Force a data refresh
# with `make install FORCE_DATA=1`.
install: EBOOT.PBP
	@test -d "$(MSTICK)" || { \
	  echo "no Memory Stick at $(MSTICK) — set MSTICK=<mount point>"; exit 1; }
	@mkdir -p "$(CARD)"
	cp EBOOT.PBP "$(CARD)/"
	@if [ -n "$(FORCE_DATA)" ] || [ ! -f "$(CARD)/CODE/ECSTATIC.FAN" ]; then \
	  test -d "$(GAME_DATA)" || { \
	    echo "no game data at $(GAME_DATA) — set GAME_DATA=<dir>"; exit 1; }; \
	  echo "copying game data from $(GAME_DATA) ..."; \
	  cp -R "$(GAME_DATA)/" "$(CARD)/"; \
	  rm -f "$(CARD)/ecstatica" "$(CARD)/ecstatica_debug.log"; \
	else \
	  echo "game data already on card — EBOOT only (FORCE_DATA=1 to refresh)"; \
	fi
	@echo "Installed to $(CARD)"

# build.mak's clean only knows about $(OBJS) and the ELF/PBP; obj/ and build/
# are ours. Extra prerequisite, so its recipe still runs.
clean: psp-clean-dirs

psp-clean-dirs:
	@rm -rf $(OBJ) build

.PHONY: release install psp-clean-dirs
