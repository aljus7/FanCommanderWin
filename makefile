# nmake MSVC Makefile - run in "x64 Native Tools Command Prompt for VS"

LHM_DIR = LibreHardwareMonitor
LHM_DLL = LHMBridge.dll
LHM_LIB = LHMBridge.lib
LHM_SRC = src\LHMBridge\LHMBridge.cpp
LHM_DLLREF = $(LHM_DIR)\LibreHardwareMonitorLib.dll

VCPKG_INCLUDE = $(USERPROFILE)\vcpkg\installed\x64-windows\include

PROG = output\fanCommander.exe
MAIN = src\main.cpp
FANCONTROL = src\fanControl.cpp
READJSON = src\readJson.cpp
LOGGER = src\eventLogger.cpp

# NOTE: removed /EHsc because /clr is incompatible with /EH options
CFLAGS = /nologo /MD /W3 /O2 /std:c++17 /D_HAS_STD_BYTE=0 /I"." /I"src"
CLRFLAGS = /clr
DLLFLAGS = /LD
LINKER_FLAGS = /subsystem:windows /ENTRY:mainCRTStartup

ALL: $(LHM_DLL) $(PROG)

$(LHM_DLL):
	if not exist output mkdir output
	if not exist "$(LHM_DLLREF)" echo WARNING: "$(LHM_DLLREF)" not found
	cl $(CFLAGS) $(CLRFLAGS) $(DLLFLAGS) "$(LHM_SRC)" "$(LOGGER)" /Fe"$(LHM_DLL)" /I"$(LHM_DIR)" /AI"$(LHM_DIR)" /FU"$(LHM_DLLREF)" /link advapi32.lib

#Use /subsystem:console for debuging, /subsystem:windows /ENTRY:mainCRTStartup for release

$(PROG): $(LHM_LIB)
	if not exist output mkdir output
	cl $(CFLAGS) /Fe"$(PROG)" /I"$(VCPKG_INCLUDE)" "$(MAIN)" "$(FANCONTROL)" "$(READJSON)" "$(LOGGER)" "$(LHM_LIB)" /link $(LINKER_FLAGS) advapi32.lib
	copy /Y "$(LHM_DLL)" output
	if exist "$(LHM_DLLREF)" copy /Y "$(LHM_DLLREF)" output

$(LHM_LIB): $(LHM_DLL)
	rem LHMBridge.lib is produced automatically by cl when building the DLL

clean:
	-del /Q "$(LHM_DLL)" "$(LHM_LIB)" "$(PROG)" || @echo.