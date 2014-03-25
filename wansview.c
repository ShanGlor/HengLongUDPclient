

#include "wansview.h"

int wvcamctrl(char* ip, char* username, char* password, int cmd)
{
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    char sendBuff[1024];
    struct sockaddr_in serv_addr;

    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(80);

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

    sprintf(sendBuff, "GET /decoder_control.cgi?onestep=1&command=%d&user=%s&pwd=%s HTTP/1.1\r\nHost: %s\r\n\r\n", cmd, username, password, ip);

    n = write(sockfd, sendBuff, strlen(sendBuff));

    while ( (n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0)
    {
    }


    if(n < 0)
    {
        printf("\n Read error \n");
    }
    close(sockfd);
    return 0;
}

int cam_down(char* ip, char* username, char* password)
{
    return wvcamctrl(ip, username, password, 0);

}

int cam_up(char* ip, char* username, char* password)
{
    return wvcamctrl(ip, username, password, 2);
}

int cam_cw(char* ip, char* username, char* password)
{
    return wvcamctrl(ip, username, password, 4);
}

int cam_ccw(char* ip, char* username, char* password)
{
    return wvcamctrl(ip, username, password, 6);
}


int cam_nul(char* ip, char* username, char* password)
{
    int i;
    for(i=0;i<20;i++){
        usleep(300000);
        cam_down(ip, username, password);
    }
    for(i=0;i<80;i++){

        usleep(300000);
        cam_ccw(ip, username, password);
    }
    for(i=0;i<40;i++){
        usleep(300000);
        cam_cw(ip, username, password);
    }
    return 0;
}
