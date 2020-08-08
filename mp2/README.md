# MP2
# Please write down each person's responsibility in this group for this MP.
#
# General Overhead: cdgerth2
# Implement LS Routing: cdgerth2
# Implement VEC Routing: alfredo7
#
In this assignment, you will implement traditional shortest-path routing with the link state (LS) and/or the
distance/path vector (DV/PV) protocols. Your program will be run on every node in an unknown network topology
to perform routing tasks to deliver the test messages instructed by our manager program. The implementation you
need to submit depends on whether you work alone or as a group of two:

- If working alone:
You can choose one of LS, DV, or PV to implement.

- If working as a group of 2:

Your group must do both LS and DV/PV (choose one of DV or PV). The administrative, non-algorithmlogic components of LS and DV/PV are identical, so a pair can do those together. For the algorithms
themselves, we recommend that you each take one.
Besides the implementation of the routing protocols, you will have to deal with the unfortunate distinction between
“network order” and “host order”, AKA endianness. The logical way to interpret 32 bits as an integer is to call the
left-most bit the highest, and the right-most the ones place. This is the standard that programs sending raw binary
integers over the internet are expected to follow. Due to historical quirks, however, x86 processors store integers
reversed at the byte level. Look up htons, htonl, ntohs, ntohl() for information on how to deal with it.

## Test Environment 

### Network Configurations

In this MP, everything will be contained within a single VM. The test networks (i.e., nodes and network
topologies) are constructed by using virtual network interfaces (created by ifconfig) and configuring firewall
rules (configured by iptables). Your program will act as a node in the constructed network to perform routing
tasks. That is, if there are N nodes in the test network, there will be N instances of your programs being started, each
of which runs as one of the nodes with an ID that is associated to an IP address (see below for details).
We are providing you with the same Perl script (make_topology.pl) that the auto-grader uses to establish these
virtual network interfaces and firewall rules. The manager program (manager_send) used by the auto-grader to
send the test messages is also provided (see next Section). They are available at:

https://courses.engr.illinois.edu/cs438/sp2020/mp/mp2-supplement.zip

What follows summarizes the network configurations used in this MP:

• The topology's connectivity is defined in a file that gets read by the provided Perl script
(make_topology.pl) to create the network environment.

• The initial link costs between nodes are defined in files that tell nodes (your programs) what their initial link
costs to other nodes are (if they are in fact neighbors) when the nodes are being run. See Section 3 for details.

• For each node, a virtual network interface (eth0:1, eth0:2, etc) will be created and given an IP address
(10.1.1.X).

• A node with ID X gets the IP address 10.1.1.X where 0 ≤ X ≤ 255.

• Your program will be given its ID on the command line, and when binding a socket to receive UDP packets,
it should specify the corresponding IP address (rather than INADDR_ANY / NULL).

• To construct a test network topology, iptables rules will be applied to restrict which of these addresses
can talk to which others. For instance, 10.1.1.30 and 10.1.1.0 can talk to each other if and only if
they are neighbors in the test network topology.

• A node's only way to determine who its neighbors are is to see who it can directly communicate with. Nodes
do not get to see the aforementioned connectivity file.

• The links between nodes can change during the test. This is done by setting new iptables rules
during the test.

• The link costs between nodes can change during the test. This is done by letting the Manager send a cost
update packet to your node (see next Section).

##  The Manager 

While running, your nodes will receive instructions and updated information from a manager program. You are not
responsible for submitting an implementation of this manager. As mentioned above, the manager program
(manager_send) used by the auto-grader is provided to you. Your interaction with the manager is very simple:

The manager sends messages in individual UDP packets to your nodes on UDP port 7777.
Your nodes execute the instructions upon receiving the manager’s packets.
Your nodes do not need to reply to the manager in any way.

The manager's packets have two types of instructions: (1) sending a message data packet and (2) updating neighbor
link cost. 
Their packet formats are explained next.
### Sending A Message Data Packet

4 bytes 2 bytes ≤ 100 bytes

“send”

(in ASCII code)

destID

(signed 16-bit int, network order)

msg

(some ASCII text)

This message instructs the recipient to send a data packet, containing the given message data msg, to the node with
the ID destID. The message data is guaranteed to be an ASCII string. It does not come with a null-terminator in
the packet (it would be wasteful if it does).
Note that this is the packet format you’ll receive from the Manager. To deliver the given message to the destination
node, you don’t have to follow the same packet format. Your packet can have whatever format that makes the most
sense to you. However, you must follow the requirements stated in Section 3.3 to produce log files to pass test
cases.

### Updating Neighbor Link Cost

4 bytes 2 bytes 4 bytes

“cost”

(in ASCII code)

neighborID

(signed 16-bit int, net order)

newCost

(signed 32-bit int, net. order)

This message tells the recipient that its link to node neighborID is now considered to have cost newCost. This
message is valid even if the link in question did not previously exist, although in that case do NOT assume the link
now exists: rather, the next time you see that link online, this will be its new cost. 

## The Assignment – Your Nodes

### The Basics

Whether you are writing an LS or DV/PV node, your node's command line interface to the assignment environment
is the same. Your node should run like:

./ls_router <nodeid> <initialcostsfile> <logfile>
./vec_router <nodeid> <initialcostsfile> <logfile>

For examples:

./ls_router 5 node5costs logout5.txt
./vec_router 0 costs0.txt test3log0

The 1st parameter indicates that this node should use the IP address 10.1.1.<nodeid>.
The 2nd parameter points to a file containing initial link costs related to this node (see Section 3.2).
The 3rd parameter points to a file where your program should write the required log messages to (see Section 3.3).

### Initial Link Costs File Format

The format of the initial link costs file is defined as follows:

<nodeid> <nodecost>

<nodeid> <nodecost>

...
An example of the initial link costs file:

5 23453245

2 1

3 23

19 1919

200 23555

In this example, if this file was given to node 6 (10.1.1.6), then the link between nodes 5 (10.1.1.5) and
6 has cost 23453245 – so long as that link is up in the physical topology.
If you don't find an entry for a node, default to cost 1. We will only use positive costs – never 0 or negative. We will
not try to break your program with malformed inputs. A link's cost will always be < 223.

The link costs that your node receives from the input file and the manager don't tell your node whether those links
actually currently exist, just what they would cost if they did. In other words, just because this file contains a line for
node X, it does NOT imply that your node will be neighbors with node X.
Your node therefore needs to constantly monitor which other nodes it is able to send/receive UDP packets directly
to/from. We have provided code (partial) that you can use for this (see monitor_neighbors.c).

### Log Files

When originating, forwarding, or receiving a data packet, your node should log the event to its log file. The sender
of a packet should also log when it learns that the packet was undeliverable. There are four types of log messages
your program should be logging:

forward packet dest [nodeid] nexthop [nodeid] message [text text]

sending packet dest [nodeid] nexthop [nodeid] message [text text]

receive packet message [text text text]

unreachable dest [nodeid]

For example:

forward packet dest 56 nexthop 11 message Message1

receive packet message Message2!

sending packet dest 11 nexthop 11 message hello there!

unreachable dest 12

forward packet dest 23 nexthop 11 message Message4

forward packet dest 56 nexthop 11 message Message5

In this example, the node forwarded a message bound for node 56, received a message for itself, originated packets
for nodes 11 and 12 (but realized it couldn't reach 12), then forwarded another two packets.
To output the log messages correctly, it is recommended to use sprint() to from the log string. Sample code that
uses sprint() to output correct log messages is provided in the extra_note.txt file in mp2-
supplement.zip. 
Our tests will have data packets be sent relatively far apart, so don't worry about ordering.

### Other Implementation Requirements

Your nodes should accomplish:

• Using LS or DV/PV, maintain a correct forwarding table.

• Forward any data packets that come along according to the forwarding table.

• React to changes in the topology (changes in cost and/or connectivity).

• Your nodes should converge within 5 seconds of the most recent change.

#### Partition:

The network might become partitioned, and your protocols should react correctly: when a node is asked to originate
a packet towards a destination it does not know a path for, it should drop the packet, and rather than log a send event,
log an unreachable event.

#### Tie breaking:

We would like everyone to have consistent output even on complex topologies, so we ask you to follow specific tiebreaking rules.

• DV/PV: when two equally good paths are available, your node should choose the one whose next-hop node
ID is lower.

• LS: when choosing which node to move to the finished set next, if there is a tie, choose the lowest node ID.

If a current-best-known path and newly found path are equal in cost, choose the path whose last node
before the destination has the smaller ID. Example:

Source is 1, and the current-best-known path to 9 is 1→4→12→9.

We are currently adding node 10 to the finished set.

1→2→66→34→5→10→9 costs the same as path 1→4→12→9.

We will switch to the new path, since 10<12.
