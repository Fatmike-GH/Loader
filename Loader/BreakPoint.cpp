#include "Breakpoint.h"

BreakPoint::BreakPoint(HANDLE process, LPVOID va)
{
  _process = process;
  _va = va;
  _enabled = false;
  memset(_originalCode, 0, sizeof(_originalCode));
}

BreakPoint::~BreakPoint()
{
}


void BreakPoint::Enable()
{
  if (_enabled) return;

  ReadProcessMemory(_process, _va, _originalCode, sizeof(_originalCode), NULL);
  WriteProcessMemory(_process, _va, &_breakpointCode, sizeof(_breakpointCode), NULL);

  _enabled = true;
}

void BreakPoint::Disable()
{
  if (!_enabled) return;

  WriteProcessMemory(_process, _va, &_originalCode, sizeof(_originalCode), NULL);

  _enabled = false;
}