#include "include/Switch.h"
#include "include/PacketEngine.h"
#include <iostream>
#include <string>
#include <cstring>
#include "include/Logger.h"
#include "include/net.h"
#include <unistd.h>

using namespace std;

Switch::Switch(unsigned int myId) {
  registered_ = false;
  myId_ = myId;
  std::vector<std::thread> packetEngineThreads;
  for(auto &interface : myInterface_.getInterfaceList()) {
    PacketEngine packetEngine(interface, myId, &packetHandler_);
    std::pair<std::string, PacketEngine> ifPePair (interface, packetEngine); 
    ifToPacketEngine.insert(ifPePair);
  }
  /* making packetEngine threads for all interfaces */
  for(auto it = ifToPacketEngine.begin(); it != ifToPacketEngine.end(); it++) {
    packetEngineThreads.push_back(std::thread(&Switch::startSniffing, this,
                                  it->first, &it->second));
  }

  /*
   * Create the queue pairs to handle the different queues
   */
  packetTypeToQueue.insert(std::pair<unsigned short, Queue *>  
                        ((unsigned short) PacketType::SWITCH_REGISTRATION_ACK,
                          &switchRegRespQueue_));
  /* 
   * create the switch registration response
   * handler thread 
   */
  auto thread = std::thread(&Switch::handleRegistrationResp, this);

  /*
   * call the switch registration
   * until registration the switch should not do anything
   */
  auto sendRegReq = std::thread(&Switch::sendRegistration, this);
  /*
   * send hello thread
   */
  auto sendHello = std::thread(&Switch::sendHello, this);
  packetHandler_.processQueue(&packetTypeToQueue);

}

void Switch::handleRegistrationResp() {
  /* first one needs to be removed */
  (void) switchRegRespQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
  while(true) {
    auto pending = switchRegRespQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock(switchRegRespQueue_.packet_ready_mutex_);    
      if( !switchRegRespQueue_.packet_in_queue_) {
        switchRegRespQueue_.packet_ready_.wait(lock);
      }   
      continue;
    }   
    struct RegistrationPacketHeader regResponse;
    bcopy(pending->packet + PACKET_HEADER_LEN, &regResponse, 
                                      REGISTRATION_RESPONSE_HEADER_LEN);
    myController_ = regResponse.nodeId;
    controllerIf_ = pending->interface;
    registered_ = true;
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                "Registered with Controller: " + std::to_string(myController_)
                + " at interface: " + pending->interface);
  }
}

/* Effectively the packet engine thread */
void Switch::startSniffing(std::string myInterface,
                               PacketEngine *packetEngine) {
  /*
   * 1. Get a reference of the helper class.
   * 2. Call the post_task function and pass the packet in it. 
   */
  char packet[BUFLEN];
  packetEngine->receive(packet);
}

/*
 * Send Hello Thread
 */
void Switch::sendHello() {
  char packet[PACKET_HEADER_LEN + HELLO_HEADER_LEN];
  bzero(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
  struct PacketTypeHeader header;
  header.packetType = PacketType::HELLO;
  struct HelloPacketHeader helloPacketHeader;
  helloPacketHeader.nodeId = myId_;

  memcpy(packet, &header, PACKET_HEADER_LEN);
  memcpy(packet + PACKET_HEADER_LEN, &helloPacketHeader, HELLO_HEADER_LEN);
  while (true) {
    for (auto &entry : ifToPacketEngine) {
      entry.second.send(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
    }
    sleep(1);
  }
}

void Switch::sendRegistration() {
  char packet[PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN];
  bzero(packet, PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN);
  struct PacketTypeHeader header;
  header.packetType = PacketType::SWITCH_REGISTRATION;
  struct RegistrationPacketHeader regPacketHeader;
  regPacketHeader.nodeId = myId_;
  memcpy(packet, &header, PACKET_HEADER_LEN);
  memcpy(packet+PACKET_HEADER_LEN, &regPacketHeader, REGISTRATION_HEADER_LEN);
  while (!registered_) {
    for (auto &entry : ifToPacketEngine) {
      entry.second.send(packet, PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN);
    }
    sleep(2);
  }
}


