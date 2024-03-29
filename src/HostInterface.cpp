#include "include/HostInterface.h"
#include "include/Logger.h"
#include "include/Host.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <unordered_map>

char HOST_PATH[] = "/tmp/hostSocket";

HostInterface::HostInterface(Host *myHost) {
  host_ = myHost;
}

void HostInterface::readSocket() {
  char command[1700];
  struct sockaddr_un local, remote;
  socklen_t remoteLen;
  bzero(&local, sizeof(local));
  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, HOST_PATH, 104);
  unlink(HOST_PATH);
  if ((socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Unable to create the HostInterface Socket");
    close(socket_);
    close(cliSocket_);
    return;
  }
  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                "Listening at path: " + std::string(HOST_PATH));
  if (bind(socket_, (struct sockaddr *) &local, sizeof(local)) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                      "HostInterface Socket bind failed: " 
                      + std::string(local.sun_path));
    close(socket_);
    close(cliSocket_);
    return;
  }
  /* only one external interface can connect */
  if (listen(socket_, 5) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "HostInterface Socket listen failed");
    close(socket_);
    close(cliSocket_);
    return;
  }
  while (true) {
    PacketEntry pkt;
    bool done = false;
    int rc;
    remoteLen = sizeof(remote);
    if ((cliSocket_ = accept(socket_, (struct sockaddr *)&remote, 
                                                          &remoteLen)) == -1) {
      Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                  "HostInterface Socket accept failed");
      close(socket_);
      close(cliSocket_);
      return;
    }
    while (!done) {
      bzero(command, sizeof(command));
      rc = recv(cliSocket_, command, 1700, 0);
      if (rc <= 0) {
        continue;
      }
      if (strcmp(command, "quit") == 0) {
        done = true;
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "quiting");
      } else if (strcmp(command, "show publish keyword list") == 0) {
        sendPublishList();
      } else if (strcmp(command, "show subscribe keyword list") == 0) {
        sendSubscribeList();
      } else if (strncmp(command, "p", 1) == 0) {
        bzero(pkt.packet, BUFLEN);
        unsigned int len = 0;
        bcopy(command + sizeof(char), &len, sizeof(unsigned int));
        bcopy(command + sizeof(char) + sizeof(unsigned int), pkt.packet, 
                                                        len * sizeof(char));
        pkt.len = len;
        host_->queueKeywordRegistration(&pkt);
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                "received publishing request for: " + std::string(pkt.packet));
      } else if (strncmp(command, "u", 1) == 0) {
        bzero(pkt.packet, BUFLEN);
        unsigned int uniqueId = 0;
        bcopy(command + sizeof(char), &uniqueId, sizeof(unsigned int));
        bcopy(command + sizeof(char), pkt.packet, sizeof(unsigned int));
        pkt.len = sizeof(unsigned int);
        host_->queueKeywordDeregistration(&pkt);
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
              "received unpublishing request for: " + std::to_string(uniqueId));
      } else if (strcmp(command, "show packet received counter") == 0) {
        char data[BUFLEN];
        bzero(data, BUFLEN);
        sprintf(data, "%u", host_->packetsReceived_);
        sendData(data, strlen(data));
      } else if (strncmp(command, "s", 1) == 0) {
        bzero(pkt.packet, BUFLEN);
        unsigned int len = 0;
        bcopy(command + sizeof(char), &len, sizeof(unsigned int));
        bcopy(command + sizeof(char) + sizeof(unsigned int), pkt.packet, 
                                                        len * sizeof(char));
        pkt.len = len;
        host_->queueKeywordSubscription(&pkt);
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "received subscription request for: " + std::string(pkt.packet));
      } else if (strncmp(command, "d", 1) == 0) {
        bzero(pkt.packet, BUFLEN);
        unsigned int len = 0;
        bcopy(command + sizeof(char), &len, sizeof(unsigned int));
        bcopy(command + sizeof(char) + sizeof(unsigned int), pkt.packet, 
                                                        len * sizeof(char));
        pkt.len = len;
        host_->queueKeywordUnsubscription(&pkt);
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "received unsubscription request for: " + std::string(pkt.packet));
      } else if (strncmp(command, "f", 1) == 0) {
        char data[BUFLEN]; 
        bzero(data, BUFLEN);

        unsigned int numberOfPackets = 0, lastPacketSize = 0;
        //unsigned int uniqueId = 0;
        bcopy(command + sizeof(char), &numberOfPackets, sizeof(unsigned int));
        bcopy(command + sizeof(char) + sizeof(unsigned int),
                &lastPacketSize, sizeof(unsigned int));
        
        /* Packet Type Header filling up */
        struct PacketTypeHeader header;
        header.packetType = PacketType::DATA;        
        
        /* Data Packet Header filling up */
        struct DataPacketHeader dataPacketHeader;
        dataPacketHeader.sequenceNo = 0;
        bcopy(command + sizeof(char) + (2 * sizeof(unsigned int)), &dataPacketHeader.uniqueId, sizeof(unsigned int));
        dataPacketHeader.len = BUFLEN - 14;
        if(dataPacketHeader.len == 0) {
            Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "Data size = 0 ! Try sending data again");
            continue;
        }
  
       /* Copy everything into a packet and send it. */
       memcpy(data, &header, PACKET_HEADER_LEN);
       memcpy(data + PACKET_HEADER_LEN, &dataPacketHeader, DATA_HEADER_LEN); 
        
       /* Filling in the payload */
       bcopy(command + sizeof(char) + (3 * sizeof(unsigned int)),
                data + PACKET_HEADER_LEN + DATA_HEADER_LEN, BUFLEN - 14);
        
       for(unsigned int i = 0; i < numberOfPackets; i++) {
           host_->sendDataHandler(data, BUFLEN);
       }
       
       /* Sending last packet now */
       dataPacketHeader.len = lastPacketSize;
       memcpy(data + PACKET_HEADER_LEN, &dataPacketHeader, DATA_HEADER_LEN); 
       host_->sendDataHandler(data, lastPacketSize);
       Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "data sent");  
      } else if (strncmp(command, "d", 1) == 0) {
        bzero(pkt.packet, BUFLEN);
        unsigned int len = 0;
        bcopy(command + sizeof(char), &len, sizeof(unsigned int));
        bcopy(command + sizeof(char) + sizeof(unsigned int), pkt.packet,
              len * sizeof(char));
        pkt.len = len;
        host_->queueKeywordUnsubscription(&pkt);
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "received unsubscription request for: " + std::string(pkt.packet));

      } else if (strcmp(command, "clear packet received counter") == 0) {
        host_->packetsReceived_ = 0;
      } else {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "Invalid Command: " + std::string(command));
      } 
    }
    close(cliSocket_);
  }
}

void HostInterface::sendSubscribeList() {
  char data[BUFLEN];
  bzero(data, BUFLEN);
  /* this is tricky */
  /* first send the length of the forward table */
  auto subscribeMap = host_->getSubscriptionMap();
  sprintf(data, "%ld", subscribeMap.size());
  sendData(data, strlen(data));
  sleep(1);
  /* lets send the entries now */
  /* this needs to be optimized */
  for (auto entry : subscribeMap) {
    bzero(data, BUFLEN);
    auto keyword = entry.first;
    keyword.append(";");
    /* this is required bcopy doesnt work well when copying int to a char * */
    bcopy(keyword.c_str(), data, keyword.length());
    sprintf(data + keyword.length(), "%u", entry.second.front());
    sendData(data, strlen(data));
    sleep(1);
  }
  Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__,
                    "sending subscribed list");
}

void HostInterface::sendPublishList() {
  char data[BUFLEN];
  bzero(data, BUFLEN);
  /* this is tricky */
  /* first send the length of the forward table */
  auto publishingMap = host_->getPublishingMap();
  sprintf(data, "%ld", publishingMap.size());
  sendData(data, strlen(data));
  sleep(1);
  /* lets send the entries now */
  /* this needs to be optimized */
  for (auto entry : publishingMap) {
    bzero(data, BUFLEN);
    auto keyword = entry.first;
    keyword.append(";");
    /* this is required bcopy doesnt work well when copying int to a char */
    bcopy(keyword.c_str(), data, keyword.length());
    sprintf(data + keyword.length(), "%u", entry.second);
    sendData(data, strlen(data));
    sleep(1);
  }
  Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__,
                    "sending publishing list");
}

void HostInterface::sendData(char *data, int len) {
  auto rc = send(cliSocket_, data, len, 0);
  if (rc < 0) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Unable to send data over the socket");
  }
}

HostInterface::~HostInterface() {
  close(socket_);
  close(cliSocket_);
}
