// gtaw-oldhud - FiveM client ASI plugin
//
// Redraws GTA World's HUD in the style of the old one: cash and bank at the top right, the
// direction, area and street beside the minimap, speed and mileage above them, and the single
// server line along the bottom.
//
// It does this entirely in the interface the game already runs. It attaches to FiveM's own local
// CEF debugging endpoint, creates a private execution context in the GTAW frame and installs a
// stylesheet and a listener there. The listener reads the HUD messages the interface is already
// being sent and draws its own nodes from them. No game memory is read or written, nothing is
// hooked, no native is called and no message is ever sent back.

#include "core/logger.h"
#include "core/util.h"
#include "core/worker.h"

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = instance;
        DisableThreadLibraryCalls(instance);

        // Everything real happens on the worker. DllMain runs under the loader lock, so it does
        // no file, socket or log work here: creating one event and one thread is the whole of it.
        if (!createStopEvent()) return TRUE;

        HANDLE worker = CreateThread(nullptr, 0, HudWorker, nullptr, 0, nullptr);
        if (worker) CloseHandle(worker);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        // Ask the worker to stop, but never wait for it. FiveM runs detach for every loaded
        // module while tearing the process down after a crash, and blocking there would turn a
        // crash report into a hang.
        requestStop();
    }
    return TRUE;
}
