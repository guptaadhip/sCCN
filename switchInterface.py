# -*- coding: utf-8 -*-
import sys
import struct
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
          print ("Unique Id\tCount\tInterface");
          tmp = 0
          count = int(data)
          while (tmp < count):
            # If entries exist lets read them
            data = client.recv(2048);
            # need to handle this data correctly
            uid = struct.unpack('II', data[:8])
            data = data[8:].decode('ascii').encode('utf-8')
            print '%u\t%u\t%s' % (uid[0], uid[1], data)
            tmp = tmp + 1
        elif x == "show controller id":
          if data == "0":
            print "Controller: Not connected"
            continue
          print "Controller Id: " + data
        elif x == "show controller interface":
          print "Controller Interface: " + data
    except KeyboardInterrupt, k:
      print "Shutting down."
    except:
      print "Errored:", sys.exc_info()
      continue
  client.close()
else:
  print "Couldn't Connect!"
  exit(1)
print "Done"
