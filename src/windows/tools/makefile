TARGETS = ps.exe proxify.exe loader.exe perf.exe time.exe
CFLAGS = /nologo

all: $(TARGETS)

.SUFFIXES: .c .exe

.c.exe:
	$(CC) $(CFLAGS) $<

perf.exe: perf.c ..\drv\exe\drvctl.c
	$(CC) $(CFLAGS) /Ox $**

clean:
	del /Q $(TARGETS) *.obj
