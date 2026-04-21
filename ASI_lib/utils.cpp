#include <windows.h>
#include "pch.h"
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

DWORD WINAPI ExecElevRemoveCompatMode(LPVOID lpParam) {
    std::wstring exePath = GetExePath();
    
    bool success = true;
    std::wstring cmdLine = std::format(
        L"/C echo Disabling compatibility mode for \"{0}\" ",
        exePath
    );
    
    for (HKEY scope : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
    {
        HKEY hKey = nullptr;

        // Registry key does not exist
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;

        // Check if there is a value with the name equal to the absolute path of the EXE
        DWORD type = 0, size = 0;
        bool found = RegQueryValueExW(hKey, exePath.c_str(), nullptr, &type, nullptr, &size) == ERROR_SUCCESS
            && type == REG_SZ && size > sizeof(wchar_t);
        RegCloseKey(hKey);

        if (!found) 
            continue;

        // Attempt to open key for modification, elevate if fails
        HKEY hKeyWrite = nullptr;
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_WRITE, &hKeyWrite) != ERROR_SUCCESS)
            success = false;

        // If user is in administrator group but exe is not currently elevated, the first check
        // above can succeed but actually deleting the value can fail
        LSTATUS status = RegDeleteValueW(hKeyWrite, exePath.c_str());
        RegCloseKey(hKeyWrite);
        if (status != ERROR_SUCCESS)
            success = false;

        if (!success) {
            cmdLine.append(std::format(
                L"& reg DELETE \"{}\\{}\" /v \"{}\" /f ",
                scope == HKEY_LOCAL_MACHINE ? L"HKLM" : L"HKCU",
                COMPAT_REG_PATH,
                exePath
            ));
        }
    }

    if (!success) {
        // Likely needs elevation to complete the operation
        // Need to launch an external process to elevate
        DWORD exitCode = TryExecuteCmd(cmdLine.c_str(), true);
        success = (exitCode == EXIT_SUCCESS);
    }

    std::wstring msg = success ? L"Disabled compatibility mode. " : L"Unable to disable compatibility mode. ";
    msg.append(L"Click OK to exit game, or Cancel to continue loading without SSLA. ");
    if (success) 
        msg.append(L"Re-launch game for changes to take effect.");
    
    UINT icon = success ? MB_ICONINFORMATION : MB_ICONERROR;

    int result = MessageBox(
        NULL,
        msg.c_str(),
        L"SirenSetting_Limit_Adjuster.asi",
        icon | MB_OKCANCEL | MB_SYSTEMMODAL
    );

    if (result == IDOK) {
        ExitProcess(1);
    }
    
    return 0;
}

bool CheckAndRemoveCompatibilityMode()
{
    std::wstring exePath = GetExePath();
    bool foundAny = false;
    // bool needsElevation = false;

    for (HKEY scope : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
    {
        HKEY hKey = nullptr;

        // Registry key does not exist, so compatibility mode can't be set
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;

        // Check if there is a value with the name equal to the absolute path of the EXE
        DWORD type = 0, size = 0;
        bool found = RegQueryValueExW(hKey, exePath.c_str(), nullptr, &type, nullptr, &size) == ERROR_SUCCESS
            && type == REG_SZ && size > sizeof(wchar_t);
        RegCloseKey(hKey);

        if (!found) continue;
        foundAny = true;
    }

    // Compatibility mode not set for user or machine, OK to continue
    if (!foundAny) return true;

    std::wstring msg = std::format(
        L"Windows compatibility mode is enabled for \"{}\".\n\n"
        L"SirenSetting_Limit_Adjuster.asi does not work in Compatibility Mode.\n\n"
        L"Do you want to automatically disable Compatibility Mode? Click Yes to disable "
        L"(may request admin permissions), click No to continue loading the game with "
        L"SSLA disabled, or click Cancel to exit."
        , exePath);

    int result = MessageBox(
        NULL,
        msg.c_str(),
        TEXT("SirenSetting_Limit_Adjuster.asi"),
        MB_ICONWARNING | MB_YESNOCANCEL | MB_SYSTEMMODAL
    );

    if (result == IDYES) {
        CreateThread(NULL, 0, ExecElevRemoveCompatMode, NULL, 0, NULL);
    }
    else if (result == IDCANCEL) {
        ExitProcess(1);
    }

    return false;
}