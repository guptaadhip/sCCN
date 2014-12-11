#include "include/Host.h"
#include <iostream>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <string>
#include "boost/date_time/posix_time/posix_time.hpp"


#include "include/HostInterface.h"
#include "include/net.h"
#include "include/Logger.h"
#include "include/Util.h"

using namespace std;
namespace pt = boost::posix_time;

/*
 *	Host - Class Constructor
 */
Host::Host(int myId) {
  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
      "Entering Host.");
  std::vector<std::thread> packetEngineThreads;

  registered_ = false;
  myId_ = myId;
  sendHelloCounter_ = 0;
  switchDownCount_ = 0;
  dataSize_ = 0;
  packetsReceived_ = 0;

  for(auto &entry : myInterface_.getInterfaceList()) {
    PacketEngine packetEngine(entry, myId_, &packetHandler_);
    std::pair<std::string, PacketEngine> ifPePair (entry, packetEngine);
    ifToPacketEngine_.insert(ifPePair);
  }

  for(auto it = ifToPacketEngine_.begin(); it != ifToPacketEngine_.end(); it++){
    packetEngineThreads.push_back(std::thread(&Host::startSniffing, this,
        it->first, &it->second));
  }

  /*  
   * Create the host exteral interface object
   */
  HostInterface hostInterface(this);
  std::thread hostInterfaceThread = std::thread(
      &HostInterface::readSocket, hostInterface);

  /* Create the queue pairs to handle the different queues */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::HOST_REGISTRATION_ACK, &hostRegRespQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::HELLO, &helloQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::DATA, &recvDataQueue_));

  /* Control packet queue */
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::REGISTRATION_ACK, &controlPacketQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::REGISTRATION_NACK,  &controlPacketQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::DEREGISTRATION_ACK, &controlPacketQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::DEREGISTRATION_NACK, &controlPacketQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::SUBSCRIPTION_ACK, &controlPacketQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::SUBSCRIPTION_NACK, &controlPacketQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::DESUBSCRIPTION_ACK, &controlPacketQueue_));
  packetTypeToQueue_.insert(std::pair<unsigned short, Queue *>  
  ((unsigned short) PacketType::DESUBSCRIPTION_NACK, &controlPacketQueue_));

  /*
   * call the switch registration
   * until registration the host should not do anything
   */
  auto sendRegReq = std::thread(&Host::sendRegistration, this);

  /* Handler thread */
  auto regRespThread = std::thread(&Host::handleRegistrationResp, this);
  /* Handle Switch Hello thread */
  auto helloHandlerThread = std::thread(&Host::handleHello, this);
  /* Switch State Handler thread */
  auto switchStateThread = std::thread(&Host::switchStateHandler, this);
  /* Keyword Registration Handler thread */
  auto keywordRegThread = std::thread(&Host::keywordRegistrationHandler, this);
  /* Keyword Deregistration Handler thread */
  auto keywordDergThread = std::thread(&Host::keywordDeregistrationHandler, this);
  /* Keyword Subscription Handler thread */
  auto keywordSubThread = std::thread(&Host::keywordSubscriptionHandler, this);
  /* Keyword Unsubscription Handler thread */
  auto keywordUnsubThread = std::thread(&Host::keywordUnsubscriptionHandler, this);
  /* Control Packet Response Handler thread */
  auto controlRespThread = std::thread(&Host::handleControlResp, this);
  /* Send Data Handler thread */

  /*  No thread needed */
  /*auto sendDataThread = std::thread(&Host::sendDataHandler, this); */

  /* Receive Data Handler thread */
  auto recvDataThread = std::thread(&Host::recvDataHandler, this);

  /*  
   * send hello thread
   */
  auto sendHello = std::thread(&Host::sendHello, this);
  auto throughputThread = std::thread(&Host::throughput, this);
  packetHandler_.processQueue(&packetTypeToQueue_);
}

/*
 * Queue Keyword Deregistration
 */
void Host::queueKeywordDeregistration(PacketEntry *t) {
  deregisterQueue_.queuePacket(t);
}

/*
 * Queue Keyword Registration
 */
void Host::queueKeywordRegistration(PacketEntry *t) {
  registerQueue_.queuePacket(t);
}

/*
 * Queue Keyword Subscription
 */
void Host::queueKeywordSubscription(PacketEntry *t) {
  subscriberQueue_.queuePacket(t);
}

/*
 * Queue Keyword Unsubscription
 */
void Host::queueKeywordUnsubscription(PacketEntry *t) {
  unsubscriberQueue_.queuePacket(t);
}

/*
 * Queue for sending data (Publisher role)
 */
/*void Host::queueDataForSending(PacketEntry *t) {
  sendDataQueue_.queuePacket(t);
}*/

/*
 * Queue for receiving data (Subscriber role)
 */
void Host::queueDataForReceiving(PacketEntry *t) {
  recvDataQueue_.queuePacket(t);
}

/*
 * Keyword deregistration handler
 */
void Host::keywordDeregistrationHandler() {
  while(true) {
    std::unique_lock<std::mutex> lock(deregisterQueue_.packet_ready_mutex_); 
    deregisterQueue_.packet_ready_.wait(lock);

    while (!deregisterQueue_.packet_in_queue_.empty()) {
      auto pending = deregisterQueue_.packet_in_queue_.front();
      deregisterQueue_.packet_in_queue_.pop();

      if (!registered_) {
        Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
            "Cannot deregister keyword! No switch connected");
        continue;
      }
      /* Send packet (with random number) to register the keyword */
      char packet[BUFLEN];
      bzero(packet, BUFLEN);

      struct PacketTypeHeader header;
      header.packetType = PacketType::DEREGISTRATION_REQ;

      struct RequestPacketHeader requestPacketHeader;
      requestPacketHeader.hostId = myId_;
      requestPacketHeader.sequenceNo = sequenceNumberGen();
      requestPacketHeader.len = sizeof(unsigned int);

      unsigned int k;
      memcpy(&k, pending->packet, sizeof(unsigned int));
      /* Store the SeqNo to Unique Id which is sent to be registered. */
      std::pair<unsigned int, unsigned int> newPair
      (requestPacketHeader.sequenceNo, k);
      deRegAckNackBook_.insert(newPair);

      /* Copy data into the packet */
      memcpy(packet, &header, PACKET_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
          REQUEST_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, pending->packet, 
          sizeof(unsigned int));
      auto entry = ifToPacketEngine_.find(switchIf_);
      entry->second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN 
          + sizeof(unsigned int));
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "sent deregistration request with seq no = "
          + std::to_string(requestPacketHeader.sequenceNo));
    }
  }
}

/*
 * Keyword registration handler
 */
void Host::keywordRegistrationHandler() {
  while(true) {
    std::unique_lock<std::mutex> lock(registerQueue_.packet_ready_mutex_); 
    registerQueue_.packet_ready_.wait(lock);

    while (!registerQueue_.packet_in_queue_.empty()) {
      auto pending = registerQueue_.packet_in_queue_.front();
      registerQueue_.packet_in_queue_.pop();

      if (!registered_) {
        Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
            "Cannot register as publisher! No switch connected");
        continue;
      }

      auto keyword = std::string(pending->packet);
      unsigned int keywordLength = pending->len;
      /* Check if the keyword is already registered */
      if(publisherKeywordData_.keywordExists(keyword)) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Keyword already exists in publishing");
        continue;
      }

      /* Send packet (with random number) to register the keyword */
      char packet[BUFLEN];
      bzero(packet, BUFLEN);

      struct PacketTypeHeader header;
      header.packetType = PacketType::REGISTRATION_REQ;

      struct RequestPacketHeader requestPacketHeader;
      requestPacketHeader.hostId = myId_;
      requestPacketHeader.sequenceNo = sequenceNumberGen();
      requestPacketHeader.len = keywordLength;

      /* Store the sequence no to keyword for registration ACK-NACK handling */
      std::pair<unsigned int, std::string> newPair(requestPacketHeader.sequenceNo,
          keyword);
      regAckNackBook_.insert(newPair);
      /* Copy data into the packet */
      memcpy(packet, &header, PACKET_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
          REQUEST_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, pending->packet,
          keywordLength);
      auto entry = ifToPacketEngine_.find(switchIf_);
      entry->second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN 
          + keywordLength);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "sent publish registration request for: " + keyword);
    }
  }
}

/*
 * Keyword Subscription handler
 */
void Host::keywordSubscriptionHandler() {
  while(true) {
    std::unique_lock<std::mutex> lock(subscriberQueue_.packet_ready_mutex_); 
    subscriberQueue_.packet_ready_.wait(lock);

    while (!subscriberQueue_.packet_in_queue_.empty()) {
      auto pending = subscriberQueue_.packet_in_queue_.front();
      subscriberQueue_.packet_in_queue_.pop();

      auto keyword = std::string(pending->packet);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Got keyword from interface : " + keyword);
      unsigned int keywordLength = pending->len;
      /* Check if the keyword is already subscribed. */
      if(subscriberKeywordData_.keywordExists(keyword)) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Keyword : " + keyword + " has already been subscribed");
        continue;
      }

      /* Send packet (with random sequence number) to register the keyword */
      char packet[PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength];
      bzero(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength);

      struct PacketTypeHeader header;
      header.packetType = PacketType::SUBSCRIPTION_REQ;

      struct RequestPacketHeader requestPacketHeader;
      requestPacketHeader.hostId = myId_;
      requestPacketHeader.sequenceNo = sequenceNumberGen();
      requestPacketHeader.len = keywordLength;

      /* Store the sequence no to keyword for subs ACK-NACK handling */
      std::pair<unsigned int, std::string> newPair (requestPacketHeader.sequenceNo,
          keyword);
      subsAckNackBook_.insert(newPair);

      /* Copy data into the packet */
      memcpy(packet, &header, PACKET_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
          REQUEST_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, pending->packet,
          keywordLength);
      auto entry = ifToPacketEngine_.find(switchIf_);
      entry->second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN 
          + keywordLength);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "sent subscription request for: " + keyword);
    }
  }
}

/*
 * Keyword Unsubscription handler
 */
void Host::keywordUnsubscriptionHandler() {
  while(true) {
    std::unique_lock<std::mutex> lock(unsubscriberQueue_.packet_ready_mutex_); 
    unsubscriberQueue_.packet_ready_.wait(lock);

    while (!unsubscriberQueue_.packet_in_queue_.empty()) {
      auto pending = unsubscriberQueue_.packet_in_queue_.front();
      unsubscriberQueue_.packet_in_queue_.pop();

      if (!registered_) {
        Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
            "Cannot register as subscriber! No switch connected");
        continue;
      }

      auto keyword = std::string(pending->packet);
      unsigned int keywordLength = pending->len;

      /* Check if the keyword is already subscribed. */
      if(!subscriberKeywordData_.keywordExists(keyword)) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Keyword : " + keyword + " has not been subscribed");
        continue;
      }

      /* Send packet (with random sequence number) to register the keyword */
      char packet[PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength];
      bzero(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN + keywordLength);

      struct PacketTypeHeader header;
      header.packetType = PacketType::DESUBSCRIPTION_REQ;

      struct RequestPacketHeader requestPacketHeader;
      requestPacketHeader.hostId = myId_;
      requestPacketHeader.sequenceNo = sequenceNumberGen();
      requestPacketHeader.len = keywordLength;

      /* Store the sequence no to keyword for subs ACK-NACK handling */
      std::pair<unsigned int, std::string> newPair (requestPacketHeader.sequenceNo,
          keyword);
      unsubsAckNackBook_.insert(newPair);

      /* Copy data into the packet */
      memcpy(packet, &header, PACKET_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN, &requestPacketHeader, 
          REQUEST_HEADER_LEN);
      memcpy(packet + PACKET_HEADER_LEN + REQUEST_HEADER_LEN, pending->packet,
          keywordLength);
      auto entry = ifToPacketEngine_.find(switchIf_);
      entry->second.send(packet, PACKET_HEADER_LEN + REQUEST_HEADER_LEN 
          + keywordLength);
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "sent subscription request for: " + keyword);
    }
  }
}

/*
 * Handle Switch Down Registration
 */
void Host::switchStateHandler() {
  while (true) {
    while (!registered_) {
      sleep(1);
      continue;
    }
    switchDownCount_++;
    if (switchDownCount_ >= 3) {
      registered_ = false;
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Switch: " + std::to_string(mySwitch_) + " is down");
      mySwitch_ = 0;
      switchIf_ = "";
    }
    sleep(1);
  }
}

/*
 * Handle Hello message 
 */
void Host::handleHello() {
  while(true) {
    std::unique_lock<std::mutex> lock(helloQueue_.packet_ready_mutex_); 
    helloQueue_.packet_ready_.wait(lock);

    while (!helloQueue_.packet_in_queue_.empty()) {
      auto pending = helloQueue_.packet_in_queue_.front();
      helloQueue_.packet_in_queue_.pop();
      struct HelloPacketHeader helloPacket;
      bcopy(pending->packet + PACKET_HEADER_LEN, &helloPacket, HELLO_HEADER_LEN);
      if (helloPacket.nodeId == mySwitch_) {
        switchDownCount_ = 0;
      }
    }
  }
}

void Host::handleRegistrationResp() {
  while(true) {
    std::unique_lock<std::mutex> lock(hostRegRespQueue_.packet_ready_mutex_); 
    hostRegRespQueue_.packet_ready_.wait(lock);

    while (!hostRegRespQueue_.packet_in_queue_.empty()) {
      auto pending = hostRegRespQueue_.packet_in_queue_.front();
      hostRegRespQueue_.packet_in_queue_.pop();
      struct RegistrationPacketHeader regResponse;
      bcopy(pending->packet + PACKET_HEADER_LEN, &regResponse, 
          REGISTRATION_RESPONSE_HEADER_LEN);
      mySwitch_ = regResponse.nodeId;
      switchIf_ = pending->interface;
      registered_ = true;
      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Registered with Switch: " + std::to_string(mySwitch_)
      + " at interface: " + pending->interface);
    }
  }
}

void Host::sendRegistration() {
  char packet[PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN];
  bzero(packet, PACKET_HEADER_LEN + REGISTRATION_HEADER_LEN);
  struct PacketTypeHeader header;
  header.packetType = PacketType::HOST_REGISTRATION;
  struct RegistrationPacketHeader regPacketHeader;
  regPacketHeader.nodeId = myId_;
  memcpy(packet, &header, PACKET_HEADER_LEN);
  memcpy(packet + PACKET_HEADER_LEN, &regPacketHeader, REGISTRATION_HEADER_LEN);
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
 * Thread to Sniff for interface and packet engine
 */
void Host::startSniffing(std::string, PacketEngine *packetEngine){
  char packet[BUFLEN];
  while(true) {
    packetEngine->receive(packet);
  }
}

void Host::handleControlResp() {
  while(true) {
    std::unique_lock<std::mutex> lock(controlPacketQueue_.packet_ready_mutex_); 
    controlPacketQueue_.packet_ready_.wait(lock);

    while (!controlPacketQueue_.packet_in_queue_.empty()) {
      auto pending = controlPacketQueue_.packet_in_queue_.front();
      controlPacketQueue_.packet_in_queue_.pop();

      struct PacketTypeHeader packetTypeHeader;
      bcopy(pending->packet, &packetTypeHeader,PACKET_HEADER_LEN);

      struct ResponsePacketHeader responsePacketHeader;
      bcopy(pending->packet + PACKET_HEADER_LEN, &responsePacketHeader, RESPONSE_HEADER_LEN);

      /* Check if destined for this Host and seqNo exists */
      if(responsePacketHeader.hostId != myId_) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, "got a control response");
        continue;
      }

      /* if the response was for registration */
      if (packetTypeHeader.packetType == PacketType::REGISTRATION_ACK ||
          packetTypeHeader.packetType == PacketType::REGISTRATION_NACK) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "got registration response");

        if(regAckNackBook_.count(responsePacketHeader.sequenceNo) <= 0) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Spurious Sequence no, not present in regAckNackBook_ map  = "
              + std::to_string(responsePacketHeader.sequenceNo));
          continue;
        }

        std::string keyword = regAckNackBook_[responsePacketHeader.sequenceNo];

        if (packetTypeHeader.packetType == PacketType::REGISTRATION_NACK) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "got nack while registering for keyword: " + keyword
              + " will try again");
          continue;
        }

        /* 
         * Remove the entry from the regAckNackBook_ ,
         * to stop the request Thread 
         */
        regAckNackBook_.erase(responsePacketHeader.sequenceNo);
        unsigned int uniqueId;
        bcopy(pending->packet + PACKET_HEADER_LEN + RESPONSE_HEADER_LEN, 
            &uniqueId, sizeof(unsigned int));
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Got registration ack " + keyword + " " + std::to_string(uniqueId));
        publisherKeywordData_.addKeywordIDPair(keyword, uniqueId);

      } else if (packetTypeHeader.packetType == PacketType::DEREGISTRATION_ACK ||
          packetTypeHeader.packetType == PacketType::DEREGISTRATION_NACK) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "got deregistration response");

        if(deRegAckNackBook_.count(responsePacketHeader.sequenceNo) <= 0) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Spurious Sequence no, not present in deRegAckNackBook_ map  = "
              + std::to_string(responsePacketHeader.sequenceNo));
          continue;
        }

        unsigned int uniqueId = deRegAckNackBook_[responsePacketHeader.sequenceNo];
        if (packetTypeHeader.packetType == PacketType::DEREGISTRATION_NACK) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "got nack while deregistering for uniqueId: "
              + std::to_string(uniqueId) + " will try again");
          continue;
        }

        /* Remove the entry from the deRegAckNackBook_ , to stop the request Thread */
        deRegAckNackBook_.erase(responsePacketHeader.sequenceNo);
        /* Now delete the entry for the uniqueId from publisherKeyword map */
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Got DeRegistration ack for uniqueId =  " + std::to_string(uniqueId));
        bool isDeleted = publisherKeywordData_.removeKeywordIDPair
            (publisherKeywordData_.fetchKeyword(uniqueId));
        if(!isDeleted) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Could not delete the uniqueId: " + std::to_string(uniqueId) +
              "from publisher keyword map");
        }

      } else if (packetTypeHeader.packetType == PacketType::SUBSCRIPTION_ACK ||
          packetTypeHeader.packetType == PacketType::SUBSCRIPTION_NACK) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "got subscription response");

        if(subsAckNackBook_.count(responsePacketHeader.sequenceNo) <= 0) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Spurious Sequence no, not present in subsAckNackBook_ map  = "
              + std::to_string(responsePacketHeader.sequenceNo));
          continue;
        }

        std::string keyword = subsAckNackBook_[responsePacketHeader.sequenceNo];
        if (packetTypeHeader.packetType == PacketType::SUBSCRIPTION_NACK) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "got nack while subscribing for keyword: " + keyword
              + " will try again");
          continue;
        }

        if (responsePacketHeader.len <= 0) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
              "Invalid Subscription Response length found! keyword: "
              + keyword);
          continue;
        }        

        unsigned int uniqueId = 0;
        /* Remove the entry from the subsAckNackBook_ , to stop the request Thread */
        subsAckNackBook_.erase(responsePacketHeader.sequenceNo);
        for(unsigned int idx = 0; idx < responsePacketHeader.len; 
            idx = idx + sizeof(unsigned int)) {
          bcopy(pending->packet + PACKET_HEADER_LEN + RESPONSE_HEADER_LEN 
              + idx, &uniqueId, sizeof(unsigned int));
          subscriberKeywordData_.addKeyword(keyword, uniqueId);
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Got subscription ack " + keyword + " Uid: " + std::to_string(uniqueId));
        }

        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Got subscription ack " + keyword);

      } else if (packetTypeHeader.packetType == PacketType::DESUBSCRIPTION_ACK ||
          packetTypeHeader.packetType == PacketType::DESUBSCRIPTION_NACK) {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "got desubscription response");

        if(unsubsAckNackBook_.count(responsePacketHeader.sequenceNo) <= 0) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Spurious Sequence no, not present in unsubsAckNackBook_ map  = "
              + std::to_string(responsePacketHeader.sequenceNo));
          continue;
        }

        std::string unsubsKeyword = unsubsAckNackBook_[responsePacketHeader.sequenceNo];
        if (packetTypeHeader.packetType == PacketType::DESUBSCRIPTION_NACK) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "got nack while unsubscribing for keyword: "
              + unsubsKeyword + " will try again");
          continue;
        }

        /* Remove the entry from the unsubsAckNackBook_ , to stop the request Thread */
        unsubsAckNackBook_.erase(responsePacketHeader.sequenceNo);

        /* Now delete the entry for the uniqueId from subscriberKeywordData_ map */
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
            "Got Unsubscription ack for keyword =  " + unsubsKeyword);

        bool isDeleted = subscriberKeywordData_.removeKeyword(unsubsKeyword);
        if(!isDeleted) {
          Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "Could not delete the keyword: " + unsubsKeyword +
              "from publisher keyword map");
        }
      }
    }
  }
}


/*
 * Handler for sending data packets.
 */
void Host::sendDataHandler(const char *data, unsigned int len) {
  /*Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
      "Sending a data packet....");*/

  /* Sanity check - to see if the switch is registered or not */
  if (!registered_) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
        "Cannot send data ! No switch connected");
    return;
  }

  char packet[len];
  bzero(packet, len);
  bcopy(data, packet, len);

  auto entry = ifToPacketEngine_.find(switchIf_);
  entry->second.send(packet, len);
  /*Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
      "Sent the data.");*/
}

/*
 * Handle Incoming Data Packets
 */
void Host::recvDataHandler() {
  while(true) {
    std::unique_lock<std::mutex> lock(recvDataQueue_.packet_ready_mutex_);    
    recvDataQueue_.packet_ready_.wait(lock);

    while (!recvDataQueue_.packet_in_queue_.empty()) {
      auto pending = recvDataQueue_.packet_in_queue_.front();
      recvDataQueue_.packet_in_queue_.pop();
      packetsReceived_++;
      dataSize_ += pending->len;

      /*Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Received a data packet....");

      struct DataPacketHeader dataPacketHeader;
      bcopy(pending->packet + PACKET_HEADER_LEN, &dataPacketHeader,
          DATA_HEADER_LEN);

      char data[dataPacketHeader.len];
      bcopy(pending->packet + PACKET_HEADER_LEN + DATA_HEADER_LEN, data,
          dataPacketHeader.len);

      Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
          "Received Data : " + std::string(data));*/
    }
  }
}

void Host::throughput() {
  while (true) {
    pt::ptime time_start(pt::microsec_clock::local_time());
    dataSize_ = 0;
    usleep(50000);
    unsigned int data = dataSize_;
    pt::ptime time_end(pt::microsec_clock::local_time());
    pt::time_duration duration(time_end - time_start);
    auto throughput = (data / duration.total_microseconds()) * 8;
    if (dataSize_ != 0) {
      Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__,
              "Throughput (mbps): " + std::to_string(throughput));
      /*" Packets Received: " + std::to_string(packetsReceived_));*/
    }
  }
}

/*
 * Send Hello Thread
 */
void Host::sendHello() {
  char packet[PACKET_HEADER_LEN + HELLO_HEADER_LEN];
  bzero(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
  struct PacketTypeHeader header;
  header.packetType = PacketType::HELLO;
  struct HelloPacketHeader helloPacketHeader;
  helloPacketHeader.nodeId = myId_;

  memcpy(packet, &header, PACKET_HEADER_LEN);
  memcpy(packet + PACKET_HEADER_LEN, &helloPacketHeader, HELLO_HEADER_LEN);
  while (true) {
    /* dont send any hello until registered with a switch */
    while (!registered_) {
      continue;
    }
    auto entry = ifToPacketEngine_.find(switchIf_);
    entry->second.send(packet, PACKET_HEADER_LEN + HELLO_HEADER_LEN);
    sleep(1);
  }
}

std::unordered_map<std::string, unsigned int> Host::getPublishingMap() {
  return publisherKeywordData_.getList();
}

std::unordered_map<std::string, std::vector<unsigned int>> Host::getSubscriptionMap() {
  return subscriberKeywordData_.getList();
}
