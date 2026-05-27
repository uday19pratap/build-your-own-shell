#include <iostream>
#include <string>
#include <sstream>
#include <unordered_set>
#include <unistd.h>
#include <vector>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif
const std::unordered_set<std::string> built_in_commands = {"echo", "type", "exit"};

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true) {
    std::cout << "$ ";
    std::string user_input;
    std::getline(std::cin, user_input);
    if(user_input == "exit") {
      return 0;
    }else if(user_input.substr(0,5) == "echo ") {
      std::string args = user_input.substr(5);
      std::stringstream ss(args);
      std::string arg;
      while(ss >> arg) {
        std::cout << arg << " ";
      }
      std::cout << std::endl;
    }else if(user_input.substr(0,5) == "type ") {
      std::string args = user_input.substr(5);
      std::stringstream ss(args);
      std::string arg;
      while(ss >> arg) {
        //built in command
        bool bFoundArg = false;
        if(built_in_commands.find(arg) != built_in_commands.end()) {
          std::cout << arg << " is a shell builtin" << std::endl;
          bFoundArg = true;
        }else {
          //not a built in command, search for binary in PATH
          char* path_env = std::getenv("PATH");
          if(path_env != nullptr) { 
            std::stringstream path_ss(path_env);
            std::string path;
            while(std::getline(path_ss, path, PATH_LIST_SEPARATOR)) {
              std::string exec_path = path + "/" + arg;
              // 0 means file exists and has exec permission
              // -1 means no file/no exec permission
              if(access(exec_path.c_str(), X_OK) == 0) {
                std::cout << arg << " is " << exec_path << std::endl;
                bFoundArg = true;
                break;
              }
            }
          }
        }
        //neither a built in command nor a executable in PATH. Print not found.
        if(!bFoundArg) {
          std::cout << arg << ": not found" << std::endl;
        }
      }
    } else {
      //check if command is an executable in PATH.
      bool bExecuted = false;
      char* path_env = std::getenv("PATH");
      if(path_env != nullptr) {
        std::stringstream user_input_ss(user_input);
        std::stringstream path_ss(path_env);
        std::string cmd;
        user_input_ss >> cmd;
        std::stringstream ss(path_env);
        std::string path;
        while(std::getline(path_ss, path, PATH_LIST_SEPARATOR)) {
          std::string exec_path = path + "/" + cmd;
          // 0 means file exists and has exec permission
          // -1 means no file/no exec permission
          if(access(exec_path.c_str(), X_OK) == 0) {
            std::system(user_input.c_str());
            bExecuted = true;
            break;
          }
        }
      }
      if(!bExecuted) {
      std::cout << user_input << ": command not found" << std::endl;
      }
    }
  }
  return 1;
}
