# Project 2: Reliable File Transfer Protocol

> Yusi Qi (yq22@rice.edu)
>
> Yangfenghui Huang (yh97@rice.edu)
>
> Rui Xiao (rx12@rice.edu)
>
> Xinyu Zhao (xz90@rice.edu)

This project is the course project 2 for COMP 556 @ CS.Rice University



## Brief

We have implemented a modified stop and wait protocol to provide reliable file transfer ability. It could generally transfer a specific file of at least 30MB in size through a unreliable network environment with guaranteed consistence of file content.

## How to Compile

We have prepared a Makefile for you to easily compile the code. All you need to do is to run the following one line command:

```shell
make
```

If you want to re-compile the code, please remember to clean all the previously compiled files prior to compiling them again:

```shell
make clean
```

## How to Run

### Start Receiver

The first step is to start he receiver listening to a specific port waiting for the connection from a sender. The default port range on clear server is set to be `18000~18200`. Other port values out of this range is valid but would invoke a warning.

You could start the receiver by running the following command:

```shell
./recvfile -p <recv_port>
```

### Start Sender

After starting the receiver, you could start the sender with a file to be transmitted. You could simply start the sending process with the following command:

```shell
./sendfile -r <recv_host>:<recv_port> -f <subdir/filename>
```

**Attention**: the subdir folder should be within the current directory where the program is running and the current user should have permission to access.

## What We Designed

### Modified Stop and Wait Protocol

Our implementation is based on a modified stop and wait protocol. In the original stop and wait protocol, the sender would send the packet continously until the it has received the ACK with the exact sequence number from the receiver. And then it will continue to start tring to send the next packet with `sequence_number = 1 - previous_sequence_number`. We have the following formula for the process of sequence number:
$$
SeqenceNumber(i+1)=1-SequenceNumber(i)
$$
This means the sequence number of packet with index $i-2$ will be the same to the sequence number of packet with index $i$, which could be illustrated as:
$$
\begin{aligned}
SequenceNumber(i)&=1-SequenceNumber(i-1) \\
&=1-(1-SequenceNumber(i-2)) \\
&=SequenceNumber(i-2)
\end{aligned}
$$
This would not be fault when the network situation is not as bad as we could imagine. But when the delay of packet transmission is popular and varies in time on the network, there could be possibly a packet that was sent two sequence numbers ago but just released by a router reaching the receiver. In this situation, the receiver would not be able to tell the identity of this packet and would mistakenly regard it as the packet it wants now.

To avoid this, we modified the sequence number which is originally a one bit `boolean` value in essence into a two byte `short` value in C. And the sequence number of the packets start from 1 increasing by 1 with each desired packet. The new seqeunce number transition formula should be like:
$$
SequenceNumber(i+1)=SequenceNumber(i)+1
$$
Then the fault in the original protocol should have been fixed. This indicates how we arranged the transimission process in our implementation.

### Memory Management

Since we are required to limit the memory usage of our implementation into approximately 1MB, we have carefully designed the size of each buffer in both sending and receiving procedures. The following table shows them in details:

#### Sender Side

| name           | description              | length     |
| -------------- | ------------------------ | ---------- |
| send_buffer    | for sending packets      | 20069 byte |
| receive_buffer | for received acks        | 3 byte     |
| file_data      | for writing file content | 20069 byte |

#### Receiver Side

| name     | description              | length       |
| -------- | ------------------------ | ------------ |
| recv_buf | for received packets     | 20069 byte   |
| ack_buf  | for sending acks         | 2 byte       |
| data     | for writing file content | 0~20000 byte |

Each of these buffer would be allocated with corresponding memory only once. The maximum memory consumption for both sides during a file transmission task should be no more than 50KB in our estimation. And all these buffers would be freed after the task is finished.

### Graceful Exit

Both sender and receiver should exit gracefully.

For the sender, when it has sent all the data, it will additionally send an end packet (endflag = 1) to let the receiver know that this is the end of the transmission. After the sender finishes sending the end packet, it will exit.

For the receiver, after receiving the end packet (endflag = 1) from the sender, it will exit.

### Packet Structure

#### Data Packet (sendfile -> recvfile)

| name       | length                     |
| ---------- | -------------------------- |
| endflag    | 1 byte (only 1 bit useful) |
| seqno      | 2 byte                     |
| packetsize | 2 byte                     |
| filepath   | 60 byte                    |
| data       | 0~20000 byte               |
| crc        | 4 byte                     |

#### ACK (recvfile -> sendfile)

| name  | length |
| ----- | ------ |
| seqno | 2 byte |

## Showcase

