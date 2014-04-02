
#include "checkkillswitch.h"


int getPage(char* ip, uint16_t port, char* url, char* recvBuffer, uint32_t size)
{
    int sockfd = 0, n = 0;
    //char recvBuff[1024];
    char sendBuff[1024];
    struct sockaddr_in serv_addr;

    memset(recvBuffer, '0',size);
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    }

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return 1;
    }

    sprintf(sendBuff, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", url, ip);

    n = write(sockfd, sendBuff, strlen(sendBuff));

    n = read(sockfd, recvBuffer, size-1);
    recvBuffer[n] = 0;

    if(n < 0)
    {
        printf("\n Read error \n");
    }
    close(sockfd);
    return n;
}

int checkkillswitch(char* ip, uint16_t port, char* url)
{
    char rb[1024];
    int n;
    if(0==ip[0]) return 0;
    getPage(ip, port, url, rb, sizeof(rb));
    n = strstr(rb, "true");
    if(NULL != n) return 1;
    return 0;
}
