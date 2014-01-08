
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "henglong.h"

typedef struct input_thread_t
{
    char* filename;
    struct input_event event;
} input_thread_t;


void *input_thread_fcn(void * arg)
{
    printf("pthread input started\n");

    struct input_event ev;
    int fevdev;
    int size = sizeof(struct input_event);
    int rd;
    input_thread_t* args;

    args = (input_thread_t*) arg;
    fevdev = open(args->filename, O_RDONLY);
    if (fevdev == -1) {
        printf("Failed to open event device.\n");
        exit(1);
    }

    while (1)
    {
        if ((rd = read(fevdev, &ev, size)) < size) {
            break;
        }

        if(EV_KEY == ev.type) {
            args->event = ev;
            printf("%d %d\n", ev.code, ev.value);

        }
        // quit
        if(16==args->event.code && 1==args->event.value) break;
    }

    printf("Exiting input thread.\n");
    ioctl(fevdev, EVIOCGRAB, 1);
    close(fevdev);

    pthread_exit(0);
}


typedef struct refl_thread_args_t
{
    int sockfd;
    uint64_t timestamps[256];
    uint16_t frame_nbr_send;
    uint8_t timeout;
} refl_thread_args_t;


uint64_t get_us(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (uint64_t)tv.tv_usec + 1000000* (uint64_t)tv.tv_sec;
}

void *refl_thread_fcn(void* arg)
{
    int n, i = 0;
    int frame_refl;
    uint64_t time_us_refl;
    unsigned char recvline[64];
    uint16_t frame_nbr_refl;
    refl_thread_args_t* refl_thread_args_ptr;
    uint8_t clinbr, clisel;
    unsigned char servoff;


    printf("pthread refl started\n");

    refl_thread_args_ptr = (refl_thread_args_t*) arg;

    while(1){
        n=recvfrom(refl_thread_args_ptr->sockfd,recvline,64,0,NULL,NULL);
        frame_nbr_refl = 0;
        for(i=0;i<2;i++){
            frame_nbr_refl |= recvline[i] << i*8;
        }
        time_us_refl = 0ULL;
        for(i=0;i<8;i++){
            time_us_refl |= ((uint64_t)recvline[i+2]) << i*8;
        }
        frame_refl = 0;
        for(i=0;i<4;i++){
            frame_refl |= recvline[i+10] << i*8;
        }
        clinbr = recvline[14];
        clisel = recvline[15];
        servoff = recvline[16];
        printf("REFL FRAME -- FRM_NBR: %5d, RTT: %7" PRIu64 ", BYTES recv: %3d, REFL_FRM: %#x, CLINBR: %d, CLISEL: %d, SERVOFF: %d\n", frame_nbr_refl, get_us() - time_us_refl, n, frame_refl, clinbr, clisel, servoff);
        refl_thread_args_ptr->timestamps[frame_nbr_refl % refl_thread_args_ptr->timeout] = 0;
    }
    pthread_exit(0);
}

typedef struct henglongconf_t
{
    uint32_t frame_us;
    char inpdevname[256];
    in_addr_t ip; // v4 only
    uint16_t port;
    uint8_t timeout;
    uint8_t clinbr;
} henglongconf_t;

henglongconf_t getconfig(char* conffilename)
{
    FILE *configFile;
    char line[256];
    char parameter[16], value[256];
    char ip[16];
    henglongconf_t conf;
    configFile = fopen(conffilename, "r");

    // defaults
    conf.timeout = 16;
    conf.frame_us = 100000;
    strcpy(conf.inpdevname, "/dev/input/event2");
    conf.port = 32000;
    conf.ip = inet_addr("127.0.0.1");

    while(fgets(line, 256, configFile)){
        sscanf(line, "%16s %256s", parameter, value);
        if(0==strcmp(parameter,"INPUTDEV")){
            sscanf(value, "%256s", conf.inpdevname);
        }
        if(0==strcmp(parameter,"FRAME_US")){
            sscanf(value, "%" SCNu32 , &conf.frame_us);
        }
        if(0==strcmp(parameter,"SERVER")){
            sscanf(value, "%16[^:]:%" SCNu16, ip, &conf.port);
            conf.ip = inet_addr(ip);
        }
        if(0==strcmp(parameter,"TIMEOUT")){
            sscanf(value, "%" SCNu8 , &conf.timeout);
        }
        if(0==strcmp(parameter,"CLINBR")){
            sscanf(value, "%" SCNu8 , &conf.clinbr);
        }
    }
    return conf;
}


int main(int argc, char* argv[])
{
    pthread_t inpthread, refl_thread;
    input_thread_t input_thread_args;
    refl_thread_args_t refl_thread_args;
    int i=0;
    int frame;
    uint16_t frame_nbr;
    int sockfd, n_send;
    struct sockaddr_in servaddr;
    unsigned char sendline[64];
    henglong_t hl1;
    uint64_t time_us;
    henglongconf_t conf;


    if(2!=argc){
        printf("\nThis program is intented to be run on the PC as client to control the server on the heng long tank. \n\n USAGE: UDPclient client.config\n\n Copyright (C) 2014 Stefan Helmert <stefan.helmert@gmx.net>\n\n");
        return 0;
    }

    inithenglong(&hl1);


    conf = getconfig(argv[1]);

    input_thread_args.filename = conf.inpdevname;


    if (pthread_create(&inpthread, NULL, input_thread_fcn , (void *) &input_thread_args)) printf("failed to create thread\n");


    sockfd = socket(AF_INET,SOCK_DGRAM,0);

    refl_thread_args.sockfd = sockfd;
    refl_thread_args.timeout = conf.timeout;

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=conf.ip;
    servaddr.sin_port=htons(conf.port);

    if (pthread_create(&refl_thread, NULL, refl_thread_fcn, (void *) &refl_thread_args)) printf("failed to create thread\n");

    memset(refl_thread_args.timestamps, 0, 256*sizeof(uint64_t));
    frame_nbr = 0;
    while(1){
        usleep(conf.frame_us);

        frame = data2frame(event2data(&hl1, input_thread_args.event));
        time_us = get_us();
        frame_nbr++;
        refl_thread_args.frame_nbr_send = frame_nbr;
        if(refl_thread_args.timestamps[frame_nbr % conf.timeout]) printf("*** Frameloss detected! Lost frame from local time: %" PRIu64 "\n", refl_thread_args.timestamps[frame_nbr % conf.timeout]);
        refl_thread_args.timestamps[frame_nbr % conf.timeout] = time_us;

        for(i=0;i<2;i++){
            sendline[i] = (frame_nbr >> i*8) & 0xFF;
        }
        for(i=0;i<8;i++){
            sendline[i+2] = (time_us >> i*8) & 0xFF;
        }
        for(i=0;i<4;i++){
            sendline[i+10] = (frame >> i*8) & 0xFF;
        }
        sendline[14] = conf.clinbr;
        sendline[15] = hl1.clisel;
        sendline[16] = hl1.servoff;

        n_send = sendto(sockfd, sendline, 32, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        printf("SEND FRAME -- FRM_NBR: %5d,               BYTES send: %3d, SEND_FRM: %#x, CLINBR: %d, CLISEL: %d, SERVOFF: %d\n", frame_nbr, n_send, frame, conf.clinbr, hl1.clisel, hl1.servoff);
        if(pthread_kill(inpthread, 0)) break;
    }

    return 0;
}



