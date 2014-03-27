#ifndef CHECKVIDEO_H_INCLUDED
#define CHECKVIDEO_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>

int connectionstate(char* remip, uint16_t remport, uint32_t state);
int checkvideo(char* remip, uint16_t remport);

#endif // CHECKVIDEO_H_INCLUDED
