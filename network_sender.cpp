/*
 *  network_sender.cpp
 *  NetworkHelper
 *
 *  Created by zac on 5/16/11.
 *  Copyright 2011 Caffeinated Mind Inc. All rights reserved.
 *
 */

#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <cstdlib>
#include <netdb.h>
#include <unistd.h>
#include <sys/poll.h>
#elif defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif
#endif

#include <fstream>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <signal.h>

#include "holepoke/network.h"
#include "network_sender.h"
#include "holepoke/sender.h"

#include "hole_poke_delegate.h"
#include "cc.h"

#ifdef __linux__
#include <bsd/bsd.h>
#endif

using namespace std;

NetworkSender::NetworkSender(int port, int64_t speed, int count, const char** file_array)
{
	// initialize the UDT
	UDT::startup();
	
	listen_port = port;
	file_count = count;
	file_locations = file_array;
	file_names = (char**)malloc(sizeof(char*)*count);
	file_sizes = (int64_t*)malloc(sizeof(int64_t)*count);
	max_speed = speed;
	send_finished = false;
	total_size = 0;
	
	for (int i=0; i < file_count; i++)
	{
		// collect file information
		char* file_location_copy = strdup(file_locations[i]);
		
		char* filename = NULL;
		char* split_file = NULL;
		split_file = strtok(file_location_copy, "/\\");
		while (split_file != NULL)
		{
			filename = split_file;
			split_file = strtok(NULL, "/\\");
		}
		
		file_names[i] = strdup(filename);
				
#if defined(__linux__) || defined(__APPLE__)
		fstream ifs(file_locations[i], ios::in | ios::binary);
		ifs.seekg(0, ios::end);
		file_sizes[i] = ifs.tellg();
		ifs.seekg(0, ios::beg);
		ifs.close();		
#elif defined(WIN32)
		HANDLE file = CreateFile((LPCSTR)file_locations[i],
								 GENERIC_READ,
								 FILE_SHARE_READ,
								 NULL,
								 OPEN_EXISTING,
								 FILE_ATTRIBUTE_NORMAL,
								 NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			LPVOID lpMsgBuf;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
						  FORMAT_MESSAGE_FROM_SYSTEM | 
						  FORMAT_MESSAGE_IGNORE_INSERTS,
						  NULL,
						  GetLastError(),
						  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
						  (LPSTR) &lpMsgBuf,
						  0,
						  NULL);
			
			cout << "error\tCreateFile\t" << (LPCSTR)lpMsgBuf << endl;
			exit(1);
		}
		
		GetFileSizeEx(file, (LARGE_INTEGER*)&file_sizes[i]);
		CloseHandle(file);
#else
#error Not implemented on this platform
#endif
		
		total_size += file_sizes[i];
		
		free(file_location_copy);
	}
}

int NetworkSender::startSend()
{
#if defined(__linux__) || defined(__APPLE__)
	pthread_t inputthread;
	pthread_create(&inputthread, NULL, &this->startInputThread, this);
	pthread_detach(inputthread);
#elif defined(WIN32)
	HANDLE inputthread;
	inputthread = CreateThread(NULL, 0, &NetworkSender::startInputThread, this, 0, NULL);
#else
#error Not implemented on this platform
#endif

#if defined(__linux__) || defined(__APPLE__)
	pthread_t statusthread;
	pthread_create(&statusthread, NULL, &this->startStatusThread, this);
	pthread_detach(statusthread);
#elif defined(WIN32)
	HANDLE statusthread;
	statusthread = CreateThread(NULL, 0, &NetworkSender::startStatusThread, this, 0, NULL);
#endif
	
	bool listening = false;
	while (!send_finished)
	{
		int64_t speed;
		if (max_speed > 0)
		{
			speed = max_speed/(send_sockets.size()+1);
		}
		else
		{
			speed = -1;
		}
				
		if (listen_port == 0)
		{
			listen_socket = UDT::socket(AF_INET, SOCK_STREAM, 0);
			UDT::setsockopt(listen_socket, 0, UDT_CC, new CCCFactory<ZUDTCC>, sizeof(CCCFactory<ZUDTCC>));
			UDT::setsockopt(listen_socket, 0, UDT_SNDBUF, new int(1024*1024*500), sizeof(int));
			UDT::setsockopt(listen_socket, 0, UDP_SNDBUF, new int(1024*1024*50), sizeof(int));
			if (speed > 0)
			{
				UDT::setsockopt(listen_socket, 0, UDT_MAXBW, new int64_t(speed), sizeof(int64_t));
			}
#ifdef WIN32
			int mss = 1052;
			UDT::setsockopt(listen_socket, 0, UDT_MSS, &mss, sizeof(int));
#endif			
			
			const char* holepokeIPAddressString = "50.16.103.211";
			const char* holepokePortString = "3333";
			
			struct sockaddr_storage saddrHolepokeStorage;
			socklen_t saddrHolepokeLen = sizeof(saddrHolepokeStorage);
			struct sockaddr* saddrHolepoke = (struct sockaddr*)&saddrHolepokeStorage;
			
			if ( !network::MakeSocketAddress(holepokeIPAddressString, holepokePortString, saddrHolepoke, &saddrHolepokeLen) )
			{
				cout << "error\tMakeSocketAddress\tError making socket address for peer." << endl;
				goto end;
			}
					
			senderDelegate sender_delegate;
			holepoke::Sender sender(saddrHolepoke, saddrHolepokeLen);
			sender.setDelegate(&sender_delegate);
			
			cout << "listening" << endl;
			sender.connectToReceiver();
			
			int udp_socket;
			udp_socket = sender.takeSocket();
			if (udp_socket < 0 || sender.isConnected() == false)
			{
				continue;
			}
			
			struct sockaddr_storage peer_addr_storage;
			struct sockaddr* peer_addr = (struct sockaddr*)&peer_addr_storage;
			socklen_t addr_len = sizeof(peer_addr_storage);
			
			sender.getPeerAddress(peer_addr, &addr_len);
			
			if (UDT::ERROR == UDT::bind(listen_socket, udp_socket))
			{
				// Something broke trying to bind, this is bad, retry
				cout << "error\tbind\t" << UDT::getlasterror().getErrorMessage() << endl;
				return 1;
			}
			
			if (UDT::ERROR == UDT::listen(listen_socket, 10))
			{
				cout << "error\tlisten\t" << UDT::getlasterror().getErrorMessage() << endl;
				return 1;
			}
		}
		else if (!listening)
		{
			listen_socket = UDT::socket(AF_INET, SOCK_STREAM, 0);
			UDT::setsockopt(listen_socket, 0, UDT_CC, new CCCFactory<ZUDTCC>, sizeof(CCCFactory<ZUDTCC>));
			UDT::setsockopt(listen_socket, 0, UDT_SNDBUF, new int(1024*1024*500), sizeof(int));
			UDT::setsockopt(listen_socket, 0, UDP_SNDBUF, new int(1024*1024*50), sizeof(int));
			if (speed > 0)
			{
				UDT::setsockopt(listen_socket, 0, UDT_MAXBW, new int64_t(speed), sizeof(int64_t));
			}
#ifdef WIN32
			int mss = 1052;
			UDT::setsockopt(listen_socket, 0, UDT_MSS, &mss, sizeof(int));
#endif
			
			sockaddr_in my_addr;
			my_addr.sin_family = AF_INET;
			my_addr.sin_port = htons(listen_port);
			my_addr.sin_addr.s_addr = INADDR_ANY;
			memset(&(my_addr.sin_zero), '\0', 8);
			
			if (UDT::ERROR == UDT::bind(listen_socket, (sockaddr*)&my_addr, sizeof(my_addr)))
			{
				cout << "error\tbind\t" << UDT::getlasterror().getErrorMessage() << endl;
				return 1;
			}
			
			if (UDT::ERROR == UDT::listen(listen_socket, 10))
			{
				cout << "error\tlisten\t" << UDT::getlasterror().getErrorMessage() << endl;
				return 1;
			}
		}
		
		listening = true;
		
		sockaddr_storage clientaddr;
		int addrlen = sizeof(clientaddr);
		
		UDTSOCKET send_socket;
		if (UDT::INVALID_SOCK == (send_socket = UDT::accept(listen_socket, (sockaddr*)&clientaddr, &addrlen)))
		{
			cout << "error\taccept\t" << UDT::getlasterror().getErrorMessage() << endl;
			return 1;
		}
		
		char* remote_ip = (char*)malloc(NI_MAXHOST);
		char* remote_port = (char*)malloc(NI_MAXSERV);
		getnameinfo((sockaddr*)&clientaddr, addrlen, remote_ip, NI_MAXHOST, remote_port, NI_MAXSERV, NI_NUMERICHOST|NI_NUMERICSERV);
		
		time_t start_time;
		time(&start_time);
		SocketListItem* newItem = new SocketListItem(send_socket, start_time, remote_ip, remote_port);
		send_sockets.push_back(newItem);
		
		cout << "starting\t" << remote_ip << "\t" << remote_port << "\t" << send_sockets.size() << endl;
		
#if defined(__linux__) || defined(__APPLE__)
		pthread_t sendthread;
		pthread_create(&sendthread, NULL, &this->startSendThread, this);
		pthread_detach(sendthread);
#elif defined(WIN32)
		HANDLE sendthread;
		sendthread = CreateThread(NULL, 0, &NetworkSender::startSendThread, this, 0, NULL);
#else
#error Not implemented on this platform.
#endif
	}
	
end:
	send_finished = true;

#if defined(__linux__) || defined(__APPLE__)
	sleep(1);
#elif defined(WIN32)
	Sleep(1000);
#else
#error Not implemented on this platform
#endif
	
	send_sockets.clear();
	if (file_names)
		free(file_names);
	if (file_sizes)
		free(file_sizes);
//#if defined(__linux__) || defined(__APPLE__)
//	if (inputthread)
//		pthread_kill(inputthread, SIGINT);
//	if (statusthread)
//		pthread_kill(statusthread, SIGINT);
//#elif defined(WIN32)
//	if (inputthread)
//		TerminateThread(inputthread, 0);
//	if (statusthread)
//		TerminateThread(statusthread, 0);
//#else
//#error Not implemented on this platform.
//#endif

	UDT::cleanup();
	return 0;
}


	
#if defined(__linux__) || defined(__APPLE__)
void* NetworkSender::startSendThread(void* obj)
#elif defined(WIN32)
DWORD NetworkSender::startSendThread(LPVOID obj)
#else
#error Not defined for this platform
#endif
{
	reinterpret_cast<NetworkSender *>(obj)->sendThread();
	return NULL;
}
	
void NetworkSender::sendThread()
{
	list<SocketListItem*>::iterator socket_it;
	socket_it = send_sockets.end();
	socket_it--;
	
	UDTSOCKET send_socket = (*socket_it)->socket();
	
	// send total size
	if (UDT::ERROR == UDT::send(send_socket, (char*)&total_size, sizeof(int64_t), 0))
	{
		cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
		goto end;
	}
	
	// send file count
	if (UDT::ERROR == UDT::send(send_socket, (char*)&file_count, sizeof(int), 0))
	{
		cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
		goto end;
	}
	
	for (int i=0; i < file_count; i++)
	{
		// send file name and size
		int len = strlen(file_names[i]);
		if (UDT::ERROR == UDT::send(send_socket, (char*)&len, sizeof(int), 0))
		{
			cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
			goto end;
		}
		
		if (UDT::ERROR == UDT::send(send_socket, file_names[i], len, 0))
		{
			cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
			goto end;
		}
		
		if (UDT::ERROR == UDT::send(send_socket, (char*)&file_sizes[i], sizeof(int64_t), 0))
		{
			cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
			goto end;
		}
	}
	
	int64_t transferred;
	if (UDT::ERROR == UDT::recv(send_socket, (char*)&transferred, sizeof(transferred), 0))
	{
		cout << "error\trecv\t" << UDT::getlasterror().getErrorMessage() << endl;
		goto end;
	}
	
	int64_t start_at;
	int64_t size_count;
	for (start_at=size_count=0; size_count+file_sizes[start_at] <= transferred; start_at++)
	{
		size_count += file_sizes[start_at];
	}
	int64_t offset;
	offset = transferred-size_count;
	int64_t remaining;
	remaining = file_sizes[start_at]-offset;
	
	time_t start_time;
	time(&start_time);
	(*socket_it)->startTime(start_time);
	
	int64_t total_sent;
	total_sent = 0;
	for (int64_t i=start_at; i < file_count; i++)
	{
		fstream ifs(file_locations[i], ios::in | ios::binary);
		
		if (i != start_at)
		{
			offset = 0;
			remaining = file_sizes[i];
		}		
		
		// send the actual file
		int64_t send_size;
		if (UDT::ERROR == (send_size = UDT::sendfile(send_socket, ifs, offset, remaining)))
		{
			cout << "error\tsendfile\t" << UDT::getlasterror().getErrorMessage() << endl;
			goto end;
		}
		
		total_sent += send_size;
		ifs.close();
	}
	
	time_t end_time;
	if (UDT::ERROR == UDT::recv(send_socket, (char*)&end_time, sizeof(end_time), 0))
	{
		time(&end_time);
	}
	
	cout << "finished\t" << send_sockets.size()-1 << "\t" << (*socket_it)->remoteIP() << "\t" << (*socket_it)->remotePort() << "\t" << total_sent << endl;
	
end:
	// cleanup
	send_sockets.erase(socket_it);
	
	return;
}

#if defined(__linux__) || defined(__APPLE__)
void* NetworkSender::startStatusThread(void* obj)
#elif defined(WIN32)
DWORD WINAPI NetworkSender::startStatusThread(LPVOID obj)
#else
#error Not implemented on this platform
#endif
{
	reinterpret_cast<NetworkSender *>(obj)->statusThread();
	return NULL;
}


void NetworkSender::statusThread()
{
	UDT::TRACEINFO trace;
	time_t curtime;
	
	const int millisecondsToSleep = 1000;

#if defined(__linux__) || defined(__APPLE__)
	usleep(1000*1000);
#elif defined(WIN32)
	Sleep(1000);
#else
#error Not implemented on this platform
#endif
	
	while(!send_finished)
	{
		double total_current_speed = 0;
//		double total_overall_speed = 0;
		
		list<SocketListItem*>::iterator socket_it;
		for (socket_it=send_sockets.begin(); socket_it != send_sockets.end(); socket_it++)
		{
			UDTSOCKET send_socket = (*socket_it)->socket();
//			time_t starttime = (*socket_it)->startTime();
			time(&curtime);
			UDT::perfmon(send_socket, &trace);
			
			total_current_speed += trace.mbpsSendRate/8;
			//total_overall_speed += ((double)(trace.pktSentTotal-trace.pktRetransTotal)*1476)/(1024*1024*(double)(curtime-starttime));
		}
		
		//double guesstimated_speed = (total_current_speed+(total_overall_speed*2))/3;
		//cout << "status\t" << send_sockets.size() << "\t" << total_current_speed << "\t" << total_overall_speed << "\t" << guesstimated_speed << endl;
		cout << "status\t" << send_sockets.size() << "\t" << total_current_speed << "\t" << endl;

#if defined(__linux__) || defined(__APPLE__)
		usleep(millisecondsToSleep*1000);
#elif defined(WIN32)
		Sleep(millisecondsToSleep);
#else
#error Not implemented on this platform
#endif
	}
}

#if defined(__linux__) || defined(__APPLE__)
void* NetworkSender::startInputThread(void* obj)
#elif defined(WIN32)
DWORD NetworkSender::startInputThread(LPVOID obj)
#else
#error Not implemented on this platform
#endif
{
	reinterpret_cast<NetworkSender *>(obj)->inputThread();
	return NULL;
}

void NetworkSender::inputThread()
{
	string command;
#ifndef WIN32
	pollfd cinfd[1];
	cinfd[0].fd = fileno(stdin);
	cinfd[0].events = POLLIN;
#else
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
#endif
	while (!send_finished)
	{
#ifndef WIN32
		if (poll(cinfd, 1, 1000))
#else
		if (WaitForSingleObject(h, 0) == WAIT_OBJECT_0)
#endif
		{
			getline(cin, command);
			if (command.size() > 0)
			{
				processCommand((char*)command.c_str(), command.size());
			}
		}
	}
}

void NetworkSender::processCommand(char* command, size_t len)
{
	char* command_split;
	command_split = strtok(command, "\t");
	
	if (strncmp(command_split, "max_speed", 9) == 0)
	{
		int64_t new_speed = atoi(strtok(NULL, "\t"));
		setMaxSpeed(new_speed);
	}
	else if (strncmp(command_split, "stop", 4) == 0)
	{
		send_finished = true;
		send_sockets.clear();
		exit(0);
	}
}

void NetworkSender::setMaxSpeed(int64_t new_speed)
{
	max_speed = new_speed;
	if (send_sockets.size() > 1)
	{
		int64_t speed;
		if (new_speed > 0)
		{
			speed = new_speed/send_sockets.size();
		}
		else
		{
			speed = -1;
		}
		
		list<SocketListItem*>::iterator socket_it;
		for (socket_it=send_sockets.begin(); socket_it != send_sockets.end(); socket_it++)
		{
			ZUDTCC* cchandle = NULL;
			int size;
			UDT::getsockopt((*socket_it)->socket(), 0, UDT_CC, &cchandle, &size);
			cchandle->setBW(speed);
		}		
	}
}

