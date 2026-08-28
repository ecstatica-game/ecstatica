# Ecstatica — DOS build (Open Watcom + DOS/4GW)
#
# This is a wmake makefile, not GNU make. Build it with Open Watcom's own
# wmake, which runs on the host:
#
#   export WATCOM=$HOME/watcom
#   export PATH=$WATCOM/bino64:$PATH
#   export INCLUDE=$WATCOM/h
#   cd dos && wmake
#
# Output is dos/ecstatic.exe, a DOS/4GW protected-mode binary. Copy it next to
# the game data along with DOS4GW.EXE from $(WATCOM)/binw.
#
# Like pocket/, this compiles ../src directly rather than going through CMake —
# the toolchain and target are fixed, so there is nothing to configure.

WATCOM_H = $(%WATCOM)/h
SRC      = ../src

CC     = wcc386
CFLAGS = -bt=dos -6s -otexan -zp1 -zq -za99 -i=$(SRC) -i=$(WATCOM_H)

# -6s   Pentium Pro scheduling, stack-based FP (no FPU instructions inlined)
# -otexan  full optimisation: time, expand inlines, loop opt, no stack frames
# -zp1  byte-align structs, matching the #pragma pack the engine relies on
# -za99 C99 mode

OBJS = init.obj asm_f.obj display.obj debug_overlay.obj edit.obj game.obj &
       file.obj move.obj menu.obj req.obj music.obj topo.obj map.obj &
       anim.obj ellipse.obj tri.obj icon.obj win.obj chars.obj main.obj &
       viewer.obj dos.obj

all: ecstatic.exe

ecstatic.exe: $(OBJS)
	%create link.lnk
	@%append link.lnk system dos4g
	@%append link.lnk name ecstatic.exe
	@%append link.lnk option quiet,map=ecstatic.map
	@for %i in ($(OBJS)) do @%append link.lnk file %i
	wlink @link.lnk

.c: $(SRC)

init.obj: $(SRC)/init.c
	$(CC) $(CFLAGS) $(SRC)/init.c -fo=$^@
asm_f.obj: $(SRC)/asm_f.c
	$(CC) $(CFLAGS) $(SRC)/asm_f.c -fo=$^@
display.obj: $(SRC)/display.c
	$(CC) $(CFLAGS) $(SRC)/display.c -fo=$^@
debug_overlay.obj: $(SRC)/debug_overlay.c
	$(CC) $(CFLAGS) $(SRC)/debug_overlay.c -fo=$^@
edit.obj: $(SRC)/edit.c
	$(CC) $(CFLAGS) $(SRC)/edit.c -fo=$^@
game.obj: $(SRC)/game.c
	$(CC) $(CFLAGS) $(SRC)/game.c -fo=$^@
file.obj: $(SRC)/file.c
	$(CC) $(CFLAGS) $(SRC)/file.c -fo=$^@
move.obj: $(SRC)/move.c
	$(CC) $(CFLAGS) $(SRC)/move.c -fo=$^@
menu.obj: $(SRC)/menu.c
	$(CC) $(CFLAGS) $(SRC)/menu.c -fo=$^@
req.obj: $(SRC)/req.c
	$(CC) $(CFLAGS) $(SRC)/req.c -fo=$^@
music.obj: $(SRC)/music.c
	$(CC) $(CFLAGS) $(SRC)/music.c -fo=$^@
topo.obj: $(SRC)/topo.c
	$(CC) $(CFLAGS) $(SRC)/topo.c -fo=$^@
map.obj: $(SRC)/map.c
	$(CC) $(CFLAGS) $(SRC)/map.c -fo=$^@
anim.obj: $(SRC)/anim.c
	$(CC) $(CFLAGS) $(SRC)/anim.c -fo=$^@
ellipse.obj: $(SRC)/ellipse.c
	$(CC) $(CFLAGS) $(SRC)/ellipse.c -fo=$^@
tri.obj: $(SRC)/tri.c
	$(CC) $(CFLAGS) $(SRC)/tri.c -fo=$^@
icon.obj: $(SRC)/icon.c
	$(CC) $(CFLAGS) $(SRC)/icon.c -fo=$^@
win.obj: $(SRC)/win.c
	$(CC) $(CFLAGS) $(SRC)/win.c -fo=$^@
chars.obj: $(SRC)/chars.c
	$(CC) $(CFLAGS) $(SRC)/chars.c -fo=$^@
main.obj: $(SRC)/main.c
	$(CC) $(CFLAGS) $(SRC)/main.c -fo=$^@
viewer.obj: $(SRC)/tools/viewer.c
	$(CC) $(CFLAGS) $(SRC)/tools/viewer.c -fo=$^@
dos.obj: $(SRC)/platforms/dos.c
	$(CC) $(CFLAGS) $(SRC)/platforms/dos.c -fo=$^@

clean: .SYMBOLIC
	@if exist *.obj del *.obj
	@if exist ecstatic.exe del ecstatic.exe
	@if exist ecstatic.map del ecstatic.map
	@if exist link.lnk del link.lnk
