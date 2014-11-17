#include "include/ControllerInterface.h"
#include "include/Logger.h"
#include "include/Controller.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <unordered_map>

char PATH_CONTROLLER[] = "/tmp/controllerSocket";

ControllerInterface::ControllerInterface(Controller *controller) {
  controller_ = controller;
}

void ControllerInterface::readSocket() {
  char command[BUFLEN];
  struct sockaddr_un local, remote;
  socklen_t remoteLen;
  bzero(&local, sizeof(local));
  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, PATH_CONTROLLER, 104);
  unlink(PATH_CONTROLLER);
  if ((socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Unable to create the ControllerInterface Socket");
    close(socket_);
    close(cliSocket_);
    return;
  }
  Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                "Listening at path: " + std::string(PATH_CONTROLLER));
  if (bind(socket_, (struct sockaddr *) &local, sizeof(local)) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                      "ControllerInterface Socket bind failed: " 
                      + std::string(local.sun_path));
    close(socket_);
    close(cliSocket_);
    return;
  }
  /* only one external interface can connect */
  if (listen(socket_, 5) == -1) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "ControllerInterface Socket listen failed");
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
                  "ControllerInterface Socket accept failed");
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
      } else if (strcmp(command, "show controller id") == 0) {
        bzero(data, sizeof(data));
        sprintf(data, "%d", controller_->getId()); 
        sendData(data, strlen(data));
      } else {
        Logger::log(Log::DEBUG, __FILE__, __FUNCTION__, __LINE__,
                    "Invalid Command: " + std::string(command));
      } 
    }
    close(cliSocket_);
  }
}

void ControllerInterface::sendData(char *data, int len) {
  auto rc = send(cliSocket_, data, len, 0);
  if (rc < 0) {
    Logger::log(Log::WARN, __FILE__, __FUNCTION__, __LINE__,
                "Unable to send data over the socket");
  }
}

ControllerInterface::~ControllerInterface() {
  close(socket_);
  close(cliSocket_);
}
