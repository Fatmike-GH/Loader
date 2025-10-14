#include <windows.h>
#include <iostream>
#include <string>

int main()
{
  MessageBoxA(NULL, "Try to find the correct key!", "Information", MB_OK);

  std::cout << "Enter the key: ";
  
  std::string input;
  std::cin >> input;

  if (input == "secret")
  {
    std::cout << "The key is correct!";
  }
  else
  {
    std::cout << "The key is WRONG!";
  }

  std::cout << std::endl << "Press Enter to exit...";
  std::cin.ignore();
  std::cin.get();
}
