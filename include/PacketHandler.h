#pragma once
#include "include/net.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>


class PacketHandler {
 public:
  void queuePacket(packet *t);
  void processQueue();
 private:
  std::mutex packet_ready_mutex_;
  std::condition_variable packet_ready_;
  std::atomic<packet*> packet_in_queue_;
};
