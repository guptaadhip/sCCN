#include "include/PacketHandler.h"

void Queue::queuePacket(PacketEntry *t) {
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
