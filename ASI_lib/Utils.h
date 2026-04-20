#pragma once
#include "pch.h"
#pragma comment(lib, "Version.lib")
#include "debug.h"
#include <cstdint>
#include <vector>
#include <string>

#pragma comment(lib, "shell32.lib")

struct FileVersion
{
    uint16_t Major = 0;
    uint16_t Minor = 0;
    uint16_t Build = 0;
    uint16_t Revision = 0;

    constexpr uint64_t ToComparable() const noexcept
    {
        return (uint64_t(Major) << 48) |
            (uint64_t(Minor) << 32) |
            (uint64_t(Build) << 16) |
            uint64_t(Revision);
    }

    friend constexpr bool operator<(const FileVersion& a, const FileVersion& b) noexcept
    {
        return a.ToComparable() < b.ToComparable();
    }

    friend constexpr bool operator>=(const FileVersion& a, const FileVersion& b) noexcept
    {
        return a.ToComparable() >= b.ToComparable();
    }
};

inline bool ReadCurrentProcessFileVersion(FileVersion& outVersion)
{
    wchar_t path[MAX_PATH]{};

    if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
        return false;

    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &dummy);
    if (size == 0)
        return false;

    std::vector<std::byte> buffer(size);

    if (!GetFileVersionInfoW(path, 0, size, buffer.data()))
        return false;

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;

    if (!VerQueryValueW(
        buffer.data(),
        L"\\",
        reinterpret_cast<LPVOID*>(&info),
        &infoSize))
    {
        return false;
    }

    outVersion.Major = HIWORD(info->dwFileVersionMS);
    outVersion.Minor = LOWORD(info->dwFileVersionMS);
    outVersion.Build = HIWORD(info->dwFileVersionLS);
    outVersion.Revision = LOWORD(info->dwFileVersionLS);

    return true;
}

inline const FileVersion& GetGameVersion()
{
    static const FileVersion& cachedVersion = []
        {
            FileVersion v{};
            if (ReadCurrentProcessFileVersion(v)) {
                return v;
            }
            log("Could not determine the game version. Terminating process...");
            TerminateProcess(GetCurrentProcess(), 1);
        }();

    return cachedVersion;
}

// Will be used soon ... 
inline bool IsEnhanced() {
    static const bool isEnhanced = []() -> bool 
        {
            char path[MAX_PATH];
            GetModuleFileNameA(GetModuleHandleA(nullptr), path, MAX_PATH);

            const char* filename = strrchr(path, '\\');
            filename = filename ? filename + 1 : path;

            return (_stricmp(filename, "GTA5_Enhanced.exe") == 0);
        }();
    return isEnhanced;
}


#pragma region Compatibility mode stuff

static const wchar_t* COMPAT_REG_PATH =
L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";

inline std::wstring GetExePath()
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}

inline void CheckAndRemoveCompatibilityMode()
{
    std::wstring exePath = GetExePath();
    bool foundAny = false;
    bool needsElevation = false;

    for (HKEY scope : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;

        DWORD type = 0, size = 0;
        bool found = RegQueryValueExW(hKey, exePath.c_str(), nullptr, &type, nullptr, &size) == ERROR_SUCCESS
            && type == REG_SZ && size > sizeof(wchar_t);
        RegCloseKey(hKey);

        if (!found) continue;
        foundAny = true;

        HKEY hKeyWrite = nullptr;
        if (RegOpenKeyExW(scope, COMPAT_REG_PATH, 0, KEY_SET_VALUE, &hKeyWrite) != ERROR_SUCCESS)
        {
            needsElevation = true;
        }
        else
        {
            LSTATUS status = RegDeleteValueW(hKeyWrite, exePath.c_str());
            RegCloseKey(hKeyWrite);
            if (status == ERROR_ACCESS_DENIED)
                needsElevation = true;
        }
    }

    if (!foundAny) return;

    if (needsElevation)
    {
        // At least on my machine, trying to remove system-wide compatibility mode
        // with elevated priviliges fails and the game won't even terminate properly
        // So we'll just ask the users to do it manually to avoid any problems
        std::wstring msg =
            L"Windows compatibility mode is enabled system-wide for this game "
            L"and requires administrator privileges to remove.\n\n"
            L"Please deactivate it manually then relaunch the game.\n\n"
            L"The game will now close.";

        MessageBoxW(nullptr, msg.c_str(), L"Compatibility Mode Detected", MB_OK | MB_ICONWARNING);
    }
    else
    {
        MessageBoxW(
            nullptr,
            L"Windows compatibility mode was enabled for this game and has been deactivated.\n\n"
            L"Please relaunch the game.",
            L"Compatibility Mode Detected",
            MB_OK | MB_ICONINFORMATION
        );
    }

    TerminateProcess(GetCurrentProcess(), 1);
}

#pragma endregion