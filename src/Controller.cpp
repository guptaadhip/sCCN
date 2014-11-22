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

using namespace std;

/*
 * 1. Get the interface list
 * 2. Create the map of interface to PacketEngine
 */

Controller::Controller(unsigned int myId) {
  myId_ = myId;
  lastUniqueId_ = 0;
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
  auto subscriptionHandlerThread = std::thread(&Controller::handleKeywordSubscription, this);
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
    //Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
    //            "Received hello from: " + std::to_string(helloPacket.nodeId));
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

/*
*  TBD : Handle Keyword Registration -> INCOMPLETE
*/
void Controller::handleKeywordRegistration()
{
  (void) registrationQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
  while(true) {
    auto pending = registrationQueue_.packet_in_queue_.exchange(0, std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock(registrationQueue_.packet_ready_mutex_);    
      if( !registrationQueue_.packet_in_queue_) {
        registrationQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    
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
    responsePacketHeader.sequenceNo = requestPacketHeader.sequenceNo;
    responsePacketHeader.hostId = requestPacketHeader.hostId;
    bzero(responsePacket, BUFLEN);

    /* Prepare the packet engine */
    auto packetEngine = ifToPacketEngine_.find(pending->interface);
    
    /*
     *  Handling Registration Request
     */
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
        for (auto item : keywordList) {
          /* If the entry for this single keyword exists
           *  in map number 2, then just update its set,
           *  else add a new entry.
           */
          auto keywordToUIdsIterator = keywordToUniqueIds_.find(item);
          if(keywordToUIdsIterator != keywordToUniqueIds_.end()) {
            keywordToUniqueIds_[item].insert(uniqueId);
          } else {
            std::set<unsigned int> uniqueIdSet;
            uniqueIdSet.insert(uniqueIdSet.end(), uniqueId);
            keywordToUniqueIds_.insert(std::pair<std::string,
                                  std::set<unsigned int>> (item, uniqueIdSet));
          }
        }
        /* Updating the map of keyword to publishers */
        auto keywordToPubsIterator = keywordToPublishers_.find(keywords);
        if(keywordToPubsIterator != keywordToPublishers_.end()) {
          keywordToPublishers_[keywords].insert(requestPacketHeader.hostId);
        } else {
          std::set<unsigned int> pubsHostIdSet;
          pubsHostIdSet.insert(pubsHostIdSet.end(), requestPacketHeader.hostId);
          keywordToPublishers_.insert(std::pair<std::string,
                            std::set<unsigned int>> (keywords, pubsHostIdSet));
        }
        /* copy the unique Id */
        bcopy(&uniqueId, responsePacket + PACKET_HEADER_LEN +
              RESPONSE_HEADER_LEN, sizeof(unsigned int));
      } else {
        /* reply with the existing unique id */
        bcopy(&found->second, responsePacket + PACKET_HEADER_LEN +
              RESPONSE_HEADER_LEN, sizeof(unsigned int));
      }
      /* send ACK */
      responsePacketHeader.len = 1;
      replyPacketTypeHeader.packetType = PacketType::REGISTRATION_ACK;
      memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
      memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
             RESPONSE_HEADER_LEN);
      packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
                                    RESPONSE_HEADER_LEN + sizeof(unsigned int));
      continue;
      
    } else if (packetTypeHeader.packetType == PacketType::DEREGISTRATION_REQ) {
      if (requestPacketHeader.len == 0 || requestPacketHeader.len > 1) {
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
      /* the deregistration request will only be one unique Id */
      unsigned int uniqueId;
      bcopy(pending->packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, &uniqueId,
            sizeof(unsigned int));
      
      /* 
       * searching for the unique id in Map #1.
       *  This is O(n) algorithm. See if it can be optimized.
       */
      for(auto &entry : keywordsToUniqueId_){
        if (entry.second == uniqueId) {
          /* unique id found */
          
          /* erase from map number 1 */
          keywordsToUniqueId_.erase(entry.first);
          
          std::vector<std::string> keywordList;
          boost::split(keywordList, entry.first, boost::is_any_of(";"));
          
          /* erase the Map number 2 */
          for (auto item : keywordList) {
            /* 
             * If the entry for this item exists in map number 2
             */
            auto keywordToUIdsIterator = keywordToUniqueIds_.find(item);
            if(keywordToUIdsIterator != keywordToUniqueIds_.end()) {
              auto it = keywordToUIdsIterator->second.find(uniqueId);
              keywordToUIdsIterator->second.erase(it,
                                          keywordToUIdsIterator->second.end());
              /* send ack */
              responsePacketHeader.len = 1;
              replyPacketTypeHeader.packetType = PacketType::REGISTRATION_ACK;
              memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
              memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                     RESPONSE_HEADER_LEN);
              packetEngine->second.send(responsePacket,
                                      PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
              continue;
            } else {
              /* send nack */
              Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                          "Controller Strutures inconsistent sending NACK");
              /* Send Nack */
              responsePacketHeader.len = 0;
              replyPacketTypeHeader.packetType = PacketType::REGISTRATION_NACK;
              memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
              memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
                     RESPONSE_HEADER_LEN);
              packetEngine->second.send(responsePacket, PACKET_HEADER_LEN +
                                        RESPONSE_HEADER_LEN);
              
            }
          }
        }
      }
      /* Unique Id was not found */
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
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


/*
 * Handle Keyword Subscription
*/
void Controller::handleKeywordSubscription() {
	char payload[BUFLEN]; /* get payload into this */
	char responsePacket[BUFLEN]; 
	bool flag = false, isFirst = false;

  /* first one needs to be removed */
  (void) subscriptionQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
  while(true) {
    auto pending = subscriptionQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock(subscriptionQueue_.packet_ready_mutex_);    
      if( !subscriptionQueue_.packet_in_queue_) {
        subscriptionQueue_.packet_ready_.wait(lock);
      }
      continue;
    }

    struct RequestPacketHeader requestPacketHeader;
    struct PacketTypeHeader packetTypeHeader;

    /* Parsing the request packet just received */
    bcopy(pending->packet, &packetTypeHeader, PACKET_HEADER_LEN);
    bcopy(pending->packet + PACKET_HEADER_LEN, 
      &requestPacketHeader, REQUEST_HEADER_LEN);
    bcopy(pending->packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, 
      payload, requestPacketHeader.len);
    /* 
     * TBD : May be sstream should not be used. Look into Regex
    */
    auto keywords = std::string(payload);
    std::vector<std::string> keywordList;

    /* Preparing response packet */
    bzero(responsePacket, BUFLEN); /* zeroing the response packet */
    struct PacketTypeHeader replyPacketTypeHeader;
    struct ResponsePacketHeader responsePacketHeader;
  	responsePacketHeader.sequenceNo = requestPacketHeader.sequenceNo;
  	responsePacketHeader.hostId = requestPacketHeader.hostId;

  	/* Prepare the packet engine */
  	auto packetEngineIterator = ifToPacketEngine_.find(pending->interface);

    /* 
    *  If it is a Subscription Request
    */
    if(packetTypeHeader.packetType == PacketType::SUBSCRIPTION_REQ) {
    	Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
      "Received Subscription request from " + std::to_string(requestPacketHeader.hostId));

      std::set<unsigned int> common;
      std::set<unsigned int> *bc = &common;

      boost::split(keywordList, keywords, boost::is_any_of(";"));
      /* Parse the payload */
      for(auto item : keywordList) {
      	/* Lookup for this item in Map numbe 2 */
      	auto keywordToUIdsIterator = keywordToUniqueIds_.find(item);

      	/* if the item is present in Map 2 */
      	if(keywordToUIdsIterator != keywordToUniqueIds_.end()) {
      	  /* Take intersection of all the sets for this payload */
      		if(isFirst == false) {
      			isFirst = true;
      			common = keywordToUIdsIterator->second;
      		} else {
      		  // Remove elements from bc which are missing from ac.
      		  // The time required is proportional to log(ac.size()) * bc->size()
      		  const std::set<unsigned int>::const_iterator a_end = keywordToUIdsIterator->second.end();
      		  const std::set<unsigned int>::const_iterator b_end = bc->end();
      		  for (std::set<unsigned int>::iterator b = bc->begin(); b != b_end; ) {
      		    if (keywordToUIdsIterator->second.find(*b) == a_end) {  // Not found.
      		    	const std::set<unsigned int>::iterator b_old = b++;
      		    	bc->erase(b_old);  // erase doesn't invalidate b.
					    } else {
					    	++b;
					    }
					  }
      		}

      	} else {
      		/* 
      		* sending a NACK. One of the items not found in
      		* map number 2.
      		*/ 
      		responsePacketHeader.len = 0;
      		replyPacketTypeHeader.packetType = PacketType::SUBSCRIPTION_NACK;
      		memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
      		memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader,
      			RESPONSE_HEADER_LEN);
      		packetEngineIterator->second.send(responsePacket, 
      			PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
      		flag = true;
      		break;
      	}
      } /* parsing the payload done */

      if(flag == false) {
      	/* This subscriber has been added to the subscriber count */
      	keywordToCount_[keywords]++;
      	/* Send the common unique Ids back to the host thru an ACK */
      	responsePacketHeader.len = common.size();
      	replyPacketTypeHeader.packetType = PacketType::SUBSCRIPTION_ACK;
      	memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
      	memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader, RESPONSE_HEADER_LEN);
      	int count = 0; 
      	for(auto entry : common) {
      		bcopy(&entry, responsePacket + PACKET_HEADER_LEN + RESPONSE_HEADER_LEN + (count * sizeof(unsigned int)), sizeof(unsigned int));
      		count++;
      	}
      	int length = PACKET_HEADER_LEN + RESPONSE_HEADER_LEN 
      	+ (count * sizeof(unsigned int)); 
      	packetEngineIterator->second.send(responsePacket, length);
      } else {
      	/* Sending a NACK */
      	responsePacketHeader.len = 0;
      	replyPacketTypeHeader.packetType = PacketType::SUBSCRIPTION_NACK;
      	memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
      	memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader, RESPONSE_HEADER_LEN);
      	packetEngineIterator->second.send(responsePacket, PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
      }
    } else {

    	/* DESUBSCRIPTION REQUEST */
    	Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
      "Received Desubscription request from " + std::to_string(requestPacketHeader.hostId));

	    auto entry = keywordToCount_.find(keywords);
	    if (entry != keywordToCount_.end()) {
	    	keywordToCount_[keywords]--;
	    	if(keywordToCount_[keywords] == 0) {
	    		keywordToCount_.erase(keywords);
	    	}

	    	/* Sending an ACK */
      	responsePacketHeader.len = 0;
      	replyPacketTypeHeader.packetType = PacketType::DEREGISTRATION_ACK;
      	memcpy(responsePacket, &replyPacketTypeHeader, PACKET_HEADER_LEN);
      	memcpy(responsePacket + PACKET_HEADER_LEN, &responsePacketHeader, RESPONSE_HEADER_LEN);
      	packetEngineIterator->second.send(responsePacket, PACKET_HEADER_LEN + RESPONSE_HEADER_LEN);
	    } else {
	    	/* TBD : Discuss what to do */
	    }
    }
  }  
}


/*
 * Handle Network Update messages coming from switches 
 */
void Controller::handleNetworkUpdate() {
  /* first one needs to be removed */
  (void) networkUpdateQueue_.packet_in_queue_.exchange(0,std::memory_order_consume);
  while(true) {
    auto pending = networkUpdateQueue_.packet_in_queue_.exchange(0, 
                                                    std::memory_order_consume);
    if( !pending ) { 
      std::unique_lock<std::mutex> lock(networkUpdateQueue_.packet_ready_mutex_);    
      if( !networkUpdateQueue_.packet_in_queue_) {
        networkUpdateQueue_.packet_ready_.wait(lock);
      }
      continue;
    }
    struct NetworkUpdatePacketHeader networkUpdatePacket;
    bcopy(pending->packet + PACKET_HEADER_LEN, &networkUpdatePacket, NETWORK_UPDATE_HEADER_LEN);
    Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
      "Received network update handling packet from: " + pending->interface + 
      "regarding" + std::to_string(networkUpdatePacket.nodeId));
    /* TBD : Write the code for network Update handling and
     changing the network map */
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

unsigned int Controller::getId() const {
  return myId_; 
}
