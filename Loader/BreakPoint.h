#pragma once
#include "windows.h"

class BreakPoint
{
public:
  BreakPoint(HANDLE process, LPVOID va);
  ~BreakPoint();

  LPVOID GetVa() { return _va; }

  void Enable();
  void Disable();

private:
  HANDLE _process;
  LPVOID _va;
  bool _enabled;
  const BYTE _breakpointCode[2] = { 0xEB, 0xFE }; // JMP EIP/RIP
  BYTE _originalCode[2];
};
