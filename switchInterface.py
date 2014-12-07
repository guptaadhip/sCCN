# -*- coding: utf-8 -*-
import sys
import struct
import socket
import os, os.path

client = None

def signal_handler(signal, frame):
  print "Shutting down."
  try:
    client.send("quit")
    client.close()
  except:
    print "Switch down"
  sys.exit(0)

def printHelp():
  print "Commands:"
  print "\tquit"
  print "\tshow switch id"
  print "\tshow controller id"
  print "\tshow controller interface"
  print "\tshow forwarding table"

def getControllerInterface(x, client):
  client.send(x)
  data = client.recv(1024);
  print "Controller Interface: " + data

def getControllerId(x, client):
  client.send(x)
  data = client.recv(1024);        
  if data == "0":
    print "Controller: Not connected"
    return
  print "Controller Id: " + data

def getSwitchId(x, client):
  data = client.recv(2048);
  print "Switch Id: " + data

def getForwardingTable():
  client.send(x)
  data = client.recv(1024);
  if data == "0":
    print "No entries in the forwarding table"
    return
  print ("Unique Id\tCount\tInterface");
    tmp = 0
    count = int(data)
    while (tmp < count):
      data = client.recv(1024);
      uidx = data.find(';')
      cidx = string.find(';', data, 0, uidx)
      print data[:uidx], data[uidx, cidx], data[cidx:]
      tmp = tmp + 1


print "Connecting..."
if os.path.exists( "/tmp/switchSocket" ):
  client = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
  client.connect( "/tmp/switchSocket" )
  print "Ready."
  signal.signal(signal.SIGINT, signal_handler)
  printHelp()
  while True:
    try:
      x = raw_input( "> " )
      x = x.strip()
      if "" != x:
        if "help" == x:
          printHelp()
          continue
        elif "quit" == x:
          print "Shutting down."
          client.send(x)
          break
        elif "show forwarding table" == x:
          getForwardingTable(x)
        elif x == "show controller id":
          getControllerId(x, client)
        elif x == "show controller interface":
          getControllerInterface(x, client)
        elif x == "show controller interface":
          getSwitchId(x, client)
        else:
          print "Invalid Command"
          continue
    except KeyboardInterrupt, k:
      print "Shutting down."
      break
  client.close()
else:
  print "Couldn't Connect!"
  exit(1)
print "Done"
