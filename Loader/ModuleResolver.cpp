#include <windows.h>
#include <tlhelp32.h>
#include <memory>
#include <string.h>
#include "ModuleResolver.h"


ModuleResolver::ModuleResolver()
{
}

ModuleResolver::~ModuleResolver()
{
}

HMODULE ModuleResolver::GetRemoteModuleHandle(HANDLE hProcess, const wchar_t* moduleName)
{
  if (hProcess == nullptr || moduleName == nullptr)
    return nullptr;

  HANDLE hSnapshot = CreateModuleSnapshot(hProcess);
  if (hSnapshot == INVALID_HANDLE_VALUE)
    return nullptr;

  HMODULE hModule = FindModuleInSnapshot(hSnapshot, moduleName);
  CloseHandle(hSnapshot);
  return hModule;
}

FARPROC ModuleResolver::GetRemoteProcAddress(HANDLE hProcess, HMODULE hModule, LPCSTR lpProcName)
{
  if (hProcess == nullptr || hModule == nullptr || lpProcName == nullptr)
    return nullptr;

  IMAGE_DOS_HEADER dosHeader = { 0 };
  if (!ReadDosHeader(hProcess, hModule, dosHeader))
    return nullptr;

  IMAGE_NT_HEADERS ntHeaders = { 0 };
  if (!ReadNtHeaders(hProcess, hModule, dosHeader, ntHeaders))
    return nullptr;

  const IMAGE_DATA_DIRECTORY& exportDataDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (exportDataDir.VirtualAddress == 0 || exportDataDir.Size == 0)
    return nullptr;

  IMAGE_EXPORT_DIRECTORY exportDir = { 0 };
  if (!ReadExportDirectory(hProcess, hModule, exportDataDir, exportDir))
    return nullptr;

  std::unique_ptr<DWORD[]> funcAddresses;
  std::unique_ptr<DWORD[]> nameAddresses;
  std::unique_ptr<WORD[]> nameOrdinals;

  if (!ReadExportTables(hProcess, hModule, exportDir, funcAddresses, nameAddresses, nameOrdinals))
    return nullptr;

  return FindProcedureByName(hProcess, hModule, exportDir, lpProcName, funcAddresses, nameAddresses, nameOrdinals);
}

HANDLE ModuleResolver::CreateModuleSnapshot(HANDLE hProcess)
{
  DWORD processId = GetProcessId(hProcess);
  return CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
}

HMODULE ModuleResolver::FindModuleInSnapshot(HANDLE hSnapshot, const wchar_t* moduleName)
{
  MODULEENTRY32 me32 = { 0 };
  me32.dwSize = sizeof(MODULEENTRY32);

  if (!Module32First(hSnapshot, &me32))
    return nullptr;

  do
  {
    if (_wcsicmp(me32.szModule, moduleName) == 0)
      return me32.hModule;
  } while (Module32Next(hSnapshot, &me32));

  return nullptr;
}

bool ModuleResolver::ReadDosHeader(HANDLE hProcess, HMODULE hModule, IMAGE_DOS_HEADER& dosHeader)
{
  return ReadProcessMemory(hProcess, hModule, &dosHeader, sizeof(dosHeader), nullptr)
    && dosHeader.e_magic == IMAGE_DOS_SIGNATURE;
}

bool ModuleResolver::ReadNtHeaders(HANDLE hProcess, HMODULE hModule, const IMAGE_DOS_HEADER& dosHeader, IMAGE_NT_HEADERS& ntHeaders)
{
  LPVOID ntHeadersAddress = (LPBYTE)hModule + dosHeader.e_lfanew;
  return ReadProcessMemory(hProcess, ntHeadersAddress, &ntHeaders, sizeof(ntHeaders), nullptr)
    && ntHeaders.Signature == IMAGE_NT_SIGNATURE;
}

bool ModuleResolver::ReadExportDirectory(HANDLE hProcess, HMODULE hModule, const IMAGE_DATA_DIRECTORY& dataDir, IMAGE_EXPORT_DIRECTORY& exportDir)
{
  LPVOID exportDirAddress = (LPBYTE)hModule + dataDir.VirtualAddress;
  return ReadProcessMemory(hProcess, exportDirAddress, &exportDir, sizeof(exportDir), nullptr);
}

bool ModuleResolver::ReadExportTables(HANDLE hProcess,
                                      HMODULE hModule,
                                      const IMAGE_EXPORT_DIRECTORY& exportDir,
                                      std::unique_ptr<DWORD[]>& funcAddresses,
                                      std::unique_ptr<DWORD[]>& nameAddresses,
                                      std::unique_ptr<WORD[]>& nameOrdinals)
{
  funcAddresses = std::make_unique<DWORD[]>(exportDir.NumberOfFunctions);
  nameAddresses = std::make_unique<DWORD[]>(exportDir.NumberOfNames);
  nameOrdinals = std::make_unique<WORD[]>(exportDir.NumberOfNames);

  bool success = ReadProcessMemory(hProcess, (LPBYTE)hModule + exportDir.AddressOfFunctions, funcAddresses.get(), exportDir.NumberOfFunctions * sizeof(DWORD), nullptr) &&
    ReadProcessMemory(hProcess, (LPBYTE)hModule + exportDir.AddressOfNames, nameAddresses.get(), exportDir.NumberOfNames * sizeof(DWORD), nullptr) &&
    ReadProcessMemory(hProcess, (LPBYTE)hModule + exportDir.AddressOfNameOrdinals, nameOrdinals.get(), exportDir.NumberOfNames * sizeof(WORD), nullptr);

  if (!success)
  {
    funcAddresses.reset();
    nameAddresses.reset();
    nameOrdinals.reset();
  }

  return success;
}

FARPROC ModuleResolver::FindProcedureByName(HANDLE hProcess,
                                            HMODULE hModule,
                                            const IMAGE_EXPORT_DIRECTORY& exportDir,
                                            LPCSTR lpProcName,
                                            const std::unique_ptr<DWORD[]>& funcAddresses,
                                            const std::unique_ptr<DWORD[]>& nameAddresses,
                                            const std::unique_ptr<WORD[]>& nameOrdinals)
{
  for (DWORD i = 0; i < exportDir.NumberOfNames; ++i)
  {
    char currentProcName[256] = { 0 };
    if (ReadProcessMemory(hProcess, (LPBYTE)hModule + nameAddresses[i], currentProcName, sizeof(currentProcName), nullptr))
    {
      if (_stricmp(lpProcName, currentProcName) == 0)
      {
        WORD ordinal = nameOrdinals[i];
        DWORD functionRva = funcAddresses[ordinal];
        return (FARPROC)((LPBYTE)hModule + functionRva);
      }
    }
  }
  return nullptr;
}
