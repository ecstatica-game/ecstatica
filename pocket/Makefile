#------------------------------------------------------------------------------
# Ecstatica — Analogue Pocket / MiSTer (openfpgaOS) build & deploy
#------------------------------------------------------------------------------
#
#   make                       build app.elf
#   make iso                   pack game data into ISO images
#   make release               assemble build/ as a Pocket SD card tree
#   make deploy                release, then rsync onto the SD card
#   make debug                 push the ELF over UART (PHDP)
#   make test                  desktop SDL2 build via the SDK's PC backend
#
# Common overrides:
#   OF_SDK=<path>   openfpgaSDK checkout           (default ../../openfpgaSDK)
#   AUTHOR=<name>   core author, sets the core id  (default xesf)
#   SDCARD=<path>   SD mount point                 (default: auto-detect)
#   E1_DATA=<dir>   Ecstatica 1 data directory     (default ../data/e1-dos)
#   E2_DATA=<dir>   Ecstatica 2 data directory     (default ../data/e2)
#
# The SDK compiles RISC-V in a Docker image by default; add USE_SDK_CONTAINER=0
# to use a host riscv64-unknown-elf toolchain instead.

OF_SDK   ?= ../../openfpgaSDK
AUTHOR   ?= xesf
VERSION  ?= 0.1.0
PLATFORM := ecstatica
CORE_ID  := $(AUTHOR).Ecstatica

APP      := ecstatica
APP_DIR  := $(OF_SDK)/src/apps/$(APP)
ELF      := $(OF_SDK)/.obj/sdk/$(APP)/app.elf
RUNTIME  := $(OF_SDK)/runtime
SRC_DIR  := $(CURDIR)/../src
OUT      := $(CURDIR)/build

ENGINE_SRCS  := $(wildcard $(SRC_DIR)/*.c)
ENGINE_HDRS  := $(wildcard $(SRC_DIR)/*.h)
PLATFORM_SRC := $(SRC_DIR)/platforms/openfpga.c

#
# -fsigned-char matters: `char` is unsigned on RISC-V but signed on the x86
# the original Watcom binary targeted, and the engine stores signed data in
# plain char arrays (shade_tab, shadow_tab, the bitmap planes).
APP_CFLAGS := -DOF_POCKET -fwrapv -fsigned-char \
              -Wno-unused-parameter -Wno-missing-field-initializers \
              -Wno-sign-compare -Wno-unused-but-set-variable \
              -Wno-char-subscripts

# music.c hands platform_midi_play a Standard MIDI File, so the app links
# the SDK's software synth (sample-based, driven by an .ofsf bank).
MIDI_SRCS := $$(SDK_DIR)/of_midi.c $$(SDK_DIR)/of_smp_bank.c \
             $$(SDK_DIR)/of_smp_voice.c $$(SDK_DIR)/of_smp_tables.c

# data/e1 is the CD layout: a DOS install with the Win95 one nested in W/.
# That combination boots at 320x200 (the root database decides) while still
# carrying the 640x480 set the in-game graphics toggle switches to. Point
# E1_DATA at data/e1-dos for the plain DOS release with no enhanced mode.
E1_DATA ?= ../data/e1
E2_DATA ?= ../data/e2
E1_ISO  := $(CURDIR)/ecstatica.iso
E2_ISO  := $(CURDIR)/ecstatica2.iso

.DEFAULT_GOAL := all
.PHONY: all sync elf iso iso-e1 iso-e2 release deploy test debug clean distclean check-sdk

check-sdk:
	@test -f "$(OF_SDK)/src/sdk/sdk.mk" || { \
		echo "openfpgaSDK not found at $(OF_SDK)"; \
		echo "Clone https://github.com/openfpgaOS/openfpgaSDK and pass OF_SDK=<path>"; \
		exit 1; }

# ── Build ────────────────────────────────────────────────────────────
# The SDK builds RISC-V from a directory inside its own tree (the build
# container mounts only the SDK checkout), so the engine sources are copied
# there flat — symlinks out of the tree would not resolve.
sync: check-sdk
	@mkdir -p "$(APP_DIR)"
	@cp $(ENGINE_SRCS) $(ENGINE_HDRS) "$(APP_DIR)/"
	@cp $(PLATFORM_SRC) "$(APP_DIR)/"
	@printf 'SDK_DIR = ../../sdk\n' > "$(APP_DIR)/Makefile"
	@printf 'CFLAGS += %s\n' '$(APP_CFLAGS)' >> "$(APP_DIR)/Makefile"
	@printf 'SRCS = $$(wildcard *.c) %s\n' '$(MIDI_SRCS)' >> "$(APP_DIR)/Makefile"
	@printf 'include ../app.mk\n' >> "$(APP_DIR)/Makefile"
	@echo "staged $(words $(ENGINE_SRCS)) engine sources + openfpga.c -> $(APP_DIR)"

all elf: sync
	@$(MAKE) -C "$(APP_DIR)"

test: sync
	@$(MAKE) -C "$(APP_DIR)" test

debug: sync
	@$(MAKE) -C "$(APP_DIR)" debug

# ── Game data ────────────────────────────────────────────────────────
iso-e1:
	@test -d "$(E1_DATA)" || { echo "no E1 data at $(E1_DATA) — set E1_DATA="; exit 1; }
	@./mkiso.sh "$(E1_DATA)" "$(E1_ISO)" $(ISOFLAGS)

iso-e2:
	@test -d "$(E2_DATA)" || { echo "no E2 data at $(E2_DATA) — set E2_DATA="; exit 1; }
	@./mkiso.sh "$(E2_DATA)" "$(E2_ISO)" $(ISOFLAGS)

# Build whichever data sets are actually present.
iso:
	@built=0; \
	if [ -d "$(E1_DATA)" ]; then $(MAKE) iso-e1 && built=1; fi; \
	if [ -d "$(E2_DATA)" ]; then $(MAKE) iso-e2 && built=1; fi; \
	if [ $$built -eq 0 ]; then \
		echo "no game data found at $(E1_DATA) or $(E2_DATA)"; exit 1; fi

# ── SD card image ────────────────────────────────────────────────────
# Assembles build/ in the layout the Pocket expects:
#
#   Cores/<author>.Ecstatica/     bitstream, loader, core JSON
#   Platforms/ecstatica.json      platform metadata
#   Assets/ecstatica/common/      os.bin, os.ini, ELF, soundfont, ISOs
#   Assets/ecstatica/<core id>/   one instance JSON per game
#
# ISOs are copied only if built — they are ~135 MB each, so `make release`
# stays useful for iterating on the ELF alone.
release: elf
	@test -f "$(RUNTIME)/os.bin" || { \
		echo "$(RUNTIME)/os.bin missing — this SDK checkout has no runtime/"; exit 1; }
	@rm -rf "$(OUT)"
	@mkdir -p "$(OUT)/Cores/$(CORE_ID)" \
	          "$(OUT)/Platforms" \
	          "$(OUT)/Assets/$(PLATFORM)/common" \
	          "$(OUT)/Assets/$(PLATFORM)/$(CORE_ID)"

	@# Core: FPGA bitstream + chip32 loader from the SDK runtime, JSON from core/.
	@cp "$(RUNTIME)/bitstream.rbf_r" "$(RUNTIME)/loader.bin" "$(OUT)/Cores/$(CORE_ID)/"
	@for f in data video audio input interact variants; do \
		cp "core/$$f.json" "$(OUT)/Cores/$(CORE_ID)/"; done
	@sed -e 's/{{AUTHOR}}/$(AUTHOR)/g' \
	     -e 's/{{VERSION}}/$(VERSION)/g' \
	     -e 's/{{DATE}}/$(shell date +%Y-%m-%d)/g' \
	     core/core.json > "$(OUT)/Cores/$(CORE_ID)/core.json"
	@# 36x36 core icon. Every shipped openfpgaOS core carries one; generate
	@# ours if it is missing rather than leaving the only structural
	@# difference from a known-good core in place.
	@test -f core/icon.bin || python3 tools/mkicon.py core/icon.bin
	@cp core/icon.bin "$(OUT)/Cores/$(CORE_ID)/"

	@cp core/platform.json "$(OUT)/Platforms/$(PLATFORM).json"
	@# 521x165 platform banner. Supply core/platform_image.bin for custom
	@# art; otherwise reuse the generic openfpgaOS one from the SDK, which
	@# is exactly what the stock cores ship.
	@mkdir -p "$(OUT)/Platforms/_images"
	@if [ -f core/platform_image.bin ]; then \
		cp core/platform_image.bin "$(OUT)/Platforms/_images/$(PLATFORM).bin"; \
	elif [ -f "$(OF_SDK)/dist/sdk/Platforms/_images/openfpgaos.bin" ]; then \
		cp "$(OF_SDK)/dist/sdk/Platforms/_images/openfpgaos.bin" \
		   "$(OUT)/Platforms/_images/$(PLATFORM).bin"; \
	else echo "  ! no platform banner — Platforms/_images/$(PLATFORM).bin absent"; fi

	@# Assets/common: what the OS and app load at boot.
	@cp "$(RUNTIME)/os.bin" "$(OUT)/Assets/$(PLATFORM)/common/"
	@cp core/os.ini "$(OUT)/Assets/$(PLATFORM)/common/"
	@cp "$(ELF)" "$(OUT)/Assets/$(PLATFORM)/common/$(APP).elf"
	@if [ -f "$(RUNTIME)/bank.ofsf" ]; then \
		cp "$(RUNTIME)/bank.ofsf" "$(OUT)/Assets/$(PLATFORM)/common/"; \
	else echo "  ! no bank.ofsf in runtime/ — music will be silent"; fi

	@# One instance JSON per game; the Pocket browser lists these by filename.
	@if [ -f "$(E1_ISO)" ]; then \
		cp "$(E1_ISO)" "$(OUT)/Assets/$(PLATFORM)/common/"; \
		cp core/instance.e1.json "$(OUT)/Assets/$(PLATFORM)/$(CORE_ID)/Ecstatica.json"; \
	else echo "  ! $(notdir $(E1_ISO)) not built — run 'make iso-e1'"; fi
	@if [ -f "$(E2_ISO)" ]; then \
		cp "$(E2_ISO)" "$(OUT)/Assets/$(PLATFORM)/common/"; \
		cp core/instance.e2.json "$(OUT)/Assets/$(PLATFORM)/$(CORE_ID)/Ecstatica2.json"; \
	else echo "  ! $(notdir $(E2_ISO)) not built — run 'make iso-e2'"; fi

	@echo "Ready: $(OUT) ($$(du -sh '$(OUT)' | cut -f1))"

deploy: release
	@./deploy.sh "$(OUT)" "$(SDCARD)"

# ── Clean ────────────────────────────────────────────────────────────
clean:
	@test -d "$(APP_DIR)" && $(MAKE) -C "$(APP_DIR)" clean || true
	@rm -rf "$(OUT)"

distclean: clean
	@rm -rf "$(APP_DIR)" "$(E1_ISO)" "$(E2_ISO)"
