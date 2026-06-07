#include <iostream>
#include <queue>
#include <string>
#include<cstring>
#include<sys/wait.h>
#include <sstream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <functional>
#include <unistd.h>
#include <vector>
#include<fcntl.h>
#include<termios.h>
#include<filesystem>
#include<fstream>

#ifdef _WIN32
constexpr char PATH_LIST_SEPARATOR = ';';
#else
constexpr char PATH_LIST_SEPARATOR = ':';
#endif
std::vector<std::string> commands_history;
static size_t history_index = 0;
size_t history_append_idx = 0;

std::unordered_map<std::string, std::string> declare_map;


const std::unordered_set<std::string> built_in_commands = {"echo", "type", "exit", "complete", "jobs", 
"history", "declare"};
struct ParsedCommand {
  std::string command;
  std::vector<std::string> args;
  std::string user_input;
};
enum class Direction {
  Previous, 
  Next
};
std::unordered_set<char> special_characters_in_double_quotes_set = {'\\', '$', '`', '\n', '"'};

std::vector<std::string> pre_process_input(const std::string& input) {

  std::vector<std::string> processed_args;
  std::string cur;

  enum State {NORMAL, IN_SINGLE, IN_DOUBLE};
  State state = NORMAL;

  // at a time we can only be inside one. if inside single quote double quotes are regular chars
  //and vice versa
  bool is_escape_char_activated = false;
  for(char ch : input) {

    //prev character was \ so this one will 
    if(is_escape_char_activated) {
      //only escape certain special characters
      // like " \ $ ` and newline
      if(state == IN_DOUBLE && !special_characters_in_double_quotes_set.contains(ch)) {
        cur += '\\';
      }
      cur += ch;
      is_escape_char_activated = false;
      continue;
    }
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
      if(ch == '\\' && state == IN_DOUBLE) {
        is_escape_char_activated = true;
        continue;
      }
      cur += ch;
    }else {
      if(ch == '\\') {
        is_escape_char_activated = true;
        continue;
      }
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



void expand_shell_variables(std::vector<std::string>& tokens) {
  for(int i = 0; i < tokens.size(); i++) {
    std::string& token = tokens[i];
    std::string token_updated;
    int idx = 0; 
    while(idx < token.length()) {
      char ch = token[idx];
      if(ch != '$') {
        token_updated += ch;
        idx++;
      }else {
        std::string ros = token.substr(idx + 1);
        for(const auto& [key, value] : declare_map) {
          if(ros.starts_with(key) == false) {
            continue;
          }
          token_updated += value;
          idx = idx + key.size() + 1;
          break;
        }
      }
    }
    token = token_updated;
  }
}
ParsedCommand parse_command(const std::string& user_input) {

  ParsedCommand parsed;

  std::vector<std::string> tokens = pre_process_input(user_input);
  expand_shell_variables(tokens);
  if(tokens.size() > 0) {
   parsed.command = tokens[0];
  }
  for(int i = 1; i < tokens.size(); i++) {
    parsed.args.push_back(tokens[i]);
  }
  parsed.user_input = user_input;
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
  std::string echo_text = "";
  for(const std::string& arg : parsed_command.args) {
    //last argument should not be followed by a space
    //deal in the if case afterwards
    echo_text += arg + " ";
  }
  if(echo_text.size() > 0) {
    echo_text.pop_back();
  }
  std::cout << echo_text << std::endl;
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
// command -->exec script for command


std::unordered_map<std::string, std::string> completeMap;
void handle_complete(const ParsedCommand& parsed_command) {
  std::vector<std::string> args = parsed_command.args;
  if(args.size() == 0) {
    return;
  }

  //support for -p option of complete command
  auto it = std::find(args.begin(), args.end(), "-p");
  size_t idx = it - args.begin();
  if(idx < args.size() - 1) {
    auto it = completeMap.find(args[idx + 1]);
    if(it == completeMap.end()) {
      std::cout << "complete: " << args[idx + 1] << ": no completion specification" << std::endl;
    }else {
      std::cout << "complete -C '" << it->second << "' " << it->first << std::endl;
    }
    return;
  }

  // support for -C option of complete command
  it = std::find(args.begin(), args.end(), "-C");
  idx = it - args.begin();
  if(idx < args.size() - 2 /*&& access((*(it + 1)).c_str(), X_OK) == 0*/) {
    std::string keyCommand = *(it + 2);
    std::string valuePath = *(it + 1);
    completeMap[keyCommand] = valuePath;
    return;
  }

  //support for -r option of complete command
  //remove the registration of a certain command
  it = std::find(args.begin(), args.end(), "-r");
  idx = it - args.begin();
  if(idx < args.size() - 1) {
    std::string keyCommand = *(it + 1);
    size_t n_commands_deregisterd = completeMap.erase(keyCommand);
    if(n_commands_deregisterd == 0) {
      std::cout << "complete: " << keyCommand << ": no completion specification to de-register" << std::endl;
    }
    return;
  }
}

struct Job {
  int id;
  pid_t pid;
  std::string command;
  char status[24] = "Running                ";
  // 24 spaces for padding
};

using minHeap = std::priority_queue<int, std::vector<int>, std::greater<int>>;
minHeap available_slots;

std::map<int, Job> jobid_to_job_map;

bool is_job_done(Job& job) {
  int status;
  pid_t ret = waitpid(job.pid, &status, WNOHANG);
  
  if(ret == job.pid && WIFEXITED(status)) {
    std::strcpy(job.status, "Done                   ");
    while(job.command.back() != '&') {
      job.command.pop_back();
    }
    job.command.pop_back();
    return true;
  }
  return false;
}

int get_next_available_jobid() {

  //empty table... job can be created with id = 1
  int sz = jobid_to_job_map.size();
  if(sz == 0) {
    return 1;
  }
  if(sz == jobid_to_job_map.rbegin()->first) {
    available_slots = minHeap(); //clear the minHeap map
    return sz + 1;
  }
  int retVal = available_slots.top();
  available_slots.pop();
  return retVal;

}
void handle_jobs(const ParsedCommand& parsed_command) {

  //list bg processes
  std::vector<int> reap_ids;

  int total_bg_jobs = jobid_to_job_map.size();
  int idx = 0;
  for(auto& [id, job] : jobid_to_job_map) {
    if(is_job_done(job)) {
      reap_ids.push_back(id);
    }
    // most recent bg jobs --> + ; second most recent --> - ; others --> ' '
    char marker = ' ';
    int diff = total_bg_jobs - idx;
    switch(diff) {
      case 1 : {marker = '+';} break;
      case 2 : {marker = '-';} break;
      default: {marker = ' ';} break;
    }
    std::cout << "[" << job.id << "]" << marker << "  " << job.status << job.command << std::endl;
    idx++;
  }

  //delete done processes from process table
  for(int reap_id : reap_ids) {
    jobid_to_job_map.erase(reap_id);
    //add to available slots
    available_slots.push(reap_id);
  }
}

void handle_history(const ParsedCommand& parsed_command) {
  //if no argument is given
  //n is implicity commands_history.size()
  if(parsed_command.args.size() >=0 && parsed_command.args.size() <= 1) {
    int start_index = commands_history.size();
    int count = start_index;
    if(parsed_command.args.size() > 0) {
      count = atoi(parsed_command.args[0].c_str());
      if(count <= 0 || count > commands_history.size()) {
        return;
      }
    }
    start_index = start_index - count;
    //history n
    //n should range between 1 to commands_history.size()
    for(int i = start_index; i < commands_history.size(); i++) {
      const std::string& command = commands_history[i];
      std::cout << "    " << (i + 1) << " " << command << std::endl;
    }
  }else if(parsed_command.args.size() == 2) {
    //support -r option of history
    std::vector<std::string> args = parsed_command.args;
    auto it = std::find(args.begin(), args.end(), "-r");
    int idx = it - args.begin();
    bool is_r_present = it != args.end() &&  idx != args.size() - 1;
    if(is_r_present) {
      std::string file_name = args[idx + 1];
      std::ifstream file(file_name);
      if(!file) {
        std::cerr << "Failed to open file" << std::endl;
        return;
      }
      std::string line;
      while(std::getline(file, line)) {
        commands_history.push_back(line);
      }
    }
    it = std::find(args.begin(), args.end(), "-w");
    idx = it - args.begin();
    bool is_w_present =  it != args.end() && idx != args.size() - 1;
    if(is_w_present) {
      std::string file_name = args[idx + 1];
      std::ofstream file(file_name, std::ios::trunc);
      if(!file) {
        std::cerr << "Failed to open file" << std::endl;
      }
      for(std::string cmd : commands_history) {
        file << cmd << std::endl;
      }
    }
    
    it = std::find(args.begin(), args.end(), "-a");
    idx = it - args.begin();
    bool is_a_present =  it != args.end() && idx != args.size() - 1;
    if(is_a_present) {
      std::string file_name = args[idx + 1];
      std::ofstream file(file_name, std::ios::app);
      if(!file) {
        std::cerr << "Failed to open file" << std::endl;
      }
      //only append history that was previously not present
      //keep tracking it with history_append_index
      for(int i = history_append_idx; i < commands_history.size(); i++) {
        std::string cmd = commands_history[i];
        file << cmd << std::endl;
      }
      history_append_idx = commands_history.size();
    }
  }
}
std::vector<char*> create_argv_vector_for_fork(ParsedCommand& parsed_command) {
  std::vector<char*> argv;

  std::string& cmd = parsed_command.command;
  std::vector<std::string>& args = parsed_command.args;
  argv.push_back(cmd.data());
  for(std::string& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  return argv;
}
void handle_external(ParsedCommand& parsed_command, bool is_bg) {
  std::string exec_path = find_executable_path(parsed_command.command);
  if(exec_path.empty()) {
    std::cout << parsed_command.command << ": command not found" << std::endl;
    return;
  }

  if(!is_bg) {
    std::vector<char*> argv = create_argv_vector_for_fork(parsed_command);
    pid_t ppid = fork();
    if(ppid == 0) {
      execv(exec_path.c_str(), argv.data());
      perror("execv");
      exit(1);
    }
    int status;
    waitpid(ppid, &status, 0);
  }else {
    std::vector<char*> argv = create_argv_vector_for_fork(parsed_command);
    pid_t ppid = fork();
    if(ppid == 0) {
      execv(exec_path.c_str(), argv.data());
      perror("execv");
      exit(1);
    }else {
      int id = get_next_available_jobid();
      std::cout << "[" << id << "] " << ppid << std::endl;
      Job bg_job = {id, ppid, parsed_command.user_input};
      jobid_to_job_map[id] = bg_job;
    }
  }
}


bool is_key_valid(std::string& key) {
  for(int i = 0; i < key.length(); i++) {
    char ch = key[i];
    if(i == 0) {
      if(std::isalpha(ch) || ch == '_') {
        continue;
      }
      return false;
    }else {
      if(std::isalnum(ch) || ch == '_') {
        continue;
      }
      return false;
    }
  }
  return true;
}
void handle_declare(const ParsedCommand& parsed_command) {
  const std::vector<std::string>& args = parsed_command.args;
  // handle declare key=value
  if(args.size() == 1) {
    std::string key_value = args[0];
    size_t idx = key_value.find('=');
    if(idx != std::string::npos) {
      std::string key = key_value.substr(0, idx);
      std::string value = key_value.substr(idx + 1);
      if(key.empty()) {
        return;
      }
      //key has to start with wither _ or be an english letter
      if(is_key_valid(key)) {
        declare_map[key] = value;
      }else {
        std::cout << "declare: " << "`" << key_value << "': not a valid identifier" << std::endl; 
      }
    }

  }
  //handle declare -p
  auto it = std::find(args.begin(), args.end(), "-p");
  if(it != args.end() && (it + 1) != args.end()) {
    std::string var = *(it + 1);
    if(!var.empty() && declare_map.contains(var)) {
      std::cout << "declare -- " << var << "=\"" << declare_map[var] << "\"" << std::endl; 
    }else {
      std::cout << "declare: " << var << ": not found" << std::endl;
    }
  }

}
const std::unordered_map<std::string, CommandHandler> built_in_handlers = {
  {"echo", handle_echo},
  {"type", handle_type},
  {"complete", handle_complete},
  {"jobs", handle_jobs},
  {"history", handle_history},
  {"declare", handle_declare}
};

void adjust_out_stream(ParsedCommand& parsed_command) {
  std::vector<std::string> args = parsed_command.args;
  if(args.size() >= 3) {
    std::string second_last_args = args[args.size() - 2];
    if(second_last_args == ">" || second_last_args == "1>" ||
       second_last_args == "2>" ||
       second_last_args == ">>" || second_last_args == "1>>" ||
       second_last_args == "2>>") {

      std::string fname = args[args.size() - 1];

      int new_fd;
      if(second_last_args == ">>" || second_last_args == "1>>" || second_last_args == "2>>") {
      // set falgs to make it write only, create if non-existent and append 0644 for permissions
        new_fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
      }else {
      //set flags to make it write only, create if non-existent and truncate(erase and start fresh) 0644 sets permissions
      new_fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      }
      //pop the > arg and the filename arg
      parsed_command.args.pop_back();
      parsed_command.args.pop_back();
      if(second_last_args == "2>" || second_last_args == "2>>") {
        dup2(new_fd, 2); //change the std error stream to point to new fd (file descriptor)
      }else {
        dup2(new_fd, 1); //change output stream to point to new fg (file descriptor)
      }
    }
  }
}

bool is_bg_job(ParsedCommand& parsed_command) {
  std::vector<std::string>& args = parsed_command.args;
  if(args.size() < 1) {
    return false;
  }
  if(args.back() == "&") {
    parsed_command.args.pop_back();
    return true;
  }
  return false;
}

bool handle_command(ParsedCommand& parsed_command) {
  if(parsed_command.command == "exit") {
    return false;
  }
  bool treat_as_bg_job = is_bg_job(parsed_command);
  int saved_fd_1 = dup(1);
  int saved_fd_2 = dup(2);
  adjust_out_stream(parsed_command);
  auto handler_it = built_in_handlers.find(parsed_command.command);
  if(handler_it != built_in_handlers.end()) {
    handler_it->second(parsed_command);
  }else {
    handle_external(parsed_command, treat_as_bg_job);
  }
  dup2(saved_fd_1, 1);
  dup2(saved_fd_2, 2);
  return true;
}

// a vector is important if there are multiple commands chained together with pipes |
using ParsedCommandList = std::vector<ParsedCommand>;
ParsedCommandList parse_user_input(const std::string& user_input) {

  ParsedCommandList commandList;
  std::stringstream input_ss(user_input);
  std::string cmd;
  while(std::getline(input_ss, cmd, '|')) {
    ParsedCommand parsed_cmd = parse_command(cmd);
    commandList.push_back(parsed_cmd);
  }
  return commandList;
}

struct Pipe {
  int fd[2];
};  
void execute_pipe_commands(ParsedCommandList& parsed_command_list) {
  size_t n = parsed_command_list.size();
  if (n < 2) {
    return;
  }

  std::vector<Pipe> pipes(n - 1);
  for (auto& p : pipes) {
    pipe(p.fd);
  }

  std::vector<pid_t> child_pids;
  child_pids.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    ParsedCommand cmd = parsed_command_list[i];
    int in_fd = (i == 0 ? STDIN_FILENO : pipes[i - 1].fd[0]);
    int out_fd = (i == n - 1 ? STDOUT_FILENO : pipes[i].fd[1]);

    pid_t pid = fork();
    if (pid == 0) {
      if (in_fd != STDIN_FILENO) {
        dup2(in_fd, STDIN_FILENO);
      }
      if (out_fd != STDOUT_FILENO) {
        dup2(out_fd, STDOUT_FILENO);
      }

      for (auto& pipe : pipes) {
        close(pipe.fd[0]);
        close(pipe.fd[1]);
      }

      handle_command(cmd);
      _exit(0);
    }

    if (in_fd != STDIN_FILENO) {
      close(in_fd);
    }
    if (out_fd != STDOUT_FILENO) {
      close(out_fd);
    }

    child_pids.push_back(pid);
  }

  for (pid_t pid : child_pids) {
    waitpid(pid, nullptr, 0);
  }
}

bool repl(const std::string& user_input) {

  ParsedCommandList parsed_command_list = parse_user_input(user_input);

  //regular case...only one command to run...lets not complicate
  if(parsed_command_list.size() == 1) {
    bool retVal = handle_command(parsed_command_list[0]);
    return retVal;
  }
  //multiple smaller command chunks seperated by | pipes
  //considering 2 or more commands, run loop from beg to second last
  execute_pipe_commands(parsed_command_list);
  return true;
}

std::set<std::string> matched_commands_set;
std::string longest_common_prefix_string(const std::set<std::string>& set) {
  if(set.size() == 0) {
    return "";
  }
  //for a sorted set of strings
  //the lcp string is the lcp of the first and last strings in the set
  std::string first = *(set.begin());
  std::string last = *(set.rbegin());

  size_t i = 0;
  while(true) {
    if(i == first.size() || i == last.size()) {
      break;
    }
    if(first[i] != last[i]) {
      break;
    }
    i++;
  }
  std::string lcp = first.substr(0, i);
  return lcp;
}
bool auto_completion_handler(std::string& user_input, bool second_consecutive_tab) {
  
  if(user_input.empty() || user_input.back() == ' ') {
    std::cout << "\a" << std::flush; // ring bell
    return false; // no need to wait for second tab. no results...so equivalent to result being flushed
  }

  if(second_consecutive_tab == true) {
    if(matched_commands_set.size() > 1) {
      std::cout << std::endl;
      for(const auto& match : matched_commands_set) {
        std::cout << match << "  " << std::flush;
      }
      std::cout << std::endl << "$ " << user_input << std::flush;
    }else {
      // 0 matches --> ring the bell
      std::cout << "\a" << std::flush;
    }
    matched_commands_set.clear();
    return false;
  }

  //find matches in built-in-commmands
  for(const std::string& built_in_command : built_in_commands) {
    if(built_in_command.starts_with(user_input)) {
      matched_commands_set.insert(built_in_command);
    }           
  }
  //find matches in exec(binaries)
  char* path_env = std::getenv("PATH");
  if(path_env) {
    std::stringstream path_ss(path_env);
    std::string dir;
    while(std::getline(path_ss, dir, PATH_LIST_SEPARATOR)) {
      std::error_code dir_ec;
      if(!std::filesystem::is_directory(dir, dir_ec) || dir_ec) {
        continue;
      }

      std::error_code it_ec;
      std::filesystem::directory_iterator it(dir, it_ec);
      if(it_ec) {
        // couldn't open this directory; skip it
        continue;
      }

      for(; it != std::filesystem::directory_iterator(); it.increment(it_ec)) {
        if(it_ec) {
          // skip this problematic entry and continue iterating
          it_ec.clear();
          continue;
        }

        std::error_code st_ec;
        const auto p = it->path();
        if(!std::filesystem::is_regular_file(p, st_ec) || st_ec) {
          continue;
        }
        if(access(p.string().c_str(), X_OK) != 0) {
          continue;
        }
        std::string local_fname = p.filename().string();
        if(local_fname.rfind(user_input, 0) == 0) {
          // a match
          matched_commands_set.insert(local_fname);
        }
      }
    }
  }

  //std::cout << "matchsize: " << matched_commands_set.size() << std::endl;
  if(matched_commands_set.size() == 1) {
    user_input = *(matched_commands_set.begin()) + " ";
    std::cout << "\r$ " << user_input << std::flush;
    matched_commands_set.clear();
    return false; //false means -->flushed.. so no scope for second tab
  }else if(matched_commands_set.size() > 1) {
   //extend suggestion to LCP. then on next tab display all the options

    //dont need to send matched_commands_list as parameter because it is a global variable
    std::string lcp = longest_common_prefix_string(matched_commands_set);
    if(lcp.size() > user_input.size()) {
      user_input = lcp;
      std::cout << "\r$ " << user_input << std::flush; //dont add space, because LCP string may not be a suggestion
      return false;
    }else {
      std::cout << "\a" << std::flush;
    }
    return true;
  }else {
    //ring the bell on the first tab if no matches
    std::cout << "\a" << std::flush;
    return true;
  }
}

std::set<std::string> run_completer_script(const ParsedCommand& parsed_command, const std::string& user_input) {
  std::set<std::string> completer_script_results;
  auto it = completeMap.find(parsed_command.command);
  if(it == completeMap.end()) {
    return completer_script_results;
  }
  const char* completer_script = (it->second).c_str();
  std::string exec = (it->second) + " " + parsed_command.command; //argv[0] --> command
  const std::vector<std::string>& args = parsed_command.args;
  if(args.size() > 0) {
    exec += " " + args.back() + " ";  //argv[1] --> word being completed
  }
  if(args.size() > 1) {
    exec += args[args.size() - 2]; //argv[2] --> preceeding word being completed
  }else if(args.size() == 1) {
    exec += parsed_command.command;
  }
  // "r" mode makes sure you can only read from the pipe
  // the child process triggered with completer_script, whatever it writes onto its std out stream
  // we can read from it only.. with "w" we could write to child's input stream and drive it through
  // a series of commands.
  std::string point_str = std::to_string(user_input.size());
  setenv("COMP_LINE", user_input.c_str(), 1);
  setenv("COMP_POINT", point_str.c_str(), 1);

  FILE* pipe = popen(exec.c_str(), "r");
  char buffer[256];
  while(fgets(buffer, sizeof(buffer), pipe)) {
    std::string line = buffer;
    if(line.empty()) continue;
    if(line.back() == '\n') {
      line.pop_back();
    }
    if(line.empty()) continue;
    completer_script_results.insert(line);
  }
  pclose(pipe);
  unsetenv("COMP_LINE");
  unsetenv("COMP_POINT");
  return completer_script_results;
}

void completer_auto_complete(std::set<std::string>& candidates, ParsedCommand& parsed_command, std::string& user_input, bool& consecutive_completer_tab) {

  if(candidates.size() == 1) {
    //move cursor behind. So we can override the half completed argument
    //with completer script reply stored in candidates set
    if(parsed_command.args.size() > 0) {
      int delSize = parsed_command.args.back().size();
      for(int i = 0; i < delSize; i++) {
        std::cout << "\b";
      }
    }
    //write
    std::cout << *candidates.begin() << " " << std::flush;
  }else if(candidates.size() > 1) {
    if(!consecutive_completer_tab) {
      std::string lcp = longest_common_prefix_string(candidates);
      if(lcp.size() > parsed_command.args.back().size()) {
        int delSize = parsed_command.args.back().size();
        for(int i = 0; i < delSize; i++) {
          std::cout << "\b";
          user_input.pop_back();
        }
        user_input += lcp;
        parsed_command.args[parsed_command.args.size() - 1] = lcp;
        std::cout << lcp << std::flush;
      }else {
        std::cout << "\a" << std::flush;
      }  
    }else {
      std::cout << std::endl;
      for(const std::string& candidate : candidates) {
        std::cout << candidate << "  ";
      }
      std::cout << std::endl;
      std::cout << "$ " << user_input << std::flush;
    }
    consecutive_completer_tab = !consecutive_completer_tab;
  }
  return;
}

void move_through_history(Direction dir, std::string& user_input) {
  size_t old_index = history_index;
  if(dir == Direction::Previous) {
    if(history_index == 0) {
      return;
    }
    history_index--;
  } else if(dir == Direction::Next) {
    if(history_index >= commands_history.size()) {
      return;
    }
    history_index++;
  }

  if(history_index >= commands_history.size()) {
    history_index = old_index;
    return;
  }

  std::string cmd = commands_history[history_index];
  user_input = cmd;
  std::cout << "\r\033[K"; //clear line
  std::cout << "\r$ " << cmd;
}

bool handle_arrow_key(char escape_char, std::string& user_input) {
  if (escape_char != 27) {
    return false;
  }

  char ch1;
  if (read(STDIN_FILENO, &ch1, 1) != 1) {
    return true;
  }
  if (ch1 != '[') {
    std::cout << static_cast<char>(escape_char) << ch1;
    user_input.push_back(static_cast<char>(escape_char));
    user_input.push_back(ch1);
    return true;
  }

  char ch2;
  if (read(STDIN_FILENO, &ch2, 1) != 1) {
    return true;
  }

  if (ch2 == 'B') {
    //down key
    move_through_history(Direction::Next, user_input);
  } else if (ch2 == 'A') {
    //up key
    move_through_history(Direction::Previous, user_input);
  } else {
    std::cout << static_cast<char>(escape_char) << ch1 << ch2;
    user_input.push_back(static_cast<char>(escape_char));
    user_input.push_back(ch1);
    user_input.push_back(ch2);
  }
  return true;
}

std::string register_keystrokes_for_command() {
  std::string user_input;
  char ch;
  bool second_consecutive_tab = false;

  std::set<std::string> candidates;
  bool completer_consecutive_tab = false;
  while(true) {
    read(STDIN_FILENO, &ch, 1);
    if(ch != '\t') {
      matched_commands_set.clear();
      second_consecutive_tab = false;
      candidates.clear();
      completer_consecutive_tab = false;
    }
    if(ch == '\n') { //for newline
      std::cout << std::endl << std::flush;
      break;
    }else if (ch == '\t') { //for tab...auto completion
      ParsedCommand parsed_command = parse_command(user_input);
      if(!completer_consecutive_tab) {
        candidates = run_completer_script(parsed_command, user_input);
        //std::cout << std::endl << "CANDIDATES SIZE: " << candidates.size() << std::endl;
      }
      completer_auto_complete(candidates, parsed_command, user_input, completer_consecutive_tab);
      if(candidates.size() == 0) {
        bool second_consecutive_tab = auto_completion_handler(user_input, second_consecutive_tab);
      }
    }else if(ch == 8 || ch == 127) { //for backspace/delete
      if(user_input.size() > 0) {
        user_input.pop_back();
        std::cout << "\b \b" << std::flush; // hello^ -> hell^o -> hell ^ -> hell^
      }
    }else {
      if(ch == 27) {
        handle_arrow_key(ch, user_input);
      } else {
        std::cout << ch;
        user_input.push_back(ch);
      }
    }
  }
  return user_input;
}

void set_raw_terminal_mode_for_keystrokes() {
  termios original_termios;
  tcgetattr(STDIN_FILENO, &original_termios);
  termios raw_termios = original_termios;
  raw_termios.c_lflag = raw_termios.c_lflag & ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
}

void update_history_cursor() {
  history_index = commands_history.size();
}
void push_to_commands_history(const std::string& input) {
  commands_history.push_back(input);
  update_history_cursor();
}

void reap_all_bg_jobs() {

  int total_bg_jobs = jobid_to_job_map.size();

  int idx = 0;
  for(auto it = jobid_to_job_map.begin(); it != jobid_to_job_map.end();) {
    Job job = (it->second);
    if(is_job_done(job)) {
      char marker = ' ';
      int diff = total_bg_jobs - idx;
      switch(diff) {
        case 1 : {marker = '+';} break;
        case 2 : {marker = '-';} break;
        default: {marker = ' ';} break;
      }
      std::strcpy(job.status, "Done                   ");
      std::cout << "[" << job.id << "]" << marker << "  " << job.status << job.command << std::endl;
      //add to available slots
      available_slots.push(job.id);
      it = jobid_to_job_map.erase(it);
    }else {
      it++;
    }
    idx++;
  }
}

void load_commands_history() {
  char* hist_file = std::getenv("HISTFILE");
  if(hist_file == nullptr) {
    return;
  }
  std::ifstream file(hist_file);
  if(!file) {
    std::cout << "Error opening file: " << hist_file << std::endl;
    return;
  }
  std::string line;
  while(std::getline(file, line)) {
    if(line.empty()) {
      continue;
    }
    push_to_commands_history(line);
  }
  history_append_idx = commands_history.size();
  update_history_cursor();
}
void write_commands_history() {
  char* hist_file = std::getenv("HISTFILE");
  if(hist_file == nullptr) {
    return;
  }
  std::ofstream file(hist_file, std::ios::app);
  if(!file) {
    std::cout << "Error opening HIST_FILE: " << hist_file << std::endl;
  }
  for(int i = history_append_idx; i < commands_history.size(); i++) {
    std::string cmd = commands_history[i];
    file << cmd << std::endl;
  }
}
int main() {
  load_commands_history();
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  set_raw_terminal_mode_for_keystrokes();
  while(true) {

    reap_all_bg_jobs();
    std::cout << "$ ";
    std::string user_input = register_keystrokes_for_command();
    if(user_input.empty()) {
      continue;
    }
    //repl stand for read, evaluate,process, loop
    // the standard design of any shell process
    push_to_commands_history(user_input);
    if(!repl(user_input)) {
      write_commands_history();
      break;
    }
    
  }
  return 0;
}
