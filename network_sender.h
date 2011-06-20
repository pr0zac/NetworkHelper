/*
 *  network_sender.h
 *  NetworkHelper
 *
 *  Created by zac on 5/16/11.
 *  Copyright 2011 Caffeinated Mind Inc. All rights reserved.
 *
 */

#ifndef NETSEND
#define NETSEND

#include <udt.h>
#include <ccc.h>
#include <list>
#include "socket_list_item.h"

using namespace std;

class NetworkSender
{
public:
	NetworkSender(int port, int64_t speed, int count, const char** file_array);
	int startSend();
	
private:
	list<SocketListItem*> send_sockets;
	int listen_port;
	UDTSOCKET listen_socket;
	int64_t max_speed;
	int file_count;
	const char** file_locations;
	char** file_names;
	int64_t* file_sizes;
	int64_t total_size;
	bool send_finished;

#if defined(__linux__) || defined(__APPLE__)
	static void* startSendThread(void* obj);
#elif defined(WIN32)
	static DWORD WINAPI startSendThread(LPVOID obj);
#else
#error Not implemented on this platform
#endif
	void sendThread();
#if defined(__linux__) || defined(__APPLE__)
	static void* startStatusThread(void* obj);
#elif defined(WIN32)
	static DWORD WINAPI startStatusThread(LPVOID obj);
#else
#error Not implemented on this platform
#endif
	void statusThread();
#if defined(__linux__) || defined(__APPLE__)
	static void* startInputThread(void* obj);
#elif defined(WIN32)
	static DWORD WINAPI startInputThread(LPVOID obj);
#else
#error Not implemented on this platform
#endif
	void inputThread();
	void processCommand(char* command, size_t len);
	void setMaxSpeed(int64_t new_speed);
};

#endif