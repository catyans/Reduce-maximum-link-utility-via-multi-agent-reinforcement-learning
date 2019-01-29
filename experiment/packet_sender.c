#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h> 
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <pthread.h>

#define serverIP "192.168.0.134"
#define serverPort 8888
#define MAXBUF 2048
#define IPBUFLEN 20
#define PCAPPATH "/root/gn16/ipv4data.txt"
#define MAXPKTNUM 1000000
#define NODENUM 6
#define HOSTNUM 6
#define MAXPATHNUM 5
#define GROUPTABLE false
#define UDPPORT(src, dst, pathid) 8000+src*100+dst*10+pathid
#define PKT_DISTRI_PATH "/root/gn16/pkt_distri.txt"
#define MAX_PKTLEN 1500
#define MIN_PKTLEN 60
#define DELAY_PATH "/root/gn16/delay.log"

typedef enum{false=0,true} bool;

char *dstIPSet[NODENUM] = {"10.168.100.121", "10.168.100.122", "10.168.100.123", 
                     "10.168.100.124", "10.168.100.125", "10.168.100.126"};
int pathNum[NODENUM] = {0};
int sessRate[NODENUM] = {0};
double splitRatios[NODENUM][MAXPATHNUM];
int hostId = 0;
int updateCount = 0;
int maxUpdateCount = 0;

int serverSock, rawSock;
struct sockaddr_in addr_in;
pthread_t thread[HOSTNUM];
bool sendFlag = false;

uint8_t *srcIPList;
uint16_t *srcPortList;
uint16_t *dstPortList;
uint16_t *pktLenList;

/*pseudoUDPPacket: Pseudo header needed for calculating the UDP header checksum*/
struct pseudoUDPPacket {
	uint32_t srcAddr;
	uint32_t dstAddr;
	uint8_t zero;
	uint8_t protocol;
	uint16_t UDP_len;
};

/*init_server_sock(): connect to controller server*/
void init_server_sock() {
	struct sockaddr_in dest;
	if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Socket");  
	}
	bzero(&dest, sizeof(dest));  
	dest.sin_family = AF_INET;
	dest.sin_port = htons(serverPort);
	if (inet_aton(serverIP, (struct in_addr *) &dest.sin_addr.s_addr) == 0) {
		printf("bind error!\n");
	}
	if (connect(serverSock, (struct sockaddr *) &dest, sizeof(dest)) != 0) {  
		perror("Connect ");
		exit(1);
	}
}

/*free_all(): free allocated memory*/
void free_all() {
	free(srcIPList);
	free(srcPortList);
	free(dstPortList);
	free(pktLenList);
}

/*pthread_socket(): receive command from the controller*/
void pthread_socket() {
	char msgBuf[MAXBUF];
	memset(msgBuf, 0, MAXBUF);
	int len = 0;
	bool listenFlag = true;
	while (listenFlag) {
		usleep(5000);
		memset(msgBuf, 0, MAXBUF);
		len = recv(serverSock, msgBuf, MAXBUF, MSG_DONTWAIT);
		if (len > 0) {
			int pStart, pEnd, pContent;
			int i, nodeid;
			printf("msgBuf: %s\n", msgBuf);
			switch(msgBuf[0]) {
				case '0':
					pStart = 2;
					pEnd = pStart + 1;
					int contZero[3*(NODENUM-1)+1];
					pContent = 0;
					while(msgBuf[pEnd] != '#') {
						if(msgBuf[pEnd] == ',') {
							char cont[10] = {0};
							strncpy(cont, msgBuf+pStart, pEnd-pStart);
							contZero[pContent] = atof(cont);
							pContent++;
							pStart = pEnd + 1;
						}
						pEnd++;
					}
					for(i = 0; i < NODENUM-1; i++) {
						nodeid = contZero[i*3];
						pathNum[nodeid] = contZero[i*3+1];
						sessRate[nodeid] = contZero[i*3+2];
					}
					maxUpdateCount = contZero[i*3];
					break;
				case '1':
					pStart = 2;
					pEnd = pStart + 1;
					double contOne[(MAXPATHNUM+1)*(NODENUM-1)];
					pContent = 0;
					while(msgBuf[pEnd] != '#') {
						if(msgBuf[pEnd] == ',') {
							char cont[10] = {0};
							strncpy(cont, msgBuf+pStart, pEnd-pStart);
							contOne[pContent] = atof(cont);
							pContent++;
							pStart = pEnd + 1;
						}
						pEnd++;
					}
					i = 0;
					while(i < pContent) {
						nodeid = (int)contOne[i++];
						int j;
						for(j = 0; j < pathNum[nodeid]; j++) {
							splitRatios[nodeid][j] = contOne[i++];
						}
					}
					sendFlag = true;
					updateCount += 1;
					break;
				case '2':
					pStart = 2;
					pEnd = pStart + 1;
					int contTwo[2*(NODENUM-1)];
					pContent = 0;
					while(msgBuf[pEnd] != '#') {
						if(msgBuf[pEnd] == ',') {
							char cont[10] = {0};
							strncpy(cont, msgBuf+pStart, pEnd-pStart);
							contTwo[pContent] = atof(cont);
							pContent++;
							pStart = pEnd + 1;
						}
						pEnd++;
					}
					for(i = 0; i < NODENUM-1; i++) {
						nodeid = contTwo[i*2];
						sessRate[nodeid] = contTwo[i*2+1];
					}
					break;
				case '3':
					sendFlag = false;
					listenFlag = false;
					sleep(1);
					free_all();
					break;
				default:
					printf("Unknown msg type\n");
			}
		}
	}
	pthread_exit(NULL);
}

/*read_pcap(): read *.pcap file to prepare for traffic replay*/
void read_pcap() {
	FILE *f = fopen(PCAPPATH, "r");
	char line[200] = {0};
	char timeStr[20];
	uint8_t srcIPBuf[IPBUFLEN];
	uint8_t dstIPBuf[IPBUFLEN];
	uint16_t pktLen, srcPort, dstPort, protoNum;

	srcIPList = (uint8_t *)malloc(IPBUFLEN*MAXPKTNUM);
	srcPortList = (uint16_t *)malloc(MAXPKTNUM*sizeof(uint16_t));
	dstPortList = (uint16_t *)malloc(MAXPKTNUM*sizeof(uint16_t));
	pktLenList = (uint16_t *)malloc(MAXPKTNUM*sizeof(uint16_t));

	uint32_t count = 0;
	while (count <= MAXPKTNUM) {
		fgets(line, sizeof(line), f);
		memset(srcIPBuf, 0, IPBUFLEN);
		sscanf(line, "%s %hd %s %s %hd %hd %hd", timeStr, &pktLen, srcIPBuf, dstIPBuf, &srcPort, &dstPort, &protoNum);
		memcpy(srcIPList + count*IPBUFLEN, &srcIPBuf, IPBUFLEN);
		srcPortList[count] = srcPort;
		dstPortList[count] = dstPort;
		pktLenList[count] = pktLen<1500?pktLen:1500;
		count++;
	}
	fclose(f);
}


/*test_no_pcap(): just for test without *.pcap file*/
void test_no_pcap() {
	FILE *f = fopen(PKT_DISTRI_PATH, "r");
	char timeStr[20];
	uint8_t srcIPBuf[IPBUFLEN] = "10.0.0.1";
	uint8_t dstIPBuf[IPBUFLEN] = "10.0.0.2";
	uint16_t pktLen=1024, srcPort=8000, dstPort=8000, protoNum=17;

	srcIPList = (uint8_t *)malloc(IPBUFLEN*MAXPKTNUM);
	srcPortList = (uint16_t *)malloc(MAXPKTNUM*sizeof(uint16_t));
	dstPortList = (uint16_t *)malloc(MAXPKTNUM*sizeof(uint16_t));
	pktLenList = (uint16_t *)malloc(MAXPKTNUM*sizeof(uint16_t));
	
	// read pkt distri
	double cdf[MAX_PKTLEN + 1];
	int ind;
	double prob;
	memset(cdf, 0, sizeof(cdf));
	while(fscanf(f, "%d %lf", &ind, &prob) != EOF){
		cdf[ind] = prob;
	}
	for (ind = MIN_PKTLEN + 1; ind <= MAX_PKTLEN; ind ++){
		if(cdf[ind] < cdf[ind - 1]){
			cdf[ind] = cdf[ind - 1];
		}
	}
	
	uint32_t count = 0;
	while (count <= MAXPKTNUM) {
		memcpy(srcIPList + count*IPBUFLEN, srcIPBuf, IPBUFLEN);
		srcPortList[count] = srcPort;
		dstPortList[count] = dstPort;
		// sample pktLen
		double randnum = (double)rand() / RAND_MAX;
		for (ind = MIN_PKTLEN; ind <= MAX_PKTLEN; ind ++){
			if(randnum <= cdf[ind]){
				pktLen = ind;
				break;
			}
		}
		pktLenList[count] = pktLen;
		count++;
	}
}

/*csum(): compute checksum from the packet*/
uint16_t csum(uint16_t *buf, int nwords)
{ 
	uint32_t sum;
	for (sum = 0; nwords > 0; nwords--)
	{
		sum += *buf++;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (uint16_t)(~sum);
}

/*pthread_sender(): construct UDP packet and send it to the specific dst host*/
void pthread_sender(void *arg) {
	int dstHostId = *(int*)arg;
	printf("pthread_sender:%d started\n", dstHostId);
	char *srcIPStr = dstIPSet[hostId];
	char *dstIPStr = dstIPSet[dstHostId];

	char packet[1600];
	memset(packet, 0, sizeof(packet));
	struct iphdr *ipHdr = (struct iphdr *) packet;;
	struct udphdr *udpHdr = (struct udphdr *) (packet + sizeof(struct iphdr));
	char *data = (char *) (packet + sizeof(struct iphdr) + sizeof(struct udphdr));

	struct pseudoUDPPacket pUDPPacket;
	char pseudo_packet[1600];

	/*init ipHdr*/
	ipHdr->ihl = 5;
	ipHdr->version = 4;
	ipHdr->tos = 0;
	ipHdr->id = htons(54321);
	ipHdr->frag_off = 0x00;
	ipHdr->ttl = 64;
	ipHdr->protocol = IPPROTO_UDP;
	/*init udpHdr*/
	udpHdr->check = 0;
	/*init pseudo packet which is used for computing checksum*/
	pUDPPacket.zero = 0;
	pUDPPacket.protocol = IPPROTO_UDP;

	uint32_t pPkt = 0;
	uint16_t srcPort, dstPort, pktLen, dataLen;

	sleep(5);
	printf("Thread %d begins to send pkts\n", dstHostId);
	while(sendFlag) {
		pPkt = (pPkt+1)%MAXPKTNUM;

		dstPort = dstPortList[pPkt];
		if(GROUPTABLE)
			srcPort = srcPortList[pPkt];
		else {
			int cumlRatioInt[MAXPATHNUM+1];
			int i, max = 1000;
			double ratioSum = 0.0;
			cumlRatioInt[0] = 0;
			for(i = 0; i < pathNum[dstHostId] - 1; i++) {
				ratioSum += splitRatios[dstHostId][i];
				cumlRatioInt[i+1] = ratioSum*max;
			}
			cumlRatioInt[i+1] = max;
			
			int pathid = 0;
			int randNumber = rand()%1000;
			for(i = 0; i < pathNum[dstHostId]; i++) {
				if(randNumber >= cumlRatioInt[i] && randNumber <= cumlRatioInt[i+1])
					pathid = i;
			}
			srcPort = UDPPORT(hostId, dstHostId, pathid);
		}
		pktLen = pktLenList[pPkt];
		dataLen = pktLen - 42;
		if (dataLen < 10) dataLen = 10;

		//iphdr check
		ipHdr->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + dataLen;
		ipHdr->check = 0;
		ipHdr->saddr = inet_addr(srcIPStr);
		ipHdr->daddr = inet_addr(dstIPStr);
		ipHdr->check = csum((uint16_t *) packet, (ipHdr->tot_len)/2); 
		//udphdr
		udpHdr->source = htons(srcPort);
		udpHdr->dest = htons(dstPort);
		udpHdr->len = htons(sizeof(struct udphdr) + dataLen);
		//pseudo packet
		pUDPPacket.srcAddr = inet_addr(srcIPStr);
		pUDPPacket.dstAddr = inet_addr(dstIPStr);
		pUDPPacket.UDP_len = htons(sizeof(struct udphdr) + dataLen);
		memset(pseudo_packet, 0, 1600);
		memcpy(pseudo_packet, (char *) &pUDPPacket, sizeof(struct pseudoUDPPacket));
		//udphdr check
		udpHdr->check = 0;
		memcpy(pseudo_packet + sizeof(struct pseudoUDPPacket), udpHdr, sizeof(struct udphdr));
		memset(pseudo_packet + sizeof(struct pseudoUDPPacket) + sizeof(struct udphdr), 0, dataLen);
		udpHdr->check = (csum((uint16_t *) pseudo_packet, ((int) (sizeof(struct pseudoUDPPacket) + 
		      sizeof(struct udphdr) +  dataLen + 1))/2));

		//send packet
		int bytes = sendto(rawSock, packet, ipHdr->tot_len, 0, (struct sockaddr *) &addr_in, sizeof(addr_in));
		if(bytes < 0) {
			perror("Error on sendto()");
		}

		//sleep
		// for sessRate = 0, not send pkt
		while(!sessRate[dstHostId]){
			usleep(5127);
		}
		int pktGap = 5127 / sessRate[dstHostId];
		usleep(pktGap);
	}
	pthread_exit(NULL);
}

/*ping_func(): ping function and return the rtt*/
double ping_func(char *cmd) {
	FILE *pp = popen(cmd, "r");
	if (!pp) {
		return -1;
	}
	char output[1024];
	int lineNum = 0;
	double rtt = 0.0;
	
	
	while (fgets(output, sizeof(output), pp) != NULL) {
		if (output[strlen(output) - 1] == '\n') {
			output[strlen(output) - 1] = '\0';
		}
		lineNum++;
		if(lineNum == 2){
			char *pStart = strstr(output, "time=");
			int len = 0;
			while(*(pStart+len) != ' ') len++;
			char cont[10] = {0};
			strncpy(cont, pStart+strlen("time="), len-1);
			rtt = atof(cont);
		}
	}
	pclose(pp);
	return rtt;
}

/*pthread_ping(): get rtt of diffirent paths*/
void pthread_ping() {
	int i = 0, j = 0;
	FILE *f = fopen(DELAY_PATH, "w");
	sleep(0.2);
	while(updateCount < maxUpdateCount - 5 && sendFlag) {
		double avg_rtt = 0.;
		int sess_num = 0;		
		for(i = 0; i < HOSTNUM; i++) {
			if(i == hostId || sessRate[i] == 0) continue;
			sess_num ++;
			for(j = 0; j < pathNum[i]; j++) {
				char cmd[256] = {0};
				sprintf(cmd, "%s %s %s%d%s%d", "ping -c 1", dstIPSet[i], "-I 10.168.10", j, ".12", hostId+1);
				double rtt = ping_func(cmd);
				avg_rtt += splitRatios[i][j] * rtt;
			}
		}
		fprintf(f, "%d %lf\n", sess_num, avg_rtt);
		sleep(6);
	}
	fclose(f);
	pthread_exit(NULL);
}

/*main()*/
int main (int argc, char **argv) {
	hostId = atoi(argv[1]);
	printf("hostId:%d\n", hostId);
	srand(time(NULL));

	init_server_sock();

	pthread_t tid;
	int ret = pthread_create(&tid, NULL, (void*) pthread_socket, NULL);
	if (ret != 0) {
		perror("Error while creating thread!");
	}

	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(8000);
	addr_in.sin_addr.s_addr = inet_addr(dstIPSet[(hostId+1)%HOSTNUM]);
	int one = 1;
	if ((rawSock = socket(PF_INET, SOCK_RAW, IPPROTO_UDP)) < 0) {
		perror("Error while creating socket");
		exit(-1);
	}
	if (setsockopt(rawSock, IPPROTO_IP, IP_HDRINCL, (char *)&one, sizeof(one)) < 0) {
		perror("Error while setting socket options");
		exit(-1);
	}

	#if 0
	read_pcap();
	#else
	test_no_pcap();
	#endif

	while(!sendFlag)
		sleep(2);
	memset(thread, 0, sizeof(pthread_t)*HOSTNUM);
	int i;
	for(i = 0; i < NODENUM; i++) {
		if(i == hostId) continue;
		if((ret = pthread_create(thread+i*sizeof(pthread_t), NULL, (void*) pthread_sender, (void *)&i)) != 0)
			perror("Error while creating thread!\n");
		sleep(1);
	}
	pthread_t tping;
	ret = pthread_create(&tping, NULL, (void*) pthread_ping, NULL);
	if (ret != 0) {
		perror("Error while creating thread!");
	}

	while(sendFlag) {
		sleep(5);
	}
	sleep(5);
	return 0;
}
