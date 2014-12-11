#!/usr/bin/env python
# -*- coding: utf-8 -*-
import signal, sys
from random import choice
from string import lowercase
import math
import socket
import struct
import os, os.path

publishUidList = []
subsList = []
client = None

def signal_handler(signal, frame):
  print "Shutting down."
  try:
    client.send("quit")
    client.close()
  except:
    print "Host down"
  sys.exit(0)

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
  print "\tshow packet received counter"
  print "\tclear packet received counter"
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
        #strKeywords += ';'
        return strKeywords, len(strKeywords)
      keywords.append(x)
      count = count + 1
      continue
    except KeyboardInterrupt, k:
      return 

def dataEntry():
  try:
    x = raw_input("Enter Data > " )
    x = x.strip()
    return x, len(x)
  except KeyboardInterrupt, k:
    return

def getSize(dataLen):
    x = raw_input("Amount of data> " )
    x = x.strip()
    data = x.split()
    size = int(data[0])
    if len(data) == 2:
      unit = data[1]
    else:
      unit = ''
    if unit == 'Mb' or unit == 'mb':
      totalSize = size * 1024 * 1024
    elif unit == 'Gb' or unit == 'gb':
      totalSize = size * 1024 * 1024 * 1024
    elif unit == 'Kb' or unit == 'kb':
      totalSize = size * 1024
    else:
      # consider size in bytes
      totalSize = size
    #find the number of packets
    count = totalSize / dataLen
    count = int(math.floor(count))
    lastSize = int(totalSize - (dataLen * count))
    return count, lastSize

def generateData():
  n = 1470 - 14
  data = "".join(choice(lowercase) for i in range(n))
  return data, len(data)

def sendData(client):
  global publishUidList
  publishingList(client)
  print publishUidList
  x = raw_input( "\nEnter Unique Id to publish: " )
  x = x.strip()
  if "" != x:
    if x not in publishUidList:
      print "Invalid Unique Id"
    else:
        data, dataLen = generateData()
        count, lastSize = getSize(dataLen)
        print count, lastSize
        packet = "f" + struct.pack("I", count) + struct.pack("I", lastSize) + struct.pack("I", int(x)) + data
        client.send(packet)
  else:
    print "Invalid Unique Id"

def subscribeList(client):
  global subsList
  subsList = []
  client.send("show subscribe keyword list")
  data = client.recv(1024)
  if data == "0":
    print "No keywords subscribed"
    return
  print "List is:"
  tmp = 0
  count = int(data)
  while tmp < count:
    data = client.recv(1024)
    idx = data.rfind(';')
    subsList.append(str(data[:idx]))
    print data[:idx]
    tmp = tmp + 1

def publishingList(client):
  global publishUidList
  publishUidList = []
  client.send("show publish keyword list")
  data = client.recv(1500)
  if data == "0":
    print "Not publishing any keywords"
    return
  print "List is:"
  tmp = 0
  count = int(data)
  while tmp < count:
    data = client.recv(1500)
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
      payload = payload.strip()
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

def unsubscribe(client):
  global subsList
  subscribeList(client)
  keywords, listLen = getKeywords()
  if "" == keywords:
    print "No keywords founds"
    return
  temp = keywords
  if temp not in subsList:
    print "Invalid Unique Id"
  else:
    payload = "d" + struct.pack("I", listLen) + keywords
    client.send(payload)

def subscribe(client):
  keywords, listLen = getKeywords()
  if not keywords:
    print "No keywords founds"
    return
  payload = "s" + struct.pack("I", listLen) + keywords
  client.send(payload)

print "Connecting..."
if os.path.exists( "/tmp/hostSocket" ):
  client = socket.socket( socket.AF_UNIX, socket.SOCK_STREAM )
  client.connect( "/tmp/hostSocket" )
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
        elif "publish" == x:
          publish(client)
          continue
        elif "unpublish" == x:
          unpublish(client)
          continue
        elif "show publish keyword list" == x:
          publishingList(client)
          continue
        elif "show subscribe keyword list" == x:
          subscribeList(client)
          continue
        elif "subscribe" == x:
          subscribe(client)
          continue
        elif "show packet received counter" == x:
          client.send(x)
          data = client.recv(1500)
          print data
          continue
        elif "clear packet received counter" == x:
          client.send(x)
          continue
        elif "unsubscribe" == x:
          unsubscribe(client)
          continue
        elif "send data" == x:
          sendData(client)
          continue
        elif "quit" == x:
          print "Shutting down."
          client.send(x)
          break
        else:
          print "Invalid Command"
          continue
        data = client.recv(1500);
    except KeyboardInterrupt, k:
      print "Shutting down."
  client.close()
else:
  print "Couldn't Connect!"
  exit(1)
print "Done"
