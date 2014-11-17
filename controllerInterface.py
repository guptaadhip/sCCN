# -*- coding: utf-8 -*-
import socket
import os, os.path

print "Connecting..."
if os.path.exists( "/tmp/controllerSocket" ):
  client = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
  client.connect( "/tmp/controllerSocket" )
  print "Ready."
  print "Commands:"
  print "\tquit"
  print "\tshow controller id"
  while True:
    try:
      x = raw_input( "> " )
      x = x.strip()
      if "" != x:
        client.send( x ) 
        if "quit" == x:
          print "Shutting down."
          break
        data = client.recv(2048);
        if x == "show controller id":
          print "My Id: " + data
        else:
          print "Invalid command";
    except KeyboardInterrupt, k:
      print "Shutting down."
  client.close()
else:
  print "Couldn't Connect!"
  exit(1)
print "Done"
