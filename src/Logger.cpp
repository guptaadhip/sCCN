#include "include/Logger.h"
#include <iostream>

bool DEBUG_MODE = false;

void Logger::log(Log logType, std::string fileName, 
                    std::string function, int line, std::string msg) {
  if (logType == Log::CRITICAL) {
    std::cout << "CRITICAL: " << fileName << ": " << function << "::" 
              << line << ": " << msg << std::endl;
    exit(1);
  } else if (logType == Log::DEBUG) {
    if (DEBUG_MODE) {
      std::cout << "DEBUG: " << fileName << ": " << function << "::" 
                << line << ": " << msg << std::endl;
    }
  } else if (logType == Log::WARN) {
    std::cout << "WARN: " << fileName << ": " << function << "::" 
              << line << ": " << msg << std::endl;
  } else {
    std::cout << "INFO: " << msg << std::endl;
  }
}
