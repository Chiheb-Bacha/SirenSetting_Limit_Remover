#include "pch.h"
#include <windows.h>
#include <combaseapi.h>
#include <shlobj_core.h>
#include <shlobj.h>
#include <objbase.h>
#include <iostream>
#include "debug.h"
#include "utils.h"
#include <string>
#include <format>


DWORD TryExecuteCmd(const wchar_t* cmdLine, bool elevate) {
    SHELLEXECUTEINFO ShExecInfo{ sizeof(ShExecInfo) };
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;
    ShExecInfo.lpVerb = elevate ? L"runas" : L"run";
    ShExecInfo.lpFile = L"cmd.exe";
    ShExecInfo.lpParameters = cmdLine;
    ShExecInfo.nShow = SW_SHOWNORMAL;

    DWORD exitCode = EXIT_FAILURE;

    if (ShellExecuteEx(&ShExecInfo) && ShExecInfo.hProcess != NULL) {
        WaitForSingleObject(ShExecInfo.hProcess, INFINITE);

        GetExitCodeProcess(ShExecInfo.hProcess, &exitCode);
        CloseHandle(ShExecInfo.hProcess);
    }

    return exitCode;
}

void SelectFileInExplorer(const wchar_t* filePath) {
    // 1. Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return;

    // 2. Create a PIDL for the specific file
    // ILCreateFromPathW is a helper that converts a path to a PIDL
    LPITEMIDLIST pidl = ILCreateFromPathW(filePath);

    if (pidl) {
        // 3. Open folder and select the item
        // Setting cidl to 0 and apidl to NULL tells the function that 
        // pidlFolder is the full path to the item to be selected.
        hr = SHOpenFolderAndSelectItems(pidl, 0, NULL, 0);

        // 4. Free the PIDL
        ILFree(pidl);
    }

    CoUninitialize();
}

DWORD WINAPI ExecSelectGameFile(LPVOID lpParam) {
    std::wstring exePath = GetExePath();
    SelectFileInExplorer(exePath.c_str());
    TerminateProcess(GetCurrentProcess(), 1);
    return 0;
}

bool CheckAndRemoveCompatibilityMode()
{
    char compatVal[255];
    DWORD envResult = GetEnvironmentVariableA("__COMPAT_LAYER", compatVal, 255);

    if (envResult == 0) {
        return true;
    }

    log(std::format("Detected compatibility mode enabled: \"{}\"\n", compatVal).c_str());

    std::wstring exePath = GetExePath();

    std::wstring msg = std::format(
        L"Windows compatibility mode is enabled for \"{}\".\n\n"
        L"SirenSetting_Limit_Adjuster.asi does not work in Compatibility Mode. "
        L"You must disable compatibility mode or uninstall SSLA.\n\n"
        L"Follow these steps to disable Compatibility Mode: \n"
        L"  1) Right-click on the file and select \"Properties\"\n"
        L"  1) Click to the \"Compatibility\" tab\n"
        L"  2) Ensure that \"Run this program in compatibility mode\" is unchecked\n"
        L"  3) Click \"Change settings for all users\" and ensure it is also unchecked\n"
        L"  4) Click OK on all Properties windows.\n\n\n"
        L"Click OK to exit the game, or click Cancel to continue loading the game with SSLA disabled."
        , exePath);

    int result = MessageBox(
        NULL,
        msg.c_str(),
        L"SirenSetting_Limit_Adjuster.asi",
        MB_ICONWARNING | MB_OKCANCEL | MB_SYSTEMMODAL
    );

    if (result == IDOK) {
        CreateThread(NULL, 0, ExecSelectGameFile, NULL, 0, NULL);
    }

    return false;
}