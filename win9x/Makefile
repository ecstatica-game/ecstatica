# Ecstatica — Win9x build (Open Watcom, Win32 PE)
#
# A wmake makefile, not GNU make. Driven from the root Makefile:
#
#   make win9x        — build win9x/ecstatica.exe
#   make win9x-e2     — build + run under Wine against data/e2
#
# or directly:
#
#   export WATCOM=$HOME/watcom
#   export PATH=$WATCOM/bino64:$PATH
#   cd win9x && wmake
#
# Produces a 32-bit PE that targets Win95 through modern Windows, and runs
# under Wine for testing. The backend is src/platforms/windows.c, the same one
# the MSVC/MinGW builds use — Watcom just takes a different path through it in
# two places, because XInput does not exist on Win9x and Watcom has no
# <xinput.h>; that target uses the winmm joystick API instead.

WATCOM_H  = $(%WATCOM)/h
WATCOM_NT = $(%WATCOM)/h/nt
SRC       = ../src

CC     = wcc386

# Overridable, as in dos/Makefile: `make win9x OPT=-od` builds unoptimised.
!ifndef OPT
OPT = -otexan
!endif

CFLAGS = -bt=nt -6s $(OPT) -zp1 -zq -za99 -j -i=$(SRC) -i=$(WATCOM_H) -i=$(WATCOM_NT)

OBJS = init.obj asm_f.obj display.obj debug_overlay.obj edit.obj game.obj &
       file.obj move.obj menu.obj req.obj music.obj topo.obj map.obj &
       anim.obj ellipse.obj tri.obj icon.obj win.obj chars.obj main.obj &
       viewer.obj windows.obj desktop_common.obj

all: ecstatica.exe

ecstatica.exe: $(OBJS)
	%create link.lnk
	@%append link.lnk system nt_win
	@%append link.lnk name ecstatica.exe
	@%append link.lnk option quiet,map=ecstatica.map
	@%append link.lnk libpath $(%WATCOM)/lib386;$(%WATCOM)/lib386/nt
	@%append link.lnk library user32.lib,gdi32.lib,winmm.lib
	@for %i in ($(OBJS)) do @%append link.lnk file %i
	wlink @link.lnk

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
windows.obj: $(SRC)/platforms/windows.c
	$(CC) $(CFLAGS) $(SRC)/platforms/windows.c -fo=$^@
desktop_common.obj: $(SRC)/platforms/desktop_common.c
	$(CC) $(CFLAGS) $(SRC)/platforms/desktop_common.c -fo=$^@

clean: .SYMBOLIC
	@if exist *.obj del *.obj
	@if exist ecstatica.exe del ecstatica.exe
	@if exist ecstatica.map del ecstatica.map
	@if exist link.lnk del link.lnk
