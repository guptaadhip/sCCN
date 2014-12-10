#include "include/PacketHandler.h"
#include "include/Logger.h"
#include <cstring>

void PacketHandler::queuePacket(PacketEntry *t) {
  packet_in_queue_.push(t);
  packet_ready_.notify_one();
}

void PacketHandler::processQueue(PacketTypeToQueue *packetTypeToQueue) {
  /* first one needs to be removed */
  while(true) {
    std::unique_lock<std::mutex> lock(packet_ready_mutex_);                
    packet_ready_.wait(lock);

    while (!packet_in_queue_.empty()) {
      auto pending = packet_in_queue_.front();
      packet_in_queue_.pop();
      //Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
      //            "received packet from " + pending->interface);
      PacketTypeHeader header;
      bcopy(pending->packet, &header, PACKET_HEADER_LEN);
      auto entry = packetTypeToQueue->find((unsigned short) header.packetType);
      if (entry != packetTypeToQueue->end()) {
        entry->second->queuePacket(pending);
      } else {
        // Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__, 
        //           "no packet handling queue found " + std::to_string(
        //           (unsigned short) header.packetType) + " from " 
        //           + pending->interface);
      }
    }
  }
}
