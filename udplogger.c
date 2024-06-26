// (c) 2018-2022 HomeAccessoryKid
// see udplogger.h for instructions
#include <stdio.h>
#ifdef ESP_PLATFORM
 #include <esp_wifi.h>
 #include "esp_netif.h"
 #include <esp_system.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "esp_timer.h"
#else
 #include <espressif/esp_wifi.h>
 #include <espressif/esp_sta.h>
 #include <espressif/esp_system.h>
 #include <esp8266.h>
 #include <FreeRTOS.h>
 #include <task.h>
#endif
#include <string.h>
#include <lwip/sockets.h>
#include <lwip/raw.h>
#include <udplogger.h>

SemaphoreHandle_t xUDPlogSemaphore = NULL;
#ifdef ESP_PLATFORM
 #define UDPlogsendSTACKsize 2048
 #define GETTIMEFN esp_timer_get_time
 FILE              *old_stdout;
 char stdout_buf[128];
#else
 #define UDPlogsendSTACKsize 320
 #define GETTIMEFN sdk_system_get_time
 _WriteFunction    *old_stdout_write;
#endif //ESP_PLATFORM
char udplogstring[UDPLOGSTRING_SIZE]={0};
int  udplogmembers=0,udploglSocket,udplogstring_len=0;
struct sockaddr_in udplogsClntAddr;

void udplog_send(void *pvParameters){
    int timeout=1,n,holdoff=20*HOLDOFF; //should represent 2 seconds after trigger
    long unsigned int length;
    struct sockaddr_in sLocalAddr;
    char buffer[2];

   #ifdef ESP_PLATFORM
    uint64_t oldtime=0;
    esp_netif_ip_info_t info;
    esp_netif_t* esp_netif;
    while ((esp_netif=esp_netif_next_unsafe(NULL))==NULL || !esp_netif_is_netif_up(esp_netif) || esp_netif_get_ip_info(esp_netif,&info)!=ESP_OK || info.ip.addr==0) vTaskDelay(20);
   #else
    uint32_t oldtime=0;
    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) vTaskDelay(20); //Check if we have an IP every 200ms
   #endif //ESP_PLATFORM

    udploglSocket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
    memset((char *)&udplogsClntAddr,  0, sizeof(udplogsClntAddr));
    /*Destination*/
    udplogsClntAddr.sin_family = AF_INET;
    udplogsClntAddr.sin_len = sizeof(udplogsClntAddr);
    udplogsClntAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // inet_addr("255.255.255.255");  or htonl(INADDR_BROADCAST);
    udplogsClntAddr.sin_port =htons(45677);
    /*Source*/
    sLocalAddr.sin_family = AF_INET;
    sLocalAddr.sin_len = sizeof(sLocalAddr);
    sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sLocalAddr.sin_port =htons(45676);
    lwip_bind(udploglSocket, (struct sockaddr *)&sLocalAddr, sizeof(sLocalAddr));

    lwip_sendto(udploglSocket, INIT, strlen(INIT), 0, (struct sockaddr *)&udplogsClntAddr, sizeof(udplogsClntAddr));
    fd_set   rset;
    FD_ZERO(&rset); // clear the descriptor set
    struct timeval tv = { 0, 10000 }; /* 10 millisecond cycle time for the while(1) loop */

    while (1) {
        FD_SET(udploglSocket,&rset);
        select(udploglSocket+1, &rset, NULL, NULL, &tv); //is a delay of 10ms
        if (FD_ISSET(udploglSocket, &rset)) {
            length = sizeof(udplogsClntAddr);
            n = recvfrom(udploglSocket, (char *)buffer, 2, 0, ( struct sockaddr *) &udplogsClntAddr, &length);
            if (n==1) {
                udplogmembers=1; holdoff=0; timeout=(int)buffer[0];
                oldtime= GETTIMEFN()/1000;
            }
        }
        if ((holdoff<=0 && udplogstring_len) || (holdoff<HOLDOFF && udplogstring_len>UDPLOGSTRING_SIZE*3/4)) {
            if (( GETTIMEFN()/1000-oldtime)>timeout*1000) udplogmembers=0;
            if( xSemaphoreTake( xUDPlogSemaphore, ( TickType_t ) 1 ) == pdTRUE ) {
                UDPLOGFLUSHNOFIT(UDPLOGSTRING_SIZE);
                xSemaphoreGive( xUDPlogSemaphore );
                holdoff=HOLDOFF;
            }
        }
        if (holdoff<=0) holdoff=HOLDOFF;
        holdoff--;
    }
}

#ifdef  UDPLOG_PRINTF_TO_UDP
 #pragma message "UDPLOG_PRINTF_TO_UDP activated"
 #ifdef ESP_PLATFORM
  static int new_stdout_write(void* cookie, const char* ptr, int len) {
 #else
  static int new_stdout_write(struct _reent *r, int fd, const void *ptr, size_t len) {
 #endif //ESP_PLATFORM
   #ifdef  UDPLOG_PRINTF_ALSO_SERIAL
    OLDWRITEFN(ptr,len);
   #endif  //UDPLOG_PRINTF_ALSO_SERIAL
    if (xSemaphoreTake( xUDPlogSemaphore, ( TickType_t ) 1 ) == pdTRUE) {
        UDPLOGFLUSHNOFIT(len);
        if (len>UDPLOGSTRING_SIZE) { //normally never used since printf only sends in chunks of 128 bytes at a time
            if (udplogmembers) lwip_sendto(udploglSocket, ptr, len,
                                             0, (struct sockaddr *)&udplogsClntAddr,sizeof(udplogsClntAddr));
        } else {
            memcpy(udplogstring+udplogstring_len,ptr,len); udplogstring_len+=len;
        }
        xSemaphoreGive( xUDPlogSemaphore );
    }
    return len;
  }
#endif  //UDPLOG_PRINTF_TO_UDP

void udplog_init(int prio) {
  #ifdef ESP_PLATFORM
    old_stdout=_GLOBAL_REENT->_stdout;
   #ifdef  UDPLOG_PRINTF_TO_UDP
    _GLOBAL_REENT->_stdout=fwopen(NULL, &new_stdout_write);
    setvbuf(stdout, stdout_buf, _IOLBF, sizeof(stdout_buf)); // enable line buffering for this stream (to be similar to the regular UART-based output)
   #endif  //UDPLOG_PRINTF_TO_UDP
  #else
    old_stdout_write=get_write_stdout();
   #ifdef  UDPLOG_PRINTF_TO_UDP
    set_write_stdout(new_stdout_write);
   #endif  //UDPLOG_PRINTF_TO_UDP
  #endif //ESP_PLATFORM
    
    xUDPlogSemaphore   = xSemaphoreCreateMutex();
    xTaskCreate(udplog_send, "udplog", UDPlogsendSTACKsize, NULL, prio, NULL);
}
