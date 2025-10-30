#pragma once
#include <windows.h>
#include <string>
#include "Breakpoint.h"

class Loader
{
public:
  Loader();
  ~Loader();

  bool CreateSuspended(const std::wstring& path);
  void Suspend();
  void Resume();

  CONTEXT ResumeUntilVa(LPVOID va);
  CONTEXT ResumeUntilRva(LPVOID rva);  
  CONTEXT ResumeUntilApi(const wchar_t* moduleName, LPCSTR procName);

  void ReadFromVa(LPVOID va, BYTE* data, SIZE_T size);
  void ReadFromRva(LPVOID rva, BYTE* data, SIZE_T size);

  void WriteToVa(LPVOID va, BYTE* data, SIZE_T size);
  void WriteToRva(LPVOID rva, BYTE* data, SIZE_T size);
  
  void UpdateContext(CONTEXT& context);

  LPVOID AllocateMemory(SIZE_T size);

  LPVOID GetEntryPoint() { return _entryPoint; }

private:
  CONTEXT Wait(BreakPoint& breakpoint);
  LPVOID GetImageBaseFromSuspendedProcess();
  LPVOID GetEntryPointFromHeader();
  LPVOID RvaToVa(LPVOID rva) { return (LPVOID)((DWORD64)_imageBase + (DWORD64)rva); }

private:
  STARTUPINFO _startInfo;
  PROCESS_INFORMATION _processInfo;
  LPVOID _imageBase;
  LPVOID _entryPoint;
};