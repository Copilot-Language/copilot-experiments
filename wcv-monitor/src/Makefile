CFLAGS=-I../include -Wall -Wpedantic --std=c99

player: player.c

listener: listener.c copilot-sbv-codegen/driver.c copilot-sbv-codegen/internal.a

# Probably can remove the CC/CCFLAGS= if compcert is installed.
copilot-sbv-codegen/internal.a:
	make internal.a CC="${CC}" CCFLAGS="${CCFLAGS}" -C copilot-sbv-codegen

clean:
	-rm *.o
	-rm player
	-rm listener
