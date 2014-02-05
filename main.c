
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

typedef struct outtty_t
{
    int16_t motor_r;
    int16_t motor_l;
    int16_t servo_tilt;
    int16_t servo_pan;
} outtty_t;

typedef struct input_thread_t
{
    char* filename;
    int frame;
    henglong_t hl;
    outtty_t outtty;
} input_thread_t;

typedef struct RCdatagram_t
{
    uint16_t frame_nbr;
    uint64_t time_us;
    int frame_recv;
    uint8_t clisel;
    uint8_t clinbr;
    uint8_t client_selected;
    unsigned char servoff;
    outtty_t outtty;
} RCdatagram_t;

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
            printf("%d %d\n", ev.code, ev.value);
            args->frame = data2frame(event2data(&args->hl, ev));
            args->outtty.motor_r = (2*0b01111 - args->hl.velocity - args->hl.direction)*40;
            args->outtty.motor_l = (- args->hl.velocity + args->hl.direction)*40;
            args->outtty.servo_pan += args->hl.pan_left - args->hl.pan_right;
            if(args->outtty.servo_pan>50) args->outtty.servo_pan = 50;
            if(args->outtty.servo_pan<-50) args->outtty.servo_pan = -50;
            if(args->outtty.servo_tilt>50) args->outtty.servo_tilt = 50;
            if(args->outtty.servo_tilt<-50) args->outtty.servo_tilt = -50;
            args->outtty.servo_tilt += args->hl.tilt_up - args->hl.tilt_down;
            if(args->hl.ignation) {
                args->outtty.servo_pan = 0;
                args->outtty.servo_tilt= 0;
            }
        }
        // quit
        if(16==ev.code && 1==ev.value) break;
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
    int n;
    int frame_refl;
    uint64_t time_us_refl;
    uint16_t frame_nbr_refl;
    refl_thread_args_t* refl_thread_args_ptr;
    uint8_t clinbr, clisel;
    unsigned char servoff;
    RCdatagram_t refldata;


    printf("pthread refl started\n");

    refl_thread_args_ptr = (refl_thread_args_t*) arg;

    while(1){
        n=recvfrom(refl_thread_args_ptr->sockfd,&refldata,64,0,NULL,NULL);


        frame_nbr_refl = refldata.frame_nbr;

        time_us_refl = refldata.time_us;


        frame_refl = refldata.frame_recv;

        clinbr = refldata.clinbr;
        clisel = refldata.clisel;
        servoff = refldata.servoff;
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
    int frame = 0;
    uint16_t frame_nbr;
    int sockfd, n_send;
    struct sockaddr_in servaddr;
    uint64_t time_us;
    henglongconf_t conf;
    RCdatagram_t senddata;


    if(2!=argc){
        printf("\nThis program is intented to be run on the PC as client to control the server on the heng long tank. \n\n USAGE: UDPclient client.config\n\n Copyright (C) 2014 Stefan Helmert <stefan.helmert@gmx.net>\n\n");
        return 0;
    }

    inithenglong(&input_thread_args.hl);

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

        frame = input_thread_args.frame;

        time_us = get_us();
        frame_nbr++;
        refl_thread_args.frame_nbr_send = frame_nbr;
        if(refl_thread_args.timestamps[frame_nbr % conf.timeout]) printf("*** Frameloss detected! Lost frame from local time: %" PRIu64 "\n", refl_thread_args.timestamps[frame_nbr % conf.timeout]);
        refl_thread_args.timestamps[frame_nbr % conf.timeout] = time_us;

        senddata.frame_nbr = frame_nbr;
        senddata.time_us = time_us;
        senddata.frame_recv = frame;
        senddata.clinbr = conf.clinbr;
        senddata.clisel = input_thread_args.hl.clisel;
        senddata.servoff = input_thread_args.hl.servoff;
        senddata.outtty = input_thread_args.outtty;

        n_send = sendto(sockfd, &senddata, sizeof(senddata), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        printf("SEND FRAME -- FRM_NBR: %5d,               BYTES send: %3d, SEND_FRM: %#x, CLINBR: %d, CLISEL: %d, SERVOFF: %d\n", frame_nbr, n_send, frame, conf.clinbr, input_thread_args.hl.clisel, input_thread_args.hl.servoff);
        if(pthread_kill(inpthread, 0)) break;
    }

    return 0;
}



