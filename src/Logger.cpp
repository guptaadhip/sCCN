#include "include/Logger.h"
#include <iostream>

bool DEBUG_MODE = false;

void Logger::log(Log logType, std::string function, int line, std::string msg) {
  if (logType == Log::CRITICAL) {
    std::cout << "CRITICAL: " << function << "::" << line << ": " << msg << std::endl;
    exit(1);
  } else if (logType == Log::DEBUG) {
    if (DEBUG_MODE) {
      std::cout << "DEBUG: " << function << "::" << line << ": " << msg << std::endl;
    }
  } else if (logType == Log::WARN) {
    std::cout << "WARN: " << function << "::" << line << ": " << msg << std::endl;
  }
}
