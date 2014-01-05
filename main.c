
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

typedef struct input_thread_t
{
    char* filename;
    struct input_event event;
} input_thread_t;


void *input_thread_fcn(void * arg)
{
    printf("pthread started\n");

    struct input_event ev[2];
    int result = 0;
    int fevdev;
    int size = sizeof(struct input_event);
    int rd;
    int value;
    input_thread_t* args;

    args = (input_thread_t*) arg;
    fevdev = open(args->filename, O_RDONLY);
    if (fevdev == -1) {
        printf("Failed to open event device.\n");
        exit(1);
    }

    while (1)
    {
        if ((rd = read(fevdev, ev, size * 2)) < size) {
            break;
        }

        value = ev[0].value;

        args->event = ev[1];
        // quit
        if(16==args->event.code && 1==args->event.value) break;
    }

    printf("Exiting.\n");
    result = ioctl(fevdev, EVIOCGRAB, 1);
    close(fevdev);

    pthread_exit(0);
}

int CRC(int data)
{
  int c;
  c = 0;
  c ^= data & 0x03;
  c ^= (data >> 2) & 0x0F;
  c ^= (data >> 6) & 0x0F;
  c ^= (data >> 10) & 0x0F;
  c ^= (data >> 14) & 0x0F;
  c ^= (data >> 18) & 0x0F;
  return c;
}

int data2frame(int data)
{
  int frame;
  frame = 0;
  frame |= CRC(data) << 2;
  frame |= data << 6;
  frame |= 0xFE000000;
  return frame;
}


int values2data(int velocity, int direction, int ignation, int mg, int fire, int turretelev, int turret_left, int turret_right, int recoil)
{
    int data = 0;
    data = (mg & 1) | (ignation & 1) << 1 | (direction & 0b11111) << 2 | (fire & 1) << 7 | (turretelev & 1) << 8 | (turret_left & 1) << 9 | (turret_right & 1) << 10 | (recoil & 1) << 11 | (velocity & 0b11111) << 12;
    return data;
}

typedef struct henglong_t
{
    int velocity, direction;
    int ignation, mg, fire, turretelev, turret_left, turret_right, recoil;
} henglong_t;

void inithenglong(henglong_t* henglong)
{
    henglong->velocity     = 0b10000;
    henglong->direction    = 0b01111;
    henglong->ignation     =       0;
    henglong->mg           =       0;
    henglong->fire         =       0;
    henglong->turretelev   =       0;
    henglong->turret_left  =       0;
    henglong->turret_right =       0;
    henglong->recoil       =       0;
}


int event2data(henglong_t* henglong, struct input_event event)
{
        if(108==event.code){
            if(event.value){
                henglong->velocity = 0b11111;
            }else{
                henglong->velocity = 0b10000;
            }
        }
        if(103==event.code){
            if(event.value){
                henglong->velocity = 0b00000;
            }else{
                henglong->velocity = 0b10000;
            }
        }
        if(105==event.code){
            if(event.value){
                henglong->direction = 0b00000;
            }else{
                henglong->direction = 0b01111;
            }
        }
        if(106==event.code){
            if(event.value){
                henglong->direction = 0b11111;
            }else{
                henglong->direction = 0b01111;
            }
        }
        if(23==event.code){
            if(event.value){
                henglong->ignation = 1;
            }else{
                henglong->ignation = 0;
            }
        }
        if(34==event.code){
            if(event.value){
                henglong->mg = 1;
            }else{
                henglong->mg = 0;
            }
        }
        if(33==event.code){
            if(event.value){
                henglong->fire = 1;
            }else{
                henglong->fire = 0;
            }
        }
        if(29==event.code){
            if(event.value){
                henglong->turret_left = 1;
            }else{
                henglong->turret_left = 0;
            }
        }
        if(20==event.code){
            if(event.value){
                henglong->turretelev = 1;
            }else{
                henglong->turretelev = 0;
            }
        }
        if(19==event.code){
            if(event.value){
                henglong->recoil = 1;
            }else{
                henglong->recoil = 0;
            }
        }

    return values2data(henglong->velocity, henglong->direction, henglong->ignation, henglong->mg, henglong->fire, henglong->turretelev, henglong->turret_left, henglong->turret_right, henglong->recoil);
}

typedef struct refl_thread_args_t
{
    int sockfd;
    uint64_t timestamps[256];
    uint16_t frame_nbr_send;
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
        printf("REFL FRAME -- FRM_NBR: %5d, RTT: %7llu, BYTES recv: %3d, REFL_FRM: %#x\n", frame_nbr_refl, get_us() - time_us_refl, n, frame_refl);
        refl_thread_args_ptr->timestamps[frame_nbr_refl&0x0F] = 0;
    }
    pthread_exit(0);
}


int main(int argc, char* argv[])
{
    pthread_t inpthread, refl_thread;
    pthread_attr_t inp_thread_attr;
    input_thread_t input_thread_args;
    refl_thread_args_t refl_thread_args;
    int inpdetachstate;
    int i=0;
    int frame;
    uint16_t frame_nbr;
    uint16_t frame_nbr_refl;

    int velocity = 0b10000, direction=0b01111;
    int ignation = 0, mg = 0, fire = 0, turretelev = 0, turret_left = 0, turret_right = 0, recoil = 0;


    int sockfd,n, n_send;
    struct sockaddr_in servaddr,cliaddr;
    unsigned char sendline[64];
    unsigned char recvline[64];
    unsigned char tmpchr;
    henglong_t hl1;

    uint64_t time_us, time_us_refl;
    int frame_refl;

    inithenglong(&hl1);

    input_thread_args.filename = argv[1];


    pthread_attr_init(&inp_thread_attr);
    if (pthread_create(&inpthread, &inp_thread_attr, input_thread_fcn , (void *) &input_thread_args)) printf("failed to create thread %d\n", i);


    sockfd = socket(AF_INET,SOCK_DGRAM,0);

    refl_thread_args.sockfd = sockfd;

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=inet_addr(argv[2]);
    servaddr.sin_port=htons(32000);

    if (pthread_create(&refl_thread, NULL, refl_thread_fcn, (void *) &refl_thread_args)) printf("failed to create thread %d\n", i);

    memset(refl_thread_args.timestamps, 0, 256);
    frame_nbr = 0;
    while(1){
        usleep(100000);
        //printf("%d %d %d %d %d %d %d %d %d | %d %d %d %d\n", velocity, direction, ignation, mg, fire, turret_left, turret_right, turretelev, recoil, input_thread_args.event.code, input_thread_args.event.value, input_thread_args.event.type, input_thread_args.event.time);

        frame = data2frame(event2data(&hl1, input_thread_args.event));
        time_us = get_us();
        frame_nbr++;
        refl_thread_args.frame_nbr_send = frame_nbr;
        if(refl_thread_args.timestamps[frame_nbr & 0x0F]) printf("*** Frameloss detected! Lost frame from local time: %llu \n", refl_thread_args.timestamps[frame_nbr & 0x0F]);
        refl_thread_args.timestamps[frame_nbr & 0x0F] = time_us;

        for(i=0;i<2;i++){
            sendline[i] = (frame_nbr >> i*8) & 0xFF;
        }
        for(i=0;i<8;i++){
            sendline[i+2] = (time_us >> i*8) & 0xFF;
        }
        for(i=0;i<4;i++){
            sendline[i+10] = (frame >> i*8) & 0xFF;
        }


        n_send = sendto(sockfd, sendline, 16, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));





        printf("SEND FRAME -- FRM_NBR: %5d,               BYTES send: %3d, SEND_FRM: %#x\n", frame_nbr, n_send, frame);
        if(pthread_kill(inpthread, 0)) break;
    }

    return 0;
}



