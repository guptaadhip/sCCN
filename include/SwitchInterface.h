#pragma once

class Switch; 

class SwitchInterface {
 public:
  /* 
   * Switch Interface Contructor 
   */
  SwitchInterface(Switch *mySwitch); 

  /*
   * Destructor to kill the sockets
   */
  ~SwitchInterface();
  /* 
   * initialize and read from the Unix Socket 
   */
  void readSocket();
  /* 
   * Respond with the forwarding table of the switch
   */
  void sendForwardTable();
  /*
   * Send data over the socket
   */
  void sendData(char *data, int len);

 private:
  /* Switch object */
  Switch *switch_;
  int socket_;
  int cliSocket_;
};
