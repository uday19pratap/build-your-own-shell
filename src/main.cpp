#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true) {
    std::cout << "$ ";
    std::string input;
    std::getline(cin, input);
    std::cout << input << ": command not found" << std::endl;
  }
  return 0;
}
