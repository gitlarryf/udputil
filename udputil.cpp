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
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <cctype>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <winsock.h>
typedef int socklen_t;
#pragma warning(disable: 4127) // incompatible with FD_SET()
#define MAX_COMPUTERNAME    MAX_COMPUTERNAME_LENGTH
#define snprintf _snprintf
#else
#include <netdb.h>
#include <signal.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
typedef unsigned int socklen_t;
typedef int SOCKET;
const int INVALID_SOCKET = -1;
inline void closesocket(int x) { close(x); }
#define WSAGetLastError()   errno
#define WSACleanup()
#define MAX_COMPUTERNAME    15
#endif

#ifndef  SD_BOTH
#define  SD_BOTH                0x02
#endif

#define VERSION_INFO            1.6
#define MAX_DATAGRAM_MESSAGE    512

SOCKET  ServerSocket    = INVALID_SOCKET;
bool    bShutdown       = false;
char    *szAppName      = NULL;
short   nScreenCols     = 0;

enum DisplayModes {
    None,
    ASCII,
    VerboseASCII,
    Hex,
    UpperHex,
    Decimal,
    Raw,
    MAX_TYPES,
} DISPLAY_MODES;

                                     // None     // ASCII    // Verbose ASCII   // Hex      //UpperHex      // Decimal      // Raw
static const char *FORMAT_STR[] = {  "%c",       "%c",       "%s",             "%02x ",    "%02X ",        "%03d ",        "%c"    };
static const char *DATA_WIDTHS[] ={  " ",        " ",        "",               "   ",      "   ",          "    ",         " "     };
static const char *ASCII_DESC[] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS ", "HT ", "LF ", "VT ", "FF ", "CR ", "SOH", "SI ",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM ", "SUB", "ESC", "FS ", "GS ", "RS ", "US ",
};

typedef struct tagTDatagram {
    char Computername[MAX_COMPUTERNAME + 1];
    char Payload[MAX_DATAGRAM_MESSAGE + 1];
    short Counter;
    short Quit;
} TDatagram;

char *getSocketError(int err)
{
    static char msgbuf[256] = { '\0' };
#ifdef _MSC_VER
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  msgbuf,
                  sizeof(msgbuf),
                  NULL);
#else
    sprintf(&msgbuf[0], "%s.\n", strerror(err));
#endif
    if (!*msgbuf) {
       sprintf(msgbuf, "(No Error Text) - %d", err);
    }
    return &msgbuf[0];
}

std::string FormatData(const char *pszDirSym, const char *Buff, size_t nBuffLen, size_t nCX)
{
    std::string sRetval = "";
    size_t nCols = nCX;
    size_t nRowCol = 0;
    char *rowStr = new char[6];

    nCols = (((nCX - strlen(pszDirSym)) - 1) / 4);

    for (size_t y = 0; y < ((nBuffLen / nCols) + 1); y++) {
        sRetval += pszDirSym;
        std::string sRaw;
        std::string sVals;
        for (size_t x = 0; x < nCols; x++, nRowCol++) {
            if (nRowCol >= nBuffLen) {
                // We ran out of data, so just print spaces.
                sVals += DATA_WIDTHS[UpperHex];
                continue;
            }
            // First, build the NUMERICAL part of the string.
            snprintf(rowStr, 5, FORMAT_STR[UpperHex], (Buff[nRowCol] & 0xFF));
            sVals += rowStr;
            // Then the actual ASCII part
            if ((std::isgraph(Buff[nRowCol] & 0xFF) || (Buff[nRowCol] & 0xFF) == 0x20)) {
                snprintf(rowStr, 2, "%c", (Buff[nRowCol] & 0xFF));
                sRaw += rowStr;
            } else if ((Buff[nRowCol] & 0xFF) > 0x7F) {
                sRaw += ".";
            }
        }
        sRetval += sVals;
        sRetval += sRaw + "\n";
    }
    return sRetval;
}

int DatagramServer(unsigned short nServerPort)
{
    uint32_t uiPacketNumber = 0;
    TDatagram datagram;

    ServerSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ServerSocket == INVALID_SOCKET) {
        int err = WSAGetLastError();
        std::cerr << "Could not create server datagram socket.  Error: (" << err << ") - " << getSocketError(err);
        return 1;
    }
    sockaddr_in sin;
    memset(&sin, 0x00, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(nServerPort);
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(ServerSocket, (sockaddr *)&sin, sizeof(sin)) != 0) {
        int err = WSAGetLastError();
        std::cerr << "Could not bind server datagram socket on port: " << nServerPort <<  ".  Error: (" << err << ") - " << getSocketError(err);
        return 1;
    }

    while (!bShutdown) {
        memset(&datagram, 0x00, sizeof(TDatagram));
        int nfs = 0;
        sockaddr_in fin;
        socklen_t fromlen = sizeof(fin);
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
            std::cerr << "select() returned an error.  Error: (" << errno << ") - " << strerror(errno);
            break;
        } else if (err == 0) {
            // Ignore timeout periods now.
            //std::cerr << "Recv: " << uiPacketNumber << " valid datagrams during timeout period." << std::endl;
            //uiPacketNumber = 0;
            continue;
        }
        if (FD_ISSET(ServerSocket, &efds)) {
            int error = WSAGetLastError();
            std::cerr << "Socket exception occurred on socket.  Shutting down server.  Error: (" << error << ") - " << getSocketError(error);
            break;
        }
        if (FD_ISSET(ServerSocket, &rfds)) {
            int r = recvfrom(ServerSocket, (char *)&datagram, sizeof(TDatagram), 0, (sockaddr *)&fin, &fromlen);
            if ((r > 0) && (r != sizeof(TDatagram))) {
                std::cerr << "Received " << r << " bytes of unknown datagram." << std::endl;
                std::cout << FormatData("PAYLOAD:", reinterpret_cast<char*>(&datagram), r, nScreenCols);
                continue;
            } else if (r == sizeof(TDatagram)) {
                uiPacketNumber++;
                std::cout << "*** DATAGRAM ***" << std::endl;
                if (datagram.Quit) {
                    std::cout << "--- Shutdown Datagram receieved. ---" << std::endl;
                }
                std::cout << "IPADR: " << inet_ntoa(fin.sin_addr) << std::endl;
                std::cout << "COUNT: " << datagram.Counter << std::endl << "WKSTN: " << datagram.Computername << std::endl << "PAYLD: " << datagram.Payload << std::endl;
                if (datagram.Quit) {
                    std::cout << "SHTDN: " << (datagram.Quit ? "TRUE" : "FALSE") << std::endl;
                    bShutdown = true;
                }
                std::cout << "*** END DGRM ***" << std::endl;
            }
        }
        FD_CLR(ServerSocket, &rfds);
        FD_CLR(ServerSocket, &efds);
    }
    closesocket(ServerSocket);
    return 0;
}

bool SendDatagram(const char *pszHostAddr, unsigned short nPort, bool bBroadcast, void *pData, size_t datalen)
{
    bool bRetval = false;
    SOCKET s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s != INVALID_SOCKET) {
        sockaddr_in sin;
        memset(&sin, 0x00, sizeof(sin));
        if (bBroadcast) {
            uint32_t tru = 1;
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&tru, sizeof(tru));
        }
        sin.sin_family = PF_INET;
        sin.sin_port = htons(nPort);
        sin.sin_addr.s_addr = bBroadcast ? INADDR_BROADCAST : inet_addr(pszHostAddr);
        if (sendto(s, reinterpret_cast<char*>(pData), datalen, 0, (sockaddr *)&sin, sizeof(sin)) > 0) {
            bRetval = true;
        } else {
            int error = WSAGetLastError();
            std::cerr << "Datagram failed to " << (bBroadcast ? "broadcast" : "send") << ".  Error: (" << error << ") - " << getSocketError(error);
        }
        closesocket(s);
    }
    return bRetval;
}

void GetHostName(TDatagram *pDatagram)
{
#ifdef _MSVC_VER
    TCHAR ComputerName[MAX_COMPUTERNAME_LENGTH + 1];
    ZeroMemory(ComputerName, sizeof(ComputerName));
    DWORD len = MAX_COMPUTERNAME_LENGTH;
    GetComputerName(pDatagram->Computername, &len);
#else
    gethostname(pDatagram->Computername, MAX_COMPUTERNAME);
#endif
}

#ifdef _MSC_VER
BOOL __stdcall CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:  // Handle the CTRL+C signal.
            bShutdown = true;
            std::cerr << std::endl << "***** Shutting down Datagram listener..." << std::endl;
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            return TRUE; 
        case CTRL_CLOSE_EVENT:
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            Sleep(500);
            bShutdown = true;
            std::cerr << std::endl << "**** Closing application, shutting down Datagram listener..." << std::endl;
            return TRUE; 
        case CTRL_LOGOFF_EVENT:
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            Sleep(500);
            bShutdown = true;
            std::cerr << std::endl << "**** User Logoff event occurred, shutting down Datagram listener." << std::endl;
        case CTRL_SHUTDOWN_EVENT:
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            Sleep(500);
            bShutdown = true;
            std::cerr << std::endl << "**** Windows Shutdown invoked!  Kindly closing all active threads..." << std::endl;
        case CTRL_BREAK_EVENT:
            return FALSE;   //Break out of the app, non-gracefully.
        default:
            return FALSE;
#else
void CtrlHandler(int sig)
{
    switch (sig) {
        case SIGQUIT:
            std::cerr << std::endl << "**** Dumping core..." << std::endl;
            break;
        case SIGTERM:
            bShutdown = true;
            std::cerr << std::endl << "**** Closing application, shutting down Datagram listener..." << std::endl;
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            break;
        case SIGINT:
            bShutdown = true;
            std::cerr << std::endl << "***** Shutting down Datagram listener..." << std::endl;
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            break;
        case SIGHUP:
            bShutdown = true;
            std::cerr << std::endl << "**** Connection to user lost, or user logoff event occurred, shutting down Datagram listener." << std::endl;
            shutdown(ServerSocket, SD_BOTH);
            closesocket(ServerSocket);
            break;
#endif
    }
}

unsigned short getPortNumber(const char *port)
{
    if (atoi(port) <= 0) {
        std::cerr << port << " is an invalid port number.  Port must be a positive integer between 1 and " << USHRT_MAX << "." << std::endl;
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
#ifdef _MSC_VER
    CONSOLE_SCREEN_BUFFER_INFO   csbi;  // used to get Screen Window size.
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    nScreenCols = csbi.dwMaximumWindowSize.X;
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
    WSAData wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    nScreenCols = w.ws_col;
    signal(SIGINT,  CtrlHandler);
    signal(SIGQUIT, CtrlHandler);
    signal(SIGTERM, CtrlHandler);
    signal(SIGHUP,  CtrlHandler);
#endif
    bool bBroadcast = false;
    bool bRaw = false;
    unsigned short nPort = 0;

    if (argc < 2) {
        std::cerr << szAppName << " - UDP Datagram Utility (C) 2018 MLinks Technologies, Inc." << std::endl;
        std::cerr << "Version " << VERSION_INFO << " by Larry Frieson" << std::endl;
        std::cerr << "Usage:" << std::endl << std::endl;
        std::cerr << "    " << szAppName << " [options] ipaddr port \"payload\" counter" << std::endl;
        std::cerr << std::endl << " Where [options] can be one of the following:" << std::endl;
        std::cerr << "     -b       Send broadcast datagram on 'port'." << std::endl;
        std::cerr << "     -q       Send QUIT datagram." << std::endl;
        std::cerr << "     -r       Send RAW datagram." << std::endl;
        std::cerr << "     -s       Start Datagram SERVER on 'port'." << std::endl;
        std::cerr << std::endl;
        return 1;
    }

    int argnum = 1;

    TDatagram dg;
    memset(&dg, 0x00, sizeof(TDatagram));
    if (strcmp(argv[argnum], "-b") == 0) {
        bBroadcast = true;
    } else if (strcmp(argv[argnum], "-s") == 0) {
        argnum++;
        if (argc < argnum + 1) {
            std::cerr << "You must provide a port#." << std::endl;
            WSACleanup();
            return 1;
        }
        nPort = getPortNumber(argv[argnum]);
        std::cout << "Starting Datagram listener on port " << nPort << "." << std::endl;
        return DatagramServer(nPort);
    } else if (strcmp(argv[argnum], "-q") == 0) {
        argnum++;
        dg.Quit = 1;
    } else if (strcmp(argv[argnum], "-r") == 0) {
        argnum++;
        bRaw = true;
    }


    if (argc < argnum+1) {
        std::cerr << "You must provide a target IP." << std::endl;
        WSACleanup();
        return 1;
    }
    char *target = argv[argnum++];

    if (argc < argnum+1) {
        std::cerr << "You must provide a port#." << std::endl;
        WSACleanup();
        return 1;
    }
    nPort = getPortNumber(argv[argnum++]);

    if (argc < argnum+1) {
        std::cerr << "You must provide a payload." << std::endl;
        WSACleanup();
        return 1;
    }
    char *pRawPacket = argv[argnum];
    strcpy(dg.Payload, argv[argnum++]);

    if (!bRaw && argc >= argnum+1) {
        dg.Counter = static_cast<short>(atoi(argv[argnum++]));
    }

    //in_addr in = { 0 };
    //in.s_addr = inet_addr("127.0.0.1");

    //hostent *host = gethostbyaddr((char*)&in, sizeof(in), AF_INET);
    //strcpy(dg.Computername, host->h_name);

    GetHostName(&dg);
    bool bRet = false;
    if (bRaw) {
        bRet = SendDatagram(target, nPort, bBroadcast, pRawPacket, strlen(pRawPacket));
    } else {
        bRet = SendDatagram(target, nPort, bBroadcast, &dg, sizeof(TDatagram));
    }

    if (bRet) {
        std::cout << "Datagram " << (bBroadcast ? "broadcast" : "sent") << " succcessfully." << std::endl;
    }
    WSACleanup();
    return 0;
}
