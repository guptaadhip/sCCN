#include "include/PacketHandler.h"
#include "include/Logger.h"
#include <cstring>

void PacketHandler::queuePacket(PacketEntry *t) {
  PacketEntry *stale_head = packet_in_queue_.load(std::memory_order_relaxed);
  do { 
    t->next = stale_head;
  } while (!packet_in_queue_.compare_exchange_weak
            (stale_head, t, std::memory_order_release));

  if( !stale_head ) { 
    std::unique_lock<std::mutex> lock(packet_ready_mutex_);
    packet_ready_.notify_one();
  }
}

void PacketHandler::processQueue(PacketTypeToQueue *packetTypeToQueue) {
  /* first one needs to be removed */
  (void) packet_in_queue_.exchange(0, std::memory_order_consume);
  while(true) {
    auto pending = packet_in_queue_.exchange(0, std::memory_order_consume);
    if( !pending ) {
      std::unique_lock<std::mutex> lock(packet_ready_mutex_);                
      if( !packet_in_queue_ ) packet_ready_.wait(lock);
      continue;
    }
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
