#include <dirent.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#define PATH_LEN 256 /* fixed length to use getcwd() */

// global variable stores all built-in instructions
const std::vector<std::string> BUILTIN = {"cd", "set", "export", "inc"};

// several function prototype for class use
std::vector<char *> input2Args(std::string & input);
std::vector<char *> input2Args(std::string & input, std::string & modified);
std::string findPath(std::vector<char *> paths, char * command);
bool determineRange(char c);
void executePath(std::string & path_found, std::vector<char *> & args, std::vector<char *> & envs);
std::string pruneInput(std::string input);
void printShell();

/* Class for command, like 'cd', 'ls', etc. */
class MyCommand
{
 protected:
  std::vector<char *> envs;  // stores environment variables
  char * env_path;           // stores PATH specially
  std::string input;         // stores user input before modified with '\\'
  std::string modified;      // stores user input after modified
  std::vector<char *> args;  // stores parsed input to pass parameters for system call

 public:
  MyCommand(std::vector<char *> curt_envs, char * curt_path, std::string curt_input) :
      envs(curt_envs),
      env_path(curt_path),
      input(curt_input),
      modified(curt_input),
      args() {
    // use vector args to parse command line arguments
    args = input2Args(input, modified);
  }

  /*
    Execute command.
   */
  void execute() {
    // get first word of command
    std::string first(args[0]);

    // find whether '/' exists in command name
    std::size_t found = first.find("/");

    if (found != std::string::npos) { /* path provided, search in specific path */
      // use loop to let pos point at the last occurence of '/', which aims to find directory
      size_t pos = first.find_last_of("/");

      // if '/' appears at last, command is purely a directory, which is incorrect
      if (pos == first.size() - 1) {
        std::cerr << "Invalid command: need a command not a pure directory!" << std::endl;
        _exit(atoi(args[0]));
      }
      else {
        // seperate directory and command
        std::string command_dir = first.substr(0, pos + 1);
        std::string command_name = first.substr(pos + 1);

        // paths store all possible paths, since provided only one
        std::vector<char *> paths;
        paths.push_back(&command_dir[0]);

        // find the path use function findPath() & execute it
        std::string path_found = findPath(paths, &command_name[0]);
        executePath(path_found, args, envs);
      }
    }
    else { /* no path provided, search it */
      // store all paths in vector paths
      std::vector<char *> paths;
      char * curt_path = std::strtok(env_path, ":");
      while (curt_path != nullptr) {
        paths.push_back(curt_path);
        curt_path = std::strtok(nullptr, ":");
      }

      // find the path use function findPath() & execute it
      std::string path_found = findPath(paths, args[0]);
      executePath(path_found, args, envs);
    }

    // don't expect return unless error
    _exit(atoi(args[0]));
  }

  /*
    Find path from environment variable PATH.
  */
  std::string findPath(std::vector<char *> paths, char * command) {
    std::string answer = "";

    // iterate each possible path in paths
    for (std::vector<char *>::iterator it = paths.begin(); it != paths.end(); ++it) {
      // open directory
      DIR * d = opendir(*it);
      if (!d) {
        std::cerr << "invalid directory path." << std::endl;
        exit(EXIT_FAILURE);
      }

      // seperate command's name itself and directory name
      std::string command_name(command);
      std::string dirname(*it);
      dirname += "/";

      // search command
      struct dirent * entry;
      while ((entry = readdir(d)) != nullptr) {
        std::string filename(entry->d_name);

        // handle . and ..
        if (filename == "." || filename == "..")
          continue;

        // nested directory, then search into that directory
        if (entry->d_type == DT_DIR) {
          std::string find = findNestedPath(dirname + entry->d_name + "/", command_name);
          if (find != "")
            return find;
        }

        // command matches!
        if (command_name == filename) {
          answer += dirname + filename;  // update answer with full path
          return answer;
        }
      }
    }

    // no matches, return empty string
    return answer;
  }

  /*
    Helper function to find nested directory when we search command.
   */
  std::string findNestedPath(std::string dirname, std::string command) {
    // answer = empty string by default
    std::string answer;

    // open directory
    DIR * d = opendir(dirname.c_str());
    if (!d) {
      std::cerr << "invalid directory path." << std::endl;
      exit(EXIT_FAILURE);
    }

    // search command
    struct dirent * entry;
    while ((entry = readdir(d)) != nullptr) {
      std::string filename(entry->d_name);

      // handle . or ..
      if (filename == "." || filename == "..")
        continue;

      // nested directory, go into the directory and search
      if (entry->d_type == DT_DIR)
        return findNestedPath(dirname + entry->d_name + "/", command);

      // command matches!
      if (command == filename) {
        answer += dirname + filename;  // update answer with full path
        return answer;
      }
    }

    // no match, return empty string
    return answer;
  }

  /*
    Execute command according to path found, arguments and environment variables.
  */
  void executePath(std::string & path_found,
                   std::vector<char *> & args,
                   std::vector<char *> & envs) {
    if (path_found == "") { /* no match with command */
      std::cout << "Command " << args[0] << " not found" << std::endl;
      _exit(atoi(args[0]));
    }
    else { /* command found in environment path */
      // update command with full path
      args[0] = &path_found[0];

      execve(args[0], &args[0], &envs[0]);
    }
  }
};

/* Class for built-in instructions */
class MyBuiltInIns : public MyCommand
{
 private:
  std::unordered_map<std::string, std::string> & vars;  // stores variables for set
  std::string unmodified_input;                         // stores another unmodified input

 public:
  MyBuiltInIns(std::vector<char *> curt_envs,
               char * curt_path,
               std::string curt_input,
               std::unordered_map<std::string, std::string> & curt_vars) :
      MyCommand(curt_envs, curt_path, curt_input),
      vars(curt_vars),
      unmodified_input(curt_input) {}

  // override execute
  void execute() {
    // handle different instructions accordingly
    std::string ins(args[0]);
    if (ins == "cd") {
      changePath();
    }
    else if (ins == "set") {
      setVariable();
    }
    else if (ins == "export") {
      exportVariable();
    }
    else if (ins == "inc") {
      incrementVariable();
    }
  }

  /*
    "cd" instruction.
   */
  void changePath() {
    if (args.size() == 2) { /* no path provided, change to HOME */
      if (chdir(getenv("HOME")) != 0) {
        std::cerr << "Cannot redirect to HOME directory!\n";
      }
      else {
        setenv("PWD", getenv("HOME"), 1);  // set env var "PWD"
      }
    }
    else if (args.size() == 3) {         /* change by given path */
      std::string destination(args[1]);  // for '~', root directory, which is defined as home here
      if (destination == "~") {          // to home as root directory
        if (chdir(getenv("HOME")) != 0) {
          std::cerr << "Cannot redirect to HOME directory!\n";
        }
        else {
          setenv("PWD", getenv("HOME"), 1);  // set env var "PWD"
        }
      }
      else if (chdir(args[1]) != 0) {
        std::cerr << "Invalid destination diretory.\n";
      }
      else {  // set env var "PWD"
        char cwd[PATH_LEN];
        getcwd(cwd, PATH_LEN);
        setenv("PWD", cwd, 1);
      }
    }
    else { /* otherwise arguments fault */
      std::cerr << "cd: too many arguments\n";
    }
    printShell();
  }

  /*
    "set" instruction.
   */
  void setVariable() {
    if (args.size() < 3) { /* no enough set arguments */
      std::cerr << "set: no variable provided\n";
    }
    else if (args.size() == 3) { /* only var name, set empty string to its value */
      std::string key(args[1]);
      for (size_t i = 0; i < key.size(); i++) {  // check name valid
        if (!determineRange(key[i])) {
          std::cout << "set: invalid variable name\n";
          printShell();
          return;
        }
      }
      std::string value = "";
      vars[key] = value;
    }
    else { /* everything provided */
      // first check if the variable name is valid
      std::string var_name(args[1]);
      for (size_t i = 0; i < var_name.size(); i++) {
        if (!determineRange(var_name[i])) {
          std::cout << "set: invalid variable name\n";
          printShell();
          return;
        }
      }

      // valid variable name
      // find first non-space word
      size_t pos = unmodified_input.find_first_not_of(" ");
      while (unmodified_input[pos] != ' ')
        pos++;

      // find second non-space word
      pos = unmodified_input.find_first_not_of(" ", pos);
      while (unmodified_input[pos] != ' ')
        pos++;

      // find third non-space word, which is value
      pos = unmodified_input.find_first_not_of(" ", pos);

      std::string key(args[1]);
      std::string value = unmodified_input.substr(pos);

      vars[key] = value;
    }
    printShell();
  }

  /*
    "export" instruction.
   */
  void exportVariable() {
    if (args.size() > 3) { /* too many arguments */
      std::cerr << "export: too many arguments\n";
    }
    else if (args.size() == 2) { /* lack variable */
      std::cerr << "export: no variable name provided\n";
    }
    else {
      std::string key(args[1]);  // "key"
      if (vars.find(key) == vars.end()) {
        std::cerr << "export: no variable matched\n";
      }
      else {
        std::string value = vars[key];

        if (setenv(key.c_str(), value.c_str(), 1) !=
            0) { /* use setenv() to export and override if variable exists */
          std::cerr << "unable to export " << key << std::endl;
        }
      }
    }
    printShell();
  }

  /*
    Helper function for inc to determine if variable is number for "inc" instruction.
  */
  bool isNumber(std::string str) {
    // if there's no number at all, return false;
    bool hasDigit = false;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
      if (std::isdigit(*it)) {
        hasDigit = true;
        break;
      }
    }

    if (!hasDigit) { /* no nunber at all, of course return false */
      return false;
    }

    if (!std::isdigit(str[str.size() - 1])) { /* ends with dot, like 0. 1. 3. */
      return false;
    }

    size_t start = 0; /* mark first digit, considering possible '-' or '+' */
    if (str[0] == '-' || str[0] == '+') {
      start = 1;
    }

    size_t point = str.find(".");     /* mark potential . */
    if (point != std::string::npos) { /* has . */
      // step1: make sure everything after point is digit
      for (size_t after = point + 1; after < str.size(); after++) {
        if (!std::isdigit(str[after])) {
          return false;
        }
      }

      // step2: make sure everything from start to point is digit
      for (size_t before = start; before < point; before++) {
        if (!std::isdigit(str[before])) {
          return false;
        }
      }
    }
    else { /* no . */
      // from start to end must all be digit
      for (size_t i = start; i < str.size(); i++) {
        if (!std::isdigit(str[i])) {
          return false;
        }
      }
    }

    return true;
  }

  /*
    "inc" instruction.
   */
  void incrementVariable() {
    if (args.size() != 3) { /* invalid argument number */
      std::cerr << "inc: please provide one valid argument\n";
    }
    else {
      // check variable exists
      std::string key(args[1]);
      if (vars.find(key) == vars.end()) { /* not exist, set "1" */
        vars[key] = "1";
      }
      else {
        std::string value = vars[key];

        if (!isNumber(value)) { /* not a number, set "1" */
          vars[key] = "1";
        }
        else { /* number, increment by 1 */
          vars[key] = incrementNumber(value);
        }
      }
    }
    printShell();
  }

  /*
  Helper function to determine a number's integer part is purely zero or not
 */
  bool onlyZero(std::string str, size_t point) {
    // mark position of '.'
    size_t end = point == std::string::npos ? str.size() : point;

    for (size_t i = 1; i < end; i++) {
      if (str[i] != '0') {
        return false;
      }
    }

    return true;
  }

  /*
   Helper function to determine a number's fraction part zero or not, input is fraction part just traverse
  */
  bool onlyZero(std::string str) {
    for (size_t i = 0; i < str.size(); i++) {
      if (str[i] != '0') {
        return false;
      }
    }
    return true;
  }

  /*
   Handle increment for string representation of number, a helper function for "inc".
  */
  std::string incrementNumber(std::string str) {
    std::string answer;
    size_t point = str.find(".");  // position of '.'

    if (str[0] == '-') { /* negative */
      if ((onlyZero(str, point) && point == std::string::npos) ||
          (point != std::string::npos && onlyZero(str, point) &&
           onlyZero(
               str.substr(point + 1))))  // condition1 like -0000000 &&  condition2 like -000.0000
        return "1";
      else if (
          !onlyZero(
              str,
              point)) {  // like -13.309, do normal minus for integer number part and concat fraction
        size_t curt = str.size() - 1;  // mark concatenate position

        if (point != std::string::npos) {  // has point, concat fraction first
          answer += str.substr(point);
          curt = point - 1;
        }

        bool decarry = true;  // do minus for integer part
        while (curt > 0 && decarry) {
          if (str[curt] == '0') {
            answer = "9" + answer;
          }
          else {
            answer = std::to_string((str[curt] - '0') - 1) + answer;
            decarry = false;
          }
          curt--;
        }

        // end condition
        if (curt ==
            0) {  // for numbers like -100.123, curt traversed every digit of integer and stops at '-'
          answer = "-" + answer;
        }
        else {  // curt stops at intermediate digit between '-' and '.'
          answer = str.substr(0, curt - 0 + 1) + answer;
        }
      }
      else {  // like -0.123
        size_t curt = str.size() - 1;
        for (; curt > point; curt--) {  // find first non-zero from right to left
          if (str[curt] != '0') {
            break;
          }
          else {
            answer = "0" + answer;
          }
        }

        // handle least significant digit, use 10 to minus it
        answer = std::to_string(10 - (str[curt] - '0')) + answer;
        curt--;

        // other fraction digits use 9 to minus, use 9 to minus it because there's decarry of lower digit
        while (curt > point) {
          answer = std::to_string(9 - (str[curt] - '0')) + answer;
          curt--;
        }

        // add "0."
        answer = "0." + answer;
      }
    }
    else {                  /* positive */
      if (str[0] == '+') {  // handle positive number with "+"
        return "+" + incrementNumber(str.substr(1));
      }
      size_t curt = str.size() - 1;  // mark concatenate position

      if (point != std::string::npos) { /* fraction */
        answer += str.substr(point);    // copy number after point directly
        curt = point - 1;
      }

      bool carry = true;  // initial carry with inc's 1
      while (curt != SIZE_MAX && carry) {
        if (str[curt] == '9') {
          answer = "0" + answer;  // carry
        }
        else {
          answer = std::to_string((str[curt] - '0') + 1) + answer;  // no carry
          carry = false;
        }
        curt--;
      }

      if (curt == SIZE_MAX && carry) {  // still carry after all digits
        answer = "1" + answer;
      }
      else if (curt == SIZE_MAX && !carry) {  // no carry after all digits
        return answer;
      }
      else {  // no carry with remaining digits
        answer = str.substr(0, curt - 0 + 1) + answer;
      }
    }

    return answer;
  }
};

/*
  Set up environment variables with a vector to store them.
*/
std::vector<char *> setEnv(char ** envp) {
  std::vector<char *> new_envs;

  for (char ** env = envp; *env != nullptr; ++env) {
    new_envs.push_back(*env);
  }
  new_envs.push_back(nullptr);

  return new_envs;
}

/*
 Print shell message with current directory
*/
void printShell() {
  // get current directory
  char cwd[PATH_LEN];
  if (!getcwd(cwd, PATH_LEN)) {
    std::cerr << "getcwd() error" << std::endl;
    exit(EXIT_FAILURE);
  }

  // print
  std::cout << "myShell$:" << cwd << " $ ";
}

/* 
 Determine whether input is exit
 */
bool isExit(std::string input) {
  // corner case: exit with space around - still exit
  // corner case: e\x\i\t - should not exit
  char * command = nullptr;
  command = std::strtok(&input[0], " ");
  std::string compare(command);

  if (compare == "exit") {
    return true;
  }
  else {
    return false;
  }
}

/*                                                                                   
 Determine whether input is space or empty
*/
bool isSpace(std::string input) {
  // loop for all characters, any nonspace found return false
  for (size_t i = 0; i < input.size(); i++) {
    if (input[i] != ' ')
      return false;
  }

  // empty input or only space, return true
  return true;
}

/*
  Function to help determine a character in specific range for $variable
*/
bool determineRange(char c) {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
    return true;
  else
    return false;
}

/*
  Function to get $var with its corresponding value.
*/
std::string pruneForVariable(std::string input,
                             std::unordered_map<std::string, std::string> & vars) {
  std::string answer;
  for (size_t i = 0; i < input.size(); i++) {
    if (input[i] != '$') {
      answer += input[i];
    }
    else {
      // something like DFS to find longedt possible match for variable name
      size_t start = i + 1;       // position right after '$'
      size_t curt = i + 1;        // position we currently looking
      size_t match_position = i;  // last match position
      std::string temp;           // current longest match

      // DFS to search longest match and update i
      while (curt <= input.size() && determineRange(input[curt])) { /* curt is valid */
        std::string curtcut = input.substr(start, curt - start + 1);

        if (vars.find(curtcut) != vars.end()) { /* substring from start to curt is valid */
          temp = vars[curtcut];
          match_position = curt;
        }
        curt++;
      }

      // update answer and match position to know where to continue after this match
      answer += temp;
      i = match_position;
    }
  }
  return answer;
}

/* 
  Prune input in 2 steps:
  1. detect $ in input
  2. detect '\' in args after command, deliminate if chracter next to it is not space
*/
std::string pruneInput(std::string input, std::unordered_map<std::string, std::string> & vars) {
  /* Step1: prune for $ variables */
  input = pruneForVariable(input, vars);

  /* Step2: prune for '/' in args after command */
  std::string answer;

  // sorry for my naive approach..
  // first_space mark the status of if currently on the left space of command
  // second_space mark the status of if currently on the right space of command
  /* for example "  ls  -a"
     first_space detect space on the left of "ls"
     second_space detect space between "ls" and "-a"
   */
  bool first_space = true;
  bool second_space = true;

  /* 
     Using those two space flags to detect if we are on the right of our command.
     When both of them are false, we are on the right of command.
     This is where we should ignore all '/'.
   */
  for (size_t i = 0; i < input.size(); i++) {
    if (first_space && second_space) { /* on the left of command or already pass it */
      if (input[i] != ' ') {           /* found first nonspace character */
        first_space = false;
        if (input[i] != '\\')  // ignore '\\'
          answer += input[i];
      }
    }
    else if (second_space) { /* on command itself or just after it */
      if (input[i] == ' ') { /* just after command, first space after command */
        second_space = false;
      }
      if (input[i] != '\\')  // ignore '\\'
        answer += input[i];
    }
    else { /* passed commands, handle '\\' */
      if (input[i] != '\\') {
        answer += input[i];
      }
      else {                                                // '\\'
        if (i + 1 < input.size() && input[i + 1] == ' ') {  // space after it, keep them both
          answer += input[i];
          answer += input[i + 1];
          i++;
        }
      }
    }
  }

  return answer;
}

/*
  Decide whether built-in instructions or not
*/
bool isBuiltIn(std::string input) {
  // get the first word from command
  std::string first = input2Args(input)[0];

  // traverse BUILTIN, any instruction match return true
  for (size_t i = 0; i < BUILTIN.size(); i++) {
    if (BUILTIN[i] == first)
      return true;
  }
  return false;
}

/*
  Convert input string to vector arguments
  Version: for isBuiltIn.
*/
std::vector<char *> input2Args(std::string & input) {
  std::vector<char *> args;

  // delimiter = " ", cut original string into pieces
  char * p = std::strtok(&input[0], " ");
  while (p != nullptr) {
    args.push_back(p);
    p = std::strtok(nullptr, " ");
  }
  p = nullptr;
  args.push_back(p);

  return args;
}

/*
  Convert input string to vector arguments.
  Version: for class constructor.
*/
std::vector<char *> input2Args(std::string & input, std::string & modified) {
  std::vector<char *> args;  // stores pointer to string for variables

  size_t end = modified.size() -
               1;       // mark end of modified input, because size will shink when encoutering '\ '
  bool inword = false;  // mark if it's currently a word
  for (size_t i = 0, j = 0; i < input.size(); i++) {
    if (input[i] != ' ' && inword == false) {  // just at the first character of the word
      if (input[i] != '\\') {                  // no encounter '\'
        modified[j] = input[i];
      }
      else {  // encouter '\'
        modified[j] = ' ';
        i++;
        modified[end] = 0;  // help for decrease size
        end--;              // decrease size because '\ ' becomes ' '
      }
      inword = true;                 // mark we encounter word
      args.push_back(&modified[j]);  // store word position
      j++;
    }
    else if (input[i] != ' ' && inword == true) {  // at middle of word
      if (input[i] != '\\') {                      // not encounter '\'
        modified[j] = input[i];
      }
      else {  // encounter '\'
        modified[j] = ' ';
        i++;
        modified[end] = 0;
        end--;
      }
      j++;
    }
    else if (input[i] == ' ') {  // at space, no matter what set it to '\0'
      input[i] = 0;
      modified[j] = 0;
      j++;
      inword = false;
    }
  }

  // remember to add nullptr
  args.push_back(nullptr);

  return args;
}

/*
  Handle built-in instructions
*/
void handleBuiltIn(std::vector<char *> & envs,
                   char * env_path,
                   std::string input,
                   std::unordered_map<std::string, std::string> & vars) {
  MyBuiltInIns new_ins(envs, env_path, input, vars);
  new_ins.execute();
}

/*
  Handle process according to child and parent
*/
void handleProcess(pid_t pid, std::vector<char *> & envs, char * env_path, std::string input) {
  // child process error
  if (pid == -1) {
    std::cerr << "fork";
    exit(EXIT_FAILURE);
  }

  if (pid == 0) { /* code excuted by child */
    // generate new MyCommand object and execute corresponding command
    MyCommand new_command(envs, env_path, input);
    new_command.execute();
  }
  else { /* code executed by parent */
    pid_t w;
    int wstatus;
    do {
      // parent process waits for child's state change
      w = waitpid(pid, &wstatus, WUNTRACED | WCONTINUED);

      // error happened
      if (w == -1) {
        std::cerr << "waitpid";
        exit(EXIT_FAILURE);
      }

      // different termination
      if (WIFSIGNALED(wstatus)) {
        std::cout << "Program was killed by signal " << WTERMSIG(wstatus) << std::endl;
      }
      else if (WIFEXITED(wstatus)) {
        std::cout << "Program exited with status " << WEXITSTATUS(wstatus) << std::endl;
      }
    } while (!WIFEXITED(wstatus) &&
             !WIFSIGNALED(
                 wstatus)); /* exit unnormally && no signal raised to cause exit, then loop again */
  }

  printShell();
}
