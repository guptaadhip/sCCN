#include "include/Switch.h"
#include "include/PacketEngine.h"
#include "include/SwitchInterface.h"
#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>
#include "include/Logger.h"
#include "include/net.h"
#include <unistd.h>

using namespace std;

Switch::Switch(unsigned int myId) {
  registered_ = false;
  myId_ = myId;
  myController_ = 0;
  std::vector<std::thread> packetEngineThreads;
  for(auto &interface : myInterface_.getInterfaceList()) {
    PacketEngine packetEngine(interface, myId, &packetHandler_);
    std::pair<std::string, PacketEngine> ifPePair (interface, packetEngine); 
    ifToPacketEngine_.insert(ifPePair);
  }
  /* making packetEngine threads for all interfaces */
  for(auto it = ifToPacketEngine_.begin(); it != ifToPacketEngine_.end(); it++) {
    packetEngineThreads.push_back(std::thread(&Switch::startSniffing, this,
                                  it->first, &it->second));
  }

  /*
   * Create the switch exteral interface object
   */
  SwitchInterface switchInterface(this);
  std::thread switchInterfaceThread = std::thread(&SwitchInterface::readSocket, switchInterface);

  /*
   * Create the queue pairs to handle the different queues
   */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
                        ((unsigned short) PacketType::SWITCH_REGISTRATION_ACK,
                          &switchRegRespQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
                        ((unsigned short) PacketType::HOST_REGISTRATION,
                          &hostRegistrationReqQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
                        ((unsigned short) PacketType::HELLO, &helloQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
                        ((unsigned short) PacketType::DATA, &dataQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
                        ((unsigned short) PacketType::RULE, &ruleQueue_));
  /*
   * Control Request Packets
   */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
        ((unsigned short) PacketType::REGISTRATION_REQ, &controlRequestQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
      ((unsigned short) PacketType::DEREGISTRATION_REQ, &controlRequestQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
        ((unsigned short) PacketType::SUBSCRIPTION_REQ, &controlRequestQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
       ((unsigned short) PacketType::DESUBSCRIPTION_REQ, &controlRequestQueue_));

  /* 
   * create the switch registration response
   * handler thread 
   */
  auto regRespthread = std::thread(&Switch::handleRegistrationResp, this);
  auto hellothread = std::thread(&Switch::handleHello, this);
  auto controlPacketthread = std::thread(&Switch::handleControlRequest, this);
  auto dataPacketthread = std::thread(&Switch::handleData, this);
  auto rulethread = std::thread(&Switch::handleRuleUpdate, this);
  auto hostRegthread = std::thread(&Switch::handleHostRegistration, this);

  auto nodeStatethread = std::thread(&Switch::nodeStateHandler, this);

  /*
   * call the switch registration
   * until registration the switch should not do anything
   */
  auto sendRegReq = std::thread(&Switch::sendRegistration, this);

  /*
   * send hello thread
   */
  auto sendHello = std::thread(&Switch::sendHello, this);
  packetHandler_.processQueue(&packetTypeToQueue_);

}

/*
 * Handle Host Registration Request
 */
void Switch::handleHostRegistration() {
  /* first one needs to be removed */
  (void) hostRegistrationReqQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
  while(true) {
    auto pending = hostRegistrationReqQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock (
                                hostRegistrationReqQueue_.packet_ready_mutex_); 
      if( !hostRegistrationReqQueue_.packet_in_queue_) {
        hostRegistrationReqQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    /* handle host registration */
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "host Registration request received from "
                  + std::string(pending->interface));
    if (pending->interface.compare(controllerIf_) == 0) {
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "got registration request from controller interface");
      continue;
    }
    struct RegistrationPacketHeader regRequest;
    bcopy(pending->packet + PACKET_HEADER_LEN, &regRequest, 
                                                      REGISTRATION_HEADER_LEN);

    /* no duplicates should go in the vector */
    if (std::find(nodeList_.begin(), nodeList_.end(), 
                   regRequest.nodeId) == nodeList_.end()) {
      nodeList_.push_back(regRequest.nodeId);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "New Host found: " + std::to_string(regRequest.nodeId));
    }
    /* store in the list of connected host list */
    if (std::find(connectedHostList_.begin(), connectedHostList_.end(), 
                   regRequest.nodeId) == connectedHostList_.end()) {
      connectedHostList_.push_back(regRequest.nodeId);
      nodeIdToIf_.insert(std::pair<unsigned int, std::string> 
                          (regRequest.nodeId, pending->interface));
      sendNetworkUpdate(UpdateType::ADD_HOST, regRequest.nodeId);
    }
    /*  
     * Send ACK back to the switch
     */
    auto entry = ifToPacketEngine_.find(pending->interface);
    if (entry == ifToPacketEngine_.end()) {
      Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__, 
                  "cannot find packet engine for interface "
                  + pending->interface);
    }   
    char packet[REGISTRATION_RESPONSE_HEADER_LEN + PACKET_HEADER_LEN];
    PacketTypeHeader header;
    header.packetType = PacketType::HOST_REGISTRATION_ACK;
    RegistrationResponsePacketHeader respHeader;
    respHeader.nodeId = myId_;
    memcpy(packet, &header, PACKET_HEADER_LEN);
    memcpy(packet + PACKET_HEADER_LEN, &respHeader, 
                                    REGISTRATION_RESPONSE_HEADER_LEN);
    int len = REGISTRATION_RESPONSE_HEADER_LEN + PACKET_HEADER_LEN;
    entry->second.send(packet, len);
  }
}

/*
 * Handle Rule update packet
 */
void Switch::handleRuleUpdate() {
  /* first one needs to be removed */
  (void) ruleQueue_.packet_in_queue_.exchange(0, std::memory_order_consume);
  while(true) {
    auto pending = ruleQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock (ruleQueue_.packet_ready_mutex_); 
      if( !ruleQueue_.packet_in_queue_) {
        ruleQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    if (pending->interface.compare(controllerIf_) != 0) {
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "incorrect interface for the rule update packet");
      continue;
    }
    RuleUpdatePacketHeader ruleHeader; 
    bcopy(pending->packet + PACKET_HEADER_LEN, &ruleHeader, RULE_UPDATE_HEADER_LEN);
    if ((int) ruleHeader.type == (int) UpdateType::ADD_RULE) {
      /* add rule to the forwarding table */
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "write the code to add");

    } else if ((int) ruleHeader.type == (int) UpdateType::DELETE_RULE) {
      /* delete rule to the forwarding table */
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "write the code to delete");
    } else {
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "incorrect update type: " + (char) ruleHeader.type);
    }
  }
}

/*
 * Print the forwarding Table
 */
void Switch::printForwardingTable() {
  Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, 
              "Unique Id :: Interface");
  for (auto& entry : forwardingTable_) {
    Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, 
                std::to_string(entry.first) + " :: " + entry.second);
  }
}

/*
 * Handle data packet forwarding
 */
void Switch::handleData() {
  /* first one needs to be removed */
  (void) dataQueue_.packet_in_queue_.exchange(0, std::memory_order_consume);
  while(true) {
    auto pending = dataQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock (dataQueue_.packet_ready_mutex_); 
      if( !dataQueue_.packet_in_queue_) {
        dataQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    DataPacketHeader dataHeader; 
    bcopy(pending->packet + PACKET_HEADER_LEN, &dataHeader, DATA_HEADER_LEN);
    auto entries = forwardingTable_.equal_range(dataHeader.uniqueId);
    for (auto entry = entries.first; entry != entries.second; ++entry) {
      auto packetEngine = ifToPacketEngine_.find(entry->second);
      if (packetEngine == ifToPacketEngine_.end()) {
        Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__, 
                    "Packet Engine not found for " + entry->second);
        continue;
      }
      packetEngine->second.forward(pending->packet, 
                          dataHeader.len + DATA_HEADER_LEN + PACKET_HEADER_LEN);
    }
  }
}

/*
 * Handle Control Response Packets from Controller
 */
void Switch::handleControlResponse() {
  /* first one needs to be removed */
  (void) controlResponseQueue_.packet_in_queue_.exchange(0,
                                                    std::memory_order_consume);
  while(true) {
    auto pending = controlResponseQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock
                                    (controlResponseQueue_.packet_ready_mutex_); 
      if( !controlResponseQueue_.packet_in_queue_) {
        controlResponseQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    /* if no controller is attached just drop the control packets */
    if (!registered_) {
      continue;
    }
    /* if packet is not from the interface of the controller then drop it */
    if (pending->interface.compare(controllerIf_) != 0) {
      Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__, 
                "Got control response packet from the non controller interface:"
                + pending->interface);
      continue;
    }
    /* if all looks good lets forward the packet to the host */
    struct ResponsePacketHeader respPacket;
    bcopy(pending->packet + PACKET_HEADER_LEN, &respPacket,RESPONSE_HEADER_LEN);
    /* If the nodeId is not in the list of known hosts then drop the packet */
    if (std::find(connectedHostList_.begin(), connectedHostList_.end(), 
                   respPacket.hostId) == connectedHostList_.end()) {
      Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__, 
                "node id not found in the connectedHostlist: nodeId" 
                + respPacket.hostId);
      continue;
    }
    auto interface = nodeIdToIf_.find(respPacket.hostId);
    /* no interface found to the node then lets drop the packet */
    if (interface == nodeIdToIf_.end()) {
      Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__, 
                "node id interface not found for host:" 
                + respPacket.hostId);
      continue;
    }
    auto entry = ifToPacketEngine_.find(interface->second);
    if (entry == ifToPacketEngine_.end()) {
      Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__, 
                "node interface PacketEngine not found:" 
                + interface->second);
      continue;
    }
    entry->second.forward(pending->packet, RESPONSE_HEADER_LEN);
  }
}

/*
 * Handle Control Packets from hosts
 */
void Switch::handleControlRequest() {
  /* first one needs to be removed */
  (void) controlRequestQueue_.packet_in_queue_.exchange(0,
                                                    std::memory_order_consume);
  while(true) {
    auto pending = controlRequestQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock
                                    (controlRequestQueue_.packet_ready_mutex_); 
      if( !controlRequestQueue_.packet_in_queue_) {
        controlRequestQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    /* if no controller is attached just drop the control packets */
    if (!registered_) {
      continue;
    }
    struct RequestPacketHeader reqPacket;
    bcopy(pending->packet + PACKET_HEADER_LEN, &reqPacket, REQUEST_HEADER_LEN);
    /* no duplicates should go in the vector */
    if (std::find(nodeList_.begin(), nodeList_.end(), 
                   reqPacket.hostId) == nodeList_.end()) {
      nodeList_.push_back(reqPacket.hostId);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "New Node found: " + std::to_string(reqPacket.hostId));
    } else {
      /* TBD: Check if all handled */
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                  "Got hello from unknown host: " 
                  + std::to_string(reqPacket.hostId));
      continue;
    }
    auto entry = ifToPacketEngine_.find(controllerIf_);
    entry->second.forward(pending->packet, reqPacket.len 
                          + REQUEST_HEADER_LEN + PACKET_HEADER_LEN);
  }
}

/*
 * Handle Hello message 
 */
void Switch::handleHello() {
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
    nodeToHello_[helloPacket.nodeId] = true;
    /* Set counter to 0 */
    nodeToHelloCount_[helloPacket.nodeId] = 0;
    /* no duplicates should go in the vector */
    if (std::find(nodeList_.begin(), nodeList_.end(), 
                   helloPacket.nodeId) == nodeList_.end()) {
      nodeList_.push_back(helloPacket.nodeId);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "New Node found: " + std::to_string(helloPacket.nodeId));
    }
    /* controller and host then continue as already added in the lists */
    if (helloPacket.nodeId == myController_ || 
        std::find(connectedHostList_.begin(), connectedHostList_.end(),
          helloPacket.nodeId) != connectedHostList_.end()) {
      continue;
    }  
    /* no duplicates should go in the list of connected switches */
    if (std::find(connectedSwitchList_.begin(), connectedSwitchList_.end(),
                  helloPacket.nodeId) == connectedSwitchList_.end()) {
      connectedSwitchList_.push_back(helloPacket.nodeId);
      nodeIdToIf_.insert(std::pair<unsigned int, std::string> 
                          (helloPacket.nodeId, pending->interface));
      sendNetworkUpdate(UpdateType::ADD_SWITCH, helloPacket.nodeId);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                "New Switch found: " + std::to_string(helloPacket.nodeId));
    }
    
    //Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
    //            "Received hello from: " + std::to_string(helloPacket.nodeId));
  }
}

/*
 * Thead to manage Node State based on the hello message received
 */
void Switch::nodeStateHandler() {
  while (true) {
    for (auto nodeId : nodeList_) {
      
      if (nodeToHello_[nodeId] == false) {
        nodeToHelloCount_[nodeId] = nodeToHelloCount_[nodeId] + 1;
      }
      nodeToHello_[nodeId] = false;
      /* if counter is 3 then set the node status to down */
      if (nodeToHelloCount_[nodeId] >= 3) {
        /* remove all the entries */
        nodeList_.erase(std::remove(nodeList_.begin(), 
                             nodeList_.end(), nodeId), nodeList_.end());
        nodeToHelloCount_.erase(nodeId);
        nodeToHello_.erase(nodeId);
        /* handle if controller went down */
        if (nodeId == myController_) {
          myController_ = 0;
          controllerIf_ = "";
          registered_ = false;
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                    "Controller: " + std::to_string(nodeId) + " is down");
        } else if (std::find(connectedSwitchList_.begin(), 
            connectedSwitchList_.end(), nodeId) != connectedSwitchList_.end()) {
          /* if the switch went down */
          connectedSwitchList_.erase(std::remove(connectedSwitchList_.begin(),
                                      connectedSwitchList_.end(), nodeId), 
                                      connectedSwitchList_.end());
          /* Now send a delete network update to the controller */
          sendNetworkUpdate(UpdateType::DELETE_SWITCH, nodeId);
          /* Now remove the entry from the unique ID to interface map */
          nodeIdToIf_.erase(nodeId);
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                    "Switch: " + std::to_string(nodeId) + " is down");
        } else {
          /* if the host went down */
          connectedHostList_.erase(std::remove(connectedHostList_.begin(),
                                      connectedHostList_.end(), nodeId), 
                                      connectedHostList_.end());
          /* Now send a delete network update to the controller */
          sendNetworkUpdate(UpdateType::DELETE_HOST, nodeId);
          nodeIdToIf_.erase(nodeId);
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
                    "Host: " + std::to_string(nodeId) + " is down");
        }
      }
    }
    sleep(1);
  }
}

/*
 * Send Network Update to Controller
 */
void Switch::sendNetworkUpdate(UpdateType updateType, unsigned int nodeId) {
  char packet[PACKET_HEADER_LEN + NETWORK_UPDATE_HEADER_LEN];
  bzero(packet, PACKET_HEADER_LEN + NETWORK_UPDATE_HEADER_LEN);
  struct PacketTypeHeader header;
  header.packetType = PacketType::NETWORK_UPDATE;
  struct NetworkUpdatePacketHeader networkUpdatePacketHeader;
  
  /* filling the update packet header */
  networkUpdatePacketHeader.type = updateType;
  networkUpdatePacketHeader.nodeId = nodeId;
  auto nodeIdToIfIterator = nodeIdToIf_.find(nodeId);
  strncpy(networkUpdatePacketHeader.interface, 
          nodeIdToIfIterator->second.c_str(), 
          sizeof(networkUpdatePacketHeader.interface));
  memcpy(packet, &header, PACKET_HEADER_LEN);
  memcpy(packet + PACKET_HEADER_LEN, &networkUpdatePacketHeader, NETWORK_UPDATE_HEADER_LEN);
  
  /* check if the switch is registered */
  if(!registered_) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Switch is not registered with any controller");
  } else {
    auto packetEngineIterator = ifToPacketEngine_.find(controllerIf_);
    packetEngineIterator->second.send(packet, PACKET_HEADER_LEN + NETWORK_UPDATE_HEADER_LEN);
  }
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
    /* no duplicates should go in the vector */
    if (std::find(nodeList_.begin(), nodeList_.end(), 
                   regResponse.nodeId) == nodeList_.end()) {
      nodeList_.push_back(regResponse.nodeId);
    }
    /* make sure this is not in the host or switch list */
    connectedSwitchList_.erase(std::remove(connectedSwitchList_.begin(),
            connectedSwitchList_.end(), myController_), connectedSwitchList_.end());
    connectedHostList_.erase(std::remove(connectedHostList_.begin(),
            connectedHostList_.end(), myController_), connectedHostList_.end());
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
    for (auto &entry : ifToPacketEngine_) {
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
  while(true) {
    while (!registered_) {
      for (auto &entry : ifToPacketEngine_) {
        entry.second.send(packet, PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN);
      }
      sleep(2);
    }
  }
}

/*
 * Return the controller Id
 */
unsigned int Switch::getControllerId() const {
  return myController_;
}

/*
 * Return the controller Interface
 */
std::string Switch::getControllerIf() const {
  return controllerIf_;
}

/*
 * Return the switch Id
 */
unsigned int Switch::getSwitchId() const {
  return myId_;
}

/*
 * Return the switch forwarding table
 */
std::unordered_multimap<unsigned int, std::string> Switch::getForwardingTable() const {
  return forwardingTable_;
}
