#include "include/MyInterfaces.h"
#include "include/Logger.h"
#include <sys/types.h>
#include <netinet/in.h> 
#include <string> 
#include <arpa/inet.h>
#include <iostream>
#include <algorithm>

MyInterfaces::MyInterfaces() {
  struct ifaddrs *ifa = NULL;
  getifaddrs(&ifAddrStruct_);
  for (ifa = ifAddrStruct_; ifa != NULL; ifa = ifa->ifa_next) {
    /* The interface needs to have either IPv4 or IPv6 address */
    if (ifa->ifa_addr->sa_family != AF_INET && 
        ifa->ifa_addr->sa_family != AF_INET6) { 
      continue;
    }
    std::string interface = std::string(ifa->ifa_name);
    /* dont add loopback interface to the list */
    if (interface.compare("lo") == 0 || interface.compare("eth0") == 0) {
      continue;
    }
    interfaces_.push_back(interface);
  }
  sort(interfaces_.begin(), interfaces_.end());
  interfaces_.erase(std::unique(interfaces_.begin(), interfaces_.end()), 
                    interfaces_.end());
}

std::vector<std::string> MyInterfaces::getInterfaceList() const {
  return interfaces_;
}

void MyInterfaces::printInterfaceList() {
  for(auto &entry : interfaces_) {
    Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, entry);
  }
}
