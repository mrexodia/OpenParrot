#pragma optimize("", off)
#include <StdInc.h>
#include "Utility/GameDetect.h"
#include "Utility/InitFunction.h"
#include "Functions/Global.h"
#include "Utility/Helper.h"
#if _M_IX86
using namespace std::string_literals;
extern int* wheelSection;
extern int* ffbOffset;
extern int* ffbOffset3;
extern int* ffbOffset4;
extern int* ffbOffset5;

extern void BG4ManualHack(Helpers* helpers);
extern void BG4ProInputs(Helpers* helpers);
extern void KOFSkyStageInputs(Helpers* helpers);
static bool ProMode;
extern bool BG4EnableTracks;

void AddCommOverride(HANDLE hFile);

static char moveBuf[256];
static LPCSTR ParseFileNamesA(LPCSTR lpFileName)
{
#ifdef _DEBUG
	info(true, "ParseFileNamesA original: %s", lpFileName);
#endif
	// Tetris ram folder redirect
	if (!strncmp(lpFileName, ".\\TGM3\\", 7)) 
	{
		memset(moveBuf, 0, 256);
		sprintf(moveBuf, ".\\OpenParrot\\%s", lpFileName + 2);
		return moveBuf;
	}

	// KOF98 test menu
	if (!strncmp(lpFileName, "Ranking*.txt", 8) || !strncmp(lpFileName, "Setting*.txt", 7)
		|| !strncmp(lpFileName, "CoinFile*.txt", 8))
	{
		memset(moveBuf, 0, 256);
		sprintf(moveBuf, ".\\OpenParrot\\%s", lpFileName);
		return moveBuf;
	}

	// KOFMIRA test menu
	if (!strncmp(lpFileName, "*.txt", 5))
	{
		memset(moveBuf, 0, 256);
		sprintf(moveBuf, ".\\OpenParrot\\%s", lpFileName);
		return moveBuf;
	}

	if (!strncmp(lpFileName, "D:", 2) || !strncmp(lpFileName, "d:", 2))
	{
		memset(moveBuf, 0, 256);

		char pathRoot[MAX_PATH];
		GetModuleFileNameA(GetModuleHandle(nullptr), pathRoot, _countof(pathRoot));
		strrchr(pathRoot, '\\')[0] = '\0';

		if (lpFileName[2] == '\\' || lpFileName[2] == '/')
		{
			sprintf(moveBuf, ".\\OpenParrot\\%s", lpFileName + 3);
#ifdef _DEBUG
			info(true, "PathRoot: %s", pathRoot);
			info(true, "ParseFileNamesA - 3: %s", lpFileName + 3);
			info(true, "ParseFileNamesA movBuf: %s", moveBuf);
#endif
			// convert char to string, and replace '/' to '\\'
			std::string movBufOP = moveBuf;
			std::replace(movBufOP.begin(), movBufOP.end(), '/', '\\');

			// if redirected path contains the full path, don't redirect, fixes running games from D:
			if (movBufOP.find(pathRoot + 3) != std::string::npos)
			{
#ifdef _DEBUG
				info(true, "!!!!!!!!!!!NO REDIRECT!!!!!!!!!!!!");
#endif
				return lpFileName;
			}
		}
		else
		{
			if (!strncmp(lpFileName, "D:data", 6)) // needed for ChaseHQ, KOFMIRA
			{
				sprintf(moveBuf, ".\\%s", lpFileName + 2);
#ifdef _DEBUG
				info(true, "D:data redirect: %s", moveBuf);
#endif
				return moveBuf;
			}

			if (!strncmp(lpFileName, "D:.\\data", 8)) // BG4
			{
				sprintf(moveBuf, ".\\%s", lpFileName + 4);
#ifdef _DEBUG
				info(true, "D:.\\data redirect: %s", moveBuf);
#endif
				return moveBuf;
			}

			// Magical Beat has d: WTF?
			sprintf(moveBuf, ".\\OpenParrot\\%s", lpFileName + 2);
		}
		return moveBuf;
	}

	return lpFileName;
}

static wchar_t moveBufW[256];
static LPCWSTR ParseFileNamesW(LPCWSTR lpFileName)
{
#ifdef _DEBUG
	info(true, "ParseFileNamesW original: %ls", lpFileName);
#endif
	if (!wcsncmp(lpFileName, L"D:", 2) || !wcsncmp(lpFileName, L"d:", 2))
	{
		memset(moveBufW, 0, 256);
		if (lpFileName[2] == '\\' || lpFileName[2] == '/')
		{
			wchar_t pathRootW[MAX_PATH];
			GetModuleFileNameW(GetModuleHandle(nullptr), pathRootW, _countof(pathRootW));

			wcsrchr(pathRootW, L'\\')[0] = L'\0';

			swprintf(moveBufW, L".\\OpenParrot\\%ls", lpFileName + 3);

#ifdef _DEBUG
			info(true, "PathRootW: %ls", pathRootW);
			info(true, "ParseFileNamesW: %ls", lpFileName + 3);
			info(true, "ParseFileNamesW movBufW: %ls", moveBufW);
#endif
			// convert wchar to wstring, and replace '/' to '\\'
			std::wstring movBufWOP = moveBufW;
			std::replace(movBufWOP.begin(), movBufWOP.end(), '/', '\\');

			// if redirected path contains the full path, don't redirect, fixes running games from D:
			if (movBufWOP.find(pathRootW + 3) != std::wstring::npos)
			{
#ifdef _DEBUG
				info(true, "!!!!!!!!!!!NO REDIRECT_W!!!!!!!!!!!!");
#endif
				return lpFileName;
			}
		}
		else
		{
			// Magical Beat has d: WTF?
			swprintf(moveBufW, L".\\OpenParrot\\%ls", lpFileName + 2);
		}
		return moveBufW;
	}

	return lpFileName;
}

static BOOL __stdcall SetCurrentDirectoryAWrap(LPCSTR lpPathName)
{
	char pathRoot[MAX_PATH];
	GetModuleFileNameA(GetModuleHandleA(nullptr), pathRoot, _countof(pathRoot)); // get full pathname to game executable

	strrchr(pathRoot, '\\')[0] = '\0'; // chop off everything from the last backslash.
#ifdef _DEBUG
	info(true, "SetCurrentDirectoryA: %s", lpPathName);
#endif
	if (!strncmp(lpPathName, ".\\sh", 4)) // wacky racers fix
	{
		// info(true, "Redirect: %s", (pathRoot + ""s + "\\sh").c_str());
		return SetCurrentDirectoryA((pathRoot + ""s + "\\sh").c_str());
	}

	return SetCurrentDirectoryA((pathRoot + ""s).c_str());
}

static HANDLE __stdcall CreateFileAWrap(LPCSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile)
{
	if (GameDetect::X2Type == X2Type::BG4 || GameDetect::X2Type == X2Type::BG4_Eng || GameDetect::X2Type == X2Type::VRL)
	{
		if (strncmp(lpFileName, "COM1", 4) == 0)
		{
			HANDLE hFile = (HANDLE)0x8001;

			AddCommOverride(hFile);

			return hFile;
		}
	}

	return CreateFileA(ParseFileNamesA(lpFileName),
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile);
}

static HANDLE __stdcall CreateFileWWrap(LPCWSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile)
{
	if (GameDetect::X2Type == X2Type::BG4 || GameDetect::X2Type == X2Type::BG4_Eng || GameDetect::X2Type == X2Type::VRL)
	{
		if (wcsncmp(lpFileName, L"COM1", 4) == 0)
		{
			HANDLE hFile = (HANDLE)0x8001;

			AddCommOverride(hFile);

			return hFile;
		}
	}

	return CreateFileW(ParseFileNamesW(lpFileName),
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		hTemplateFile);
}

static BOOL __stdcall CreateDirectoryAWrap(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return CreateDirectoryA(ParseFileNamesA(lpPathName), nullptr);
}

static BOOL __stdcall CreateDirectoryWWrap(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return CreateDirectoryW(ParseFileNamesW(lpPathName), nullptr);
}

static HANDLE __stdcall FindFirstFileAWrap(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
	return FindFirstFileA(ParseFileNamesA(lpFileName), lpFindFileData);
}

static HANDLE __stdcall FindFirstFileWWrap(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
	return FindFirstFileW(ParseFileNamesW(lpFileName), lpFindFileData);
}

static HANDLE __stdcall FindFirstFileExAWrap(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS  fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
	return FindFirstFileExA(ParseFileNamesA(lpFileName), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

static HANDLE __stdcall FindFirstFileExWWrap(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPWIN32_FIND_DATAW lpFindFileData, FINDEX_SEARCH_OPS  fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
	return FindFirstFileExW(ParseFileNamesW(lpFileName), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

static DWORD __stdcall GetFileAttributesAWrap(LPCSTR lpFileName)
{
	return GetFileAttributesA(ParseFileNamesA(lpFileName));
}

static DWORD __stdcall GetFileAttributesWWrap(LPCWSTR lpFileName)
{
	return GetFileAttributesW(ParseFileNamesW(lpFileName));
}

static BOOL __stdcall GetDiskFreeSpaceExAWrap(LPCSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes)
{
	return GetDiskFreeSpaceExA(NULL, lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

static BOOL __stdcall GetDiskFreeSpaceExWWrap(LPCWSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailableToCaller, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes)
{
	return GetDiskFreeSpaceExW(NULL, lpFreeBytesAvailableToCaller, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes);
}

static BOOL __stdcall GetDiskFreeSpaceAWrap(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
	return GetDiskFreeSpaceA(NULL, lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}

static BOOL __stdcall GetDiskFreeSpaceWWrap(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
	return GetDiskFreeSpaceW(NULL, lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}


#include <deque>
#include <iphlpapi.h>

static std::map<HANDLE, std::deque<BYTE>> g_replyBuffers;

static BOOL __stdcall ReadFileWrapTx2(HANDLE hFile,
	LPVOID lpBuffer,
	DWORD nNumberOfBytesToRead,
	LPDWORD lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped)
{
	if (hFile == (HANDLE)0x8001)
	{
		auto& outQueue = g_replyBuffers[hFile];

		int toRead = min(outQueue.size(), nNumberOfBytesToRead);

		std::copy(outQueue.begin(), outQueue.begin() + toRead, reinterpret_cast<uint8_t*>(lpBuffer));
		outQueue.erase(outQueue.begin(), outQueue.begin() + toRead);

		*lpNumberOfBytesRead = toRead;

		return TRUE;
	}
	return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL __stdcall WriteFileWrapTx2(HANDLE hFile,
	LPVOID lpBuffer,
	DWORD nNumberOfBytesToWrite,
	LPDWORD lpNumberOfBytesWritten,
	LPOVERLAPPED lpOverlapped)
{
	if (hFile == (HANDLE)0x8001)
	{
		DWORD pos = 0;

		char bytes[2] = { 0 };
		uint8_t* taskBuffer = (uint8_t*)lpBuffer;

		while (pos < nNumberOfBytesToWrite)
		{
			switch (taskBuffer[pos])
			{
			case 0x20:
				g_replyBuffers[hFile].push_back(1);

				pos += 2;
				break;

			case 0x1F:
			case 0x00:
			case 0x04:
			{
				int wheelValue = *wheelSection;

				if (GameDetect::X2Type == X2Type::VRL)
				{
					wheelValue = 255 - wheelValue;
				}

				wheelValue = (int)((wheelValue / 256.0f) * 1024.0f);
				wheelValue += 1024;

				g_replyBuffers[hFile].push_back(HIBYTE(wheelValue));
				g_replyBuffers[hFile].push_back(LOBYTE(wheelValue));

				pos += 2;
				break;
			}
			default:
				pos += 2;
				break;
			}
		}

		*lpNumberOfBytesWritten = nNumberOfBytesToWrite;

		return TRUE;
	}
	return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

static DWORD WINAPI RunningLoop(LPVOID lpParam)
{
	while (true)
	{
		switch (GameDetect::currentGame)
		{
		case GameID::BG4:
			if (ProMode)
				BG4ProInputs(0);
			else
				BG4ManualHack(0);
			break;
		case GameID::KOFSkyStage100J:
			KOFSkyStageInputs(0);
			break;
		}
		Sleep(16);
	}
}

static int ReturnsTrue()
{
	return 1;
}

static InitFunction initFunction([]()
{
	DWORD imageBase = (DWORD)GetModuleHandleA(0);
	if (GameDetect::X2Type == X2Type::None)
	{
		return;
	}
	CreateDirectoryA("OpenParrot", nullptr);
	iatHook("kernel32.dll", CreateFileAWrap, "CreateFileA");
	iatHook("kernel32.dll", CreateFileWWrap, "CreateFileW");
	iatHook("kernel32.dll", CreateDirectoryAWrap, "CreateDirectoryA");
	iatHook("kernel32.dll", CreateDirectoryWWrap, "CreateDirectoryW");
	iatHook("kernel32.dll", SetCurrentDirectoryAWrap, "SetCurrentDirectoryA");
	iatHook("kernel32.dll", FindFirstFileAWrap, "FindFirstFileA");
	iatHook("kernel32.dll", FindFirstFileWWrap, "FindFirstFileW");
	iatHook("kernel32.dll", FindFirstFileExAWrap, "FindFirstFileExA");
	iatHook("kernel32.dll", FindFirstFileExWWrap, "FindFirstFileExW");
	iatHook("kernel32.dll", GetFileAttributesAWrap, "GetFileAttributesA");
	iatHook("kernel32.dll", GetFileAttributesWWrap, "GetFileAttributesW");

	iatHook("kernel32.dll", GetDiskFreeSpaceAWrap, "GetDiskFreeSpaceA");
	iatHook("kernel32.dll", GetDiskFreeSpaceWWrap, "GetDiskFreeSpaceW");
	iatHook("kernel32.dll", GetDiskFreeSpaceExAWrap, "GetDiskFreeSpaceExA");
	iatHook("kernel32.dll", GetDiskFreeSpaceExAWrap, "GetDiskFreeSpaceExW");
	
	switch (GameDetect::X2Type)
	{
		case X2Type::Wontertainment: // By 00C0FFEE
		{
			// error display routine noping
			injector::MakeNOP(0x004DE4B4, 2);
			// do not hide the cursor
			injector::WriteMemory<BYTE>(0x004E827D, 0x01, true);
			// jump from logos to main menu scene (NO ATTRACT enjoy!)
			injector::WriteMemory<BYTE>(0x004E86CB, 0x05, true);
			injector::WriteMemory<BYTE>(0x004E8C52, 0x05, true);
			injector::WriteMemory<BYTE>(0x004E8702, 0x05, true);
			// card dispenser scene bypass (cause it hangs without one)
			injector::WriteMemory<BYTE>(0x004E9391, 0x0A, true);
			injector::WriteMemory<BYTE>(0x004E93B3, 0x0A, true);

			break;
		}
		case X2Type::MB4:
		{
			// Redirect messagelog file
			injector::WriteMemoryRaw(0x00AD8B6C, ".\\messagelog.dat\0", 18, true);

			break;
		}
		case X2Type::VRL:
		{
			// TODO: DOCUMENT PATCHES
			//injector::MakeNOP(0x0040E22E, 2, true);
			injector::WriteMemory<DWORD>(0x0041C800, 0x08C2C033, true);
			injector::WriteMemory<BYTE>(0x0041C804, 0x00, true);
			
			iatHook("kernel32.dll", ReadFileWrapTx2, "ReadFile");
			iatHook("kernel32.dll", WriteFileWrapTx2, "WriteFile");

			// IP stuff for working LAN
			static const char* cab1IP = config["Network"]["Cab1IP"].c_str(); // 192.168.64.100
			static const char* cab2IP = config["Network"]["Cab2IP"].c_str(); // 192.168.64.101
			static const char* cab3IP = config["Network"]["Cab3IP"].c_str(); // 192.168.64.102
			static const char* cab4IP = config["Network"]["Cab4IP"].c_str(); // 192.168.64.103

			injector::WriteMemory<DWORD>(imageBase + 0x5D868, (DWORD)cab1IP, true);
			injector::WriteMemory<DWORD>(imageBase + 0x5D876, (DWORD)cab2IP, true);
			injector::WriteMemory<DWORD>(imageBase + 0x5D884, (DWORD)cab3IP, true);
			injector::WriteMemory<DWORD>(imageBase + 0x5D892, (DWORD)cab4IP, true);
			injector::WriteMemory<DWORD>(imageBase + 0x5AE0C, (DWORD)cab1IP, true);
			injector::WriteMemory<DWORD>(imageBase + 0x5AE05, (DWORD)cab3IP, true);

			break;
		}
		case X2Type::Raiden4:
		{
			// TODO: DOCUMENT PATCHES
			//injector::WriteMemory<uint32_t>(0x49DDB0, 0xC3C301B0, true);
			injector::WriteMemory<BYTE>(0x00496EA0, 0xEB, true);

			DWORD oldPageProtection = 0;

			if (ToBool(config["General"]["Windowed"]))
			{
				VirtualProtect((LPVOID)(imageBase + 0x181274), 4, PAGE_EXECUTE_READWRITE, &oldPageProtection);
				windowHooks hooks = { 0 };
				hooks.createWindowExA = imageBase + 0x181274;
				init_windowHooks(&hooks);
				VirtualProtect((LPVOID)(imageBase + 0x181274), 4, oldPageProtection, &oldPageProtection);

				// show cursor
				injector::WriteMemory<BYTE>(imageBase + 0x96E88, 0x01, true);
			}

			break;
		}
		case X2Type::BG4:
		{
			// TODO: DOCUMENT PATCHES
			injector::MakeNOP(0x4CBCB8, 10);
			injector::WriteMemory<uint8_t>(0x4CBCB8, 0xB8, true);
			injector::WriteMemory<uint32_t>(0x4CBCB9, 1, true);

			// redirect E:\data to .\data
			injector::WriteMemoryRaw(0x0076D96C, "./data/", 8, true);
			injector::WriteMemoryRaw(0x007ACA60, ".\\data", 7, true);
			
			// Fix sound only being in left ear
			injector::WriteMemoryRaw(imageBase + 0x36C3DC, "\x00\x60\xA9\x45", 4, true);

			if (ToBool(config["General"]["IntroFix"]))
			{
				// thanks for Ducon2016 for the patch!
				injector::WriteMemoryRaw(imageBase + 0x57ACE, "\x89\x68\x14\xC7\x40\x08\x00\x00\x80\x3F\x83\xC1\x08\xEB\x0A", 15, true);
				injector::WriteMemoryRaw(imageBase + 0x57AE7, "\xE9\x1B\x04\x00\x00", 5, true);
				injector::WriteMemoryRaw(imageBase + 0x57F01, "\xE9\xC8\xFB\xFF\xFF\x90", 6, true);
			}

			if (!ToBool(config["General"]["Windowed"]))
			{
				injector::MakeRET(0x5F21B0, 4);
			}

			BG4EnableTracks = (ToBool(config["General"]["Enable All Tracks"]));

			ProMode = (ToBool(config["General"]["Professional Edition Enable"]));

			if (ProMode)
			{
				injector::MakeNOP(imageBase + 0x1E7AB, 6);
				injector::MakeNOP(imageBase + 0x1E7DC, 6);
				injector::MakeNOP(imageBase + 0x1E7A5, 6);
				injector::MakeNOP(imageBase + 0x1E82B, 6);
				injector::MakeNOP(imageBase + 0x1E79F, 6);
				injector::MakeNOP(imageBase + 0x1E858, 6);
				injector::MakeNOP(imageBase + 0x1E799, 6);
				injector::MakeNOP(imageBase + 0x1E880, 6);
				injector::MakeNOP(imageBase + 0x27447, 3);
			
				// Fix 6MT warning upon key entry
				injector::WriteMemoryRaw(imageBase + 0xD4AC3, "\xE9\x0E\x01\x00", 4, true);

				if (ToBool(config["General"]["Professional Edition Hold Gear"]))
				{
					injector::MakeNOP(imageBase + 0x8ADFF, 10);
				}
			}

			CreateThread(NULL, 0, RunningLoop, NULL, 0, NULL);

			// IP stuff for working LAN
			static const char* BroadcastAddress = config["Network"]["BroadcastAddress"].c_str();
			injector::WriteMemory<DWORD>(imageBase + 0xA1004, (DWORD)BroadcastAddress, true);

			iatHook("kernel32.dll", ReadFileWrapTx2, "ReadFile");
			iatHook("kernel32.dll", WriteFileWrapTx2, "WriteFile");

			break;
		}
		case X2Type::BG4_Eng:
		{
			// TODO: DOCUMENT PATCHES
			//injector::MakeNOP(0x4CBCB8, 10);
			//injector::WriteMemory<uint8_t>(0x4CBCB8, 0xB8, true);
			//injector::WriteMemory<uint32_t>(0x4CBCB9, 1, true);

			// redirect E:\data to .\data
			injector::WriteMemoryRaw(0x0073139C, "./data/", 8, true);
			injector::WriteMemoryRaw(0x00758978, ".\\data", 7, true);

			if (ToBool(config["General"]["IntroFix"]))
			{
				// thanks for Ducon2016 for the patch!
				injector::WriteMemoryRaw(imageBase + 0x46E3E, "\x89\x68\x14\xC7\x40\x08\x00\x00\x80\x3F\x83\xC1\x08\xEB\x0A", 15, true);
				injector::WriteMemoryRaw(imageBase + 0x46E57, "\xE9\x5F\x03\x00\x00", 5, true);
				injector::WriteMemoryRaw(imageBase + 0x471B5, "\xE9\x84\xFC\xFF\xFF\x90", 6, true);
				injector::WriteMemoryRaw(imageBase + 0xEB964, "\x89\x4C", 2, true);
				injector::WriteMemoryRaw(imageBase + 0xEB967, "\x40\x8B\x70\x18\xD9\x5C\x24\x4C\x89\x4C", 10, true);
				injector::WriteMemoryRaw(imageBase + 0xEB972, "\x5C", 1, true);
				injector::WriteMemoryRaw(imageBase + 0xEB982, "\x8B\x70\x18\x89\x4C\x24\x78\x89\x74\x24\x54\x8B\xB4\x24\x9C\x00", 16, true);
				injector::WriteMemoryRaw(imageBase + 0xEB994, "\x89\x74\x24\x68\x8B\xB4\x24\xA0\x00\x00\x00\x89\x74\x24\x6C\x31\xF6\x89\x74\x24\x60\x89\x74\x24\x64\x89\xB4\x24\x80\x00\x00\x00\x8B\x70\x18\x89\x74\x24\x70\xBE\x00\x00\x80\x3F\x89\x74\x24\x2C\x89\x74\x24\x44\x89\x74\x24\x48\x89\x74\x24\x7C\x89\x74\x24\x20\x89\x74\x24\x3C\x89\x74\x24\x58\x89\x74\x24\x74\x90\x90\x90\x90\x90", 81, true);
			}

			if (!ToBool(config["General"]["Windowed"]))
			{
				injector::MakeRET(0x5B8030, 4);
			}

			// IP stuff for working LAN
			static const char* BroadcastAddress = config["Network"]["BroadcastAddress"].c_str();
			injector::WriteMemory<DWORD>(imageBase + 0x7F824, (DWORD)BroadcastAddress, true);

			iatHook("kernel32.dll", ReadFileWrapTx2, "ReadFile");
			iatHook("kernel32.dll", WriteFileWrapTx2, "WriteFile");

			break;
		}
		case X2Type::Lupin3:
		{
			// TODO: DOCUMENT PATCHES
			injector::MakeNOP(0x48C66C, 12);

			break;
		}
		case X2Type::BattleFantasia:
		{
			// restore retarded patched exes to D: instead of SV
			injector::WriteMemoryRaw(imageBase + 0x1214E0, "D:", 2, true); // 0x5214E0
			injector::WriteMemoryRaw(imageBase + 0x1588C4, "D:", 2, true);

			// skip shitty loop that converts path to uppercase letter by letter
			// what did the game devs smoke
			injector::MakeNOP(imageBase + 0xD062, 2, true);
			// injector::WriteMemory<BYTE>(imageBase + 0xD055, 0xEB, true); // alternative patch?

			injector::MakeJMP(imageBase + 0xFF0A0, ReturnsTrue); //Fixes Dual Input issue

			DWORD oldPageProtection = 0;

			if (ToBool(config["General"]["Windowed"])) 
			{
				VirtualProtect((LPVOID)(imageBase + 0x1131E4), 84, PAGE_EXECUTE_READWRITE, &oldPageProtection);
				windowHooks hooks = { 0 };
				hooks.createWindowExA = imageBase + 0x113204;
				hooks.adjustWindowRect = imageBase + 0x1131FC;
				hooks.updateWindow = imageBase + 0x113208;
				init_windowHooks(&hooks);
				VirtualProtect((LPVOID)(imageBase + 0x1131E4), 84, oldPageProtection, &oldPageProtection);
			}

			break;
		}
		case X2Type::BlazBlue:
		{
			injector::MakeJMP(imageBase + 0xECFD0, ReturnsTrue);
		}

		break;
	}

	if(GameDetect::currentGame == GameID::KOFMIRA)
	{
		// TODO: DOCUMENT PATCHES
		injector::WriteMemory<DWORD>(0x0040447C, 0x000800B8, true);
		injector::WriteMemory<WORD>(0x0040447C+4, 0x9000, true);

		static const char* coinFile = ".\\OpenParrot\\CoinFile%d%02d%02d.txt";
		static const char* RnkUsChr = ".\\OpenParrot\\RnkUsChr%d%02d%02d.txt";
		static const char* RnkWn = ".\\OpenParrot\\RnkWn%d%02d%02d.txt";
		static const char* RnkTa = ".\\OpenParrot\\RnkTa%d%02d%02d.txt";
		static const char* OptionData = ".\\OpenParrot\\OptionData%d%s%s.txt";
		static const char* d = ".\\OpenParrot\\%s";

		injector::WriteMemory<DWORD>(imageBase + 0x13270, (DWORD)coinFile, true);
		injector::WriteMemory<DWORD>(imageBase + 0x136AD, (DWORD)coinFile, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1B914C, (DWORD)RnkUsChr, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAB18, (DWORD)RnkUsChr, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAB60, (DWORD)RnkUsChr, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1B92BC, (DWORD)RnkWn, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAE1D, (DWORD)RnkWn, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAB60, (DWORD)RnkWn, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1B941C, (DWORD)RnkTa, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BB11D, (DWORD)RnkTa, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BB160, (DWORD)RnkTa, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BF1A8, (DWORD)OptionData, true);
		injector::WriteMemory<DWORD>(imageBase + 0xBFEB, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0xC0A7, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x12D61, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x13314, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BA989, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAA31, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAC79, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAD21, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BAF79, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1BB021, (DWORD)d, true);
		injector::WriteMemory<DWORD>(imageBase + 0x1C0049, (DWORD)d, true);
	}

	if (GameDetect::currentGame == GameID::ChaosBreaker)
	{
		DWORD oldPageProtection = 0;

		if (ToBool(config["General"]["Windowed"]))
		{
			// force windowed mode
			injector::MakeNOP(imageBase + 0x150BF, 2, true);

			VirtualProtect((LPVOID)(imageBase + 0x511A4), 64, PAGE_EXECUTE_READWRITE, &oldPageProtection);
			windowHooks hooks = { 0 };
			hooks.createWindowExA = imageBase + 0x511C4;
			hooks.adjustWindowRect = imageBase + 0x511B4;
			hooks.setWindowPos = imageBase + 0x511D0;
			init_windowHooks(&hooks);
			VirtualProtect((LPVOID)(imageBase + 0x511A4), 64, oldPageProtection, &oldPageProtection);

			// change window name
			static const char* title = "OpenParrot - Chaos Breaker";
			injector::WriteMemory<DWORD>(imageBase + 0x152A5, (DWORD)title, true);
		}
	}

	if(GameDetect::currentGame == GameID::ChaseHq2)
	{
		// Skip calibration
		injector::WriteMemory<BYTE>(imageBase + 0x107E3, 0xEB, true);

		if (ToBool(config["General"]["Disable Cel Shaded"]))
		{
			injector::WriteMemory<BYTE>(imageBase + 0x2E022, 1, true);
		}

		if (ToBool(config["General"]["Windowed"]))
		{
			// fix window style
			injector::WriteMemory<WORD>(imageBase + 0x57DF, 0x90CB, true);

			ShowCursor(true);

			// don't teleport the mouse lol
			injector::MakeNOP(imageBase + 0x4B14, 8, true);

			// change window name
			static const char* title = "OpenParrot - Chase H.Q. 2";
			injector::WriteMemory<DWORD>(imageBase + 0x3B58, (DWORD)title, true);
		}

		// IP stuff for working LAN
		static const char* cab1IP = config["Network"]["Cab1IP"].c_str();
		static const char* cab2IP = config["Network"]["Cab2IP"].c_str();
		//static const char* cab3IP = config["Network"]["Cab3IP"].c_str(); // unused, leftover code?
		//static const char* cab4IP = config["Network"]["Cab4IP"].c_str(); // unused, leftover code?

		injector::WriteMemory<DWORD>(imageBase + 0x6439F, (DWORD)cab1IP, true);
		injector::WriteMemory<DWORD>(imageBase + 0x643AA, (DWORD)cab2IP, true);
		//injector::WriteMemory<DWORD>(imageBase + 0x643B5, (DWORD)cab3IP, true); // unused, leftover code?
		//injector::WriteMemory<DWORD>(imageBase + 0x643C0, (DWORD)cab4IP, true); // unused, leftover code?
		injector::WriteMemory<DWORD>(imageBase + 0x6548C, (DWORD)cab1IP, true);
		injector::WriteMemory<DWORD>(imageBase + 0x65C1A, (DWORD)cab1IP, true);
	}
	
	if(GameDetect::currentGame == GameID::TetrisGM3)
	{
		// TODO: DOCUMENT PATCHES
		injector::WriteMemory<DWORD>(0x0046A0AC, 0x00005C2E, true); //required to boot
		// windowed mode patch (if set to windowed in config) Makes it a proper windowed app. Not the first person to have done this patch
		if (ToBool(config["General"]["Windowed"]))
		{
			injector::WriteMemory<BYTE>(0x44DCC9, 0x00, true);
		}
		// user configurable resolution patch.
		// credit to Altimoor
		// get resolution from config file 
		auto resx = ToInt(config["General"]["ResolutionWidth"]);        // original arcade ran at 640
		auto resy = ToInt(config["General"]["ResolutionHeight"]);       // original ran at 480.
		// can't have game window TOO small.
		if (resx < 640)
			resx = 640;
		if (resy < 480)
			resy = 480;
		// and calculate aspect ratio
		auto aspect_ratio = (float)resx / (float)resy; // hope the resolution used isn't too insane.
		// lets patch every routine that needs this. :)
		injector::WriteMemoryRaw(0x40D160, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x40D165, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x40D19A, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x40D19F, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x41F154, &resx, sizeof(resx), true);
		injector::WriteMemoryRaw(0x41F163, &resx, sizeof(resx), true);
		injector::WriteMemoryRaw(0x41F176, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x41F181, &resy, sizeof(resy), true);

		injector::WriteMemoryRaw(0x44DCA6, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44DCAB, &resx, sizeof(resx), true);
		injector::WriteMemoryRaw(0x44DCB0, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44DCB5, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x44DD2D, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44DD32, &resx, sizeof(resx), true);
		injector::WriteMemoryRaw(0x44DD4D, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44DD52, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x44DD6B, &aspect_ratio, sizeof(aspect_ratio), true); // aspect ratio goes here. yes, it's a raw float.

		injector::WriteMemoryRaw(0x44E126, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44E12B, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x44E198, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44E19D, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x44E349, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44E34E, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x44E429, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x44E42E, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x450E5B, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x450E60, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x450E90, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x450E95, &resx, sizeof(resx), true);

		injector::WriteMemoryRaw(0x450ED7, &resy, sizeof(resy), true);
		injector::WriteMemoryRaw(0x450EDC, &resx, sizeof(resx), true);
	}

	if(GameDetect::currentGame == GameID::SamuraiSpiritsSen)
	{
		DWORD oldPageProtection = 0;

		// TODO: DOCUMENT PATCHES
		injector::WriteMemory<DWORD>(0x0040117A, 0x90909090, true);
		injector::WriteMemory<BYTE>(0x0040117A+4, 0x90, true);
		injector::WriteMemory<WORD>(0x00401188, 0x9090, true);
		injector::WriteMemory<WORD>(0x004B73DE, 0x9090, true);
		injector::WriteMemory<BYTE>(0x004B73ED, 0xEB, true);
		injector::WriteMemory<BYTE>(0x004C640C, 0xEB, true);
		injector::WriteMemory<DWORD>(0x004CE1C0, 0xC340C033, true);

		if (ToBool(config["General"]["Windowed"])) 
		{
			// fix window style
			injector::WriteMemory<BYTE>(imageBase + 0xA6F3E, 0xCB, true);

			// show cursor
			injector::WriteMemory<BYTE>(imageBase + 0x1263, 0x01, true);
			injector::MakeNOP(imageBase + 0xA73C8, 7, true);

			// don't lock cursor into window
			VirtualProtect((LPVOID)(imageBase + 0xF32C0), 4, PAGE_EXECUTE_READWRITE, &oldPageProtection);
			windowHooks hooks = { 0 };
			hooks.clipCursor = imageBase + 0xF32C0;
			init_windowHooks(&hooks);
			VirtualProtect((LPVOID)(imageBase + 0xF32C0), 4, oldPageProtection, &oldPageProtection);
		}
	}

	if (GameDetect::currentGame == GameID::KOFSkyStage100J)
	{
		// TODO: fix weird "turbo fire" for the buttons, rotate screen

		//Temp Fix (Remove when properly sorted)
		injector::MakeNOP(imageBase + 0xBD675, 3);
		CreateThread(NULL, 0, RunningLoop, NULL, 0, NULL);

		// don't hide windows and don't break desktop
		injector::WriteMemory<BYTE>(imageBase + 0x12F6E2, 0xEB, true);

		// skip dinput devices (TODO: maybe make original Dinput.dll wrapper?)
		injector::WriteMemory<BYTE>(imageBase + 0xBD1D7, 0xEB, true);

		DWORD oldPageProtection = 0;

		if (ToBool(config["General"]["Windowed"]))
		{
			VirtualProtect((LPVOID)(imageBase + 0x21F290), 4, PAGE_EXECUTE_READWRITE, &oldPageProtection);
			windowHooks hooks = { 0 };
			hooks.createWindowExA = imageBase + 0x21F290;
			init_windowHooks(&hooks);
			VirtualProtect((LPVOID)(imageBase + 0x21F290), 4, oldPageProtection, &oldPageProtection);

			// show cursor
			injector::WriteMemory<BYTE>(imageBase + 0x12F6CA, 0x01, true);
			injector::WriteMemory<BYTE>(imageBase + 0x12F7D8, 0xEB, true);
			injector::WriteMemory<BYTE>(imageBase + 0x12F7FD, 0x01, true);

			// change window title
			static const char* title = "OpenParrot - The King of Fighters Sky Stage";
			injector::WriteMemory<DWORD>(imageBase + 0xBE812, (DWORD)title, true);
		}
	}

	if (GameDetect::currentGame == GameID::TroubleWitches)
	{
		static const char* save = ".\\OpenParrot\\Save";
		static const char* configd = ".\\OpenParrot\\Save\\Config%d.bin";
		static const char* config04d = ".\\OpenParrot\\Save\\Config%04d.bin";

		injector::WriteMemory<DWORD>(imageBase + 0x1F56, (DWORD)save, true);
		injector::WriteMemory<DWORD>(imageBase + 0x95CBF, (DWORD)save, true);
		injector::WriteMemory<DWORD>(imageBase + 0x95D49, (DWORD)configd, true);
		injector::WriteMemory<DWORD>(imageBase + 0x93CF0, (DWORD)config04d, true);

		if (ToBool(config["General"]["Windowed"])) // NOTE: needs external DLL patch for window style
		{
			// fix window title for non-jpn locale
			static const char* title = "OpenParrot - Trouble Witches AC Version 1.00";
			injector::WriteMemory<DWORD>(imageBase + 0x1D82, (DWORD)title, true);

			// don't hide cursor
			injector::WriteMemory<BYTE>(imageBase + 0x1E56, 0x01, true);
		}
	}

	if (GameDetect::currentGame == GameID::WackyRaces)
	{
		// IP stuff for working LAN
		static const char* cab1IP = config["Network"]["Cab1IP"].c_str();
		static const char* cab2IP = config["Network"]["Cab2IP"].c_str();
		static const char* cab3IP = config["Network"]["Cab3IP"].c_str();
		static const char* cab4IP = config["Network"]["Cab4IP"].c_str();

		injector::WriteMemory<DWORD>(imageBase + 0xA4558, (DWORD)cab1IP, true);
		injector::WriteMemory<DWORD>(imageBase + 0xA4566, (DWORD)cab2IP, true);
		injector::WriteMemory<DWORD>(imageBase + 0xA4574, (DWORD)cab3IP, true);
		injector::WriteMemory<DWORD>(imageBase + 0xA4582, (DWORD)cab4IP, true);
		injector::WriteMemory<DWORD>(imageBase + 0xA6ECF, (DWORD)cab1IP, true);
		injector::WriteMemory<DWORD>(imageBase + 0xA7371, (DWORD)cab1IP, true);
	}

	if (GameDetect::currentGame == GameID::SF4) 
	{
		DWORD oldPageProtection = 0;

		if (ToBool(config["General"]["Windowed"]))
		{
			VirtualProtect((LPVOID)(imageBase + 0x1F92EC), 4, PAGE_EXECUTE_READWRITE, &oldPageProtection);
			windowHooks hooks = { 0 };
			hooks.createWindowExW = imageBase + 0x1F92EC;
			init_windowHooks(&hooks);
			VirtualProtect((LPVOID)(imageBase + 0x1F92EC), 4, oldPageProtection, &oldPageProtection);

			// X and Y
			injector::WriteMemory<LONG>(imageBase + 0x11DDF1, (long)0x0000000, true);
			injector::WriteMemory<LONG>(imageBase + 0x11DDEC, (long)0x0000000, true);

			// don't hide mouse
			injector::MakeNOP(imageBase + 0x15D6E6, 8, true);
		}
	}

	if (GameDetect::currentGame == GameID::SSFAE)
	{
		if (ToBool(config["General"]["Windowed"]))
		{
			// change window style
			injector::WriteMemory<WORD>(imageBase + 0x210073, 0x90CB, true);

			// X and Y
			injector::WriteMemory<LONG>(imageBase + 0x210067, (long)0x0000000, true);
			injector::WriteMemory<LONG>(imageBase + 0x21006C, (long)0x0000000, true);

			// don't hide mouse
			injector::MakeNOP(imageBase + 0x20E1D2, 8, true);

			// change window title
			static const wchar_t* title = L"OpenParrot - Super Street Fighter IV Arcade Edition - %s %s";
			injector::WriteMemory<DWORD>(imageBase + 0x39EAF8, (DWORD)title, true);
		}
	}

	if (GameDetect::currentGame == GameID::SSFAE_EXP)
	{
		if (ToBool(config["General"]["Windowed"]))
		{
			// change window style
			injector::WriteMemory<LONG>(imageBase + 0x20C111, WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, true);

			// X and Y
			injector::WriteMemory<LONG>(imageBase + 0x20C10C, (long)0x0000000, true);
			injector::WriteMemory<LONG>(imageBase + 0x20C107, (long)0x0000000, true);

			// don't hide mouse
			injector::MakeNOP(imageBase + 0x20A2F2, 8, true);

			// change window title
			static const wchar_t* title = L"OpenParrot - Super Street Fighter IV Arcade Edition (Export) - %s %s";
			injector::WriteMemory<DWORD>(imageBase + 0x387E08, (DWORD)title, true);
		}
	}

	if (GameDetect::currentGame == GameID::SSFAE2012)
	{
		if (ToBool(config["General"]["Windowed"]))
		{
			// change window style
			injector::WriteMemory<LONG>(imageBase + 0x210351, WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, true);

			// X and Y
			injector::WriteMemory<LONG>(imageBase + 0x21034C, (long)0x0000000, true);
			injector::WriteMemory<LONG>(imageBase + 0x210347, (long)0x0000000, true);

			// don't hide mouse
			injector::MakeNOP(imageBase + 0x20E4B2, 8, true);

			// change window title
			static const wchar_t* title = L"OpenParrot - Super Street Fighter IV Arcade Edition Ver.2012 - %s %s";
			injector::WriteMemory<DWORD>(imageBase + 0x39F848, (DWORD)title, true);
		}
	}

	if (GameDetect::currentGame == GameID::KOFXIII) 
	{
		DWORD oldPageProtection = 0;

		if (ToBool(config["General"]["Windowed"])) 
		{
			VirtualProtect((LPVOID)(imageBase + 0x2A8288), 72, PAGE_EXECUTE_READWRITE, &oldPageProtection);
			windowHooks hooks = { 0 };
			hooks.createWindowExW = imageBase + 0x2A82C4;
			hooks.adjustWindowRectEx = imageBase + 0x2A82C8;
			hooks.setWindowPos = imageBase + 0x2A82B8;
			init_windowHooks(&hooks);
			VirtualProtect((LPVOID)(imageBase + 0x2A8288), 72, oldPageProtection, &oldPageProtection);

			// fix window style
			injector::WriteMemory<BYTE>(imageBase + 0xCE9A7, 0x90, true);

			// change window title
			static const wchar_t* title = L"OpenParrot - The King of Fighters XIII";
			injector::WriteMemory<DWORD>(imageBase + 0xCE7F9, (DWORD)title, true);

			injector::MakeNOP(imageBase + 0xCE836, 8, true);
		}
	}

	if (GameDetect::currentGame == GameID::KOFXII) 
	{
		if (ToBool(config["General"]["Windowed"])) 
		{
			injector::MakeNOP(imageBase + 0x2591D7, 2, true);

			// change window title
			static const char* title = "OpenParrot - The King of Fighters XII";
			injector::WriteMemory<DWORD>(imageBase + 0x1D6191, (DWORD)title, true);
		}
	}

	if (GameDetect::currentGame == GameID::RaidenIII) 
	{
		if (ToBool(config["General"]["Windowed"])) 
		{
			// fix window style
			injector::WriteMemory<BYTE>(imageBase + 0x50090, 0xCB, true);
			// change window title
			static const char* title = "OpenParrot - Raiden III";
			injector::WriteMemory<DWORD>(imageBase + 0x50093, (DWORD)title, true);

			// show cursor
			injector::WriteMemory<BYTE>(imageBase + 0x500E2, 0x01, true);
		}
	}

	if (GameDetect::currentGame == GameID::PowerInstinctV)
	{
		if (ToBool(config["General"]["Windowed"])) 
		{
			// show cursor
			injector::MakeNOP(imageBase + 0xBF89E, 6, true);

			// don't move cursor to 2147483647
			injector::MakeNOP(imageBase + 0xC0367, 16, true);
		}
	}

	if (GameDetect::currentGame == GameID::Shigami3)
	{
		if (ToBool(config["General"]["Windowed"])) 
		{
			// show cursor
			injector::WriteMemory<BYTE>(imageBase + 0x17FA58, 0x01, true);
		}
	}

	if (GameDetect::currentGame == GameID::SpicaAdventure)
	{
		DWORD oldPageProtection = 0;

		if (ToBool(config["General"]["Windowed"])) 
		{
			// fix window style
			injector::WriteMemory<BYTE>(imageBase + 0x4AD9E, 0xCB, true);

			// show cursor
			injector::MakeNOP(imageBase + 0x4B1D2, 7, true);

			// don't lock cursor into window
			VirtualProtect((LPVOID)(imageBase + 0x140288), 4, PAGE_EXECUTE_READWRITE, &oldPageProtection);
			windowHooks hooks = { 0 };
			hooks.clipCursor = imageBase + 0x140288;
			init_windowHooks(&hooks);
			VirtualProtect((LPVOID)(imageBase + 0x140288), 4, oldPageProtection, &oldPageProtection);
		}
	}

	if (GameDetect::currentGame == GameID::KOF98UM)
	{
		//static const char* d = ".\\OpenParrot\\%s";
		//static const char* s04d02d02d = ".\\OpenParrot\\%s%04d%02d%02d.txt";
		//static const char* s04d02d02d_03d = ".\\OpenParrot\\%s%04d%02d%02d_%03d.txt";

		//injector::WriteMemory<DWORD>(imageBase + 0x12E798, (DWORD)d, true);
		//injector::WriteMemory<DWORD>(imageBase + 0x12E8C7, (DWORD)d, true);
		//injector::WriteMemory<DWORD>(imageBase + 0x12E974, (DWORD)s04d02d02d, true);
		//injector::WriteMemory<DWORD>(imageBase + 0x12E9B9, (DWORD)s04d02d02d_03d, true);

		if (ToBool(config["General"]["Windowed"])) 
		{
			// fix window style
			injector::WriteMemory<BYTE>(imageBase + 0x1CBE, 0xCB, true);

			// show cursor
			injector::WriteMemory<BYTE>(imageBase + 0x1CD8, 0x01, true);
		}
	}
});
#endif
#pragma optimize("", on)
