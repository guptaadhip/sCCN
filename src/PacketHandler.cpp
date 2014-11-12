#include "include/PacketHandler.h"
#include <iostream>

void PacketHandler::queuePacket(packet *t) {
  packet *stale_head = packet_in_queue_.load(std::memory_order_relaxed);
  do { 
    t->next = stale_head;
  } while (!packet_in_queue_.compare_exchange_weak
            (stale_head, t, std::memory_order_release));

    if( !stale_head ) { 
        std::unique_lock<std::mutex> lock(packet_ready_mutex_);
        packet_ready_.notify_one();
    }
}

void PacketHandler::processQueue() {
  while(true) {
    auto pending = packet_in_queue_.exchange(0, std::memory_order_consume);
    /* process the packet */
    /* TBD: How to process the packet */
    std::cout << "Got Packet:" << pending->interface << std::endl;
    if( !pending ) {
      std::unique_lock<std::mutex> lock(packet_ready_mutex_);                
      if( !packet_in_queue_ ) packet_ready_.wait(lock);
    }
  }
}
