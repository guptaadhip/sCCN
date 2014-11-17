#pragma once

class Controller; 

class ControllerInterface {
 public:
  /* 
   * Controller Interface Constructor 
   */
  ControllerInterface(Controller *controller); 

  /*
   * Destructor to kill the sockets
   */
  ~ControllerInterface();
  /* 
   * initialize and read from the Unix Socket 
   */
  void readSocket();
  /*
   * Send data over the socket
   */
  void sendData(char *data, int len);

 private:
  /* Controller object */
  Controller *controller_;
  int socket_;
  int cliSocket_;
};
