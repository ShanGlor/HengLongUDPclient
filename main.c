
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

int main(int argc, char* argv[])
{
    pthread_t inpthread;
    pthread_attr_t inp_thread_attr;
    input_thread_t input_thread_args;
    int inpdetachstate;
    int i=0;
    int frame;

    int velocity = 0b10000, direction=0b01111;
    int ignation = 0, mg = 0, fire = 0, turretelev = 0, turret_left = 0, turret_right = 0, recoil = 0, smoke = 0;


    input_thread_args.filename = argv[1];


    pthread_attr_init(&inp_thread_attr);
    if (pthread_create(&inpthread, &inp_thread_attr, input_thread_fcn , (void *) &input_thread_args)) printf("failed to create thread %d\n", i);


    while(1){
        usleep(100000);
        printf("%d %d %d %d %d %d %d %d %d %d | %d %d %d %d\n", velocity, direction, ignation, mg, fire, turret_left, turret_right, turretelev, recoil, smoke, input_thread_args.event.code, input_thread_args.event.value, input_thread_args.event.type, input_thread_args.event.time);
        if(108==input_thread_args.event.code){
            if(input_thread_args.event.value){
                velocity = 0b11111;
            }else{
                velocity = 0b10000;
            }
        }
        if(103==input_thread_args.event.code){
            if(input_thread_args.event.value){
                velocity = 0b00000;
            }else{
                velocity = 0b10000;
            }
        }
        if(105==input_thread_args.event.code){
            if(input_thread_args.event.value){
                direction = 0b00000;
            }else{
                direction = 0b01111;
            }
        }
        if(106==input_thread_args.event.code){
            if(input_thread_args.event.value){
                direction = 0b11111;
            }else{
                direction = 0b01111;
            }
        }
        if(23==input_thread_args.event.code){
            if(input_thread_args.event.value){
                ignation = 1;
            }else{
                ignation = 0;
            }
        }
        if(34==input_thread_args.event.code){
            if(input_thread_args.event.value){
                mg = 1;
            }else{
                mg = 0;
            }
        }
        if(33==input_thread_args.event.code){
            if(input_thread_args.event.value){
                fire = 1;
            }else{
                fire = 0;
            }
        }
        if(29==input_thread_args.event.code){
            if(input_thread_args.event.value){
                turret_left = 1;
            }else{
                turret_left = 0;
            }
        }
        if(20==input_thread_args.event.code){
            if(input_thread_args.event.value){
                turretelev = 1;
            }else{
                turretelev = 0;
            }
        }
        if(19==input_thread_args.event.code){
            if(input_thread_args.event.value){
                recoil = 1;
            }else{
                recoil = 0;
            }
        }

        frame = data2frame(values2data(velocity, direction, ignation, mg, fire, turretelev, turret_left, turret_right, recoil));
        printf("%#x\n", frame);
        if(pthread_kill(inpthread, 0)) break;
    }

    return 0;
}



/* Sample UDP client */
/*
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

int main(int argc, char**argv)
{

    FILE *kbfp;
    char key;

    kbfp = fopen("/dev/input/event2", "rb");

    while(1){
        key = fgetc(kbfp);
        printf("%c\n", key);
    }
    fclose(kbfp);

   int sockfd,n;
   struct sockaddr_in servaddr,cliaddr;
   char sendline[1000];
   char recvline[1000];

   if (argc != 2)
   {
      printf("usage:  udpcli <IP address>\n");
      exit(1);
   }

   sockfd=socket(AF_INET,SOCK_DGRAM,0);

   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr=inet_addr(argv[1]);
   servaddr.sin_port=htons(32000);

   while (fgets(sendline, 10000,stdin) != NULL)
   {
      sendto(sockfd,sendline,strlen(sendline),0,
             (struct sockaddr *)&servaddr,sizeof(servaddr));
      n=recvfrom(sockfd,recvline,10000,0,NULL,NULL);
      recvline[n]=0;
      fputs(recvline,stdout);
   }
}
*/
