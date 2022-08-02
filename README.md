# CS118 Project 2

This is the repo for spring 2022 cs118 project 2.

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` USERID to add your userid for the `.tar.gz` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Project Details

Name: Kia Afzali 
UID: 705409740
Email: kiaafzali1@gmail.com

Name: Jacob Lemoff
UID: 505372144
Email: jlemoff@yahoo.com

## High Level Design Client:

1. Send SYN and wait for SYN ACK from server.
2. Send first ACK Packet + upto 9 additonal packets and put them in pkts[] and start timers for them in  timers[].

3. While still waiting for packages to be ACKed:
    1. Check if any package is timedout and resend
    2. Listen for ACKs
        1. If pkts[0] is ACKed, shift window to the write. If possible, read a new packet from file, and send the packet.
4. Send FIN and wait for ACK from server.

## High Level Design Server:

1. Wait for SYN from client, and send SYN ACK. Then wait for first ACK message with payload.
2. Construct sequence_expected[10], sequence_acked[10], and pkts[10] arrays.

2. While the packet is not FIN
    1. If packet.seq is in sequence_expected[10] and packet is not one of the packets buffered in pkts[10]
        1. Send ACK and buffer packet
        2. Shift the seq_expected, seq_acked, and pkts right
    2. If packet.seq is in sequence_expected[10] and packet is buffered in pkts[10]
        1. Send DUPACK
    3. If packet.seq is in sequence_expected[10]
        1. Send DUPACK
3. When FIN is recieved, reply with an ACK and FIN


## Problems Faced in the project:

We first started by coding the server side of the project. An issue that we faced was we were trying to number the packages 0-9 based on a hash function that basically did the following:

            packet_num = (packet.seqnum - min_sequence) / 512

This function worked well for grouping the pacakges into numbers 0-9 to be placed in our in-order pkts[] buffer when we were transferring smaller files. However, when we started testing with bigger files, we started getting seg fault errors because there were cases when min_sequence would be like 25000 and packet.seqnum would be 500, so packet_num would end up being a negative number. 

We ended up resorting to making an sequence_expected[10] that held the sequence number for the next 10 packets we were expecting, and another sequence_acked[10] for holding the previous 10 sequence numbers that were acked, in cases the client doesn't receive the ACK and resends the packets. 

## Tutorials Used

UCLA CS 118 Computer Network Fundementals Lectures, TA Discussions, and Lecture Slides. 
