/*
* remod:    rconmod.cpp
* date:     2007
* author:   degrave
*
* remot control
* remot control
*/

#include <stdio.h>

#ifndef WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#ifdef WIN32
#include "windows.h"
#define socklen_t int
#endif

#include "fpsgame.h"
#include "rconmod.h"
#include "rconmod_udp.h"
#include "remod.h"

extern int execute(const char *p);
namespace remod
{

namespace rcon
{
// check for \n in end of line
bool rconserver_udp::havenl(char *msg)
{
    for(int c = *msg; c; c = *++msg)
    {
        if(c=='\n') return true;
    }
    return false;
}

// add peer to broadcast list
bool rconserver_udp::addpeer(struct sockaddr_in addr)
{
    char *ipstr;
    string msg;
    for(int i=0; i<MAXRCONPEERS; i++)
    {
        if(rconpeers[i].logined==false)
        {
            rconpeers[i].addr=addr;
            rconpeers[i].logined=true;
            ipstr = inet_ntoa(addr.sin_addr);
            formatstring(msg)("Rcon: new peer [%s:%i]", ipstr, ntohs(addr.sin_port));
            conoutf(msg);
            return true;
        }
    }
    return false;
}

// update peer info when recive any data
void rconserver_udp::uppeer(struct sockaddr_in addr)
{
    // list all peers and update sockaddr information
    for(int i=0; i<MAXRCONPEERS; i++)
    {
        if(rconpeers[i].addr.sin_addr.s_addr==addr.sin_addr.s_addr && rconpeers[i].logined)
        {
            rconpeers[i].addr = addr;
        }
    }
}

// logout all peers
void rconserver_udp::logout()
{
    sendmsg("Logout due reach peer limit");
    // dont touch first 5 peers
    for(int i=4; i<MAXRCONPEERS; i++)
    {
        rconpeers[i].logined = false;
    }
}

// check peer state
bool rconserver_udp::logined(struct sockaddr_in addr, char *msg)
{
    // check all connected peers by ip and login
    for(int i=0; i<MAXRCONPEERS; i++)
    {
        if(rconpeers[i].addr.sin_addr.s_addr==addr.sin_addr.s_addr && rconpeers[i].logined)
        {
            // update peer information and return
            uppeer(addr);
            return true;
        }
    }

    // check password
    if(strncmp(rconpass, msg, strlen(rconpass))==0)
    {
        // if we don't have more slots logout all peers
        if(!addpeer(addr))
        {
            logout();
            addpeer(addr);
        }

        //
        return false;
    }
    else
    {
        // password not correct
        return false;
    }
}

//Init rcon module
rconserver_udp::rconserver_udp(int port=27070)
{
    if(!rconenable)
    {
        active = false;
        return;
    }

    sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(sock < 0)
    {
        active=false;
        conoutf("Rcon: cannot create socket");
    }
    else
    {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            conoutf("Rcon: can not bind socket");
            active = false;
        }
        else
        {
            addrlen = sizeof(fromaddr);
#ifdef WIN32
            u_long sockflag=1;
            ioctlsocket(sock, FIONBIO, &sockflag);
#else
            fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
            active=true;
            for(int i=0; i<MAXRCONPEERS; i++)
            {
                rconpeers[i].logined=false;
            }
            conoutf("Rcon: [udp] listen on port %d", port);
        }
    }
}

// send message to all logined peers
void rconserver_udp::sendmsg(const char *msg, int len)
{
    if(!active) return;

    char *data;

    // message must ends with \n
    if (msg[len-1] != '\n')
    {
        data = newstring(msg, len);
        data[len] = '\n';
        len++;
    }
    else
    {
        data = newstring(msg);
    }

    // send text to all peers
    for(int i=0; i<MAXRCONPEERS; i++)
    {
        if(rconpeers[i].logined)
        {
            sendto(sock, data, len, 0, (struct sockaddr *)&rconpeers[i].addr, addrlen);
        }
    }

    DELETEA(data);

}

void rconserver_udp::sendmsg(const char *msg)
{
    sendmsg(msg, strlen(msg));
}

// update on every server frame
void rconserver_udp::update()
{
    if(!active) return;

    int recvlen;
    // MAXBUF-1 for avoid buffer overflow
    recvlen=recvfrom(sock, buf, MAXBUF-1, 0, (struct sockaddr*)&fromaddr, (socklen_t*)&addrlen);
    if(recvlen>0)
    {
        if(logined(fromaddr, buf))
        {
            buf[recvlen] = '\0';
            execute(buf);
        }
    }
}
} // namespace rcon
} // namespace remod
