# Makefile for JISKeyMapper (nmake)
#
# Usage (x64 Native Tools Command Prompt for VS):
#   nmake          - Release build
#   nmake DEBUG=1  - Debug build
#   nmake clean    - Remove generated files
#   nmake rebuild  - clean then build

TARGET   = JISKeyMapper.exe
BUILDDIR = build
RC_SRC   = src\JISKeyMapper.rc
MANIFEST = src\JISKeyMapper.exe.manifest
RC_OBJ   = $(BUILDDIR)\JISKeyMapper.res

OBJS = \
    $(BUILDDIR)\Main.obj \
    $(BUILDDIR)\Devices.obj \
    $(BUILDDIR)\Hook.obj \
    $(BUILDDIR)\KeyMap.obj \
    $(BUILDDIR)\Settings.obj \
    $(BUILDDIR)\StringUtil.obj \
    $(BUILDDIR)\Tray.obj

# Any header change rebuilds all objects (the project is small).
HEADERS = \
    src\Common.h \
    src\Devices.h \
    src\Hook.h \
    src\KeyMap.h \
    src\Settings.h \
    src\StringUtil.h \
    src\Tray.h

ICONS = res\app.ico res\on_jp.ico res\off_jp.ico res\on_us.ico res\off_us.ico

CC = cl.exe
RC = rc.exe
LD = link.exe
LIBS = user32.lib shell32.lib setupapi.lib hid.lib comctl32.lib
CFLAGS_COMMON = /nologo /EHsc /W3 /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN

!IFDEF DEBUG
CFLAGS = $(CFLAGS_COMMON) /MTd /Od /Zi /RTC1 /DDEBUG
LFLAGS = /nologo /DEBUG /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:$(MANIFEST) /MANIFESTUAC:NO
!ELSE
CFLAGS = $(CFLAGS_COMMON) /MT /O2 /GL /DNDEBUG
LFLAGS = /nologo /LTCG /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:$(MANIFEST) /MANIFESTUAC:NO
!ENDIF

all: $(BUILDDIR) $(TARGET)
	@echo Build succeeded: $(TARGET)

$(BUILDDIR):
	@if not exist $(BUILDDIR) mkdir $(BUILDDIR)

$(TARGET): $(OBJS) $(RC_OBJ) $(MANIFEST)
	$(LD) $(LFLAGS) /OUT:$(TARGET) $(OBJS) $(RC_OBJ) $(LIBS)

# Compile src\*.cpp into build\*.obj (paths must match SRC/BUILDDIR above)
{src}.cpp{build}.obj:
	$(CC) $(CFLAGS) /c /Fo$@ $<

$(OBJS): $(HEADERS)

$(RC_OBJ): $(RC_SRC) $(ICONS)
	$(RC) /nologo /fo$(RC_OBJ) $(RC_SRC)

clean:
	@if exist $(TARGET)         del /q $(TARGET)
	@if exist $(BUILDDIR)\*.obj del /q $(BUILDDIR)\*.obj
	@if exist $(BUILDDIR)\*.res del /q $(BUILDDIR)\*.res
	@if exist $(BUILDDIR)\*.pdb del /q $(BUILDDIR)\*.pdb
	@if exist $(BUILDDIR)\*.ilk del /q $(BUILDDIR)\*.ilk
	@echo Clean done.

rebuild: clean all
