#include "include/Controller.h"
#include "include/net.h"
#include "include/Logger.h"
#include "include/MyInterfaces.h"

#include <iostream>
#include <thread>
#include <unordered_map>
#include <cstring>

using namespace std;

/*
 * 1. Get the interface list
 * 2. Create the map of interface to PacketEngine
 */

Controller::Controller(unsigned int myId) {
  Logger::log(Log::DEBUG, __FUNCTION__, __LINE__, "Entering Controller");
  std::vector<std::thread> packetEngineThreads;

  /* filling the interface to PacketEngine map */ 
  for(auto &entry : myInterfaces_.getInterfaceList()) {
    PacketEngine packetEngine(entry, myId);
    /* make a pair of interface and packet engine */
    std::pair<std::string, PacketEngine> ifPePair (entry, packetEngine);
    ifToPacketEngine.insert(ifPePair);
  }

  /* making packetEngine threads for all interfaces */
  for(auto it = ifToPacketEngine.begin(); it != ifToPacketEngine.end(); it++) {
    packetEngineThreads.push_back(std::thread(&Controller::startSniffing, this,
                                  it->first, &it->second));
  }
  
  /* waiting for all the packet engine threads */
  for (auto& joinThreads : packetEngineThreads) joinThreads.join();
}

/* Effectively the packet engine thread */
void Controller::startSniffing(std::string myInterface, PacketEngine *packetEngine) {
  /*
   * 1. Get a reference of the helper class.
   * 2. Call the post_task function and pass the packet in it. 
   */
  char packet[BUFLEN];
  PacketTypeHeader packetTypeHeader;
  HelloPacketHeader helloPacketHeader;
  while(true) {
    packetEngine->receive(packet);
    bcopy(packet, &packetTypeHeader, PACKET_HEADER_LEN);
    bcopy(packet + PACKET_HEADER_LEN, &helloPacketHeader, HELLO_HEADER_LEN);
    if(packetTypeHeader.packetType == PacketType::HELLO) {
      Logger::log(Log::DEBUG, __FUNCTION__, __LINE__, "Received a Hello packet from" 
                  + std::to_string(helloPacketHeader.nodeId) + " from " + myInterface);
    }
  }
}
