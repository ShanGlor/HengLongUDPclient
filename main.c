
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
#include <linux/joystick.h>

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

void initouttty(outtty_t* tty)
{
    tty->motor_l = 0;
    tty->motor_r = 0;
    tty->servo_pan = 0;
    tty->servo_tilt = 0;
}

typedef struct RCdatagram_t
{
    uint16_t frame_nbr;
    uint64_t time_us;
    int32_t frame_recv;
    uint8_t clisel;
    uint8_t clinbr;
    uint8_t client_selected;
    uint8_t servoff;
    outtty_t outtty;
} __attribute__ ((packed)) RCdatagram_t;

void *keyboard_thread_fcn(void * arg)
{
    printf("pthread input started\n");

    struct input_event ev;
    int fevdev;
    int size = sizeof(ev);
    int rd;
    input_thread_t* args;

    args = (input_thread_t*) arg;
    fevdev = open(args->filename, O_RDONLY);
    if (fevdev == -1) {
        printf("Failed to open keyboard device.\n");
        pthread_exit(0);
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

    printf("Exiting keyboard thread.\n");
    ioctl(fevdev, EVIOCGRAB, 1);
    close(fevdev);

    pthread_exit(0);
}


void *joystick_thread_fcn(void * arg)
{
    printf("pthread input started\n");

    struct js_event ev;
    struct JS_DATA_TYPE jsdata;
    int fevdev;
    int size = sizeof(ev);
    int rd;
    outtty_t outttyloc;
    input_thread_t* args;

    args = (input_thread_t*) arg;
    fevdev = open(args->filename, O_RDONLY);
    if (fevdev == -1) {
        printf("Failed to open joystick device.\n");
        pthread_exit(0);
    }

    jsdata.x = 0;
    jsdata.y = 0;
    jsdata.buttons = 0;

    while (1)
    {
        if ((rd = read(fevdev, &ev, size)) < size) {
            break;
        }

        if(JS_EVENT_BUTTON == ev.type) {
            //printf("%d %d %d\n", ev.type, ev.value, ev.number);
            if(1==ev.value){
                jsdata.buttons |= 1<<ev.number;
            }
            if(0==ev.value){
                jsdata.buttons &= ~(1<<ev.number);
            }
        }
        if(JS_EVENT_AXIS == ev.type){
            //printf("%d %d %d\n", ev.type, ev.value, ev.number);
            if(0==ev.number){
                jsdata.x = ev.value;
            }
            if(1==ev.number){
                jsdata.y = -ev.value;
                if(-32768==ev.value){
                    jsdata.y = +32767;
                }
            }
        }
        outttyloc.motor_r = jsdata.y/30 - jsdata.x/30;
        outttyloc.motor_l = jsdata.y/30 + jsdata.x/30 ;
        if(1022<outttyloc.motor_r) outttyloc.motor_r = 1023;
        if(-1022>outttyloc.motor_r) outttyloc.motor_r = -1023;
        if(1022<outttyloc.motor_l) outttyloc.motor_l = 1023;
        if(-1022>outttyloc.motor_l) outttyloc.motor_l = -1023;

        args->outtty = outttyloc;

        printf("%6d %6d %4x\n", jsdata.x, jsdata.y, jsdata.buttons);
    }

    printf("Exiting joystick thread.\n");
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
        n=recvfrom(refl_thread_args_ptr->sockfd,&refldata,sizeof(refldata),0,NULL,NULL);


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
    char keyboarddevname[256];
    char joystickdevname[256];
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
    conf.port = 32000;
    conf.ip = inet_addr("127.0.0.1");
    conf.keyboarddevname[0] = 0;
    conf.joystickdevname[0] = 0;

    while(fgets(line, 256, configFile)){
        sscanf(line, "%16s %256s", parameter, value);
        if(0==strcmp(parameter,"KEYBOARD")){
            sscanf(value, "%256s", conf.keyboarddevname);
        }
        if(0==strcmp(parameter,"JOYSTICK")){
            sscanf(value, "%256s", conf.joystickdevname);
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
    pthread_t keybthread, joythread, refl_thread;
    input_thread_t keyboard_thread_args;
    input_thread_t joystick_thread_args;
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

    inithenglong(&keyboard_thread_args.hl);
    initouttty(&keyboard_thread_args.outtty);
    inithenglong(&joystick_thread_args.hl);
    initouttty(&joystick_thread_args.outtty);

    conf = getconfig(argv[1]);

    joystick_thread_args.filename = conf.joystickdevname;
    keyboard_thread_args.filename = conf.keyboarddevname;

    if(keyboard_thread_args.filename[0]){
        if (pthread_create(&keybthread, NULL, keyboard_thread_fcn , (void *) &keyboard_thread_args)) printf("failed to create keyboard thread\n");
    }else{
        printf("no keyboard!\n");
    }
    if(joystick_thread_args.filename[0]){
        if (pthread_create(&joythread, NULL, joystick_thread_fcn , (void *) &joystick_thread_args)) printf("failed to create joystick thread\n");
    }else{
        printf("no joystick!\n");
    }

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

        frame = 0xc0ffee;

        time_us = get_us();
        frame_nbr++;
        refl_thread_args.frame_nbr_send = frame_nbr;
        if(refl_thread_args.timestamps[frame_nbr % conf.timeout]) printf("*** Frameloss detected! Lost frame from local time: %" PRIu64 "\n", refl_thread_args.timestamps[frame_nbr % conf.timeout]);
        refl_thread_args.timestamps[frame_nbr % conf.timeout] = time_us;

        senddata.frame_nbr = frame_nbr;
        senddata.time_us = time_us;
        senddata.frame_recv = frame;
        senddata.clinbr = conf.clinbr;
        senddata.clisel = keyboard_thread_args.hl.clisel;
        senddata.servoff = keyboard_thread_args.hl.servoff;

        senddata.outtty.motor_l = keyboard_thread_args.outtty.motor_l + joystick_thread_args.outtty.motor_l;
        senddata.outtty.motor_r = keyboard_thread_args.outtty.motor_r + joystick_thread_args.outtty.motor_r;
        senddata.outtty.servo_pan = keyboard_thread_args.outtty.servo_pan + joystick_thread_args.outtty.servo_pan;
        senddata.outtty.servo_tilt = keyboard_thread_args.outtty.servo_tilt + joystick_thread_args.outtty.servo_tilt;

        if(1022<senddata.outtty.motor_l) senddata.outtty.motor_l = 1023;
        if(-1022>senddata.outtty.motor_l) senddata.outtty.motor_l = -1023;
        if(1022<senddata.outtty.motor_r) senddata.outtty.motor_r = 1023;
        if(-1022>senddata.outtty.motor_r) senddata.outtty.motor_r = -1023;

        n_send = sendto(sockfd, &senddata, sizeof(senddata), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        printf("SEND FRAME -- FRM_NBR: %5d,               BYTES send: %3d, SEND_FRM: %#x, CLINBR: %d, CLISEL: %d, SERVOFF: %d\n", frame_nbr, n_send, frame, senddata.clinbr, senddata.clisel, senddata.servoff);
        if(pthread_kill(keybthread, 0)) break;
    }

    return 0;
}



