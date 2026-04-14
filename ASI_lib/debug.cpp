#include <windows.h>
#include <shellapi.h>
#include "pch.h"
#include "debug.h"
#include <cstdarg>
#include <cstdio>
#include <strsafe.h>
#include <cassert>

#ifdef SSA_BETA

void logDebug(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vlog(fmt, args);
	va_end(args);
}

void flushLog(HANDLE log)
{
	FlushFileBuffers(log);
}

#else

void logDebug(const char* fmt, ...)
{
	return;
}

void flushLog(HANDLE log)
{
	return;
}

#endif

DWORD WINAPI ExecFixLogFilePermissions(LPVOID lpParam) {
	LPCTSTR logPath = (LPCTSTR)lpParam;
	const TCHAR* format = TEXT("/C echo Fixing permissions on log file. Normal log should appear on next game run.>\"%s\" && icacls \"%s\" /grant *S-1-5-11:M");
	size_t totalLen = lstrlen(format) - 4 // -4 for the two %s
		+ 2 * lstrlen(logPath)
		+ 1; // null terminator
	LPTSTR cmdLine = (LPTSTR)LocalAlloc(LMEM_ZEROINIT, totalLen * sizeof(TCHAR));
	if (cmdLine) {
		StringCchPrintf(cmdLine, totalLen, format, logPath, logPath);
		HINSTANCE hInst = ShellExecute(NULL, TEXT("runas"), TEXT("cmd.exe"), cmdLine, NULL, SW_SHOWMINNOACTIVE);
		
		// Prepare message box text
		const TCHAR* successFmt = TEXT("Permissions have been updated on:\n%s");
		const TCHAR* failFmt = TEXT("Unable to fix permissions for:\n%s");
		size_t msgLen = max(lstrlen(successFmt), lstrlen(failFmt)) - 2 + lstrlen(logPath) + 1;
		LPTSTR msgText = (LPTSTR)LocalAlloc(LMEM_ZEROINIT, msgLen * sizeof(TCHAR));
		if (msgText) {
			if ((INT_PTR)hInst > 32) {
				StringCchPrintf(msgText, msgLen, successFmt, logPath);
				MessageBox(NULL, msgText, TEXT("Log permissions"), MB_OK | MB_ICONINFORMATION);
			}
			else {
				StringCchPrintf(msgText, msgLen, failFmt, logPath);
				MessageBox(NULL, msgText, TEXT("Log permissions"), MB_OK | MB_ICONERROR);
			}
			LocalFree(msgText);
		}
		
		LocalFree(cmdLine);
	}
	return 0;
}

HANDLE file = INVALID_HANDLE_VALUE;
bool setup_attempted = false;

bool setup_log()
{
	if (setup_attempted) {
		return (file != INVALID_HANDLE_VALUE);
	}
	setup_attempted = true;
	
	TCHAR fullPath[MAX_PATH] = { 0 };
	DWORD pathLen = GetFullPathName(TEXT("SirenSettings.log"), MAX_PATH, fullPath, NULL);
	
	file = CreateFile(fullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		LPTSTR errorMessage = NULL;
		LPTSTR msgBoxMessage = NULL;
		TCHAR msgBoxPrefix[] = TEXT("SirenSetting_Limit_Adjuster log creation failed.\n\nError: ");
		TCHAR msgBoxSuffix[] = TEXT("\n\nTo attempt to fix log permissions automatically, click Yes. This will request admin permissions and changes will take effect on the next game start. To continue loading the game with logging disabled, click No. To exit click Cancel.\n\nLog file path: ");
		
		// Get error message and set a default if FormatMessage fails
		DWORD errorCode = GetLastError();
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, 0, (LPTSTR)&errorMessage, 0, NULL);
		TCHAR defaultErrorMessage[] = TEXT("Unknown error.");
		bool usedDefaultErrorMessage = false;
		if (errorMessage == NULL) {
			errorMessage = defaultErrorMessage;
			usedDefaultErrorMessage = true;
		}

		SIZE_T msgBoxMessageSize = sizeof(TCHAR) * (
			lstrlen(msgBoxPrefix) +
			lstrlen(errorMessage) +
			lstrlen(msgBoxSuffix) +
			lstrlen(fullPath) + 1
			);
		
		msgBoxMessage = (LPTSTR)LocalAlloc(LMEM_ZEROINIT, msgBoxMessageSize);
		
		if (msgBoxMessage != NULL) {
			StringCchCopy(msgBoxMessage, msgBoxMessageSize / sizeof(TCHAR), msgBoxPrefix);
			StringCchCat(msgBoxMessage, msgBoxMessageSize / sizeof(TCHAR), errorMessage);
			StringCchCat(msgBoxMessage, msgBoxMessageSize / sizeof(TCHAR), msgBoxSuffix);
			StringCchCat(msgBoxMessage, msgBoxMessageSize / sizeof(TCHAR), fullPath);

			int result = MessageBox(
				NULL,
				msgBoxMessage,
				TEXT("Error creating sirensettings.log"),
				MB_ICONWARNING | MB_YESNOCANCEL
			);

			if (result == IDYES) {
				size_t allocChars = pathLen + 1;
				LPTSTR pathCopy = (LPTSTR)LocalAlloc(LMEM_ZEROINIT, allocChars * sizeof(TCHAR));
				if (pathCopy) {
					StringCchCopy(pathCopy, allocChars, fullPath);
					CreateThread(NULL, 0, ExecFixLogFilePermissions, pathCopy, 0, NULL);
				}
			}
			else if (result == IDCANCEL) {
				ExitProcess(1);
			}

			LocalFree(msgBoxMessage);
		}

		if (!usedDefaultErrorMessage && errorMessage) LocalFree(errorMessage);
		return false;
	}
	return true;
}

void log(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vlog(fmt, args);
	va_end(args);
}

void vlog(const char* fmt, va_list args)
{
	if (file == INVALID_HANDLE_VALUE || file == 0)
		if (!setup_log())
			return;
	if (file == INVALID_HANDLE_VALUE || file == 0)
		return;
	char outputString[128] = { 0 };
	char* outputStringLong = outputString;
	bool outputStrIsLong = false;
	va_list argcopy;
	va_copy(argcopy, args);
	SIZE_T outputLen = vsnprintf(outputString, 128, fmt, argcopy);
	va_end(argcopy);
	if (outputLen > 128)
	{
		outputStringLong = (char*)LocalAlloc(LMEM_ZEROINIT, outputLen + 1);
		outputStrIsLong = true;
		if (outputStringLong == NULL)
			return;
		outputLen++;
		vsnprintf(outputStringLong, outputLen, fmt, args);
	}
	WriteFile(file, outputStringLong, outputLen, NULL, NULL);
	if (outputStrIsLong)
		LocalFree(outputStringLong);
	flushLog(file);
}

void cleanup_log()
{
	if (file != INVALID_HANDLE_VALUE) {
		FlushFileBuffers(file);
		CloseHandle(file);
	}
	return;
}