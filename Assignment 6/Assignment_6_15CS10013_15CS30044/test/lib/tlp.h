#ifndef TLP_H
#define TLP_H

#define HEAD0SIZE sizeof(bool)	// packet type
#define HEAD1SIZE sizeof(unsigned int)	// seqno / ackno
#define HEAD2SIZEDATA sizeof(int)	// size of payload
#define HEAD2SIZEACK sizeof(int)	// rwnd
#define HEADSIZEDATA (HEAD0SIZE+HEAD1SIZE+HEAD2SIZEDATA)
#define HEADSIZEACK (HEAD0SIZE+HEAD1SIZE+HEAD2SIZEACK)
#define MAXPAYLOADSIZE 1024    // MSS
#define MAXACKSIZE HEADSIZEACK
#define MAXPACKETSIZE (HEADSIZEDATA+MAXPAYLOADSIZE) // data packet size always > ack packet size
#define MAXRBUFSIZE MAXPAYLOADSIZE*50
#define MAXSBUFSIZE MAXPAYLOADSIZE*50
#define TIMEOUT_SEC 1
#define TIMEOUT_USEC 0
#define TIMEOUT_COUNT 10
#define INITIALWINDOWSIZE 3*MAXPAYLOADSIZE
#define INITIALSSTHRESHOLD MAXPAYLOADSIZE*25
#define DATA true
#define ACK false
#define CONREQMSG "REQUEST"
#define CONACCMSG "ACCEPT"
#define FINMSG "FINISH"
#define FINACK "FINISHACK"
#define CONMSGLEN 20
#define SRC true
#define DEST false

int min(int a, int b);
int max(int a, int b);
bool isconnalive();
int init(char *hostname, unsigned short portno);
int bindnaccept(unsigned short portno);
void finish();
int apprecv(char *data, int n);
void appsend(char *data, int n);

#endif
