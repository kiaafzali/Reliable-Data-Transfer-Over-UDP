#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netdb.h>
#include <math.h> 

// =====================================

#define RTO 500000 /* timeout in microseconds */
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */
#define FIN_WAIT 2 /* seconds to wait after receiving FIN*/

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

double setFinTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) FIN_WAIT;
}

int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

int isEmptyPacket(struct packet a){
    if (!(a.ack || a.acknum || a.dupack || a.fin || a.length || a.seqnum || a.syn)) {
        return 1;
    }
    else {
        return 0;
    }
}

// =====================================

int main (int argc, char *argv[])
{
    if (argc != 4) {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    struct in_addr servIP;
    if (inet_aton(argv[1], &servIP) == 0) {
        struct hostent* host_entry; 
        host_entry = gethostbyname(argv[1]); 
        if (host_entry == NULL) {
            perror("ERROR: IP address not in standard dot notation\n");
            exit(1);
        }
        servIP = *((struct in_addr*) host_entry->h_addr_list[0]);
    }

    unsigned int servPort = atoi(argv[2]);

    FILE* fp = fopen(argv[3], "r");
    if (fp == NULL) {
        perror("ERROR: File not found\n");
        exit(1);
    }

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = servIP;
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    int servaddrlen = sizeof(servaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================
    // Establish Connection: This procedure is provided to you directly and is
    // already working.
    // Note: The third step (ACK) in three way handshake is sent along with the
    // first piece of along file data thus is further below

    struct packet synpkt, synackpkt;

    unsigned short seqNum = rand() % MAX_SEQN;
    buildPkt(&synpkt, seqNum, 0, 1, 0, 0, 0, 0, NULL);

    printSend(&synpkt, 0);
    sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    double timer = setTimer();
    int n;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            else if (isTimeout(timer)) {
                printTimeout(&synpkt);
                printSend(&synpkt, 1);
                sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
        }

        printRecv(&synackpkt);
        if ((synackpkt.ack || synackpkt.dupack) && synackpkt.syn && synackpkt.acknum == (seqNum + 1) % MAX_SEQN) {
            seqNum = synackpkt.acknum;
            break;
        }
    }

    // =====================================
    // FILE READING VARIABLES
    
    char buf[PAYLOAD_SIZE];
    size_t m;
    int end_of_file = 0;

    // =====================================
    // CIRCULAR BUFFER VARIABLES
    struct packet recvpkt;
    struct packet empty_packet;
    buildPkt(&empty_packet, 0, 0, 0, 0, 0, 0, 0, NULL);
    struct packet ackpkt;
    struct packet pkts[WND_SIZE] = {empty_packet};
    double timers[WND_SIZE];
    for (int i=0; i < WND_SIZE; i++){
        timers[i] = INFINITY;
    }
    short min_seq;
    short max_seq;


//    int s = 0;
//    int e = 0;
//    int full = 0;

    // =====================================
    // Send First Packet (ACK containing payload)


    // For ACK packet, build, print, send, set timer
    m = fread(buf, 1, PAYLOAD_SIZE, fp);
    if (m < PAYLOAD_SIZE){
        end_of_file = 1;
    }
    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, m, buf);
    printSend(&pkts[0], 0);
    sendto(sockfd, &pkts[0], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timers[0] = setTimer();
    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 0, 1, m, buf);
    seqNum = (seqNum + pkts[0].length) % MAX_SEQN;

    min_seq = pkts[0].seqnum;
    // For remaining packets, build, print, send, set timer
    for (int i = 1; i < WND_SIZE; i ++) {
        m = fread(buf, 1, PAYLOAD_SIZE, fp);
        if (m == 0) {
            end_of_file = 1;
            //printf("finished sending packets \n");
            break;
        }
        buildPkt(&pkts[i], seqNum, 0, 0, 0, 0, 0, m, buf);
        printSend(&pkts[i], 0);
        sendto(sockfd, &pkts[i], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    //    printf("Paylaod Size: %d \n", pkts[i].length);
        timers[i] = setTimer();
        max_seq = pkts[i].seqnum;
        seqNum = (seqNum+m) % MAX_SEQN;
    }

    

    // =====================================
    // *** TODO: Implement the rest of reliable transfer in the client ***
    // Implement GBN for basic requirement or Selective Repeat to receive bonus

    // Note: the following code is not the complete logic. It only sends a
    //       single data packet, and then tears down the connection without
    //       handling data loss.
    //       Only for demo purpose. DO NOT USE IT in your final submission
    while (1) {
        // Check for timeouts
        int all_packets_acked = 0;
        for (int i = 0; i < WND_SIZE; i++) {
            if (isTimeout(timers[i])) {
                //printf("resending packet %d because of timeout: %d \n", pkts[i].seqnum, i);
                printSend(&pkts[i], 1);
                sendto(sockfd, &pkts[i], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timers[i] = setTimer();
            }
        }


        // Listen for acks
        n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
        if (n > 0) {
            printRecv(&recvpkt);

            // If not ack or dupack => ignore
            if (! (recvpkt.ack || recvpkt.dupack)) {
                //printf("Not a ACK or DUPACK packet \n");
                continue;;
            }

            // If not within expected acknum range => ignore
            //int packet_num = (recvpkt.acknum - min_seq - 1)/512;
            int packet_num;
            int x = recvpkt.acknum;
            if (min_seq > x) {
                x = x + MAX_SEQN;
            }
            packet_num = (x - min_seq -1) / 512;    

            short recvpkt_seq = pkts[packet_num].seqnum;
            //printf("packet with ack %d for acking packet %d with seqnum %d \n", recvpkt.acknum, packet_num, recvpkt_seq);



            if ((min_seq<max_seq) &&( (recvpkt_seq < min_seq) || (recvpkt_seq > max_seq))) {
                //printf("Seq number %d not in the range: %d to %d \n", recvpkt_seq, min_seq, max_seq);
                continue;
            }

            if ((min_seq > max_seq) &&( (recvpkt_seq < min_seq && recvpkt_seq > max_seq))) {
                //printf("Seq number %d not in the range: %d to %d \n", recvpkt_seq, min_seq, max_seq);
                continue;
            }


            // *** Need to account for seq overflow
            
            struct packet target_packet = pkts[packet_num];

            // If packet has already been acked => ignore
            if (isEmptyPacket(target_packet)) {
            //    printf("Target with packetnum %d was empty: %d \n", packet_num, recvpkt_seq);
                continue;
            }

            // If packet has not been acked 
            if (target_packet.seqnum == recvpkt_seq) {
                //printf("Packet_num %d with seqnum %d has been acked \n", packet_num, pkts[packet_num].seqnum);
                pkts[packet_num] = empty_packet;
                timers[packet_num] = INFINITY;

                // While leftmost packet is ACKed
                while (isEmptyPacket(pkts[0])) {

                    //Shift window to the right
                    for (int i = 0; i < WND_SIZE-1; i++){
                        pkts[i] = pkts[i+1];
                        timers[i] = timers[i+1];
                    }
                    pkts[WND_SIZE-1] = empty_packet;
                    timers[WND_SIZE-1] = INFINITY;
                    //printf("min_seq was %d \n", min_seq);
                    min_seq = pkts[0].seqnum;
                    //printf("min_seq updated to %d \n", min_seq);
                    
                    //If possible, read from file and send new packet
                    if (! end_of_file) {
                        m = fread(buf, 1, PAYLOAD_SIZE, fp);
                        if (m < PAYLOAD_SIZE){
                            end_of_file = 1;
                        }
                        if (m != 0){
                            buildPkt(&pkts[WND_SIZE-1], seqNum, 0, 0, 0, 0, 0, m, buf);
                            printSend(&pkts[WND_SIZE-1], 0);
                            sendto(sockfd, &pkts[WND_SIZE-1], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                            timers[WND_SIZE-1] = setTimer();
                            max_seq = seqNum;
                            seqNum = (seqNum+m)%MAX_SEQN;
                        }
                    }

                    all_packets_acked = 1;
                    for (int i = 0; i < WND_SIZE; i++) {
                        if (!isinf(timers[i])) {
                            all_packets_acked = 0;
                        }
                    }
                    if (all_packets_acked){
                        break;
                    }

                }
                
 

            }

        }
        
        if (all_packets_acked && end_of_file){
            break;
        }

    }


    // Connection Teardown uses ackpkt as last recieved recvpkt
    ackpkt = recvpkt;

    // *** End of your client implementation ***
    fclose(fp);

    // =====================================
    // Connection Teardown: This procedure is provided to you directly and is
    // already working.

    struct packet finpkt;
    buildPkt(&finpkt, ackpkt.acknum, 0, 0, 1, 0, 0, 0, NULL);
    buildPkt(&ackpkt, (ackpkt.acknum + 1) % MAX_SEQN, (ackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, 0, NULL);

    printSend(&finpkt, 0);
    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
    int timerOn = 1;

    double finTimer;
    int finTimerOn = 0;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            if (timerOn && isTimeout(timer)) {
                printTimeout(&finpkt);
                printSend(&finpkt, 1);
                if (finTimerOn)
                    timerOn = 0;
                else
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
            if (finTimerOn && isTimeout(finTimer)) {
                close(sockfd);
                if (! timerOn)
                    exit(0);
            }
        }
        printRecv(&recvpkt);
        if ((recvpkt.ack || recvpkt.dupack) && recvpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN) {
            timerOn = 0;
        }
        else if (recvpkt.fin && (recvpkt.seqnum + 1) % MAX_SEQN == ackpkt.acknum) {
            printSend(&ackpkt, 0);
            sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
            finTimer = setFinTimer();
            finTimerOn = 1;
            buildPkt(&ackpkt, ackpkt.seqnum, ackpkt.acknum, 0, 0, 0, 1, 0, NULL);
        }
    }
}