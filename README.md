sCCN
====

Adhoc Content Centric Networking using SDN

Execution
=========

Boost Installation
------------------
http://www.linuxfromscratch.org/blfs/view/svn/general/boost.html

Install
-------
1) Boost:<br>
sudo apt-get install libboost-all-dev<br><br>

2) G++<br>
sudo apt-get install g++-4.8<br><br>

3) PF_RING: <br>
- sudo apt-get install flex<br>
- sudo apt-get install bison<br>
- sudo apt-get install libnuma-dev<br>
- sudo apt-get install subversion<br>
- sudo svn co https://svn.ntop.org/svn/ntop/trunk/PF_RING/ /usr/local/PF_RING<br>
- cd /usr/local/PF_RING/kernel<br>
- sudo make<br>
- sudo insmod /usr/local/PF_RING/kernel/pf_ring.ko<br>
- cd /usr/local/PF_RING/userland<br>
- sudo make<br>


Running
-------

1) Controller: <br>
sudo bin/sccn 1 Controller dbg<br><br>
2) Switch: <br>
sudo bin/sccn 2 Switch dbg<br><br>
2) Host: <br>
sudo bin/sccn 3 Host dbg<br><br>
3) Switch CLI interface: (On switch node)<br>
sudo python switchInterface.py<br>

Proposal
========

Abstract:
--------
Networks today transmit a lot of information. The transmission of information 
from one host to another host in the network contains a lot of overhead. The 
overhead is even larger if multiple receivers are interested in the same 
information. Hence, the networks are moving toward being content centric rather
than being driven by data. Hence, we propose a network which would diverge from 
traditional TCP/IP routing and be more content centric. We design a new network 
level protocol based on content centric routing of information using concepts of publisher-subscriber model. The protocol uses a Software-Defined network style approach where a centralized keyword-mapping server helps to route data over the network to the hosts which have subscribed for the same by installing rules in the switches along the path from the Publisher to the Subscriber.

Introduction:
------------
In the 80's when the networking was in its nascent stages, the goal of 
connecting different types of networks and resource sharing resulted in the 
emergence of packet-switched networks. Ever since then, the networking industry was driven towards the naming of hosts, services and applications. This served the networking industry for a long time. The initial network design had different goals based on the DARPA but the explosion of networks resulted in the goals not holding significance in the world today. Security and management were not given proper importance, but they are a major problem in the networks today. Similarly, the networks were driven mostly by the host names (IP addresses), but today the paradigm of transmitting relevant content to interested hosts is followed. Thus, the networking should no longer be driven by the host names and rather be content-centric, as also proposed by various other researchers. With the evolution of Software-Defined networking, a centralized controller now monitors the routing of data in the network. Thus networks today can now easily be moved from being host name centric to content centric with high performance and better utilization. Thus, we propose a new network protocol and routing technique which is not based on source and destination addresses, but based on the content being transmitted. 

In this solution, we use a publisher-subscriber model where each host can be a subscriber as well as a publisher of content. The host sends a registration request with the keywords for the content to the controller. The controller then registers and creates a unique identifier mapped to a particular keyword which is sent back to the host in the "Registration Successful" message. The hosts who want the information send a subscription request with the keywords they are interested in. The controller then installs the rules in the switches along the path from the publisher to the subscriber based on the unique identifier. In this solution, we propose a stripped version of the networking headers currently used in the network, to reduce the overhead and achieve high throughput. 

In this paper we define our Content Centric Network using a Software Defined Networking approach. There have been some researches in the field of content centric networking and we define them in the related works (Section 2) of the paper. We show how our Content-Centric Network and custom network protocol works (Section 3). In Section 4, we describe the architecture and Section 5 talks about the contributions of the paper. We present a detailed implementation of the working of the system in Section 6. Later, we define the timeline (Section 7) and the evaluation mechanism we would put to benchmark our system (Section 8).

Related Work:
------------
[1] encourages the deployment of OpenFlow switches at schools. The internal workings of products can be hidden from the user and a generalized version can be implemented independent of the vendor. Since an open protocol is provided with the liberty of controlling their own flows, the idea has been leveraged to use a custom protocol. Some interesting concepts that we can use are also introduced, like that of sophisticated controllers that dynamically add/remove flows, multiple users running independent experiments on different flows and so on. Content-centric networking is best explained in [2]. Well-supported arguments have been made about named data being a better abstraction than named hosts. The main advantage of CCN is that it can be layered over anything, even IP. It has also been simplified to a great extent by having two packet types - Interest and Data. A prototype CCN network stack was implemented and its use has been demonstrated. It was made available in [7].

Similar work has been done in [6]. The focus was on non-forwarding elements like proxy and cache which can be programmed like switches and routers. The caching network routes content to a cache by modifying flow definitions at intermediate switches. In [5], an architecture was proposed to support data driven services in an Information-Centric network that would result in better utilization of network resources. It can be used to solve the traditional problems of traffic engineering by switching to ICN, a content firewall service and perform network-wide cache management. This has been built on an SDN platform. Involvement of application layer has been eliminated. Reference [3] combines many of these concepts and performs certain experiments on the OFELIA testbed. OpenFlow protocol has been honed to suit ICN needs. However, no ICN protocol is used in their setup. Clients and servers only use HTTP.


Description:
-----------

A ----- 1 ----- 2 ----- 3 ----- B<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;C&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;E&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;F<br>

Figure-1

In the above Figure-1, the Host-A sends a registration request with the keyword "Politics" to the Switch-1. Once the Switch-1 receives the request, it sends that request to the controller, which generates, maps and stores a unique identifier corresponding to that keyword. Once the registration is completed the Host-A is notified by the controller with the identifier.

Now the Host-E and Host-F send a subscription request for "Politics" to the Switch-2 and Switch-3 respectively. The switches then forward these requests to the centralized controller. On receiving the subscription request, the controller validates the request by comparing the keyword provided with the available keywords in database. If the keyword is found, the controller instructs and installs new rules in Switch-1, Switch-2 and Switch-3 to forward the incoming packets with that unique identifier for "Politics" towards Host-E and Host-F. Once the rules are installed, then the controller sends a "Subscription Confirmation" message to the Host-E and Host-F with the unique identifier for that keyword via the switch. Rule installation and Packet forwarding is dictated by the keywords to which hosts are subscribed. Hosts also use these identifiers to distinguish between different subscription requests.

Now if another Host-C registers as a source of information with the keyword "Sports", the controller follows the same procedure and generates a unique identifier for the keyword and stores in the mapping database. Then the controller sends a registration response with the identifier to the Host-C. And if Host-E, Host-F, Host-B and Host-A all are interested in receiving the updates for "Sports", they all send a subscription request to the controller. The controller once again validates the requests and installs the rules in the corresponding switches. Once the rule installation is completed the controller then responds to the hosts with the unique identifier via the switch. Now the packets are routed as per the rules installed by the controller on the basis of content subscribed. All the subscribers receive the data and using the unique identifier, the hosts differentiate using those unique identifiers.

Architecture:
------------

We intend to develop our own custom controller and custom switches as per SDN standards using either C or C++ 11 standards. On end-hosts we will use either C or C++ 11 to develop mechanisms for registering, un-registering a service and receiving the packets for that service.

For realizing our implementation, we would at-least need the following hardware resources:
1. 12 dedicated systems, which will have one controller, 4 switches and 7 end-hosts.
2. 100Mbps links between the nodes. 

Contributions:
-------------

The main contributions of our approach will be the inclusion of a Central Controller in the Content-Centric Networking realm. A Content-Centric network without a centralized controller has some inherent issues as identified in the paper [8]. The first challenge is the routing of information to all the nodes interested in the particular information. The second challenge in such a network is the policy disagreements within parts of the networks. Thirdly, the accountability of such a network is another issue due to no centralized control and distributed processing. Fourthly, the issue of attention scarcity as the nodes have no control over the amount of data that is routed to it can overwhelm a small device and lead to an unstable network. Finally, a major challenge as mentioned in [3], scalability, which becomes an issue as the number of keywords is expected to be orders of magnitude larger than the number of hosts. In CCN, performing content-based routing in each node in the path between source and destination could be extremely costly in terms of processing, which results in increased delay and reduced throughput.

The design proposed by our paper addresses these issues in the Content-Centric Network as the controller in command is responsible and has control over the entire network and how the data flows through the network. Furthermore, our SDN content-based publisher/subscription model reduces the administrative overhead of defining and maintaining a large number of groups, thereby making the system easier to manage.

Proposed Implementation:
-----------------------

Our implementation of a Content-Centric Routing involes a Centralized Controller which is responsible for the registration and control of the network. As the network comes up, the switches register with the controller and the controller generates a unique identifier for each of the switches. At this point the switches don't have any rules installed hence all the flows are directed to the controller. The controller then tells the switch according to the policies as to what is to be done with the switch. The communication within our system can be divided into 3 phases: (1) Keyword Registration, (2) Subscription, (3) Data transfer, (4) Unsubscription and (5) Keyword Deregistration. 

Packet Types:
------------
|-------------------------------------------|<br>
|      Packet Type   |      Description     |<br> 
|-------------------------------------------|<br>
| REGISTRATION_REQ   | Registration Request |<br>
| REGISTRATION_SUC   | Registration Success |<br>
| SUBSCRIPTION_REQ   | Subscription Request |<br>
| SUBSCRIPTION_SUC   | Subscription Success |<br>
| DATA_PACKET        | Data Packet          |<br>
| DEREGISTRATION_REQ | Registration Request |<br>
| DEREGISTRATION_SUC | Registration Success |<br>
| DESUBSCRIPTION_REQ | Subscription Request |<br>
| DESUBSCRIPTION_SUC | Subscription Success |<br>
|-------------------------------------------|<br>

|---------------------------------------------------------------------|<br>
| Packet Type | Sequence Number | Host Identifier | Length | Keywords |<br>
|---------------------------------------------------------------------|<br>
Registration Request Packet

|---------------------------------------------------|<br>
| Packet Type | Sequence Number | Unique Identifier |<br>
|---------------------------------------------------|<br>
Registration Success Packet

|---------------------------------------------------------------------|<br>
| Packet Type | Sequence Number | Host Identifier | Length | Keywords |<br>
|---------------------------------------------------------------------|<br>
Subscription Request Packet

|---------------------------------------------------|<br>
| Packet Type | Sequence Number | Unique Identifier |<br>
|---------------------------------------------------|<br>
Subscription Success Packet

|------------------------------------------------------------|<br>
| Packet Type | Unique Identifier | Payload Length | Payload |<br>
|------------------------------------------------------------|<br>
Data Packet

|---------------------------------------------------------------------|<br>
| Packet Type | Sequence Number | Host Identifier | Length | Keywords |<br>
|---------------------------------------------------------------------|<br>
Desubscription Request Packet

|---------------------------------------------------|<br>
| Packet Type | Sequence Number | Unique Identifier |<br>
|---------------------------------------------------|<br>
Desubscription Success Packet

|---------------------------------------------------------------------|<br>
| Packet Type | Sequence Number | Host Identifier | Length | Keywords |<br>
|---------------------------------------------------------------------|<br>
Deregistration Request Packet

|---------------------------------------------------|<br>
| Packet Type | Sequence Number | Unique Identifier |<br>
|---------------------------------------------------|<br>
Deregistration Success Packet

In the first phase, the host creates a registration request with the keywords for the data it is registering for along with a host identifier as defined in the figure above. This request is sent to the switch which the host is connected to. The switch then looks at the type of packet and if the packet type is a registration request packet it forwards it to the connected controller. The controller looks at the Keywords and then creates a unique identfier for the set of keywords and then creates a mapping of the unique identifier with the host Identifier to know what all hosts are the publishers of the information. If the host entry already exists for the keywords, the request is discarded by the controller. Or else, the controller responds back to the host with the registration success message as in the figure above. 

In the second phase, the other host interested in some information sends a subscription request for a set of keywords. The switch then checks the packet type, if the packet type is a subsciption request the switch forwards the packet to the controller. The controller then checks the keyword list to find the set of keywords the host is interested in. If the controller does not have an entry for the same the controller sends a subscription unsuccessful message to the host or else the controller computes the route that the data has to take to reach from the various publishers to the interested hosts and installs the same on the switches. Once the rule has been installed on the switches along the path, the controller sends a success packet to the host with the unique identifier for the keywords. The host on getting the unique identifier stores it in a table which is used for multiplexing the data amongst various applications. 

Once the registation has succeeded, the publisher publishes its data and the switches in the network forward the packet as per the rules installed by the controller. The data packets are in the form as shown in the figure above. They contain the unique identifier based on which the data is forwarded by the switches and has the packet type set as PACKET_DATA. The length of the payload and the actual payload follows the header. 

In the fourth phase, the host sends a request to the controller to deregister for a particular keyword and the contorller then uninstalls the rules on the switches and once all the rules have been uninstalled, the controller sends a success packet back to the host. Finally, When the publisher has no more data to publish, the publisher sends the keyword deregistration request to the switch in the packet format as shown in the figure above. This packet is then forwarded to the controller. On receiving the message the controller checks for the mapping in its table and if it exists the controller then removes the mapping and all the associated entries. The controller then uninstalls the rules from the switches as per required. Then the controller sends a keyword deregistration success message back to the host. 

Evaluation:
----------
We plan to benchmark our system on the Performance and Network Utilization metrics. We will simulate multiple subscribers and sources of information and see how the benchmark metrics change with the scale of the network. Since the Content-Centric Networking is a new approach that came with the evolution of Software-Defined Networking, there are not many open-source or available solutions which we can benchmark our solution against. We will still benchmark our system against a Van Jacobson style CCN network as proposed in the related works and a broadcast approach for the same. As we move forward with the project we will try and find more possible solutions with which we can benchmark our systems.  

Conclusion:
----------
As discussed earlier, the networking architecture as it evolved was driven by the host-to-host connectivity. With the network being more used for content distribution and not based on the initial design goals as set by DARPA, we need to diverge from the host-to-host connectivity centric network architecture towards a Content-Centric network architecture. Thus, the design described in the paper decouples the content from the location where it resides in the network. This results in a higher network utilization and proper network management with the help of a centralized controller as proposed in the design. 

The designs centers around placement of content at a centralize location with the hosts being unaware of the location of the content. The hosts will only need to specify the content they are interested in with the help of keywords and the controller would be able to route the requested content to the host. This design not only reduces the network load, but also helps address manageability issues and increases the performance by decreasing the processing required in the network. 

The content specific routing FPGA logics as proposed by many of the papers will play a very important role as they can help in routing at line rate. This design can be easily integrated with the current SDN technologies, which are prevalent including OpenFlow. 

References:
----------

[1] N. McKeown, T. Anderson, H. Balakrishnan, G. Parulkar, L. Peterson, J. Rexford, S. Shenker, and J. Turner, “Openflow: enabling innovation in campus networks,” in SIGCOMM Comput. Commun. Rev., 2008.

[2] Jacobson, Van, Diana K. Smetters, James D. Thornton, Michael F. Plass, Nicholas H. Briggs, and Rebecca L. Braynard. "Networking named content." In Proceedings of the 5th international conference on Emerging networking experiments and technologies, pp. 1-12. ACM, 2009.ch

[3] Salsano, Stefano, Nicola Blefari-Melazzi, Andrea Detti, Giacomo Morabito, and Luca Veltri. "Information centric networking over SDN and OpenFlow: Architectural aspects and experiments on the OFELIA testbed." Computer Networks 57, no. 16 (2013): 3207-3221.

[4] A. Chanda and C. Westphal. "Content as a Network Primitive". In arXiv:1212.3341[cs.NI].

[5] A. Chanda, C. Westphal, and D. Raychaudhuri. "Content based traffic engineering in software defined information centric networks". NOMEN'13. IEEE, 2013.

[6] Chanda, Abhishek, and Cedric Westphal. "A content management layer for software-defined information centric networks." Proceedings of the 3rd ACM SIGCOMM workshop on Information-centric networking. ACM, 2013.

[7] Project CCNx. http://www.ccnx.org, Sep. 2009.

[8] Trossen, Dirk, Mikko Sarela, and Karen Sollins. "Arguments for an information-centric internetworking architecture." ACM SIGCOMM Computer Communication Review 40.2 (2010): 26-33.

[9] Banavar, Guruduth, Tushar Chandra, Bodhi Mukherjee, Jay Nagarajarao, Robert E. Strom, and Daniel C. Sturman. "An efficient multicast protocol for content-based publish-subscribe systems." In Distributed Computing Systems, 1999. Proceedings. 19th IEEE International Conference on, pp. 262-272. IEEE, 1999.
