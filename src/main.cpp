#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <unistd.h>
#include <vector>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif
const std::unordered_set<std::string> built_in_commands = {"echo", "type", "exit"};

struct ParsedCommand {
  std::string command;
  std::vector<std::string> args;
};

std::vector<std::string> pre_process_args(const std::string& args) {

  std::vector<std::string> processed_args;
  std::string cur;

  enum State {NORMAL, IN_SINGLE, IN_DOUBLE};
  State state = NORMAL;

  // at a time we can only be inside one. if inside single quote double quotes are regular chars
  //and vice versa

  for(char ch : args) {
    //right now ' and " are same.. only difference is
    //inside ' , " has no special meaning and
    //inside " , ' has no special meaning

    //only one of ' / " is valid. Cant be inside both at at a time
    if(ch == '\'' && state != IN_DOUBLE) {
      state = (state == IN_SINGLE) ? NORMAL : IN_SINGLE;
      continue;
    }
    if(ch == '"' && state != IN_SINGLE) {
      state = (state == IN_DOUBLE) ? NORMAL : IN_DOUBLE;
      continue;
    }

    if(state == IN_SINGLE || state == IN_DOUBLE) {
      cur += ch;
    }else {
      if(ch == ' ' && !cur.empty()) {
        processed_args.push_back(cur);
        cur.clear();
      }else if(ch == ' ') {
        continue;
      }else {
        cur += ch;
      }
    }
  }

  if(!cur.empty()) {
    processed_args.push_back(cur);
  }
  return processed_args;
}

ParsedCommand parse_command(const std::string& user_input) {

  std::istringstream iss(user_input);
  ParsedCommand parsed;
  iss >> parsed.command;
  std::string args;
  //pushes rest of arguments from cmd end to end of input in args string
  std::getline(iss, args);
  //handle single quotes
  parsed.args = pre_process_args(args);
  return parsed;
}

using CommandHandler = std::function<void(const ParsedCommand&)>;

std::string find_executable_path(const std::string& command) {
  char* path_env = std::getenv("PATH");
  if(path_env == nullptr) {
    return "";
  }

  std::stringstream path_ss(path_env);
  std::string path;
  while(std::getline(path_ss, path, PATH_LIST_SEPARATOR)) {
    std::string exec_path = path + "/" + command;
    if(access(exec_path.c_str(), X_OK) == 0) {
      return exec_path;
    }
  }

  return "";
}

void handle_echo(const ParsedCommand& parsed_command) {
  for(const std::string& arg : parsed_command.args) {
    std::cout << arg << " ";
  }
  std::cout << std::endl;
}

void handle_type(const ParsedCommand& parsed_command) {
  for(const std::string& arg : parsed_command.args) {
    bool bFoundArg = false;
    if(built_in_commands.find(arg) != built_in_commands.end()) {
      std::cout << arg << " is a shell builtin" << std::endl;
      bFoundArg = true;
    } else {
      std::string exec_path = find_executable_path(arg);
      if(!exec_path.empty()) {
        std::cout << arg << " is " << exec_path << std::endl;
        bFoundArg = true;
      }
    }

    if(!bFoundArg) {
      std::cout << arg << ": not found" << std::endl;
    }
  }
}

void handle_external(const ParsedCommand& parsed_command, const std::string& user_input) {
  std::string exec_path = find_executable_path(parsed_command.command);
  if(exec_path.empty()) {
    std::cout << user_input << ": command not found" << std::endl;
    return;
  }

  std::system(user_input.c_str());
}

const std::unordered_map<std::string, CommandHandler> built_in_handlers = {
  {"echo", handle_echo},
  {"type", handle_type}
};

bool repl(const std::string& user_input) {

  ParsedCommand parsed_command = parse_command(user_input);
  if(parsed_command.command == "exit") {
    return false;
  }
  auto handler_it = built_in_handlers.find(parsed_command.command);
  if(handler_it != built_in_handlers.end()) {
    handler_it->second(parsed_command);
    return true;
  }

  handle_external(parsed_command, user_input);
  return true;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true) {
    std::cout << "$ ";
    std::string user_input;
    std::getline(std::cin, user_input);
    if(user_input.empty()) {
      continue;
    }

    if(!repl(user_input)) {
      break;
    }
  }

  return 0;
}
