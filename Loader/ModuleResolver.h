#pragma once
#include <windows.h>
#include <memory>


class ModuleResolver
{
public:
  ModuleResolver();
  ~ModuleResolver();

  HMODULE GetRemoteModuleHandle(HANDLE hProcess, const wchar_t* moduleName);
  FARPROC GetRemoteProcAddress(HANDLE hProcess, HMODULE hModule, LPCSTR lpProcName);

private:
  HANDLE CreateModuleSnapshot(HANDLE hProcess);
  HMODULE FindModuleInSnapshot(HANDLE hSnapshot, const wchar_t* moduleName);

  bool ReadDosHeader(HANDLE hProcess, HMODULE hModule, IMAGE_DOS_HEADER& dosHeader);
  bool ReadNtHeaders(HANDLE hProcess, HMODULE hModule, const IMAGE_DOS_HEADER& dosHeader, IMAGE_NT_HEADERS& ntHeaders);
  bool ReadExportDirectory(HANDLE hProcess, HMODULE hModule, const IMAGE_DATA_DIRECTORY& dataDir, IMAGE_EXPORT_DIRECTORY& exportDir);

  bool ReadExportTables(HANDLE hProcess,
                        HMODULE hModule,
                        const IMAGE_EXPORT_DIRECTORY& exportDir,
                        std::unique_ptr<DWORD[]>& funcAddresses,
                        std::unique_ptr<DWORD[]>& nameAddresses,
                        std::unique_ptr<WORD[]>& nameOrdinals);

  FARPROC FindProcedureByName(HANDLE hProcess,
                              HMODULE hModule,
                              const IMAGE_EXPORT_DIRECTORY& exportDir,
                              LPCSTR lpProcName,
                              const std::unique_ptr<DWORD[]>& funcAddresses,
                              const std::unique_ptr<DWORD[]>& nameAddresses,
                              const std::unique_ptr<WORD[]>& nameOrdinals);
};