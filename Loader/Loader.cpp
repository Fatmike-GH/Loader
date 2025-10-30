#include <windows.h>
#include <winternl.h>
#include "Loader.h"
#include "ModuleResolver.h"

Loader::Loader()
{
  _startInfo = { 0 };
  _processInfo = { 0 };
  _imageBase = nullptr;
  _entryPoint = nullptr;
}

Loader::~Loader()
{
}

bool Loader::CreateSuspended(const std::wstring& path)
{
  _startInfo.cb = sizeof(_startInfo);
  if (CreateProcess(path.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &_startInfo, &_processInfo))
  {
    _imageBase = GetImageBaseFromSuspendedProcess();
    _entryPoint = GetEntryPointFromHeader();
    return true;
  }
  else
  {
    return false;
  }
}

void Loader::Suspend()
{
  SuspendThread(_processInfo.hThread);
}

void Loader::Resume()
{
  ResumeThread(_processInfo.hThread);
}

CONTEXT Loader::ResumeUntilVa(LPVOID va)
{
  BreakPoint breakPoint(_processInfo.hProcess, va);
  breakPoint.Enable();
  Resume();
  CONTEXT context = Wait(breakPoint);
  Suspend();
  breakPoint.Disable();
  return context;
}

CONTEXT Loader::ResumeUntilRva(LPVOID rva)
{
  return ResumeUntilVa(RvaToVa(rva));
}

CONTEXT Loader::ResumeUntilApi(const wchar_t* moduleName, LPCSTR procName)
{
  ModuleResolver moduleResolver;
  HMODULE module = moduleResolver.GetRemoteModuleHandle(_processInfo.hProcess, moduleName);
  FARPROC address = moduleResolver.GetRemoteProcAddress(_processInfo.hProcess, module, procName);
  return ResumeUntilVa(address);
}

void Loader::ReadFromVa(LPVOID va, BYTE* data, SIZE_T size)
{
  ReadProcessMemory(_processInfo.hProcess, va, data, size, NULL);
}

void Loader::ReadFromRva(LPVOID rva, BYTE* data, SIZE_T size)
{
  LPVOID va = RvaToVa(rva);
  ReadFromVa(va, data, size);
}

void Loader::WriteToVa(LPVOID va, BYTE* data, SIZE_T size)
{
  WriteProcessMemory(_processInfo.hProcess, va, data, size, NULL);
}

void Loader::WriteToRva(LPVOID rva, BYTE* data, SIZE_T size)
{
  LPVOID va = RvaToVa(rva);
  WriteToVa(va, data, size);
}

void Loader::UpdateContext(CONTEXT& context)
{
  SetThreadContext(_processInfo.hThread, &context);
}

LPVOID Loader::AllocateMemory(SIZE_T size)
{
  return VirtualAllocEx(_processInfo.hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

CONTEXT Loader::Wait(BreakPoint& breakpoint)
{
  CONTEXT context = { 0 };
  context.ContextFlags = CONTEXT_ALL;

  while (GetThreadContext(_processInfo.hThread, &context))
  {
#ifdef _WIN64
    if (context.Rip == (DWORD64)breakpoint.GetVa())
    {
      return context;
    }
#else
    if (context.Eip == (DWORD64)breakpoint.GetVa())
    {
      return context;
    }
#endif
    Sleep(200);
  }
}

LPVOID Loader::GetImageBaseFromSuspendedProcess()
{
  CONTEXT context = { 0 };
  context.ContextFlags = CONTEXT_ALL;
  GetThreadContext(_processInfo.hThread, &context);

#ifdef _WIN64
  PVOID pebAddress = (PVOID)context.Rdx;
#else
  PVOID pebAddress = (PVOID)context.Ebx;
#endif
  if (pebAddress == nullptr) return nullptr;

  PEB peb = { 0 };
  ReadProcessMemory(_processInfo.hProcess, pebAddress, &peb, sizeof(peb), NULL);
  LPVOID imageBase = peb.Reserved3[1];
  return imageBase;
}

LPVOID Loader::GetEntryPointFromHeader()
{
  IMAGE_DOS_HEADER dosHeader = { 0 };

  ReadProcessMemory(_processInfo.hProcess, _imageBase, &dosHeader, sizeof(dosHeader), nullptr);

  if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
  {
    return nullptr;
  }

  IMAGE_NT_HEADERS ntHeaders = { 0 };
  LPVOID ntHeaderAddr = (LPBYTE)_imageBase + dosHeader.e_lfanew;

  ReadProcessMemory(_processInfo.hProcess, ntHeaderAddr, &ntHeaders, sizeof(ntHeaders), nullptr);
  if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
  {
    return nullptr;
  }

  DWORD entryPointRva = ntHeaders.OptionalHeader.AddressOfEntryPoint;
  return RvaToVa((LPVOID)entryPointRva);
}