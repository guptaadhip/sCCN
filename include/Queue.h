#pragma once
#include "include/net.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class Queue {
 public:
  /*
   * Function to queue the Packet Entry
   */
  void queuePacket(PacketEntry *t);

  /*
   * Variables to be accessed by the handler threads
   */
  std::mutex packet_ready_mutex_;
  std::condition_variable packet_ready_;
  std::atomic<PacketEntry *> packet_in_queue_;
};

typedef std::unordered_map<unsigned short, Queue *> PacketTypeToQueue;
