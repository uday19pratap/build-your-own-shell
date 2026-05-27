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
    std::getline(std::cin, input);
    if(input == "exit") {
      return 0;
    }else if(input.substr(0,5) == "echo ") {
      for(int i = 5; i <= input.length(); i++) {
        if(input[i] == ' ') {
          continue;
        }
        std::cout << input.substr(i) << std::endl;
        break;
      }
    }else {
      std::cout << input << ": command not found" << std::endl;
    }
  }
  return 1;
}
