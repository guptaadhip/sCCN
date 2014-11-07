#include <iostream>
#include "include/net.h"

int main() {
  std::cout << "Hello World!!" << std::endl;
  std::cout << (int) PacketType::REGISTRATION_REQ << std::endl;
  return 0;
}
