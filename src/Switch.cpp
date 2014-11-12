#include "include/Switch.h"
#include "include/PacketEngine.h"
#include <iostream>
#include <string>
#include <cstring>
#include "include/Logger.h"
#include "include/Util.h"
#include "include/net.h"
#include <unistd.h>

using namespace std;

Switch::Switch(unsigned int switchId){
  switchId_ = switchId;
  Logger::log(Log::DEBUG, __FUNCTION__, __LINE__, 
              "Inside Switch constructor");
  for(auto &interface : myInterface_.getInterfaceList()) {
    PacketEngine packetEngine(interface, switchId);
    std::pair<std::string, PacketEngine> ifPePair (interface, packetEngine); 
    ifToPacketEngine.insert(ifPePair);
    /*for(auto it = myMap.cbegin(); it != myMap.cend(); ++it)
    {
          std::cout << it->first << " " << it->second.first << " " << it->second.second << "\n";
    }*/
  }
}

void Switch::sendHello() {
  char helloPacket[BUFLEN];
  memset(helloPacket,'\0',BUFLEN);
  struct HelloPacketHeader helloPac;
  memset(helloPacket,'\0',BUFLEN);
  PacketType pacType = PacketType::HELLO;

  helloPac.nodeId=switchId_;

  memcpy(helloPacket, &pacType, sizeof(unsigned short));
  memcpy(helloPacket + sizeof(unsigned short), &helloPac, sizeof(struct HelloPacketHeader));
  for (auto &entry : ifToPacketEngine) {
    entry.second.send(helloPacket, BUFLEN);
  }
  
}


