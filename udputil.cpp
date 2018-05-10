////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  File   : udputil.cpp
//  Author : Larry Frieson
//  Desc   : UDP Utility.  This application will allow you to generate UDP datagrams as broadcasts, or those sent directly
//           to another host, and can act as a server, listening for UDP datagrams coming in, on the selected port.
//  Date   : 05/08/2018
//
//  Copyright © 2018 MLinks Technologies. All rights reserved
//
//  Revision History:
//    05/08/2018 17:05:19 PM created.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "winsock.h"

#pragma comment(lib, "ws2_32.lib")

#ifndef  SD_BOTH
#define SD_BOTH         0x02
#endif

#define VERSION_INFO            1.1
#define MAX_DATAGRAM_MESSAGE    512

SOCKET  ServerSocket    = INVALID_SOCKET;
bool    bShutdown       = false;
char    *szAppName      = NULL;

typedef struct tagTDatagram {
    char Computername[MAX_COMPUTERNAME_LENGTH + 1];
    char Payload[MAX_DATAGRAM_MESSAGE + 1];
    short Counter;
    short Quit;
} TDatagram;

char *getSocketError(int err)
{
    static char msgbuf[256] = { '\0' };

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  msgbuf,
                  sizeof(msgbuf),
                  NULL);

    if (!*msgbuf) {
       sprintf(msgbuf, "(No Error Text) - %d", err);
    }
    return &msgbuf[0];
}

int DatagramServer(unsigned short nServerPort)
{
    UINT uiPacketNumber = 0;
    TDatagram datagram = { NULL };

    ServerSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ServerSocket == INVALID_SOCKET) {
        int err = WSAGetLastError();
        std::cerr << ("Could not create server datagram socket.  Error: (") << err << (") - ") << getSocketError(err);
        return 1;
    }
    sockaddr_in sin;
    ZeroMemory(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(nServerPort);
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(ServerSocket, (sockaddr *)&sin, sizeof(sin)) != 0) {
        int err = WSAGetLastError();
        std::cerr << ("Could not bind server datagram socket on port: ") << nServerPort <<  (".  Error: (") << err << (") - ") << getSocketError(err);
        return 1;
    }

    while (!bShutdown) {
        int nfs = 0;
        sockaddr_in fin;
        int fromlen = sizeof(fin);
        fd_set rfds;
        fd_set efds;
        FD_ZERO(&rfds);
        FD_SET(ServerSocket, &rfds);
        FD_ZERO(&efds);
        FD_SET(ServerSocket, &efds);
        nfs = ServerSocket + 1;
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        int err = select(nfs, &rfds, NULL, &efds, &tv);
        if (err < 0 && err == EAGAIN) {
            std::cerr << ("select() returned an error.  Error: (") << errno << (") - ") << strerror(errno);
            break;
        } else if (err == 0) {
            // Ignore timeout periods now.
            //std::cerr << ("Recv: ") << uiPacketNumber << (" valid datagrams during timeout period.") << std::endl;
            //uiPacketNumber = 0;
            continue;
        }
        if (FD_ISSET(ServerSocket, &efds)) {
            int error = WSAGetLastError();
            std::cerr << ("Socket exception occurred on socket.  Shutting down server.  Error: (") << error << (") - ") << getSocketError(error);
            break;
        }
        if (FD_ISSET(ServerSocket, &rfds)) {
            int r = recvfrom(ServerSocket, (char *)&datagram, sizeof(TDatagram), 0, (sockaddr *)&fin, &fromlen);
            if ((r > 0) && (r != sizeof(TDatagram))) {
                std::cerr << ("Received invalid datagram!  The ") << r << (" bytes received, will be ignored.") << std::endl;
                continue;
            } else if (r == sizeof(TDatagram)) {
                uiPacketNumber++;
                std::cout << ("*** DATAGRAM ***") << std::endl;
                if (datagram.Quit) {
                    std::cout << ("--- Shutdown Datagram receieved. ---") << std::endl;
                }
                std::cout << ("IPADR: ") << inet_ntoa(fin.sin_addr) << std::endl;
                std::cout << ("COUNT: ") << datagram.Counter << std::endl << ("WKSTN: ") << datagram.Computername << std::endl << ("PAYLD: ") << datagram.Payload << std::endl;
                if (datagram.Quit) {
                    std::cout << ("SHTDN: ") << (datagram.Quit ? ("TRUE") : ("FALSE")) << std::endl;
                    bShutdown = true;
                }
                std::cout << ("*** END DGRM ***") << std::endl;
            }
        }
        FD_CLR(ServerSocket, &rfds);
        FD_CLR(ServerSocket, &efds);
    }
    closesocket(ServerSocket);
    return 0;
}

bool SendDatagram(const char *pszHostAddr, unsigned short nPort, bool bBroadcast, const TDatagram *pData)
{
    bool bRetval = false;
    SOCKET s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s != INVALID_SOCKET) {
        sockaddr_in sin;
        ZeroMemory(&sin, sizeof(sin));
        if (bBroadcast) {
            BOOL tru = TRUE;
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&tru, sizeof(tru));
        }
        sin.sin_family = PF_INET;
        sin.sin_port = htons(nPort);
        sin.sin_addr.s_addr = bBroadcast ? INADDR_BROADCAST : inet_addr(pszHostAddr);
        if (sendto(s, (char *)pData, sizeof(TDatagram), 0, (sockaddr *)&sin, sizeof(sin)) > 0) {
            bRetval = true;
        } else {
            int error = WSAGetLastError();
            std::cerr << ("Datagram failed to ") << (bBroadcast ? ("broadcast") : ("send")) << (".  Error: (") << error << (") - ") << getSocketError(error);
        }
        closesocket(s);
    }
    return bRetval;
}

void GetHostName(TDatagram *pDatagram)
{
    TCHAR ComputerName[MAX_COMPUTERNAME_LENGTH + 1];
    ZeroMemory(ComputerName, sizeof(ComputerName));
    DWORD len = MAX_COMPUTERNAME_LENGTH;
    GetComputerName(pDatagram->Computername, &len);
}

BOOL __stdcall CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:  // Handle the CTRL+C signal.
            bShutdown = true;
            std::cerr << std::endl << ("***** Shutting down Datagram listener...") << std::endl;
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            return TRUE; 
        case CTRL_CLOSE_EVENT:
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            Sleep(500);
            bShutdown = true;
            std::cerr << std::endl << ("**** Closing application, shutting down Datagram listener...") << std::endl;
            return TRUE; 
        case CTRL_LOGOFF_EVENT:
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            Sleep(500);
            bShutdown = true;
            std::cerr << std::endl << ("**** User Logoff event occurred, shutting down Datagram listener.") << std::endl;
        case CTRL_SHUTDOWN_EVENT:
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            Sleep(500);
            bShutdown = true;
            std::cerr << std::endl << ("**** Windows Shutdown invoked!  Kindly closing all active threads...") << std::endl;
        case CTRL_BREAK_EVENT:
            return FALSE;   //Break out of the app, non-gracefully.
        default:
            return FALSE;
    }
}

unsigned short getPortNumber(const char *port)
{
    if (atoi(port) <= 0) {
        std::cerr << port << (" is an invalid port number.  Port must be a positive integer between 1 and ") << USHRT_MAX << (".") << std::endl;
        exit(1);
    }
    return static_cast<unsigned short>(atoi(port));
}

char *getApplicationName(char *arg)
{
    char *p = arg;
    char *s = p;

    while (*p) {
        if (*p == '\\' || *p == '/') {
            s = p;
        }
        p++;
    }
    return ++s;
}

int main(int argc, char *argv[])
{
    szAppName = getApplicationName(argv[0]);
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

    bool bBroadcast = false;
    unsigned short nPort = 0;

    if (argc < 2) {
        std::cerr << szAppName << (" - UDP Datagram Utility (C) 2018 MLinks Technologies, Inc.") << std::endl;
        std::cerr << ("Version ") << VERSION_INFO << (" by Larry Frieson") << std::endl;
        std::cerr << ("Usage:") << std::endl << std::endl;
        std::cerr << ("    ") << szAppName << (" [options] ipaddr port \"payload\" counter") << std::endl;
        std::cerr << std::endl << (" Where [options] can be one of the following:") << std::endl;
        std::cerr << ("     -b       Send broadcast datagram on 'port'.") << std::endl;
        std::cerr << ("     -s       Start Datagram SERVER on 'port'.") << std::endl;
        std::cerr << ("     -q       Send QUIT datagram.") << std::endl;
        std::cerr << std::endl;
        return 1;
    }

    int argnum = 1;
    WSAData wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);

    TDatagram dg { NULL };
    if (_stricmp(argv[argnum], "-b") == 0) {
        bBroadcast = true;
    } else if (_stricmp(argv[argnum], "-s") == 0) {
        argnum++;
        if (argc < argnum + 1) {
            std::cerr << ("You must provide a port#.") << std::endl;
            WSACleanup();
            return 1;
        }
        nPort = getPortNumber(argv[argnum]);
        std::cout << ("Starting Datagram listener on port ") << nPort << (".") << std::endl;
        return DatagramServer(nPort);
    } else if (_stricmp(argv[argnum], "-q") == 0) {
        argnum++;
        dg.Quit = 1;
    }


    if (argc < argnum+1) {
        std::cerr << ("You must provide a target IP.") << std::endl;
        WSACleanup();
        return 1;
    }
    char *target = argv[argnum++];

    if (argc < argnum+1) {
        std::cerr << ("You must provide a port#.") << std::endl;
        WSACleanup();
        return 1;
    }
    nPort = getPortNumber(argv[argnum++]);

    if (argc < argnum+1) {
        std::cerr << ("You must provide a payload.") << std::endl;
        WSACleanup();
        return 1;
    }
    strcpy(dg.Payload, argv[argnum++]);

    if (argc >= argnum+1) {
        dg.Counter = static_cast<short>(atoi(argv[argnum++]));
    }

    //in_addr in = { 0 };
    //in.s_addr = inet_addr("127.0.0.1");

    //hostent *host = gethostbyaddr((char*)&in, sizeof(in), AF_INET);
    //strcpy(dg.Computername, host->h_name);

    GetHostName(&dg);
    if (SendDatagram(target, nPort, bBroadcast, &dg)) {
        std::cout << ("Datagram ") << (bBroadcast ? ("broadcast") : ("sent")) << (" succcessfully.") << std::endl;
    }
    WSACleanup();
    return 0;
}
