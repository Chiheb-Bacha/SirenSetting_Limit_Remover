// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "SirenSettings_patcher.h"
#include "SirenLights.h"
#include "hooking.h"
#include "debug.h"
#include "Utils.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    bool success = true;
    switch (ul_reason_for_call)
    {

    case DLL_PROCESS_ATTACH:
        if (!CheckAndRemoveCompatibilityMode()) {
            log("Unable to load SSLA because compatibility mode is enabled!\n");
            break;
        }
        
        if (!InitializeNearHooks()) {
            log("Page allocation failed!\n");
            break;
        }

        if (!ApplyIdHooks()) {
            log("ID hook application failed!\n");
            break;
        }
        else {
            log("ID hooks applied.\n");
        }

        if (!ApplyIndexHooks())
        {
            log("Index hook application failed!\n");
            break;
        }
        else {
            log("Index hooks applied.\n");
        }

        if (!ApplySirenBufferHooks())
        {
            log("Buffer hook application failed!\n");
            break;
        }
        else {
            log("Buffer hooks applied.\n");
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

