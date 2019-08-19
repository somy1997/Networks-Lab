#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/sem.h>
#include <poll.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h> //hostent
#include <errno.h>
#include <sys/un.h>
#define SA (struct sockaddr*)
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <ifaddrs.h>

#define TIMEOUT  1000//msec

#define ICMP_PACLEN 500

unsigned short csum (uint8_t *pBuffer, int nLen)
{
    //Checksum for ICMP is calculated in the same way as for
    //IP header

    //This code was taken from: http://www.netfor2.com/ipsum.htm

    unsigned short nWord;
    unsigned int nSum = 0;
    int i;

    //Make 16 bit words out of every two adjacent 8 bit words in the packet
    //and add them up
    for (i = 0; i < nLen; i = i + 2)
    {
        nWord = ((pBuffer [i] << 8) & 0xFF00) + (pBuffer [i + 1] & 0xFF);
        nSum = nSum + (unsigned int)nWord;
    }

    //Take only 16 bits out of the 32 bit sum and add up the carries
    while (nSum >> 16)
    {
        nSum = (nSum & 0xFFFF) + (nSum >> 16);
    }

    //One's complement the result
    nSum = ~nSum;

    return ((unsigned short) nSum);
}


int hostname_to_ip(char * hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;

    if ( (he = gethostbyname( hostname ) ) == NULL)
    {
        // get the host info
        herror("gethostbyname");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    for (i = 0; addr_list[i] != NULL; i++)
    {
        //Return the first one;
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }

    return 1;
}

int sfd,sfd2;

int icmp_seqno = 1;
int sent = 0;
int received = 0;
int error = 0;
int messagesize = sizeof(struct timeval);
struct sockaddr_in addr;
unsigned srcaddr;
char *hostname;
struct timeval start;
char ip[100];
double tsum = 0, tsum2 = 0;
double max = 0, min = DBL_MAX;
unsigned short iphdrid = 0;
unsigned short userttl;

/*
struct iphdr {
     __u8 ihl:4, version:4;
     __u8 tos;
     __u16 tot_len;
     __u16 id;
     __u16 frag_off;
     __u8 ttl;
     __u8 protocol;
     __u16 check;
     __u32 saddr;
     __u32 daddr;
 };
*/

void set_ip_hdr(struct iphdr *ip)
{
    // IPv4 header length (4 bits): Number of 32-bit words in header = 5
    ip->ihl = sizeof(struct iphdr) / sizeof (uint32_t);

    // Internet Protocol version (4 bits): IPv4
    ip->version = 4;

    // Type of service (8 bits)
    ip->tos = 0;

    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr) + messagesize);
    
    //ip->id = htons(iphdrid);

    ip->frag_off = htons(0x4000);       /* no fragment */

    //Time to live
    ip->ttl = userttl;           /* default value */

    // Transport layer protocol (8 bits)
    //ip->protocol = IPPROTO_RAW; /* protocol at L4 */
    ip->protocol = IPPROTO_ICMP; /* protocol at L4 */
    
    // IPv4 header checksum (16 bits)
    ip->check = 0;

    ip->saddr = INADDR_ANY;
    //ip->saddr = srcaddr;

    ip->daddr = addr.sin_addr.s_addr;
    
    //printf("%hhu %hhu %hhu %hu %hu %hu %hhu %hhu %hu %u %u\n",ip->ihl, ip->version, ip->tos, ntohs(ip->tot_len), ntohs(ip->id), ntohs(ip->frag_off), ip->ttl, ip->protocol, ntohs(ip->check), ntohl(ip->saddr), ntohl(ip->daddr));
}

/*
struct icmphdr
{
  u_int8_t type;		/* message type
  u_int8_t code;		/* type sub-code
  u_int16_t checksum;
  union
  {
    struct
    {
      u_int16_t	id;
      u_int16_t	sequence;
    } echo;			/* echo datagram
    u_int32_t	gateway;	/* gateway address
    struct
    {
      u_int16_t	__unused;
      u_int16_t	mtu;
    } frag;			/* path mtu discovery
  } un;
};

*/

void set_icmp_hdr(struct icmphdr *icmphd)
{
    icmphd->type = ICMP_ECHO;
    icmphd->code = 0;
    icmphd->un.echo.id = htons(getpid());
    icmphd->un.echo.sequence = htons(icmp_seqno);
    icmphd->checksum = 0;
}

void retrieve_icmp_hdr(struct icmphdr *icmphd)
{
    icmphd->un.echo.id = ntohs(icmphd->un.echo.id);
    icmphd->un.echo.sequence = ntohs(icmphd->un.echo.sequence);
}

int checkValidity(struct icmphdr* icmphd, struct icmphdr *sendhdr)
{
    //retrieve_icmp_hdr(icmphd);

    //printf("Type = %hu,id = %hu, sendid = %hu, sequence = %hu\n",icmphd->type,icmphd->un.echo.id,sendhdr->un.echo.id,icmphd->un.echo.sequence);
    if (icmphd->type != ICMP_ECHOREPLY)
        return 0;
    if (icmphd->code != 0)
        return 0;
    if (icmphd->un.echo.id != sendhdr->un.echo.id)
        return 0;
    if (icmphd->un.echo.sequence != sendhdr->un.echo.sequence)
        return 0;
    return 1;
}

void printHD(struct icmphdr* icmphd)
{
    printf("Type = %hu, code = %hu, id = %hu, sequence = %hu, csum = %hu\n", icmphd->type, icmphd->code, htons(icmphd->un.echo.id), htons(icmphd->un.echo.sequence), htons(icmphd->checksum));
}


int ping()
{
    /*Prepare packet*/
    uint8_t buf[ICMP_PACLEN];
    memset(buf, 0, ICMP_PACLEN);

    struct iphdr iphd;
    set_ip_hdr(&iphd);

    struct icmphdr icmhd;
    set_icmp_hdr(&icmhd);

    struct timeval sendt, recvt, rest;
    gettimeofday(&sendt, NULL);

    memcpy(buf, &iphd, sizeof(struct iphdr));
    memcpy(buf + sizeof(struct iphdr), &icmhd, sizeof(struct icmphdr));
    if(messagesize >= sizeof(struct timeval))
    {
    	memcpy(buf + sizeof(struct iphdr) + sizeof(struct icmphdr), &sendt, sizeof(struct timeval));
    	memset(buf + sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(struct timeval), 'a', messagesize - sizeof(struct timeval));
    }
    else
    	memset(buf + sizeof(struct iphdr) + sizeof(struct icmphdr), 'a', messagesize);
    
    //printf("csum = %hu\n",csum(buf, (sizeof(struct iphdr))));
    iphd.check = htons(csum(buf, (sizeof(struct iphdr))));
    memcpy(buf, &iphd, sizeof(struct iphdr));
    //printf("csum = %hu\n",csum(buf, (sizeof(struct iphdr))));
    
    //printf("csum = %hu\n",csum(buf+sizeof(struct iphdr), (sizeof(struct icmphdr) + messagesize)));
    icmhd.checksum = htons(csum(buf + sizeof(struct iphdr), (sizeof(struct icmphdr) + messagesize)));
    memcpy(buf + sizeof(struct iphdr), &icmhd, sizeof(struct icmphdr));
    //printf("csum = %hu\n",csum(buf+sizeof(struct iphdr), (sizeof(struct icmphdr) + messagesize)));

    //printf("Sent = \n");
    //printHD(&icmhd);

    /*Packet ready*/
    socklen_t len1 = sizeof(struct sockaddr);
    //int rst = sendto (sfd, buf + sizeof(struct iphdr), messagesize + sizeof(struct icmphdr), 0, (struct sockaddr*) & addr, len1);
	int rst = sendto (sfd, buf, messagesize + sizeof(struct iphdr) + sizeof(struct icmphdr), 0, (struct sockaddr*) & addr, len1);
	
    if (rst < 0)
    {
        perror("sendto");
        exit(1);
    }
    //printf("sent = %d\n",rst);
    sent++;

    int cond = 1;
    int times = 0;
    while(cond && times < 3)
    {
        cond = 0;
        struct sockaddr_in addr2;
        socklen_t len = sizeof (struct sockaddr_in);
        fd_set rd_set;
        FD_ZERO(&rd_set);
        FD_SET(sfd2, &rd_set);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT * 1000;
        int sel = select(sfd2 + 1, &rd_set, NULL, NULL, &timeout);
        if (sel == -1)
        {
            perror("Server: Error in select");
            exit(1);
        }
        uint8_t buff[ICMP_PACLEN];
        memset(buff, 0, ICMP_PACLEN);
        if (sel > 0 && FD_ISSET(sfd2, &rd_set))
        {
            rst = recvfrom (sfd2, buff, sizeof(buff), 0, (struct sockaddr*) & addr2, &len);
            //printf("rst = %d\n",rst);
            if (rst < 0)
            {
                perror("recvfrom");
                exit(1);
            }
            gettimeofday(&recvt, NULL);
            struct iphdr *riphdr = (struct iphdr *)buff;
            struct icmphdr *ricmphdr = (struct icmphdr *)(buff + sizeof(struct iphdr));
            struct timeval *stvt;
            if(messagesize >= sizeof(struct timeval))
            	stvt = (struct timeval *)(buff + sizeof(struct iphdr) + sizeof(struct icmphdr));
            else
            	stvt = &sendt;
            //printf("received = \n");
            //printHD(ricmphdr);
            //ricmphdr->checksum = ntohs(ricmphdr->checksum);
			

            //printf("%hhu %hhu %hhu %hu %hu %hu %hhu %hhu %hu %u %u\n",riphdr->ihl, riphdr->version, riphdr->tos, ntohs(riphdr->tot_len), ntohs(riphdr->id), ntohs(riphdr->frag_off), riphdr->ttl, riphdr->protocol, ntohs(riphdr->check), ntohl(riphdr->saddr), ntohl(riphdr->daddr));
			
            /*printf("%d %d %d\n",csum(buff + sizeof(struct iphdr), (rst - sizeof(struct iphdr))),
                checkValidity(ricmphdr, &icmhd),
                memcmp(buff + sizeof(struct iphdr) + sizeof(struct icmphdr) , buf + sizeof(struct icmphdr),
                              rst - sizeof(struct icmphdr) - sizeof(struct iphdr))
                );*/

            if (csum(buff, sizeof(struct iphdr)) == 0 && csum(buff + sizeof(struct iphdr), (rst - sizeof(struct iphdr))) == 0)
                // && memcmp(buff + sizeof(struct iphdr) + sizeof(struct icmphdr) , buf + sizeof(struct icmphdr),
                //         rst - sizeof(struct icmphdr) - sizeof(struct iphdr)) == 0
            {
                if (checkValidity(ricmphdr, &icmhd))
                {
                    timersub(&recvt, stvt, &rest);
                    double elapsed = (rest.tv_sec) * 1000 + ((double)(rest.tv_usec)) / 1000;
                    if(messagesize >= sizeof(struct timeval))
                    	printf("%ld bytes from %s (%s): icmp_seq=%d ttl=%d time=%.3f ms\n",
                           rst - sizeof(struct iphdr), hostname, ip, icmp_seqno, riphdr->ttl, elapsed);
                    else
                    	printf("%ld bytes from %s (%s): icmp_seq=%d ttl=%d\n",
                           rst - sizeof(struct iphdr), hostname, ip, icmp_seqno, riphdr->ttl);
                    received++;
                    double triptime = elapsed;
                    tsum += triptime;
                    tsum2 += triptime * triptime;
                    if(triptime < min)
                    {
                        min = triptime;
                    }
                    if(triptime > max)
                    {
                        max = triptime;
                    }

                }
                else if (ricmphdr->type == 3)
                {
                    printf("From %s icmp_seq=%d Destination Unreachable\n", ip,icmp_seqno);
                    error++;
                }
                else
                {
                    //printf("here\n");
                    cond = 1;
                    times++;

                }

            }

            else
            {
                printf("icmp_seq=%d : Damaged packet\n", icmp_seqno);
            }
        }
        else
        {
            printf("icmp_seq=%d : Timeout\n", icmp_seqno);

        }
    }
    icmp_seqno++;
    //iphdrid++;


}


void finish(int sig)
{
    printf("\n--- %s ping statistics ---\n",hostname);
    double loss = (sent-received)*100.0/sent;
    struct timeval now,diff;
    gettimeofday(&now, NULL);
    timersub(&now, &start, &diff);
    double timec = diff.tv_sec*1000 + diff.tv_usec/1000;
    if(error == 0)
    {
        printf("%d packets transmiited, %d received, %.2lf%% packet loss, time %.2lf ms\n",sent,received,loss,timec);
        
    }
    else 
        printf("%d packets transmiited, %d received, +%d errors, %.2lf%% packet loss, time %.2lf ms\n\n",sent,received,error,loss,timec);
    if(received > 0 && messagesize >= sizeof(struct timeval))
    {
        tsum /= received;
        tsum2 /= received;
        double tmdev = sqrt(tsum2 - tsum * tsum);
        printf("rtt min/avg/max/mdev = %.3lf/%.3lf/%.3lf/%.3lf ms\n",min,tsum,max,tmdev);
    }
	close(sfd);
    exit(0);


}

void mystat(int sig)
{
    double loss = (sent-received)*100.0/sent;
    printf("%d/%d packets, %.2lf%% loss, min/avg/max = %.3lf/%.3lf/%.3lf ms\n",received,sent,loss,min,tsum/received,max);
}

unsigned getethinterfaceip()
{
    struct ifaddrs *iflist, *iface;

    if (getifaddrs(&iflist) < 0) {
        perror("getifaddrs");
        return 1;
    }

    for (iface = iflist; iface; iface = iface->ifa_next) {
        int af = iface->ifa_addr->sa_family;
        
        if(af == AF_INET && iface->ifa_name[0] != 'l') {
				struct in_addr srcin;
				srcin = ((struct sockaddr_in *)iface->ifa_addr)->sin_addr;
				char addrp[30];	
				if (inet_ntop(AF_INET, &srcin, addrp, sizeof addrp) == NULL) {
							perror("inet_ntop");
							exit(1);
						   }
				//printf("af value : %d Interface en has address %s\n",AF_INET, addrp);

                return srcin.s_addr;
        }
    }

    perror("eth interface not found\n");
    exit(1);
    return 0;
}

int main (int argc, char *argv[])
{
    //printf("[1] %d\n",getpid());
    if (argc != 5)
    {
        printf ("Usage: <dest_addr> <count> <msglen (atleast 16)> <ttl>\n");
        exit (0);
    }
    hostname = argv[1];
    hostname_to_ip(hostname , ip);
    //messagesize = 50;
    int count = atoi(argv[2]);
    int msize = atoi(argv[3]);
    userttl = (unsigned short) atoi(argv[4]);
    if(count < 0 || msize < 0 || userttl > 255)
    {
        printf("Invalid arguments\n");
        exit(1);
    }
    messagesize = msize;// + sizeof(struct timeval);
    // printf("%s resolved to %s\n" , hostname , ip);
    printf("PING %s (%s) %d(%d) bytes of data.\n", hostname, ip, messagesize, messagesize+28);
    //sfd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP);
    sfd = socket (AF_INET, SOCK_RAW, IPPROTO_RAW);
    
    if (sfd < 0)
    {
        perror("socket");
        exit(1);
    }
	
	sfd2 = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP);
	
	if (sfd2 < 0)
    {
        perror("socket");
        exit(1);
    }
	
	///*
    int one = 1;
    if (setsockopt (sfd2, IPPROTO_IP, IP_HDRINCL, &one, sizeof (one)) < 0)
        printf ("Cannot set HDRINCL!\n");//*/
	

    addr.sin_port = htons (9999);
    addr.sin_family = AF_INET;
    if (inet_pton (AF_INET, ip, &(addr.sin_addr)) != 1)
    {
        perror("presentaion to network conversion:");
        exit(1);
    }
	
	srcaddr = getethinterfaceip();
	
	//srand(time(0));
	//iphdrid = (unsigned short)rand();
	
    signal(SIGINT,finish);
    signal(SIGQUIT,mystat);
    gettimeofday(&start,NULL);
    
    while (sent != count || count == 0)
    {
        ping();
        //int temp;
        //scanf("%d",&temp);
        usleep(800* 1000);
    }
    finish(0);
    return 0;
}
