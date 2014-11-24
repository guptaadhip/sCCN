# -*- coding: utf-8 -*-
import socket
import struct
import os, os.path

publishUidList = []

def printHelp():
  print "Commands:"
  print "\tquit"
  print "\thelp"
  print "\tsubscribe"
  print "\tunsubscribe"
  print "\tpublish"
  print "\tunpublish"
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
        keywords.sort()
        strKeywords = ';'.join(keywords)
        strKeywords += ';'
        return strKeywords, len(strKeywords)
      keywords.append(x)
      count = count + 1
      continue
    except KeyboardInterrupt, k:
      return 

def publishingList(client):
  global publishUidList
  publishUidList = []
  client.send("show publish keyword list")
  data = client.recv(1024)
  if data == "0":
    print "Not publishing any keywords"
    return
  print "List is:"
  tmp = 0
  count = int(data)
  while tmp < count:
    data = client.recv(1024)
    idx = data.rfind(';')
    publishUidList.append(str(data[idx + 1:]))
    print data[:idx] + "\t" + data[idx + 1:]
    tmp = tmp + 1

def unpublish(client):
  global publishUidList
  publishingList(client)
  print publishUidList
  x = raw_input( "\nEnter Unique Id: " )
  x = x.strip()
  if "" != x:
    if x not in publishUidList:
      print "Invalid Unique Id"
    else:
      payload = "u" + struct.pack("I", int(x))
      client.send(payload)
  else:
    print "Invalid Unique Id"

def publish(client):
  keywords, listLen = getKeywords()
  if not keywords:
    print "No keywords founds"
    return
  payload = "p" + struct.pack("I", listLen) + keywords
  client.send(payload)

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
        elif "publish" == x:
          publish(client)
          continue
        elif "unpublish" == x:
          unpublish(client)
          continue
        elif "show publish keyword list" == x:
          publishingList(client)
          continue
        elif "subscribe" == x:
          data = subscribe()
          client.send(data)
          continue
        elif "send data" == x:
          data = sendData()
          client.send(data)
          continue
        elif "quit" == x:
          print "Shutting down."
          client.send(x)
          break
        else:
          print "Invalid Command"
          continue
        data = client.recv(1024);
        if x == "show subscribe keyword list":
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
