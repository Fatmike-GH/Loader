#pragma once
#include <windows.h>

class ModuleResolver
{
public:
  ModuleResolver();
  ~ModuleResolver();

  HMODULE GetRemoteModuleHandle(HANDLE hProcess, const wchar_t* moduleName);
  FARPROC GetRemoteProcAddress(HANDLE hProcess, HMODULE hModule, LPCSTR lpProcName);
};