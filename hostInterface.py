# -*- coding: utf-8 -*-
import socket
import struct
import os, os.path

def printHelp():
  print "Commands:"
  print "\tquit"
  print "\thelp"
  print "\tsubscribe"
  print "\tpublish"
  print "\tshow publish keyword list"
  print "\tshow subscribe keyword list"
  print "\tsend data"

def getKeywords():
  keywords = []
  print "Input keyword (one keyword per line, when done enter 'q')"
  count = 1
  while True: 
    try:
      x = raw_input( str(count) + " " )
      x = x.strip()
      if "q" == x:
        strKeywords = ';'.join(keywords)
        strKeywords += ';'
        return strKeywords, len(strKeywords)
      keywords.append(x)
      count = count + 1
      continue
    except KeyboardInterrupt, k:
      return 

def publish():
  keywords, listLen = getKeywords()
  if not keywords:
    print "No keywords founds"
    return
  payload = "p" + struct.pack("I", listLen) + keywords
  return payload

def subscribe():
  keywords, listLen = getKeywords()
  if not keywords:
    print "No keywords founds"
    return
  payload = "s" + struct.pack("I", listLen) + keywords
  return payload

print "Connecting..."
if os.path.exists( "/tmp/hostSocket" ):
  client = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
  client.connect( "/tmp/hostSocket" )
  print "Ready."
  printHelp()
  while True:
    try:
      x = raw_input( "> " )
      x = x.strip()
      if "" != x:
        if "help" == x:
          printHelp()
          continue
        if "publish" == x:
          data = publish()
          client.send(data)
          continue
        if "subscribe" == x:
          data = subscribe()
          client.send(data)
          continue
        if "send data" == x:
          data = sendData()
          client.send(data)
          continue
        client.send( x ) 
        if "quit" == x:
          print "Shutting down."
          break
        data = client.recv(2048);
        if x == "show publish keyword list":
          print "Publishing data for: \n" + data
        elif x == "show subscribe keyword list":
          print "Subscribed for data: \n" + data
        else:
          print "Invalid command";
    except KeyboardInterrupt, k:
      print "Shutting down."
  client.close()
else:
  print "Couldn't Connect!"
  exit(1)
print "Done"
