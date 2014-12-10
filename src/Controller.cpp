#include "include/Controller.h"
#include "include/ControllerInterface.h"
#include "include/net.h"
#include "include/Logger.h"
#include "include/MyInterfaces.h"
#include "include/Util.h"
#include <unistd.h>
#include <thread>
#include <algorithm>
#include <unordered_map>
#include <cstring>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

using namespace std;

/*
 * 1. Get the interface list
 * 2. Create the map of interface to PacketEngine
 */

Controller::Controller(unsigned int myId) {
  myId_ = myId;
  lastUniqueId_ = 0;
  weight_ = 1;
  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
      "Entering Controller");
  std::vector<std::thread> packetEngineThreads;

  /* filling the interface to PacketEngine map */ 
  for(auto &entry : myInterfaces_.getInterfaceList()) {
    PacketEngine packetEngine(entry, myId, &packetHandler_);
    /* make a pair of interface and packet engine */
    std::pair<std::string, PacketEngine> ifPePair (entry, packetEngine);
    ifToPacketEngine_.insert(ifPePair);
  }

  /* making packetEngine threads for all interfaces */
  for(auto it = ifToPacketEngine_.begin(); it != ifToPacketEngine_.end(); it++) {
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
        "Listening on interface: " + it->first);
    packetEngineThreads.push_back(std::thread(&Controller::startSniffing, this,
        it->first, &it->second));
  }

  /*
   * Create the controller exteral interface object
   */
  ControllerInterface controllerInterface(this);
  std::thread controllerInterfaceThread = std::thread(
      &ControllerInterface::readSocket, controllerInterface);

  /* 
   * create queue objects for the different handler threads
   */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
  ((unsigned short) PacketType::SWITCH_REGISTRATION, &switchRegQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
  ((unsigned short) PacketType::HELLO, &helloQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
  ((unsigned short) PacketType::NETWORK_UPDATE, &networkUpdateQueue_));

  /* Registration/Deregistration queue */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
  ((unsigned short) PacketType::REGISTRATION_REQ, &registrationQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
  ((unsigned short) PacketType::DEREGISTRATION_REQ, &registrationQueue_));
  /* Subscription/Desubscription queue */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
  ((unsigned short) PacketType::SUBSCRIPTION_REQ, &subscriptionQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *> 
  ((unsigned short) PacketType::DESUBSCRIPTION_REQ, &subscriptionQueue_));

  /*
   * Create Queue Handler
   */
  auto thread = std::thread(&Controller::handleSwitchRegistration, this);
  auto helloHandlerThread = std::thread(&Controller::handleHello, this);
  auto switchStateThread = std::thread(&Controller::switchStateHandler, this);
  auto helloThread = std::thread(&Controller::sendHello, this);
  auto networkUpdateThread = std::thread(&Controller::handleNetworkUpdate, this);
  auto keywordRegThread = std::thread(&Controller::handleKeywordRegistration, this);
  auto keywordSubsThread = std::thread(&Controller::handleKeywordSubscription, this);
  packetHandler_.processQueue(&packetTypeToQueue_);
  /* waiting for all the packet engine threads */
  for (auto& joinThreads : packetEngineThreads) joinThreads.join();
}

void Controller::printMap(std::unordered_map<std::string, std::set<unsigned int>> input){
  for(auto &iterator : input) {
    for(auto itr : iterator.second){
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Controller::printMap : KeywordToUniqueIds_ keyword: "
          + iterator.first+ "		UniqueId " + std::to_string(itr));
    }
  }
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
  while(true) {
    std::unique_lock<std::mutex> lock(helloQueue_.packet_ready_mutex_);
    helloQueue_.packet_ready_.wait(lock);

    while (!helloQueue_.packet_in_queue_.empty()) {
      auto pending = helloQueue_.packet_in_queue_.front();
      helloQueue_.packet_in_queue_.pop();

      struct HelloPacketHeader helloPacket;
      bcopy(pending->packet + PACKET_HEADER_LEN, &helloPacket, HELLO_HEADER_LEN);
      /* Set true that a hello was received from switch */
      switchToHello_[helloPacket.nodeId] = true;
      /* Set counter to 0 */
      switchToHelloCount_[helloPacket.nodeId] = 0;
      //Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
      //            "Received hello from: " + std::to_string(helloPacket.nodeId));
    }
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
    for (auto &entry : ifToPacketEngine_) {
      entry.second.send(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
    }
    sleep(1);
  }
}

/*
 * Handle switch registration message 
 */
void Controller::handleSwitchRegistration() {
  while(true) {
    std::unique_lock<std::mutex> lock(switchRegQueue_.packet_ready_mutex_);
    switchRegQueue_.packet_ready_.wait(lock);

    while (!switchRegQueue_.packet_in_queue_.empty()) {
      auto pending = switchRegQueue_.packet_in_queue_.front();
      switchRegQueue_.packet_in_queue_.pop();

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
      ifToSwitch_[pending->interface] = regPacket.nodeId;
      /*
       * Send ACK back to the switch
       */
      auto entry = ifToPacketEngine_.find(pending->interface);
      if (entry == ifToPacketEngine_.end()) {
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
          "Received registration packet from " + pending->interface
          + " node: " + std::to_string(regPacket.nodeId));
    }
  }
}

/*
 *  TBD : Handle Keyword Registration -> INCOMPLETE
 */
void Controller::handleKeywordRegistration(){
  while(true) {
    std::unique_lock<std::mutex> lock(registrationQueue_.packet_ready_mutex_);
    registrationQueue_.packet_ready_.wait(lock);

    while (!registrationQueue_.packet_in_queue_.empty()) {
      auto pending = registrationQueue_.packet_in_queue_.front();
      registrationQueue_.packet_in_queue_.pop();

      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Received packet from " + pending->interface);
      char responsePacket[BUFLEN];
      unsigned int uniqueId;

      /* Parsing the request packet */
      struct PacketTypeHeader packetTypeHeader;
      bcopy(pending->packet, &packetTypeHeader, PACKET_HEADER_LEN);

      struct RequestPacketHeader requestPacketHeader;
      bcopy(pending->packet + PACKET_HEADER_LEN, &requestPacketHeader,
          REQUEST_HEADER_LEN);

      /* Response Packet */
      struct PacketTypeHeader replyPacketTypeHeader;
      struct ResponsePacketHeader responsePacketHeader;
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Received sequence no. = " + std::to_string(requestPacketHeader.sequenceNo));

      responsePacketHeader.sequenceNo = requestPacketHeader.sequenceNo;
      responsePacketHeader.hostId = requestPacketHeader.hostId;
      bzero(responsePacket, BUFLEN);

      /* Prepare the packet engine */
      auto packetEngine = ifToPacketEngine_.find(pending->interface);

      /* Handling Registration Request */
      if(packetTypeHeader.packetType == PacketType::REGISTRATION_REQ) {
        if (requestPacketHeader.len == 0) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Received invalid request packet from "
              + std::to_string(requestPacketHeader.hostId)
          + " sending NACK");
          /* Send Nack */
          responsePacketHeader.len = 0;
          replyPacketTypeHeader.packetType = PacketType::REGISTRATION_NACK;
          memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
          memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
              RESPONSE_HEADER_LEN);
          packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
              RESPONSE_HEADER_LEN);
          continue;
        }

        int headerLen = PACKET_HEADER_LEN + REQUEST_HEADER_LEN;
        /* get the keywords */

        auto keywords = std::string(pending->packet + headerLen);
        std::vector<std::string> keywordList;

        auto found = keywordsToUniqueId_.find(keywords);
        /*
         * If the keyword does not exist, then insert it
         * and update both the maps and send a ACK.
         */
        if(found == keywordsToUniqueId_.end()) {
          uniqueId = uniqueIdGenerator(lastUniqueId_);
          lastUniqueId_ = uniqueId;

          /* Updating map number 1 */
          keywordsToUniqueId_.insert(std::pair<std::string,unsigned int>
          (keywords, uniqueId));
          boost::split(keywordList, keywords, boost::is_any_of(";"));
          /* Updating the Map number 2 */
          visitedSubscribers_.clear();
          for (auto item : keywordList) {
            /* If the entry for this single keyword exists
             *  in map number 2, then just update its set,
             *  else add a new entry.
             */
            auto keywordToUIdsIterator = keywordToUniqueIds_.find(item);
            if(keywordToUIdsIterator != keywordToUniqueIds_.end()) {
              installRulesDynamicRegistration(responsePacketHeader.hostId,
                  keywordToUIdsIterator, UpdateType::ADD_RULE, uniqueId);
              keywordToUniqueIds_[item].insert(uniqueId);
            } else {
              std::set<unsigned int> uniqueIdSet;
              uniqueIdSet.insert(uniqueIdSet.end(), uniqueId);
              keywordToUniqueIds_.insert(std::pair<std::string,
                  std::set<unsigned int>> (item, uniqueIdSet));
            }
          }
          /* copy the unique Id */
          bcopy(&uniqueId, responsePacket + PACKET_HEADER_LEN +
              RESPONSE_HEADER_LEN, sizeof(unsigned int));
        } else {
          uniqueId = found->second;
          /* reply with the existing unique id */
          bcopy(&found->second, responsePacket + PACKET_HEADER_LEN +
              RESPONSE_HEADER_LEN, sizeof(unsigned int));
          /* If new publisher for existing keyword comes up and
           *  subscriber count not zero
           *  then install rules for all subscribers(Done)
           */
          if(uniqueIdToSubscribers_.find(uniqueId)
              != uniqueIdToSubscribers_.end()){
            installRulesRegistration(responsePacketHeader.hostId,uniqueId,
                UpdateType::ADD_RULE);
          }
        }
        /* Updating the map of UniqueID to publishers */
        auto uniqueIdToPubsIterator = uniqueIdToPublishers_.find(uniqueId);
        if(uniqueIdToPubsIterator != uniqueIdToPublishers_.end()) {
          uniqueIdToPublishers_[uniqueId].insert(requestPacketHeader.hostId);
        } else {
          std::set<unsigned int> pubsHostIdSet;
          pubsHostIdSet.insert(pubsHostIdSet.end(), requestPacketHeader.hostId);
          uniqueIdToPublishers_.insert(std::pair<unsigned int,
              std::set<unsigned int>> (uniqueId, pubsHostIdSet));
        }

        /* send ACK */
        responsePacketHeader.len = sizeof(unsigned int);
        replyPacketTypeHeader.packetType = PacketType::REGISTRATION_ACK;
        memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
        memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
            RESPONSE_HEADER_LEN);
        packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
            RESPONSE_HEADER_LEN + sizeof(unsigned int));
        continue;

      } else if (packetTypeHeader.packetType == PacketType::DEREGISTRATION_REQ) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Received deregistration request packet from "
            + std::to_string(requestPacketHeader.hostId));

        if (requestPacketHeader.len == 0) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Received invalid request packet from "
              + std::to_string(requestPacketHeader.hostId)
          + " sending NACK");
          /* Send Nack */
          responsePacketHeader.len = 0;
          replyPacketTypeHeader.packetType = PacketType::DEREGISTRATION_NACK;
          memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
          memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
              RESPONSE_HEADER_LEN);
          packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
              RESPONSE_HEADER_LEN);
          continue;
        }
        /* De-Registration request will only be one unique Id */
        unsigned int uniqueId;
        int flag;
        bcopy(pending->packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, &uniqueId,
            sizeof(unsigned int));
        auto publisherIterator = uniqueIdToPublishers_.find(uniqueId);
        if(publisherIterator != uniqueIdToPublishers_.end()){
          if((uniqueIdToPublishers_[uniqueId].size()-1) == 0){
            flag = 0;
            /*
             * searching for the unique id in Map #1.
             * This is O(n) algorithm. See if it can be optimized.
             */
            /* printing the keywordsToUniqueId_ map */
            for(auto &entry1 : keywordsToUniqueId_) {
              Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, "keywordsToUniqueId_, "
                  "Keywords : " + entry1.first + " UniqueId : " + std::to_string(entry1.second));
            }
            for(auto &entry : keywordsToUniqueId_){
              if (entry.second == uniqueId) {
                /* unique id found */
                std::vector<std::string> keywordList;
                boost::split(keywordList, entry.first, boost::is_any_of(";"));

                /* erase from map number 1 */
                keywordsToUniqueId_.erase(entry.first);

                /* erase the Map number 2 */
                for (auto item : keywordList) {
                  /* If the entry for this item exists in map number 2 */
                  auto keywordToUIdsIterator = keywordToUniqueIds_.find(item);
                  if(keywordToUIdsIterator != keywordToUniqueIds_.end()) {
                    auto it = keywordToUIdsIterator->second.find(uniqueId);
                    keywordToUIdsIterator->second.erase(it,
                        keywordToUIdsIterator->second.end());
                    flag = 1;
                    continue;
                  } else {
                    flag = 2;
                    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                        "Setting flag to 2! Item not present in map :" + item);
                    break;
                  }
                }
              }
              if(flag)
                break;
            }
            responsePacketHeader.len = 0;
            if (flag == 1) {
              /* send ack */
              /* Delete entry for uniqueId from uniqueIdToPublishers_ */
              /* Remove rules from switches for that UniqueId from the switches */
              if(uniqueIdToSubscribers_.find(uniqueId)
                  != uniqueIdToSubscribers_.end()){
                installRulesRegistration(responsePacketHeader.hostId,uniqueId,
                    UpdateType::DELETE_RULE);
              }

              uniqueIdToPublishers_.erase(uniqueId);
              replyPacketTypeHeader.packetType = PacketType::DEREGISTRATION_ACK;
              memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
              memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                  RESPONSE_HEADER_LEN);
              packetEngine->second.send(responsePacket,
                  PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
              Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                  "Sending DEREGISTRATION_ACK to " +
                  std::to_string(requestPacketHeader.hostId) +
                  " with sequence no = " + std::to_string(requestPacketHeader.sequenceNo));

              continue;
            } else if (flag == 2) {
              Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                  "Controller Structures inconsistent sending NACK");
              /* Send Nack */
              responsePacketHeader.len = 0;
              replyPacketTypeHeader.packetType = PacketType::DEREGISTRATION_NACK;
              memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
              memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                  RESPONSE_HEADER_LEN);
              packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
                  RESPONSE_HEADER_LEN);
              continue;
            }

            /* Unique Id was not found */
            Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Received invalid unique Id from "
                + std::to_string(requestPacketHeader.hostId)
            + " sending NACK");
            /* Send Nack */
            responsePacketHeader.len = 0;
            replyPacketTypeHeader.packetType = PacketType::REGISTRATION_NACK;
            memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
            memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                RESPONSE_HEADER_LEN);
            packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
                RESPONSE_HEADER_LEN);
            continue;
          }else{
            Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                "Host Un-register UniqueID " + std::to_string(uniqueId)
            + " sending NACK");
            /* Send Ack */
            /* TBD:: Check why len is 1 */
            uniqueIdToPublishers_[uniqueId].erase(requestPacketHeader.hostId);
            responsePacketHeader.len = 1;
            replyPacketTypeHeader.packetType = PacketType::DEREGISTRATION_ACK;
            memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
            memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                RESPONSE_HEADER_LEN);
            packetEngine->second.send(responsePacket,
                PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
            continue;
          }
        }else{
          Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
              "Host tried to Un-register invalid Unique ID");
          /* Send Nack */
          responsePacketHeader.len = 0;
          replyPacketTypeHeader.packetType = PacketType::DEREGISTRATION_NACK;
          memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
          memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
              RESPONSE_HEADER_LEN);
          packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
              RESPONSE_HEADER_LEN);
          continue;
        }
      } else {
        /* this should never happen */
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Received invalid packet from "
            + std::to_string(requestPacketHeader.hostId) + " Type: "
            + std::to_string((int) packetTypeHeader.packetType));
        /* Send Nack */
        responsePacketHeader.len = 0;
        replyPacketTypeHeader.packetType = PacketType::REGISTRATION_NACK;
        memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
        memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
            RESPONSE_HEADER_LEN);
        packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
            RESPONSE_HEADER_LEN);
        continue;
      }
    }
  }
}


/*
 * Handle Keyword Subscription
 */
void Controller::handleKeywordSubscription() {
  while(true) {
    std::unique_lock<std::mutex> lock(subscriptionQueue_.packet_ready_mutex_);
    subscriptionQueue_.packet_ready_.wait(lock);

    while (!subscriptionQueue_.packet_in_queue_.empty()) {
      auto pending = subscriptionQueue_.packet_in_queue_.front();
      subscriptionQueue_.packet_in_queue_.pop();

      char payload[BUFLEN]; /* get payload into this */
      char responsePacket[BUFLEN];
      bool isFirst = false;

      struct RequestPacketHeader requestPacketHeader;
      struct PacketTypeHeader packetTypeHeader;

      bzero(payload, BUFLEN); /* zeroing the payload */

      /* Parsing the request packet just received */
      bcopy(pending->packet, &packetTypeHeader, PACKET_HEADER_LEN);
      bcopy(pending->packet + PACKET_HEADER_LEN,
          &requestPacketHeader, REQUEST_HEADER_LEN);
      bcopy(pending->packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN,
          payload, requestPacketHeader.len);

      auto keywords = std::string(payload);
      std::vector<std::string> keywordList;
      int flag;
      /*
		 Flag = 0, Send Nack - Should not happen ever
		 Flag = 1, Send Ack - Everything ok
		 Flag = 2, Send Nack - Something wrong with structures
       */

      /* Preparing response packet */
      bzero(responsePacket, BUFLEN); /* zeroing the response packet */
      struct PacketTypeHeader replyPacketTypeHeader;
      struct ResponsePacketHeader responsePacketHeader;
      responsePacketHeader.sequenceNo = requestPacketHeader.sequenceNo;
      responsePacketHeader.hostId = requestPacketHeader.hostId;

      /* Prepare the packet engine */
      auto packetEngineIterator = ifToPacketEngine_.find(pending->interface);
      if(packetTypeHeader.packetType == PacketType::SUBSCRIPTION_REQ ||
          packetTypeHeader.packetType == PacketType::DESUBSCRIPTION_REQ) {
        flag = 0;

        /* Common Pub * UnPub - Finding Intersection of keywords  -- Start */
        boost::split(keywordList, keywords, boost::is_any_of(";"));
        /*Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
    		  "Keywords : " + keywords); */

        isFirst = false;
        std::set<unsigned int> common;
        std::set<unsigned int> *outer = &common;

        for(auto keyword : keywordList) {
          auto keywordToUIdsIterator = keywordToUniqueIds_.find(keyword);
          if(keywordToUIdsIterator != keywordToUniqueIds_.end()) {
            flag = 1;
            /* Intersecting all Unique ID's for the Keyword List */
            if(isFirst == false) {
              isFirst = true;
              common = keywordToUIdsIterator->second;
            } else {
              const std::set<unsigned int>::const_iterator inner_end =
                  keywordToUIdsIterator->second.end();
              const std::set<unsigned int>::const_iterator outer_end = outer->end();
              for (auto iter = outer->begin(); iter != outer_end; ) {
                if (keywordToUIdsIterator->second.find(*iter) == inner_end) {
                  // Not found.
                  const std::set<unsigned int>::iterator iter_old = iter++;
                  outer->erase(iter_old);
                } else {
                  ++iter;
                }
              }
            }
          }else{
            flag = 2;
            Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Setting flag to 2! Item not present in map :" + keyword);
            //printMap(keywordToUniqueIds_);
            break;
          }
        }
        /* No intersection of UniqueId found, Send Nack */
        if(common.empty()){
          flag = 2;
          Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
              "Setting flag to 2! Common is empty.");
        }
        /* Common Pub * UnPub - Finding Intersection of keywords  -- End */
        if(packetTypeHeader.packetType == PacketType::SUBSCRIPTION_REQ) {
          /* SUBSCRIPTION_REQ */
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Received Subscription request from "
              + std::to_string(requestPacketHeader.hostId));
          if(flag == 1){ /* Send Ack  and Install Rules */
            unsigned int count = 0;
            responsePacketHeader.len = common.size() * sizeof(unsigned int);
            replyPacketTypeHeader.packetType = PacketType::SUBSCRIPTION_ACK;
            memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
            memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                RESPONSE_HEADER_LEN);
            for(auto uniqueID : common) {
              /* Updating the map of UniqueID to Subscriber */
              auto uniqueIdToSubsIterator = uniqueIdToSubscribers_.find(uniqueID);
              if(uniqueIdToSubsIterator != uniqueIdToSubscribers_.end()) {
                /* If  uniqueID already exists, insert subscriber into the set */
                uniqueIdToSubscribers_[uniqueID].push_back(requestPacketHeader.hostId);
              } else {
                /* If  uniqueID does not exists, insert uniqueID & subscriber into map */
                std::vector<unsigned int> subsHostIdSet;
                subsHostIdSet.push_back(requestPacketHeader.hostId);
                uniqueIdToSubscribers_.insert(std::pair<unsigned int,
                    std::vector<unsigned int>> (uniqueID, subsHostIdSet));
              }
              /* Find shortest path between all Pub to this Sub for each uniqueId
           	   	   And Install corresponding ADD rules on the switches */
              installRules(responsePacketHeader.hostId,uniqueID,UpdateType::ADD_RULE);

              bcopy(&uniqueID, responsePacket + PACKET_HEADER_LEN + RESPONSE_HEADER_LEN
                  + (count * sizeof(unsigned int)), sizeof(unsigned int));
              count++;
            }
            packetEngineIterator->second.send(responsePacket, PACKET_HEADER_LEN +
                RESPONSE_HEADER_LEN + (count * sizeof(unsigned int)));
            continue;
          }else if(flag == 2){ /* Send Nack */
            Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Controller Structures inconsistent sending NACK");
            responsePacketHeader.len = 0;
            replyPacketTypeHeader.packetType = PacketType::SUBSCRIPTION_NACK;
            memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
            memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                RESPONSE_HEADER_LEN);
            packetEngineIterator->second.send(responsePacket,
                PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
            continue;
          }

          /* Should not come here ever */
          Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
              "Received No Keyword from " + std::to_string(requestPacketHeader.hostId)
          + " sending NACK");
          responsePacketHeader.len = 0;
          replyPacketTypeHeader.packetType = PacketType::SUBSCRIPTION_NACK;
          memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
          memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
              RESPONSE_HEADER_LEN);
          packetEngineIterator->second.send(responsePacket,
              PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
          continue;
        }else if(packetTypeHeader.packetType == PacketType::DESUBSCRIPTION_REQ) {
          /* DESUBSCRIPTION REQUEST */
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Received De-Subscription request from " +
              std::to_string(requestPacketHeader.hostId));

          if(flag == 1){ /* Send Ack and Remove Rules */
            responsePacketHeader.len = 0;
            replyPacketTypeHeader.packetType = PacketType::DESUBSCRIPTION_ACK;
            memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
            memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                RESPONSE_HEADER_LEN);
            for(auto uniqueID : common) {
              /* Updating the map of UniqueID to Subscriber */
              auto uniqueIdToSubsIterator = uniqueIdToSubscribers_.find(uniqueID);
              if(uniqueIdToSubsIterator != uniqueIdToSubscribers_.end()) {
                if((uniqueIdToSubsIterator->second.size()-1) == 0){
                  /* No Subscriber for that uniqueID, remove the uniqueID */
                  uniqueIdToSubscribers_.erase(uniqueID);
                }else{
                  /* Multiple Subscribers exists for that uniqueID, remove only
          	  	  	 that Subscriber */
                  auto subscriberIterator = std::find(
                      uniqueIdToSubsIterator->second.begin(),
                      uniqueIdToSubsIterator->second.end(), requestPacketHeader.hostId);
                  if(subscriberIterator != uniqueIdToSubsIterator->second.end()){
                    uniqueIdToSubsIterator->second.erase(subscriberIterator);
                  }
                }
              }
              /* Find shortest path between all Pub to this Sub for each uniqueId
           	   	   And Install corresponding REMOVE rules on the switches */
              installRules(responsePacketHeader.hostId,uniqueID,UpdateType::DELETE_RULE);
            }
            packetEngineIterator->second.send(responsePacket, PACKET_HEADER_LEN +
                RESPONSE_HEADER_LEN);
            continue;
          }else if(flag == 2){ /* Send Nack */
            Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Controller Structures inconsistent sending NACK");
            responsePacketHeader.len = 0;
            replyPacketTypeHeader.packetType = PacketType::DESUBSCRIPTION_NACK;
            memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
            memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                RESPONSE_HEADER_LEN);
            packetEngineIterator->second.send(responsePacket,
                PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
            continue;
          }

          /* Should not come here ever */
          Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
              "Received No Keyword from " + std::to_string(requestPacketHeader.hostId)
          + " sending NACK");
          responsePacketHeader.len = 0;
          replyPacketTypeHeader.packetType = PacketType::DESUBSCRIPTION_NACK;
          memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
          memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
              RESPONSE_HEADER_LEN);
          packetEngineIterator->second.send(responsePacket,
              PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
          continue;
        }
      }
    }
  }
}

/*
 * Handle Network Update messages coming from switches 
 */
void Controller::handleNetworkUpdate() {
  while(true) {
    std::unique_lock<std::mutex> lock(networkUpdateQueue_.packet_ready_mutex_);
    networkUpdateQueue_.packet_ready_.wait(lock);

    while (!networkUpdateQueue_.packet_in_queue_.empty()) {
      auto pending = networkUpdateQueue_.packet_in_queue_.front();
      networkUpdateQueue_.packet_in_queue_.pop();

      struct NetworkUpdatePacketHeader networkUpdatePacket;
      bcopy(pending->packet + PACKET_HEADER_LEN, &networkUpdatePacket,
          NETWORK_UPDATE_HEADER_LEN);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Received network update handling packet from: " + pending->interface +
          " regarding " + std::to_string(networkUpdatePacket.nodeId));
      /* TBD : Write the code for network Update handling and
     	 changing the network map */

      /* Network Map:: Formation */
      PacketTypeHeader header;

      if(networkUpdatePacket.type == UpdateType::ADD_SWITCH ||
          networkUpdatePacket.type == UpdateType::ADD_HOST){
        /* Add a Link*/
        if(ifToSwitch_.count(pending->interface) > 0){
          boost::add_edge(ifToSwitch_[pending->interface],
              networkUpdatePacket.nodeId, weight_, graph_);
          header.packetType = PacketType::NETWORK_UPDATE_ACK;
        }else{
          Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__,
              "Network Map:: Error - Cannot find edge"
              + std::to_string(ifToSwitch_[pending->interface]) + " : " +
              std::to_string(networkUpdatePacket.nodeId));
          header.packetType = PacketType::NETWORK_UPDATE_NACK;
        }
        /* TBD :: Modify rules as per Add or Delete of Edges */
      }else if(networkUpdatePacket.type == UpdateType::DELETE_SWITCH ||
          networkUpdatePacket.type == UpdateType::DELETE_HOST){
        /* Remove a Link*/
        if(ifToSwitch_.count(pending->interface) > 0){
          std::pair <edgeDescriptor_,bool> edgeChecker =
              boost::edge(ifToSwitch_[pending->interface], networkUpdatePacket.nodeId,
                  graph_);
          if(edgeChecker.second){
            boost::remove_edge(ifToSwitch_[pending->interface],
                networkUpdatePacket.nodeId, graph_);
            header.packetType = PacketType::NETWORK_UPDATE_ACK;
          }else{
            Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__,
                "Network Map:: Error - Cannot find edge"
                + std::to_string(ifToSwitch_[pending->interface]) + "-"
                + std::to_string(networkUpdatePacket.nodeId));
            header.packetType = PacketType::NETWORK_UPDATE_NACK;
          }
        }else{
          Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__,
              "Network Map:: Error - Cannot find edge"
              + std::to_string(ifToSwitch_[pending->interface]) + "-"
              + std::to_string(networkUpdatePacket.nodeId));
          header.packetType = PacketType::NETWORK_UPDATE_NACK;
        }
        /* TBD :: Modify rules as per Add or Delete of Edges */
      }

      auto entry = ifToPacketEngine_.find(pending->interface);
      if (entry == ifToPacketEngine_.end()) {
        Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__,
            "cannot find packet engine for interface "
            + pending->interface);
      }

      char packet[NETWORK_UPDATE_RESPONSE_HEADER_LEN + PACKET_HEADER_LEN];

      NetworkUpdateResponsePacketHeader respHeader;
      respHeader.nodeId = networkUpdatePacket.nodeId;
      respHeader.seqNo = networkUpdatePacket.seqNo;

      memcpy(packet, &header, PACKET_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN, &respHeader,
          NETWORK_UPDATE_RESPONSE_HEADER_LEN);
      int len = NETWORK_UPDATE_RESPONSE_HEADER_LEN + PACKET_HEADER_LEN;
      entry->second.send(packet, len);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Received Network Update packet from " + pending->interface);
    }
  }
}

/* 
 * Install/Uninstall Rules on the Switches during Dynamic registration
 */
void Controller::installRulesDynamicRegistration(unsigned int publisherId, 
    std::unordered_map<std::string, std::set<unsigned int>>::iterator
    keywordToUIdsIterator, UpdateType type, unsigned int newUniqueId){

  char packet[PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN];
  struct PacketTypeHeader header;
  struct RuleUpdatePacketHeader ruleUpdatePacketHeader;

  for (auto uniqueId : keywordToUIdsIterator->second){
    auto uniqueIdToSubsIterator = uniqueIdToSubscribers_.find(uniqueId);
    if(uniqueIdToSubsIterator != uniqueIdToSubscribers_.end()){
      for(auto subscriber : uniqueIdToSubsIterator->second){
        if(visitedSubscribers_.find(subscriber) == visitedSubscribers_.end()){
          visitedSubscribers_.insert(subscriber);

          /* Predecessor map with key and value type of vertex_descriptor */
          std::vector<vertexDescriptor_> predecessor(boost::num_vertices(graph_));
          std::vector<int> distance(boost::num_vertices(graph_));
          /* dijkstra_shortest_paths- Compute shortest paths */
          boost::dijkstra_shortest_paths(graph_, publisherId,
              boost::predecessor_map(&predecessor[0]).distance_map(&distance[0]));

          /* Updating the map of newUniqueId to Subscriber */
          auto newUniqueIdToSubsIterator = uniqueIdToSubscribers_.find(newUniqueId);
          if(newUniqueIdToSubsIterator != uniqueIdToSubscribers_.end()) {
            /* If  newUniqueId already exists, insert subscriber into the set */
            uniqueIdToSubscribers_[newUniqueId].push_back(subscriber);
          } else {
            /* If  newUniqueId does not exists, insert newUniqueId
             *   & subscriber into map
             */
            std::vector<unsigned int> subsHostIdSet;
            subsHostIdSet.push_back(subscriber);
            uniqueIdToSubscribers_.insert(std::pair<unsigned int,
                std::vector<unsigned int>> (newUniqueId, subsHostIdSet));
          }

          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Controller::  rules for Subscriber= " + std::to_string(subscriber));

          for (unsigned int hostId = subscriber; predecessor[hostId] != publisherId;
              hostId = predecessor[hostId]) {
            Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                "Sending Rule Update Packet On Switch= " +
                std::to_string(predecessor[hostId]) +
                " uniqueID= " + " " + std::to_string(newUniqueId) +
                " Next Hop= " + std::to_string(hostId));

            /* Prepare the rule update packet */
            bzero(&header, PACKET_HEADER_LEN);
            bzero(&ruleUpdatePacketHeader, RULE_UPDATE_HEADER_LEN);
            bzero(packet, PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN);
            header.packetType = PacketType::RULE;
            memcpy(packet, &header, PACKET_HEADER_LEN);

            ruleUpdatePacketHeader.type = type;
            ruleUpdatePacketHeader.uniqueId = newUniqueId;
            ruleUpdatePacketHeader.nodeId = hostId;

            memcpy(packet + PACKET_HEADER_LEN, &ruleUpdatePacketHeader,
                RULE_UPDATE_HEADER_LEN);

            /* Prepare packetEngine and send */
            auto packetEngine =
                ifToPacketEngine_.find(switchToIf_[predecessor[hostId]]);
            if (packetEngine == ifToPacketEngine_.end()) {
              Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__,
                  "cannot find packet engine for interface "
                  + switchToIf_[predecessor[hostId]]);
            }
            sleep(1);
            packetEngine->second.send(packet,
                PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN);
          }
        }
      }
    }
  }
}

void Controller::installRulesRegistration(unsigned int publisherId, 
    unsigned int uniqueId, UpdateType type){

  char packet[PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN];
  struct PacketTypeHeader header;
  struct RuleUpdatePacketHeader ruleUpdatePacketHeader;

  auto uniqueIdToSubsIterator = uniqueIdToSubscribers_.find(uniqueId);
  /* Predecessor map with key and value type of vertex_descriptor */
  std::vector<vertexDescriptor_> predecessor(boost::num_vertices(graph_));
  std::vector<int> distance(boost::num_vertices(graph_));
  /* dijkstra_shortest_paths- Compute shortest paths */
  boost::dijkstra_shortest_paths(graph_, publisherId,
      boost::predecessor_map(&predecessor[0]).distance_map(&distance[0]));

  /*Iterate through the subscribers and send rule uninstall updates to switches*/
  for(auto entry : uniqueIdToSubsIterator->second) {
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
        "Controller::installRules: Uninstalling rules for Subscriber= " +
        std::to_string(entry));
    for (unsigned int hostId = entry; predecessor[hostId] != publisherId;
        hostId = predecessor[hostId]) {
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Controller::installRules:Sending Rule Update Packet On Switch= " +
          std::to_string(predecessor[hostId]) +
          " uniqueID= " + " " + std::to_string(uniqueId) +
          " Next Hop= " + std::to_string(hostId));

      /* Prepare the rule update packet */
      bzero(&header, PACKET_HEADER_LEN);
      bzero(&ruleUpdatePacketHeader, RULE_UPDATE_HEADER_LEN);
      bzero(packet, PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN);
      header.packetType = PacketType::RULE;
      memcpy(packet, &header, PACKET_HEADER_LEN);

      ruleUpdatePacketHeader.type = type;
      ruleUpdatePacketHeader.uniqueId = uniqueId;

      ruleUpdatePacketHeader.nodeId = hostId;
      memcpy(packet + PACKET_HEADER_LEN, &ruleUpdatePacketHeader,
          RULE_UPDATE_HEADER_LEN);
      /* Prepare packetEngine and send */
      auto packetEngine = ifToPacketEngine_.find(switchToIf_[predecessor[hostId]]);
      if (packetEngine == ifToPacketEngine_.end()) {
        Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__,
            "cannot find packet engine for interface "
            + switchToIf_[predecessor[hostId]]);
      }
      usleep(300);
      packetEngine->second.send(packet, PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN);
    }
  }
}

/*
 * Install/Uninstall Rules on the Switches 
 */
void Controller::installRules(unsigned int destHost, unsigned int uniqueId, 
    UpdateType type){

  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
      "Controller::installRules- Entering function");
  char packet[PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN];
  struct PacketTypeHeader header;
  struct RuleUpdatePacketHeader ruleUpdatePacketHeader;

  /*bzero(packet, PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN);
  header.packetType = PacketType::RULE;
  memcpy(packet, &header, PACKET_HEADER_LEN);*/

  auto uniqueIdToPubsIterator = uniqueIdToPublishers_.find(uniqueId);

  /* Predecessor map with key and value type of vertex_descriptor */
  std::vector<vertexDescriptor_> predecessor(boost::num_vertices(graph_));
  std::vector<int> distance(boost::num_vertices(graph_));

  /* Iterate through every publisher corresponding to the unique ID */
  for(auto publisherId : uniqueIdToPubsIterator->second) {
    /* dijkstra_shortest_paths- Compute shortest paths */
    boost::dijkstra_shortest_paths(graph_, publisherId,
        boost::predecessor_map(&predecessor[0]).distance_map(&distance[0]));

    /*Iterate through the shortest path and send rule updates to the switches */
    for (unsigned int hostId = destHost; predecessor[hostId] != publisherId;
        hostId = predecessor[hostId]) {
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Sending Rule Update Packet On Switch= " +
          std::to_string(predecessor[hostId]) + "interface"+switchToIf_[predecessor[hostId]]+
          " uniqueID= " + " " + std::to_string(uniqueId) +
          " Next Hop= " + std::to_string(hostId));

      /* Prepare the rule update packet */
      bzero(&header, PACKET_HEADER_LEN);
      bzero(&ruleUpdatePacketHeader, RULE_UPDATE_HEADER_LEN);
      bzero(packet, PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN);
      header.packetType = PacketType::RULE;
      memcpy(packet, &header, PACKET_HEADER_LEN);

      ruleUpdatePacketHeader.type = type;
      ruleUpdatePacketHeader.uniqueId = uniqueId;
      ruleUpdatePacketHeader.nodeId = hostId;
      memcpy(packet + PACKET_HEADER_LEN, &ruleUpdatePacketHeader,
          RULE_UPDATE_HEADER_LEN);

      /* Prepare packetEngine and send */
      auto packetEngine = ifToPacketEngine_.find(switchToIf_[predecessor[hostId]]);
      if (packetEngine == ifToPacketEngine_.end()) {
        Logger::log(Log::CRITICAL, __FILE__, __FUNCTION__, __LINE__,
            "cannot find packet engine for interface "
            + switchToIf_[predecessor[hostId]]);
      }
      usleep(300);
      packetEngine->second.send(packet, PACKET_HEADER_LEN + RULE_UPDATE_HEADER_LEN);
    }
  }
  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
      "Controller::installRules- End of function");
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

unsigned int Controller::getId() const {
  return myId_; 
}
