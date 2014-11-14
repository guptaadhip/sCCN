#pragma once
#include "include/net.h"
#include "include/Queue.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class PacketHandler {
 public:
  /*
   * Function to queue the Packet Entry
   */
  void queuePacket(PacketEntry *t);
  /*
   * Function to Process the Packet Entry
   */
  void processQueue(PacketTypeToQueue *packetTypeToQueue);
 private:
  std::mutex packet_ready_mutex_;
  std::condition_variable packet_ready_;
  std::atomic<PacketEntry *> packet_in_queue_;
};
