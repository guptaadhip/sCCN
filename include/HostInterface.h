#pragma once

class Host; 

class HostInterface {
 public:
  /* 
   * Host Interface Constructor 
   */
  HostInterface(Host *myHost); 

  /*
   * Destructor to kill the sockets
   */
  ~HostInterface();
  /* 
   * initialize and read from the Unix Socket 
   */
  void readSocket();
  /*
   * Send data over the socket
   */
  void sendData(char *data, int len);

 private:
  /* Host object */
  Host *host_;
  int socket_;
  int cliSocket_;
};
