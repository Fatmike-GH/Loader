#include <windows.h>
#include <tlhelp32.h>
#include "ModuleResolver.h"

ModuleResolver::ModuleResolver()
{
}

ModuleResolver::~ModuleResolver()
{
}

HMODULE ModuleResolver::GetRemoteModuleHandle(HANDLE hProcess, const wchar_t* moduleName)
{
  if (hProcess == NULL || moduleName == NULL)
  {
    return NULL;
  }

  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(hProcess));
  if (hSnapshot == INVALID_HANDLE_VALUE)
  {
    return NULL;
  }

  MODULEENTRY32 me32;
  me32.dwSize = sizeof(MODULEENTRY32);

  if (!Module32First(hSnapshot, &me32))
  {
    CloseHandle(hSnapshot);
    return NULL;
  }

  HMODULE hModule = NULL;
  do
  {
    if (_wcsicmp(me32.szModule, moduleName) == 0)
    {
      hModule = me32.hModule;
      break;
    }
  } while (Module32Next(hSnapshot, &me32));

  CloseHandle(hSnapshot);
  return hModule;
}

FARPROC ModuleResolver::GetRemoteProcAddress(HANDLE hProcess, HMODULE hModule, LPCSTR lpProcName)
{
  if (hProcess == NULL || hModule == NULL || lpProcName == NULL)
  {
    return NULL;
  }

  IMAGE_DOS_HEADER dosHeader = { 0 };
  if (!ReadProcessMemory(hProcess, hModule, &dosHeader, sizeof(dosHeader), NULL) || dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
  {
    return NULL;
  }

  IMAGE_NT_HEADERS ntHeaders = { 0 };
  LPVOID ntHeadersAddress = (LPBYTE)hModule + dosHeader.e_lfanew;
  if (!ReadProcessMemory(hProcess, ntHeadersAddress, &ntHeaders, sizeof(ntHeaders), NULL) || ntHeaders.Signature != IMAGE_NT_SIGNATURE)
  {
    return NULL;
  }

  IMAGE_DATA_DIRECTORY exportDataDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (exportDataDir.VirtualAddress == 0 || exportDataDir.Size == 0)
  {
    return NULL;
  }

  IMAGE_EXPORT_DIRECTORY exportDir = { 0 };
  LPVOID exportDirAddress = (LPBYTE)hModule + exportDataDir.VirtualAddress;
  if (!ReadProcessMemory(hProcess, exportDirAddress, &exportDir, sizeof(exportDir), NULL))
  {
    return NULL;
  }

  DWORD* funcAddresses = new DWORD[exportDir.NumberOfFunctions];
  DWORD* nameAddresses = new DWORD[exportDir.NumberOfNames];
  WORD* nameOrdinals = new WORD[exportDir.NumberOfNames];

  if (!ReadProcessMemory(hProcess, (LPBYTE)hModule + exportDir.AddressOfFunctions, funcAddresses, exportDir.NumberOfFunctions * sizeof(DWORD), NULL) ||
      !ReadProcessMemory(hProcess, (LPBYTE)hModule + exportDir.AddressOfNames, nameAddresses, exportDir.NumberOfNames * sizeof(DWORD), NULL) ||
      !ReadProcessMemory(hProcess, (LPBYTE)hModule + exportDir.AddressOfNameOrdinals, nameOrdinals, exportDir.NumberOfNames * sizeof(WORD), NULL))
  {
    delete[] funcAddresses;
    delete[] nameAddresses;
    delete[] nameOrdinals;
    return NULL;
  }

  FARPROC procAddress = NULL;
  for (DWORD i = 0; i < exportDir.NumberOfNames; ++i)
  {
    char currentProcName[256] = { 0 };
    if (ReadProcessMemory(hProcess, (LPBYTE)hModule + nameAddresses[i], currentProcName, sizeof(currentProcName), NULL))
    {
      if (_stricmp(lpProcName, currentProcName) == 0)
      {
        WORD ordinal = nameOrdinals[i];
        DWORD functionRva = funcAddresses[ordinal];
        procAddress = (FARPROC)((LPBYTE)hModule + functionRva);
        break;
      }
    }
  }

  delete[] funcAddresses;
  delete[] nameAddresses;
  delete[] nameOrdinals;

  return procAddress;
}