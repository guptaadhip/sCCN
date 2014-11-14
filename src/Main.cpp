#include <iostream>
#include <string>
#include <cstring>
#include "include/net.h"
#include "include/Logger.h"
#include "include/Switch.h"
#include "include/Controller.h"

extern bool DEBUG_MODE;

int main(int argc, char *argv[]) {
  unsigned int myId;
  std::string role;
  if (argc < 3) {
    Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__, 
                "too few arguments");
  } else if (argc == 4) {
    if (strncmp(argv[3], "dbg", strlen(argv[3])) == 0) {
      DEBUG_MODE = true;
    }
  } else if (argc > 4) {
    Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__, 
                "too many arguments");
  }
  myId = atoi(argv[1]);
  role = argv[2];
  if (role.compare("Switch") == 0) {
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "Role: Switch");
    Switch mySwitch(myId);
  } else if (role.compare("Controller") == 0) {
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "Role: Controller");
    Controller controller(myId);
  } else if (role.compare("Host") == 0) {
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, "Role: Host");
  } else {
    Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__, 
                "incorrect role specified");
  }
  return 0;
}
