#include "Loader.h"

const std::wstring _path = L"Example.exe";
const std::string _message = "Patched!";
BYTE _patch[2] = { 0x90, 0x90 }; // Replace the conditional jump with NOPs

// Notes
// - Example.exe was packed using UPX 5.0.1
// - The following RVAs are valid for "Debug" configuration, NOT for "Release"
#ifdef _WIN64
LPVOID _rvaJmpOep = (LPVOID)0x000323CB;
LPVOID _rvaPatch = (LPVOID)0x00017C8C;
#else
LPVOID _rvaJmpOep = (LPVOID)0x0002C8C9;
LPVOID _rvaPatch = (LPVOID)0x000180A1;
#endif


int main()
{
  Loader loader;
  loader.CreateSuspended(_path);

  // Resume until jmp OEP of UPX
  loader.ResumeUntilRva(_rvaJmpOep);

  // Now the code is fully unpacked, so we can apply our patch
  loader.WriteToRva(_rvaPatch, _patch, sizeof(_patch));

  // Resume until MessageBoxA to patch the message
  CONTEXT context = loader.ResumeUntilApi(L"User32.dll", "MessageBoxA");

  // Allocate memory in remote process and copy the _message
  LPVOID memory = loader.AllocateMemory(_message.length() + 1);
  loader.WriteToVa(memory, (BYTE*)_message.c_str(), _message.length() + 1);

#ifdef _WIN64
  // In x64 the register Rdx holds the VA for the message string
  context.Rdx = (DWORD64)memory;
  loader.UpdateContext(context);
#else
  // In x86 the VA of the message string is located at Esp + 0x8
  LPVOID messageVa = (LPVOID)(context.Esp + 0x8);
  loader.WriteToVa(messageVa, (BYTE*)&memory, sizeof(LPVOID));
#endif

  // The target is fully patched an can now resume
  loader.Resume();
}
