#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <math.h>

// =====================================

#define RTO 500000 /* timeout in microseconds */
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */

// Packet Structure: Described in Section 2.1.1 of the spec. DO NOT CHANGE!
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char syn;
    char fin;
    char ack;
    char dupack;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Printing Functions: Call them on receiving/sending/packet timeout according
// Section 2.6 of the spec. The content is already conformant with the spec,
// no need to change. Only call them at correct times.
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", (pkt->ack || pkt->dupack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "", pkt->dupack ? " DUP-ACK": "");
}

void printTimeout(struct packet* pkt) {
    printf("TIMEOUT %d\n", pkt->seqnum);
}

// Building a packet by filling the header and contents.
// This function is provided to you and you can use it directly
void buildPkt(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char syn, char fin, char ack, char dupack, unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->syn = syn;
    pkt->fin = fin;
    pkt->ack = ack;
    pkt->dupack = dupack;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// =====================================

double setTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) RTO/1000000;
}

int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

// =====================================

int main (int argc, char *argv[])
{
    if (argc != 2) {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    unsigned int servPort = atoi(argv[1]);

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        perror("bind() error");
        exit(1);
    }

    int cliaddrlen = sizeof(cliaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================

    unsigned short seqNum = (rand() * rand()) % MAX_SEQN;

    for (int i = 1; ; i++) {
        // =====================================
        // Establish Connection: This procedure is provided to you directly and
        // is already working.

        int n;

        FILE* fp;

        struct packet synpkt, synackpkt, ackpkt;

        //Wait for first syn packet from client and break
        while (1) {
            n = recvfrom(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
            if (n > 0) {
                printRecv(&synpkt);
                if (synpkt.syn)
                    break;
            }
        }

        //cliSeqNum is the ACKnum made from client's seqnum for their SYN message
        unsigned short cliSeqNum = (synpkt.seqnum + 1) % MAX_SEQN; // next message from client should have this sequence number

        buildPkt(&synackpkt, seqNum, cliSeqNum, 1, 0, 1, 0, 0, NULL);

        while (1) {
            //Send first SYN ACK message to client
            printSend(&synackpkt, 0);
            sendto(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
            
            while(1) {
                //Store response from client into ackpkt

                n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
                if (n > 0) {
                    printRecv(&ackpkt);
                    // ackpkt can either be an ACK to our SYN ACK, or a resend of SYN

                    // Check if it's an ACK
                    // If their seqnum == acknum we sent, it is an ACK message, and it's acknum = seqnum we sent + 1
                    if (ackpkt.seqnum == cliSeqNum && ackpkt.ack && ackpkt.acknum == (synackpkt.seqnum + 1) % MAX_SEQN) {
                        //printf("connection can start \n");
                        //write the content of the payload in this packet to i.file
                        int length = snprintf(NULL, 0, "%d", i) + 6;
                        char* filename = malloc(length);
                        snprintf(filename, length, "%d.file", i);

                        fp = fopen(filename, "w");
                        free(filename);
                        if (fp == NULL) {
                            perror("ERROR: File could not be created\n");
                            exit(1);
                        }

                        fwrite(ackpkt.payload, 1, ackpkt.length, fp);

                        //set seqNum and ackNUM, and send ACK message back to the socket
                        seqNum = ackpkt.acknum;
                        cliSeqNum = (ackpkt.seqnum + ackpkt.length) % MAX_SEQN;

                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                        printSend(&ackpkt, 0);
                        sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                        break;
                    }
                    //Check if it's another SYN
                    //If client is resending their syn, it means our preveious ack was not recieved, so rebuild another SYN ACK and restart
                    else if (ackpkt.syn) {
                        buildPkt(&synackpkt, seqNum, (synpkt.seqnum + 1) % MAX_SEQN, 1, 0, 0, 1, 0, NULL);
                        break;
                    }
                }
            }

            //If client responded to our SYN ACK with SYN, go through loop again, otherwise, handshake is over
            if (! ackpkt.syn)
                break;
        }


        



        // *** TODO: Implement the rest of reliable transfer in the server ***
        // Implement GBN for basic requirement or Selective Repeat to receive bonus

        // Note: the following code is not the complete logic. It only expects 
        //       a single data packet, and then tears down the connection
        //       without handling data loss.
        //       Only for demo purpose. DO NOT USE IT in your final submission
        struct packet empty_packet;
        buildPkt(&empty_packet, 0, 0, 0, 0, 0, 0, 0, NULL);
        struct packet pkts[WND_SIZE] = {empty_packet};

        struct packet recvpkt;
        short rcvbase = cliSeqNum;
        short seq_expected [10];
        short seq_prev_acked [10];
        seq_expected[0] = rcvbase;
        for (int i = 1; i < 10; i++){
            seq_expected[i] = seq_expected[i-1] + 512;
        }
        seq_prev_acked[0] = rcvbase-10*512;
        for (int i = 1; i < 10; i++){
            seq_prev_acked[i] = seq_prev_acked[i-1] + 512;
        }



        //printf("entering TODO \n");
        while(1) {
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
            if (n > 0) {
                printRecv(&recvpkt);
                //printf("Packet Payload Size %d \n", recvpkt.length);
                
                // If FIN packet => send ACK and break
                if (recvpkt.fin) {
                    cliSeqNum = (recvpkt.seqnum + 1) % MAX_SEQN;
                    buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                    printSend(&ackpkt, 0);
                    sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                    break;
                }

                int in_expected = 0;

                //If in expected sequence
                // printf("seq_expected: ");
                // for (int i = 0; i < 10; i++){
                //     printf("%d ", seq_expected[i]);
                // }
                // printf("\n");
                // printf("seq_prev_acked: ");
                // for (int i = 0; i < 10; i++){
                //     printf("%d ", seq_prev_acked[i]);
                // }
                //printf("\n");

                for (int i =0; i < 10; i++) {
                    if (recvpkt.seqnum == seq_expected[i]){
                        in_expected = 1;

                        //if already buffered => send dupack
                        int end = 0;
                        for (int j=0; j < 10; j++){
                            if (recvpkt.seqnum == pkts[j].seqnum){
                                //send dupack
                                // printf("expected and buffered \n");
                                cliSeqNum = (recvpkt.seqnum + recvpkt.length) % MAX_SEQN;
                                buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);
                                printSend(&ackpkt, 0);
                                sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                                end = 1;
                                break;
                            }
                        }
                        if (end){
                            break;
                        }

                        // printf("expected seq packet: %d  not buffered \n", i);
                        //else => buffer and ack
                        pkts[i] = recvpkt;
                        cliSeqNum = (recvpkt.seqnum + recvpkt.length) % MAX_SEQN;
                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                        printSend(&ackpkt, 0);
                        sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                       
                       
                        //deliver all in order packets
                        // printf("pkts[0].seqnum = %d, rcvbase = %d \n", pkts[0].seqnum, rcvbase);
                        while (pkts[0].seqnum == rcvbase){
                            //write to file
                            // printf("pkt is rcvbase, shifting window \n");
                            fwrite(pkts[0].payload, 1, pkts[0].length, fp);

                            //Shift window
                            for (int k = 0; k < 9; k++){
                                pkts[k] = pkts[k+1];
                            }
                            for (int k = 0; k < 10; k++){
                                seq_prev_acked[k] = (seq_prev_acked[k] + 512) % MAX_SEQN;
                                seq_expected[k] = (seq_expected[k] + 512) % MAX_SEQN;
                            }
                            rcvbase = (rcvbase + 512) % MAX_SEQN;
                            end = 1;
                        }
                        break; 
                        
                    }
                }
                if (in_expected){
                    continue;
                }

                // If in seq_prev_acked => send dupack
                int end = 0;
                for (int i=0; i < 10; i++){
                    if (recvpkt.seqnum == seq_prev_acked[i]){
                        //printf("seq previously acked \n");
                        //send dupack
                        cliSeqNum = (recvpkt.seqnum + recvpkt.length) % MAX_SEQN;
                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);
                        printSend(&ackpkt, 0);
                        sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                        end = 1;
                        break;
                    }
                }
                if(end){
                    continue;
                }
                //printf("ignored this package \n");

            }
        }

        // *** End of your server implementation ***

        fclose(fp);
        // =====================================
        // Connection Teardown: This procedure is provided to you directly and
        // is already working.
        struct packet finpkt, lastackpkt;
        buildPkt(&finpkt, seqNum, 0, 0, 1, 0, 0, 0, NULL);
        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);

        //printf("IN FIN STAGE, OUT OF TODO \n");
        printSend(&finpkt, 0);
        sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
        double timer = setTimer();

        while (1) {
            while (1) {
                n = recvfrom(sockfd, &lastackpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
                if (n > 0)
                    break;

                if (isTimeout(timer)) {
                    printTimeout(&finpkt);
                    printSend(&finpkt, 1);
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                    timer = setTimer();
                }
            }

            printRecv(&lastackpkt);
            if (lastackpkt.fin) {
                printSend(&ackpkt, 0);
                sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                printSend(&finpkt, 1);
                sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                timer = setTimer();
                
                continue;
            }
            if ((lastackpkt.ack || lastackpkt.dupack) && lastackpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN)
                break;
        }

        seqNum = lastackpkt.acknum;
    }
}
