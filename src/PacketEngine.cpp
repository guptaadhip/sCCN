#include "include/PacketEngine.h"
#include "include/net.h"
#include "include/Logger.h"
#include "include/PacketHandler.h"

#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>

using namespace std;

unsigned int bufSize = 33554432;

PacketEngine::PacketEngine(std::string interface, unsigned int id,
                           PacketHandler *packetHandler) {
   const int on = 1;
   interface_ = interface;
   myId_ = id;
   packetHandler_ = packetHandler;

   /* creating sending socket */
   socketFd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
   if (socketFd_ < 0) {
       Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__, 
       							"PacketEngine : Error in opening sending socket");
   }

  if (setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
    Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__, 
       					"PacketEngine : Error setting socket option");
  }

  if (setsockopt(socketFd_, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(int)) == -1) {
  	Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__, 
       					"PacketEngine : Error setting socket option");

  }

  initializeEngine();
}

unsigned int PacketEngine::getId() const {
  return myId_;
}

std::string PacketEngine::getInterface() const {
  return interface_;
}

void PacketEngine::initializeEngine() {
  /* Finding the index of the interface */
  char *if_name = (char *) interface_.c_str();
  struct ifreq ifr;
  size_t if_name_len = strlen(if_name);
  if (if_name_len < sizeof(ifr.ifr_name)) {
    memcpy(ifr.ifr_name,if_name,if_name_len);
    ifr.ifr_name[if_name_len]=0;
  } else {
  	Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__, 
       					"PacketEngine : interface name is too long");
  }
  if (ioctl(socketFd_, SIOCGIFINDEX, &ifr)==-1) {
  	Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__,
    						"Packet Engine: Error getting Interface Index");
  }
  interfaceIdx_ = ifr.ifr_ifindex;

  /* attempt to receive */
  struct sockaddr_ll raddrll;
  
  /* setting raddrll values */
  raddrll.sll_family = PF_PACKET;
  raddrll.sll_ifindex = interfaceIdx_;
  raddrll.sll_protocol = htons(ETH_P_ALL);
  char dest[8];
  bzero((char *) dest, sizeof(dest));
  memcpy((void*)(raddrll.sll_addr), (void*)dest, 8);
  
  /* Binding the receiving socket to the interafce if_name */
  if (bind(socketFd_, (struct sockaddr *) &raddrll, sizeof(raddrll)) < 0) {
  	Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__,
    						"PacketEngine: Error binding to socket");
  }
  
  /* Get the current flags that the device might have */
  if (ioctl (socketFd_, SIOCGIFFLAGS, &ifr) == -1) {
  	Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__,
    			"Error: Could not retrive the flags from the device.");
  }
  
  /* Set the old flags plus the IFF_PROMISC flag */
  ifr.ifr_flags |= IFF_PROMISC;
  if (ioctl (socketFd_, SIOCSIFFLAGS, &ifr) == -1) {
    Logger::log(Log::CRITICAL, __FUNCTION__, __LINE__,
    						"Error: Could not set flag IFF_PROMISC");
  }
}

void PacketEngine::forward(char *buffer, unsigned int size) {
  struct sockaddr_ll saddrll;
  memset((void*)&saddrll, 0, sizeof(saddrll));
  saddrll.sll_family = AF_PACKET;

  /* filling the interface index */
  saddrll.sll_ifindex = interfaceIdx_;
  saddrll.sll_halen = 2;
  
  char dest[8];
  bzero((char *) dest, sizeof(dest));
  memcpy((void*)(saddrll.sll_addr), (void*)dest, 8);
 
  if (sendto(socketFd_, buffer, size, 0,
             (struct sockaddr*)&saddrll, sizeof(saddrll)) < 0) {
    Logger::log(Log::DEBUG, __FUNCTION__, __LINE__,
    						"Packet Engine: Error sending Packet");
  }
}

void PacketEngine::send(char *msg, unsigned int size) {
  struct sockaddr_ll saddrll;
  
  /* Socket Addr Structure */
  memset((void*)&saddrll, 0, sizeof(saddrll));
  saddrll.sll_family = AF_PACKET;
  saddrll.sll_ifindex = interfaceIdx_;
  saddrll.sll_halen = ETH_ALEN;

  char dest[8];
  bzero((char *) dest, sizeof(dest));
  memcpy((void*)(saddrll.sll_addr), (void*)dest, ETH_ALEN);

  if (sendto(socketFd_, msg, size, 0,
             (struct sockaddr*)&saddrll, sizeof(saddrll)) < 0) {
  }
}

/* 
 * srcId will only be used by the Receiver thread, otherwise 0 to read packets
 * from all sources
 */
void PacketEngine::receive(char *packetOld) {
  char packet[BUFLEN];
  struct sockaddr_ll saddrll;
  socklen_t senderAddrLen;
  int rc;
  
  /* 
   * zeroing the sender's address struct.
   * It will be filled by the recvfrom function.
   */
  bzero(packet, BUFLEN);
  memset((void*)&saddrll, 0, sizeof(saddrll));
  senderAddrLen = (socklen_t) sizeof(saddrll);
  
  
  while (true) {
    /* Start receiving the data */
    rc = recvfrom(socketFd_, packet, BUFLEN, 0,
                          (struct sockaddr *) &saddrll, &senderAddrLen);
    
    if (rc < 0 || saddrll.sll_pkttype == PACKET_OUTGOING) {
      continue;
    }
    PacketEntry packetEntry;
    bcopy(packet, packetEntry.packet, BUFLEN); 
    packetEntry.interface = interface_;
    packetHandler_->queuePacket(&packetEntry);
  }
}
