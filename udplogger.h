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

#define UDPLOGSTRING_SIZE 1400

#define UDPLUO(format, ...)  do {   if( xSemaphoreTake( xUDPlogSemaphore, ( TickType_t ) 1 ) == pdTRUE ) { \
                                        udplogstring_len+=sprintf(udplogstring+udplogstring_len,format,##__VA_ARGS__); \
                                        xSemaphoreGive( xUDPlogSemaphore ); \
                                    } else UDPLSO("skipped a UDPLOG\n"); \
                                } while(0)
#define UDPLSO(format, ...)  do {   char *udplogsostring; size_t udplogsosize; \
                                    udplogsostring=malloc(udplogsosize=1+snprintf(NULL,0,format,##__VA_ARGS__)); \
                                    snprintf(udplogsostring,udplogsosize,format,##__VA_ARGS__); \
                                    old_stdout_write(NULL,0,udplogsostring,udplogsosize); \
                                    free(udplogsostring); \
                                } while(0)
#define UDPLUS(format, ...)  do {   UDPLSO(format,##__VA_ARGS__); \
                                    UDPLUO(format,##__VA_ARGS__); \
                                } while(0)
#define INIT "UDPlog init message. Use udplog-client to receive other log messages\n"

void udplog_send(void *pvParameters);
void udplog_init(int prio);
extern SemaphoreHandle_t xUDPlogSemaphore;
extern _WriteFunction    *old_stdout_write;
extern char udplogstring[];
extern int  udplogstring_len;

#endif //__UDPLOGGER_H__
