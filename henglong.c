
#include "henglong.h"

int CRC(int data)
{
  int c;
  c = 0;
  c ^= data         & 0x03;
  c ^= (data >>  2) & 0x0F;
  c ^= (data >>  6) & 0x0F;
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

void inithenglong(henglong_t* henglong)
{
    henglong->velocity     = 0b01111;
    henglong->direction    = 0b01111;
    henglong->ignation     =       0;
    henglong->mg           =       0;
    henglong->fire         =       0;
    henglong->turretelev   =       0;
    henglong->turret_left  =       0;
    henglong->turret_right =       0;
    henglong->recoil       =       0;
    henglong->clisel       =       0;
    henglong->servoff      =       0;
    henglong->tilt_up      =       0;
    henglong->tilt_down    =       0;
    henglong->pan_left     =       0;
    henglong->pan_right    =       0;

}


int event2data(henglong_t* henglong, struct input_event event)
{
    //printf("%d\n", event.code);
    if(107==event.code){
        if(event.value){
            henglong->servoff = 1;
        }else{
            henglong->servoff = 0;
        }
    }
    if(17==event.code){
        if(event.value){
            henglong->tilt_up = 1;
        }else{
            henglong->tilt_up = 0;
        }
    }
    if(31==event.code){
        if(event.value){
            henglong->tilt_down = 1;
        }else{
            henglong->tilt_down = 0;
        }
    }
    if(30==event.code){
        if(event.value){
            henglong->pan_left = 1;
        }else{
            henglong->pan_left = 0;
        }
    }
    if(32==event.code){
        if(event.value){
            henglong->pan_right = 1;
        }else{
            henglong->pan_right = 0;
        }
    }
    if(2<=event.code && 11>=event.code){
        if(event.value){
            henglong->clisel = (event.code - 1) % 10;
        }
    }
    if(108==event.code){
        if(event.value){
            henglong->velocity = 0b11000;
        }else{
            henglong->velocity = 0b01111;
        }
    }
    if(103==event.code){
        if(event.value){
            henglong->velocity = 0b00011;
        }else{
            henglong->velocity = 0b01111;
        }
    }
    if(105==event.code){
        if(event.value){
            henglong->direction = 0b00011;
        }else{
            henglong->direction = 0b01111;
        }
    }
    if(106==event.code){
        if(event.value){
            henglong->direction = 0b11100;
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
    if(56==event.code){
        if(event.value){
            henglong->turret_right = 1;
        }else{
            henglong->turret_right = 0;
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
