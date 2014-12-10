#include "include/PacketHandler.h"
#include "include/Queue.h"

void Queue::queuePacket(PacketEntry *t) {
  packet_in_queue_.push(t);
  packet_ready_.notify_one();
}
