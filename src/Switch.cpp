#include "include/Switch.h"
#include "include/PacketEngine.h"
#include <iostream>
#include <string>
#include <cstring>
#include "include/Logger.h"
#include "include/net.h"
#include <unistd.h>

using namespace std;

Switch::Switch(unsigned int switchId) {
  registered_ = false;
  switchId_ = switchId;
  for(auto &interface : myInterface_.getInterfaceList()) {
    PacketEngine packetEngine(interface, switchId, &packetHandler_);
    std::pair<std::string, PacketEngine> ifPePair (interface, packetEngine); 
    ifToPacketEngine.insert(ifPePair);
  }
  /* 
   * create the switch registration response
   * handler thread 
   */

  /*
   * call the switch registration
   * until registration the switch should not do anything
   */
  sendRegistration();

}

void Switch::sendHello() {
  char packet[PACKET_HEADER_LEN + HELLO_HEADER_LEN];
  bzero(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
  struct PacketTypeHeader header;
  header.packetType = PacketType::HELLO;
  struct HelloPacketHeader helloPacketHeader;
  helloPacketHeader.nodeId = switchId_;

  memcpy(packet, &header, PACKET_HEADER_LEN);
  memcpy(packet + PACKET_HEADER_LEN, &helloPacketHeader, HELLO_HEADER_LEN);
  for (auto &entry : ifToPacketEngine) {
    entry.second.send(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
  } 
}

void Switch::sendRegistration() {
  while (!registered_) {
    char packet[PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN];
    bzero(packet, PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN);
    struct PacketTypeHeader header;
    header.packetType = PacketType::SWITCH_REGISTRATION;
    struct RegistrationPacketHeader regPacketHeader;
    regPacketHeader.nodeId = switchId_;
    memcpy(packet, &header, PACKET_HEADER_LEN);
    memcpy(packet+PACKET_HEADER_LEN, &regPacketHeader, REGISTRATION_HEADER_LEN);
    for (auto &entry : ifToPacketEngine) {
      entry.second.send(packet, PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN);
    }
    /* TBD: Remove this break */
    break;
    sleep(2);
  }
}


