LHM_DIR = LibreHardwareMonitor
LHM_DLL = LHMBridge.dll
LHM_SRC = src\LHMBridge\LHMBridge.cpp

VCPKG_INCLUDE = $(USERPROFILE)\vcpkg\installed\x64-windows\include

PROG = output\fanCommander.exe
MAIN = src\main.cpp
FANCONTROL = src\fanControl.cpp
READJSON = src\readJson.cpp

GCC = g++
STD = c++17

all: $(LHM_DLL) libLHMBridge.a $(PROG)

$(LHM_DLL):
    cl /clr /LD $(LHM_SRC) /Fe$(LHM_DLL) /AI"$(LHM_DIR)" /FU"LibreHardwareMonitorLib.dll"

libLHMBridge.a: $(LHM_DLL)
    gendef $(LHM_DLL)
    dlltool -d LHMBridge.def -l libLHMBridge.a

$(PROG): libLHMBridge.a
    $(GCC) -std=$(STD) -I"$(VCPKG_INCLUDE)" -o $(PROG) \
        $(MAIN) $(FANCONTROL) $(READJSON) libLHMBridge.a -Wl,--subsystem,console
    copy /Y $(LHM_DLL) output\

clean:
    del $(LHM_DLL) LHMBridge.def LHMBridge.exp LHMBridge.lib LHMBridge.obj libLHMBridge.a $(PROG)
