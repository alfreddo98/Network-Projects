# MP3

When testing the file transmission in the VMs introduced above, the network performance will be unrealistically
good (and the same goes for testing on the localhost), so the test environment has to be restrained by using the tc
command (you only need to run this command in the Sender VM). An example is given as follows:
Assuming the Sender VM’s network interface that connects to the Receiver VM is eth0, run the following
commands in the Sender VM: 

sudo tc qdisc del dev eth0 root 2>/dev/null
sudo tc qdisc add dev eth0 root handle 1:0 netem delay 20ms loss 5%
sudo tc qdisc add dev eth0 parent 1:1 handle 10: tbf rate 100Mbit burst
40mb latency 25ms

The first command will delete any existing tc rules.
The second and the third commands will give you a 100 Mbit, ~20 ms RTT link where every packet sent has a 5%
chance to get dropped (remove “loss 5%” part if you want to test your programs without artificial drops.) Note
that it’s also possible to specify the chance of reordering by adding “reorder 25%” after the packet loss rate
argument.

## The Assignment – The Sender and Receiver Commands

###  The Basics

Your code should compile to two executables named reliable_sender and reliable_receiver that
support the following usage:

./reliable_sender <rcv_hostname> <rcv_port> <file_path_to_read> <bytes_to_send>
./reliable_receiver <rcv_port> <file_path_to_write>

The <rcv_hostname> argument is the IP address where the file should be transferred.
The <rcv_port> argument is the UDP port that the receiver listens.
The <file_path_to_read> argument is the path for the binary file to be transferred.
The <bytes_to_send> argument indicates the number of bytes from the specified file to be sent to the receiver.
The <file_path_to_write> argument is the file path to store the received data.
For example:

First, on the Receiver VM, run:

./reliable_receiver 2020 /path/to/rcv_file

Then, on the Sender VM, run:

./reliable_sender 192.168.4.38 2020 /path/to/readonly_file.dat 1000

This allows the sender command to connect to the receiver hosted at 192.168.4.38 with an UDP port 2020
and transfer the first 1000 bytes in the file named readonly_file.dat to the receiver. Upon receiving the
data (1000 bytes in this case), the receiver stores it in the file named rcv_file.

### Transmission Function Templates
You are free to write your own functions to implement the commands required in this MP. You can also start with
the files we provide: a file named sender_main.c and a file named receiver_main.c which declare the
sender’s and receiver’s functions. They are available at:

https://courses.engr.illinois.edu/cs438/sp2020/mp/mp3-supplement.zip

#### The Sender’s Function:

void reliablyTransfer(char* hostname, unsigned short int hostUDPport,
char* filename, unsigned long long int bytesToTransfer)

This function should transfer the first bytesToTransfer bytes of filename to the receiver at
hostname:hostUDPport correctly and efficiently, even if the network drops and reorders some of your packets.

#### The Receiver’s Function:

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) 

This function is reliablyTransfer’s counterpart and should write whatever it receives to a file called
destinationFile. 

### Implementation Requirements

Your job is to implement the sender and receiver’s functions/commands, with the following requirements:

• The data written to disk by the receiver must be exactly what the sender was given.

• Two instances of your protocol competing with each other must converge to roughly fair sharing the link
(same throughputs +/- 10%) within 100 RTTs. The two instances might not be started at the exact same
time.

• Your protocol must be somewhat TCP friendly: an instance of TCP competing with you must get on
average at least half as much throughput as your flow. (Hint: The auto-grader uses the command iperf to
create the competing TCP flow.)

• An instance of your protocol competing with TCP must get on average at least half as much throughput as
the TCP flow.

• All of the above should hold in the presence of any amount of dropped or reordered packets. All flows,
including the TCP flows, will see the same rate of drops/reorderings. The network will not introduce bit
errors.

• Your protocol must get reasonably good performance when there is no competing traffic and no packets are
intentionally dropped or reordered. Aim for at least 33 Mbps on a 100 Mbps link.

• You cannot use TCP in any way. Use SOCK_DGRAM (UDP), not SOCK_STREAM (TCP). The test
environment has a ≤ 100 Mbps connection and an unspecified RTT -- meaning you’ll need to estimate the
RTT for timeout purposes, like real TCP.

• The sender and receiver’s commands/programs must end once the file transmission is done.

### Extra Hints

The MTU on the test network is 1500, so up to 1472 bytes payload (IPv4 header is 20 bytes, UDP is 8
bytes) won’t get fragmented. You can use sendto() to send larger packets and the UDP socket library
will handle the fragmentation/reassembly for you. It’s up to you to reason out the benefits and drawbacks
of using large UDP packets in various settings.

• Be sure that you have a clean design for implementing the send/receive buffers. Trying to figure out which
part of the data to resend won’t be fun if your sender’s window buffer doesn’t have a nice clean interface.

• Input files on the auto-grader are READ-ONLY. In your program, please open the files with read mode
only. These files contain binary data, NOT text.

