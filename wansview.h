#ifndef WANSVIEW_H_INCLUDED
#define WANSVIEW_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/time.h>

int wvcamctrl(char* ip, char* username, char* password, int cmd);
int cam_down(char* ip, char* username, char* password);
int cam_up(char* ip, char* username, char* password);
int cam_cw(char* ip, char* username, char* password);
int cam_ccw(char* ip, char* username, char* password);
int cam_nul(char* ip, char* username, char* password);

#endif // WANSVIEW_H_INCLUDED
