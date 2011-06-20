/*
 *  network_receiver.h
 *  NetworkHelper
 *
 *  Created by zac on 5/16/11.
 *  Copyright 2011 Caffeinated Mind Inc. All rights reserved.
 *
 */

#ifndef NETRECV
#define NETRECV

#include <udt.h>
#include <ccc.h>

class NetworkReceiver
{
public:
	NetworkReceiver(char* id, int port, int64_t speed, int64_t offset, char* directory);
	int startReceive();
	
private:
	std::string peer_id;
	int peer_port;
	UDTSOCKET recv_socket;
	int64_t max_speed;
	char* save_directory;
	int file_count;
	char** file_names;
	int64_t* file_sizes;
	int64_t total_size;
	time_t starttime;
	bool recv_finished;
	int64_t transferred;

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