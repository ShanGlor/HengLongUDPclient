#ifndef WANSVIEW_H_INCLUDED
#define WANSVIEW_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/time.h>


int wvcamctrl(char* ip, int cmd);
int cam_down(char* ip);
int cam_up(char* ip);
int cam_cw(char* ip);
int cam_ccw(char* ip);
int cam_nul(char* ip);

#endif // WANSVIEW_H_INCLUDED
