/********************************
 * (c) 2018-2019 HomeAccessoryKid
 * UDP logger has 3 MACROs and 2 #defines
 * UDPLSO is like printf and does Serial Only
 * UDPLUO is like printf and does UDP Only to udp-client
 * UDPLUS is like printf and both udp-client and Serial will receive
 *
 * #define UDPLOG_PRINTF_TO_UDP      will modify the printf function to use UDP
 * #define UDPLOG_PRINTF_ALSO_SERIAL will modify the printf function to also use Serial
 *
 * use udplog_init(prio) to set up //is prio=3 a good idea??
 **************************************************************************************/

#ifndef __UDPLOGGER_H__
#define __UDPLOGGER_H__

#include <stdout_redirect.h>
#include <semphr.h>
#include <lwip/sockets.h>

#define UDPLOGSTRING_SIZE 800 //bytes
#define HOLDOFF 10 //x10ms = 100ms

#define UDPLUO(format, ...)  do {{  if( xSemaphoreTake( xUDPlogSemaphore, ( TickType_t ) 1 ) == pdTRUE ) { \
                                        int len=1+snprintf(NULL,0,format,##__VA_ARGS__); \
                                        if ((len>UDPLOGSTRING_SIZE-udplogstring_len) && udplogstring_len) { \
                                            if (udplogmembers) lwip_sendto(udploglSocket, udplogstring, udplogstring_len, 0, \
                                                                (struct sockaddr *)&udplogsClntAddr,sizeof(udplogsClntAddr));\
                                            UDPLSO("%3d->%d flush\n",udplogstring_len,udplogmembers); \
                                            udplogstring_len=0; \
                                        } \
                                        if (len>UDPLOGSTRING_SIZE) { \
                                            char *udploguostring; \
                                            udploguostring=malloc(len); \
                                            snprintf(udploguostring,len,format,##__VA_ARGS__); \
                                            if (udplogmembers) lwip_sendto(udploglSocket, udploguostring, len, 0, \
                                                                (struct sockaddr *)&udplogsClntAddr,sizeof(udplogsClntAddr));\
                                            UDPLSO("%3d->%d bigger\n",len-1,udplogmembers); \
                                            free(udploguostring); \
                                        } else { \
                                            udplogstring_len+=sprintf(udplogstring+udplogstring_len,format,##__VA_ARGS__); \
                                        } \
                                        xSemaphoreGive( xUDPlogSemaphore ); \
                                    } else UDPLSO("skipped a UDPLUO\n"); \
                                }}while(0)
#define UDPLSO(format, ...)  do {{  char *udplogsostring; size_t udplogsosize; \
                                    udplogsostring=malloc(udplogsosize=1+snprintf(NULL,0,format,##__VA_ARGS__)); \
                                    snprintf(udplogsostring,udplogsosize,format,##__VA_ARGS__); \
                                    old_stdout_write(NULL,0,udplogsostring,udplogsosize); \
                                    free(udplogsostring); \
                                }}while(0)
#define UDPLUS(format, ...)  do {   UDPLSO(format,##__VA_ARGS__); \
                                    UDPLUO(format,##__VA_ARGS__); \
                                } while(0)
#define INIT "UDPlog init message. Use udplog-client to receive other log messages\n"

void udplog_send(void *pvParameters);
void udplog_init(int prio);
extern SemaphoreHandle_t xUDPlogSemaphore;
extern _WriteFunction    *old_stdout_write;
extern char udplogstring[];
extern int  udplogmembers,udploglSocket,udplogstring_len;
extern struct sockaddr_in udplogsClntAddr;

#endif //__UDPLOGGER_H__

/* pseudo code for UDPLUO, use curly bracket scope separation so {{}}
n=1+snprintf(NULL,0,format,##__VA_ARGS__); //add one to make up for final \0
if (n>UDPLOGSTRING_SIZE-udplogstring_len) {
    deliver now
}
if (n>UDPLOGSTRING_SIZE) {
    make  a new string
    deliver new string now
    free    new string
} else {
    collect
}
*/
