#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true) {
    std::cout << "$ ";
    std::string user_input;
    std::getline(std::cin, user_input);
    if(user_input == "exit") {
      return 0;
    }else if(user_input.substr(0,5) == "echo ") {
      for(int i = 5; i <= user_input.length(); i++) {
        if(user_input[i] == ' ') {
          continue;
        }
        std::string arg = user_input.substr(i);
        std::cout << arg << std::endl;
        break;
      }
    }else {
      std::cout << user_input << ": command not found" << std::endl;
    }
  }
  return 1;
}
