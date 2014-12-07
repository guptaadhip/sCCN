#include "include/SwitchInterface.h"
#include "include/Logger.h"
#include "include/Switch.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <unordered_map>

char PATH[] = "/tmp/switchSocket";

SwitchInterface::SwitchInterface(Switch *mySwitch) {
  switch_ = mySwitch;
}

void SwitchInterface::readSocket() {
  char command[BUFLEN];
  struct sockaddr_un local, remote;
  socklen_t remoteLen;
  bzero(&local, sizeof(local));
  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, PATH, 104);
  unlink(PATH);
  if ((socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Unable to create the SwitchInterface Socket");
    close(socket_);
    close(cliSocket_);
    return;
  }
  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                "Listening at path: " + std::string(PATH));
  if (bind(socket_, (struct sockaddr *) &local, sizeof(local)) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                      "SwitchInterface Socket bind failed: " 
                      + std::string(local.sun_path));
    close(socket_);
    close(cliSocket_);
    return;
  }
  /* only one external interface can connect */
  if (listen(socket_, 5) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "SwitchInterface Socket listen failed");
    close(socket_);
    close(cliSocket_);
    return;
  }
  while (true) {
    char data[BUFLEN];
    bool done = false;
    int rc;
    remoteLen = sizeof(remote);
    if ((cliSocket_ = accept(socket_, (struct sockaddr *)&remote, 
                                                          &remoteLen)) == -1) {
      Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                  "SwitchInterface Socket accept failed");
      close(socket_);
      close(cliSocket_);
      return;
    }
    while (!done) {
      bzero(command, sizeof(command));
      rc = recv(cliSocket_, command, 1024, 0);
      if (rc <= 0) {
        continue;
      }
      if (strcmp(command, "quit") == 0) {
        done = true;
      } else if (strcmp(command, "show forwarding table") == 0) {
        sendForwardTable();
      } else if (strcmp(command, "show switch id") == 0) {
        bzero(data, sizeof(data));
        sprintf(data, "%d", switch_->getSwitchId()); 
        sendData(data, strlen(data));
      } else if (strcmp(command, "show controller id") == 0) {
        bzero(data, sizeof(data));
        sprintf(data, "%d", switch_->getControllerId()); 
        sendData(data, strlen(data));
      } else if (strcmp(command, "show controller interface") == 0) {
        if (switch_->getControllerId() == 0) {
          bzero(data, sizeof(data));
          sprintf(data, "Not connected");
        } else {
          bzero(data, sizeof(data));
          bcopy(switch_->getControllerIf().c_str(), data, 
                                switch_->getControllerIf().length());
        }
        sendData(data, strlen(data));
      } else {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "Invalid Command: " + std::string(command));
      } 
    }
    close(cliSocket_);
  }
}

void SwitchInterface::sendData(char *data, int len) {
  auto rc = send(cliSocket_, data, len, 0);
  if (rc < 0) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Unable to send data over the socket");
  }
}

void SwitchInterface::sendForwardTable() {
  char data[BUFLEN];
  bzero(data, BUFLEN);
  /* this is tricky */
  /* first send the length of the forward table */
  auto forwardingTable = switch_->getForwardingTable();
  sprintf(data, "%ld", forwardingTable.size());
  sendData(data, strlen(data));
  sleep(1);
  /* lets send the entries now */
  /* this needs to be optimized */
  for (auto entry : forwardingTable) {
    bzero(data, BUFLEN);
    /* this is required bcopy doesnt work well when copying int to a char * */
    memcpy(data, &entry.first, sizeof(unsigned int));
    memcpy(data + sizeof(unsigned int), &entry.second.count, sizeof(unsigned int));
    sendData(data, BUFLEN);
    memcpy(data + sizeof(unsigned int) + sizeof(unsigned int), entry.second.interface.c_str(), entry.second.interface.length());
    sendData(data, BUFLEN);
    sleep(1);
  }
  Logger::log(Log::INFO, __FILE__, __FUNCTION__, __LINE__,
                    "sending forward table");
}

SwitchInterface::~SwitchInterface() {
  close(socket_);
  close(cliSocket_);
}
