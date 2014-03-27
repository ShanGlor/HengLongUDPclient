
#include "checkvideo.h"

int connectionstate(char* remip, uint16_t remport, uint32_t state)
{
    FILE* tcpfile;
    char line[256];
    uint32_t remipi;
    uint32_t remporti;
    uint32_t statei;

    statei = 0;
    remipi = 0;
    remporti = 0;
    tcpfile = fopen("/proc/net/tcp","r");
    while(fgets(line,256,tcpfile)){
        sscanf(line, "%*u: %*x:%*x %x:%x %x", &remipi, &remporti, &statei);
        if((inet_addr(remip)==remipi) & (remport==remporti) & (state==statei)) {
            fclose(tcpfile);
            return 1;
        }
    }
    fclose(tcpfile);
    return 0;
}

int checkvideo(char* remip, uint16_t remport)
{
    if(0==remip[0]) return 2;
    if(1==connectionstate(remip, remport, 1)) return 1;
    return 0;
}
