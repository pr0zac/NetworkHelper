/*
 *  socket_list_item.h
 *  NetworkHelper
 *
 *  Created by zac on 6/7/11.
 *  Copyright 2011 Caffeinated Mind Inc. All rights reserved.
 *
 */

#ifndef SOCKLISTITEM
#define SOCKLISTITEM

#include <udt.h>
#include <ccc.h>
#include <list>

class SocketListItem
{
public:
	SocketListItem(UDTSOCKET socket, time_t time, char* ip, char* port);
	~SocketListItem();
	UDTSOCKET socket();
	time_t startTime();
	void startTime(time_t new_time);
	char* remoteIP();
	char* remotePort();
#if defined(__linux__) || defined(__APPLE__)
	void setSendThread(pthread_t newthread);
	pthread_t sendThread();
#elif defined(WIN32)
	void setSendThread(HANDLE newthread);
	HANDLE sendThread();
#endif
	
private:
	UDTSOCKET send_socket;
	time_t start_time;
	char* remote_ip;
	char* remote_port;
};

#endif