/*
 *  socket_list_item.cpp
 *  NetworkHelper
 *
 *  Created by zac on 6/7/11.
 *  Copyright 2011 Caffeinated Mind Inc. All rights reserved.
 *
 */

#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <cstdlib>
#include <netdb.h>
#include <unistd.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <signal.h>
#include "socket_list_item.h"

SocketListItem::SocketListItem(UDTSOCKET socket, time_t time, char* ip, char* port)
{
	send_socket = socket;
	start_time = time;
	remote_ip = ip;
	remote_port = port;
}
SocketListItem::~SocketListItem()
{
	UDT::close(send_socket);
	free(remote_ip);
	free(remote_port);
}
UDTSOCKET SocketListItem::socket()
{
	return send_socket;
}
time_t SocketListItem::startTime()
{
	return start_time;
}
void SocketListItem::startTime(time_t new_time)
{
	start_time = new_time;
}
char* SocketListItem::remoteIP()
{
	return remote_ip;
}
char* SocketListItem::remotePort()
{
	return remote_port;
}
