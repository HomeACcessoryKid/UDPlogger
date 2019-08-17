// (c) 2018-2019 HomeAccessoryKid

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_system.h> //for sdk_system_get_netif
// //#include <espressif/esp_system.h> //for timestamp report only
// #include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <lwip/sockets.h>

#include <lwip/raw.h>
#include <udplogger.h>

SemaphoreHandle_t xUDPlogSemaphore = NULL;
_WriteFunction    *old_stdout_write;
char udplogstring[UDPLOGSTRING_SIZE]={0};
int  udplogstring_len=0;
int  members=0,oldtime=0;

void udplog_send(void *pvParameters){
    int lSocket,timeout=1,n,holdoff=200; //should represent 2 seconds after trigger
    unsigned int len;
    struct sockaddr_in sLocalAddr, sClntAddr;
    char buffer[2];

    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) vTaskDelay(20); //Check if we have an IP every 200ms

    lSocket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
    memset((char *)&sClntAddr,  0, sizeof(sClntAddr));
    /*Destination*/
    sClntAddr.sin_family = AF_INET;
    sClntAddr.sin_len = sizeof(sClntAddr);
    sClntAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // inet_addr("255.255.255.255");  or htonl(INADDR_BROADCAST);
    sClntAddr.sin_port =htons(45678);
    /*Source*/
    sLocalAddr.sin_family = AF_INET;
    sLocalAddr.sin_len = sizeof(sLocalAddr);
    sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sLocalAddr.sin_port =htons(44444);
    lwip_bind(lSocket, (struct sockaddr *)&sLocalAddr, sizeof(sLocalAddr));

    lwip_sendto(lSocket, INIT, strlen(INIT), 0, (struct sockaddr *)&sClntAddr, sizeof(sClntAddr));
    fd_set rset;
    struct timeval tv = { 0, 10000 }; /* 10 millisecond cycle time for the while(1) loop */
    FD_ZERO(&rset); // clear the descriptor set

    while (1) {
        FD_SET(lSocket,&rset);
        select(lSocket+1, &rset, NULL, NULL, &tv); //is a delay of 10ms
        if (FD_ISSET(lSocket, &rset)) {
            len = sizeof(sClntAddr);
            n = recvfrom(lSocket, (char *)buffer, 2, 0, ( struct sockaddr *) &sClntAddr, &len);
            if (n==1) {members=1; holdoff=0; timeout=(int)buffer[0];oldtime=sdk_system_get_time()/1000;}
        }
        if ((holdoff<=0 && udplogstring_len) || udplogstring_len>700) {
            if ((sdk_system_get_time()/1000-oldtime)>timeout*1000) members=0;
            if( xSemaphoreTake( xUDPlogSemaphore, ( TickType_t ) 1 ) == pdTRUE ) {
                if (members) lwip_sendto(lSocket, udplogstring, udplogstring_len, 0, (struct sockaddr *)&sClntAddr, sizeof(sClntAddr));
                UDPLSO("%3d->%d @ %d\n",udplogstring_len,members,sdk_system_get_time()/1000);
                udplogstring_len=0;
                xSemaphoreGive( xUDPlogSemaphore );
                holdoff=10;
            }
        }
        if (holdoff<=0) holdoff=10; //sends output every 100ms if not more than 700 bytes
        holdoff--;
    }
}

#ifdef  UDPLOG_PRINTF_TO_UDP
#pragma message "UDPLOG_PRINTF_TO_UDP activated"
 static int new_stdout_write(struct _reent *r, int fd, const void *ptr, size_t len) {
    #ifdef  UDPLOG_PRINTF_ALSO_SERIAL
     old_stdout_write(NULL,0,ptr,len);
    #endif  //ifdef UDPLOG_PRINTF_ALSO_SERIAL
    if (xSemaphoreTake( xUDPlogSemaphore, ( TickType_t ) 1 ) == pdTRUE) {
        memcpy(udplogstring+udplogstring_len,ptr,len); udplogstring_len+=len;
        xSemaphoreGive( xUDPlogSemaphore );
    } else UDPLSO("skipped a UDPLOG\n");
    return len;
 }
#endif  //ifdef UDPLOG_PRINTF_TO_UDP

void udplog_init(int prio) {
    old_stdout_write=get_write_stdout();
    #ifdef  UDPLOG_PRINTF_TO_UDP
     set_write_stdout(new_stdout_write);
    #endif  //ifdef UDPLOG_PRINTF_TO_UDP
    
    xUDPlogSemaphore   = xSemaphoreCreateMutex();
    xTaskCreate(udplog_send, "logsend", 512, NULL, prio, NULL);
}
