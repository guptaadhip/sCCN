#include "include/Controller.h"
#include "include/net.h"
#include "include/Logger.h"
#include "include/MyInterfaces.h"
#include <unistd.h>
#include <thread>
#include <algorithm>
#include <unordered_map>
#include <cstring>

using namespace std;

/*
 * 1. Get the interface list
 * 2. Create the map of interface to PacketEngine
 */

Controller::Controller(unsigned int myId) {
  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                                                     "Entering Controller");
  std::vector<std::thread> packetEngineThreads;

  /* filling the interface to PacketEngine map */ 
  for(auto &entry : myInterfaces_.getInterfaceList()) {
    PacketEngine packetEngine(entry, myId, &packetHandler_);
    /* make a pair of interface and packet engine */
    std::pair<std::string, PacketEngine> ifPePair (entry, packetEngine);
    ifToPacketEngine.insert(ifPePair);
  }

  /* making packetEngine threads for all interfaces */
  for(auto it = ifToPacketEngine.begin(); it != ifToPacketEngine.end(); it++) {
    packetEngineThreads.push_back(std::thread(&Controller::startSniffing, this,
                                  it->first, &it->second));
  }

  /* 
   * create queue objects for the different handler threads
   */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
                    ((unsigned short) PacketType::REGISTRATION_REQ, &regQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
           ((unsigned short) PacketType::SWITCH_REGISTRATION, &switchRegQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
           ((unsigned short) PacketType::HELLO, &helloQueue_));

  /*
   * Create Queue Handler
   */
  auto thread = std::thread(&Controller::handleSwitchRegistration, this);
  auto helloHandlerThread = std::thread(&Controller::handleHello, this);
  auto switchStateThread = std::thread(&Controller::switchStateHandler, this);
  auto helloThread = std::thread(&Controller::sendHello, this);
  packetHandler_.processQueue(&packetTypeToQueue_);
  /* waiting for all the packet engine threads */
  for (auto& joinThreads : packetEngineThreads) joinThreads.join();
}

/*
 * Thead to manage Switch State based on the hello message received
 */
void Controller::switchStateHandler() {
  while (true) {
    for (auto nodeId : switchList_) {
      if (switchToHello_[nodeId] == false) {
        switchToHelloCount_[nodeId] = switchToHelloCount_[nodeId] + 1;
      }
      switchToHello_[nodeId] = false;
      /* if counter is 3 then set the switch status to down */
      if (switchToHelloCount_[nodeId] >= 3) {
        /* remove all the entries */
        switchList_.erase(std::remove(switchList_.begin(), 
                             switchList_.end(), nodeId), switchList_.end());
        switchToHelloCount_.erase(nodeId);
        switchToHello_.erase(nodeId);
        switchToIf_.erase(nodeId);
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                    "Switch: " + std::to_string(nodeId) + " is down");
      }
    }
    sleep(1);
  }
}

/*
 * Handle Hello message 
 */
void Controller::handleHello() {
  /* first one needs to be removed */
  (void) helloQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
  while(true) {
    auto pending = helloQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock(helloQueue_.packet_ready_mutex_);    
      if( !helloQueue_.packet_in_queue_) {
        helloQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    struct HelloPacketHeader helloPacket;
    bcopy(pending->packet + PACKET_HEADER_LEN, &helloPacket, HELLO_HEADER_LEN);
    /* Set true that a hello was received from switch */
    switchToHello_[helloPacket.nodeId] = true;
    /* Set counter to 0 */
    switchToHelloCount_[helloPacket.nodeId] = 0;
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "Received hello from: " + std::to_string(helloPacket.nodeId));
  }
}

/*
 * Send hello to switches
 */
void Controller::sendHello() {
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

/*
 * Handle switch registration message 
 */
void Controller::handleSwitchRegistration() {
  /* first one needs to be removed */
  (void) switchRegQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
  while(true) {
    auto pending = switchRegQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock(switchRegQueue_.packet_ready_mutex_);    
      if( !switchRegQueue_.packet_in_queue_) {
        switchRegQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    struct RegistrationPacketHeader regPacket;
    bcopy(pending->packet + PACKET_HEADER_LEN, &regPacket, 
                                               REGISTRATION_HEADER_LEN);
    /*
     * Add node to the list of switches
     */
    switchList_.push_back(regPacket.nodeId);
    /*
     * Add the switch and interface mapping
     */
    switchToIf_[regPacket.nodeId] = pending->interface;
    /*
     * Send ACK back to the switch
     */
    auto entry = ifToPacketEngine.find(pending->interface);
    if (entry == ifToPacketEngine.end()) {
      Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__, 
                  "cannot find packet engine for interface "
                  + pending->interface);
    }
    /* Set counter to 0 */
    switchToHelloCount_[regPacket.nodeId] = 0;

    char packet[REGISTRATION_RESPONSE_HEADER_LEN + PACKET_HEADER_LEN];
    PacketTypeHeader header;
    header.packetType = PacketType::SWITCH_REGISTRATION_ACK;
    RegistrationResponsePacketHeader respHeader;
    respHeader.nodeId = myId_;
    memcpy(packet, &header, PACKET_HEADER_LEN);
    memcpy(packet + PACKET_HEADER_LEN, &respHeader, 
                                    REGISTRATION_RESPONSE_HEADER_LEN);
    int len = REGISTRATION_RESPONSE_HEADER_LEN + PACKET_HEADER_LEN;
    entry->second.send(packet, len);
    /* Set true that a hello was received from switch */
    switchToHello_[regPacket.nodeId] = true;
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "received registration packet from " + pending->interface
                + " node: " + std::to_string(regPacket.nodeId));
  }
}

/* Effectively the packet engine thread */
void Controller::startSniffing(std::string myInterface, 
                               PacketEngine *packetEngine) {
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
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "Received a Hello packet from" 
                  + std::to_string(helloPacketHeader.nodeId) + " from " 
                  + myInterface);
    }
  }
}
