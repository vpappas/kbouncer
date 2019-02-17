!IF !EXIST(root)
!IF [mklink /J root \
	"\Program Files (x86)\Microsoft Research\Detours Express 3.0"] != 0
!ERROR "cannot create link!"
!ENDIF
!ENDIF

ROOT=root

!include .\root\samples\common.mak

##############################################################################

all: dirs $(BIND)\kb.dll $(BIND)\withdll.exe

clean:
	-del *~ test.txt 2>nul
	-del $(BIND)\kb.* 2>nul
	-rmdir /q /s $(OBJD) 2>nul

allclean: clean
	cd $(ROOT)\src
	nmake clean
	cd ..\samples\withdll
	nmake clean
	cd ..\syelog
	nmake clean

dirs:
	if not exist $(BIND) mkdir $(BIND)
	if not exist $(OBJD) mkdir $(OBJD)

realclean: clean
	-rmdir /q /s $(OBJDS) 2>nul

##############################################################################

CFLAGS = $(CFLAGS) /I..\drv\exe

##############################################################################

$(INCD)\detours.h : $(LIBD)\detours.lib

$(LIBD)\detours.lib :
	cd $(ROOT)\src
	nmake
	cd $(MAKEDIR)

$(BIND)\withdll.exe : $(LIBD)\syelog.lib
	cd $(ROOT)\samples\withdll
	nmake
	cd $(MAKEDIR)

$(LIBD)\syelog.lib: 
	cd $(ROOT)\samples\syelog
	nmake
	cd $(MAKEDIR)

##############################################################################

$(OBJD)\kb.obj : kb.cpp $(INCD)\detours.h

$(OBJD)\drvctl.obj : ..\drv\exe\drvctl.c
	cl.exe /c /DDETOUR /Fo$@ $**

$(BIND)\kb.dll : $(OBJD)\kb.obj $(LIBD)\detours.lib $(OBJD)\drvctl.obj
	cl /LD $(CFLAGS) /Fe$@ /Fd$(BIND)\kb.pdb \
		$(OBJD)\kb.obj $(OBJD)\drvctl.obj \
		/link $(LINKFLAGS) /incremental:no /subsystem:console \
		/entry:$(DLLENTRY) /export:NullExport \
		$(LIBD)\detours.lib user32.lib

################################################################# End of File.

