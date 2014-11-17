# -*- coding: utf-8 -*-
import socket
import os, os.path

print "Connecting..."
if os.path.exists( "/tmp/switchSocket" ):
  client = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
  client.connect( "/tmp/switchSocket" )
  print "Ready."
  print "Commands:"
  print "\tquit"
  print "\tshow switch id"
  print "\tshow controller id"
  print "\tshow controller interface"
  print "\tshow forwarding table"
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
        if x == "show switch id":
          print "Switch Id: " + data
        elif x == "show forwarding table":
          if data == "0":
            print "No entries in the forwarding table"
            continue
          print ("Unique Id : Interface");
          count = 0;
          while (count < data):
            # If entries exist lets read them
            data = client.recv(2048);
            # need to handle this data correctly
            uid = data[:4]
            data = data[data:]
            print uid + " : " +  data
            count = count + 1
        elif x == "show controller id":
          if data == "0":
            print "Controller: Not connected"
            continue
          print "Controller Id: " + data
        elif x == "show controller interface":
          print "Controller Interface: " + data
    except KeyboardInterrupt, k:
      print "Shutting down."
  client.close()
else:
  print "Couldn't Connect!"
  exit(1)
print "Done"
