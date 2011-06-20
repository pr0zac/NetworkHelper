/*
 *  network_receiver.cpp
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
#include <Rpc.h>
#define snprintf _snprintf

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
#include "network_receiver.h"
#include "holepoke/receiver.h"

#include "hole_poke_delegate.h"
#include "cc.h"

#ifdef __linux__
#include <bsd/bsd.h>
#endif

using namespace std;

NetworkReceiver::NetworkReceiver(char* id, int port, int64_t speed, int64_t offset, char* directory)
{
	// use this function to initialize the UDT library
	UDT::startup();
	
	peer_id = string(id);
	peer_port = port;
	save_directory = directory;
	max_speed = speed;
	transferred = offset;
	recv_finished = false;
}

int NetworkReceiver::startReceive()
{
	recv_socket = UDT::socket(AF_INET, SOCK_STREAM, 0);
	UDT::setsockopt(recv_socket, 0, UDT_RCVBUF, new int(1024*1024*500), sizeof(int));
	UDT::setsockopt(recv_socket, 0, UDP_RCVBUF, new int(1024*1024*50), sizeof(int));
	UDT::setsockopt(recv_socket, 0, UDT_MAXBW, new int64_t(max_speed), sizeof(int64_t));
	
	if (peer_port == 0)
	{
		const char* holepokeIPAddressString = "50.16.103.211";
		const char* holepokePortString = "3333";
		
		struct sockaddr_storage saddrHolepokeStorage;
		socklen_t saddrHolepokeLen = sizeof(saddrHolepokeStorage);
		struct sockaddr* saddrHolepoke = (struct sockaddr*)&saddrHolepokeStorage;
		
		if ( !network::MakeSocketAddress(holepokeIPAddressString, holepokePortString, saddrHolepoke, &saddrHolepokeLen) )
		{
			fprintf(stderr, "error\tError making socket address for peer.\n");
			return 1;
		}
			
	#ifdef WIN32
		HANDLE inputthread;
		inputthread = CreateThread(NULL, 0, &NetworkReceiver::startInputThread, this, 0, NULL);
	#elif defined(__linux__) || defined(__APPLE__)
		pthread_t inputthread;
		pthread_create(&inputthread, NULL, &this->startInputThread, this);
		pthread_detach(inputthread);
	#endif
		
		receiverDelegate receiver_delegate;
		holepoke::Receiver receiver(saddrHolepoke, saddrHolepokeLen);
		receiver.setDelegate(&receiver_delegate);
		
		// Find peer by ID.
		receiver.connectToSender(peer_id);
		
		int udp_socket;
		udp_socket = receiver.takeSocket();
		if (udp_socket < 0 || receiver.isConnected() == false)
		{
			cout << "error\tconnectToSender\t" << "Could not connect to sender." << endl;
			return 1;
		}
		
		if (UDT::ERROR == UDT::bind(recv_socket, udp_socket))
		{
			// Something broke trying to bind, this is bad
			cout << "error\tbind\t" << UDT::getlasterror().getErrorMessage() << endl;
			return 1;
		}
		
		struct sockaddr_storage peer_addr_storage;
		struct sockaddr* peer_addr = (struct sockaddr*)&peer_addr_storage;
		socklen_t addr_len = sizeof(peer_addr_storage);
		
		receiver.getPeerAddress(peer_addr, &addr_len);
		
		if (UDT::ERROR == UDT::connect(recv_socket, peer_addr, addr_len))
		{
			// Couldn't connect to remote peer over UDT
			cout << "error\tconnect\t" << UDT::getlasterror().getErrorMessage() << endl;
			return 1;
		}
		
		char* remote_ip = (char*)malloc(NI_MAXHOST);
		char* remote_port = (char*)malloc(NI_MAXSERV);
		getnameinfo(peer_addr, addr_len, remote_ip, NI_MAXHOST, remote_port, NI_MAXSERV, NI_NUMERICHOST|NI_NUMERICSERV);
		cout << "starting\t" << remote_ip << "\t" << remote_port << endl;
	}
	else
	{
		sockaddr_in peer_addr_in;
		peer_addr_in.sin_family = AF_INET;
		peer_addr_in.sin_port = htons(peer_port);
		inet_pton(AF_INET, peer_id.c_str(), &peer_addr_in.sin_addr);
		
		memset(&(peer_addr_in.sin_zero), '\0', 8);
		
		// connect to the server, implict bind
		if (UDT::ERROR == UDT::connect(recv_socket, (sockaddr*)&peer_addr_in, sizeof(peer_addr_in)))
		{
			cout << "connect: " << UDT::getlasterror().getErrorMessage();
			return 0;
		}
		
		cout << "starting\t" << peer_id << "\t" << peer_port << endl;
	}
	
	if (UDT::ERROR == UDT::recv(recv_socket, (char*)&total_size, sizeof(int64_t), 0))
	{
		cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
		return 1;
	}
	
	if (UDT::ERROR == UDT::recv(recv_socket, (char*)&file_count, sizeof(int), 0))
	{
		cout << "error\trecv\t" << UDT::getlasterror().getErrorMessage() << endl;
		return 1;
	}
	
	cout << "fileinfo\t" << total_size << "\t" << file_count;
	
	file_names = (char**)malloc(sizeof(char*)*file_count);
	file_sizes = (int64_t*)malloc(sizeof(int64_t)*file_count);
	for (int i=0; i < file_count; i++)
	{
		// retrieving filename from sender
		int len;		
		if (UDT::ERROR == UDT::recv(recv_socket, (char*)&len, sizeof(int), 0))
		{
			cout << "error\trecv\t" << UDT::getlasterror().getErrorMessage() << endl;
			return 1;
		}
		
		file_names[i] = (char*)malloc(len+1);
		if (UDT::ERROR == UDT::recv(recv_socket, file_names[i], len, 0))
		{
			cout << "error\trecv\t" << UDT::getlasterror().getErrorMessage() << endl;
			return 1;
		}
		file_names[i][len] = '\0';
			
		// get size information
		if (UDT::ERROR == UDT::recv(recv_socket, (char*)&file_sizes[i], sizeof(int64_t), 0))
		{
			cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
			return 1;
		}
		
		cout << "\t" << file_names[i] << "\t" << file_sizes[i];
	}
	
	cout << endl;
	
	if (UDT::ERROR == UDT::send(recv_socket, (char*)&transferred, sizeof(transferred), 0))
	{
		cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
		return 1;
	}
	
	int64_t start_at;
	int64_t size_count;
	int64_t offset;
	start_at = 0;
	size_count = 0;
	for (; size_count+file_sizes[start_at] <= transferred; start_at++)
	{
		size_count += file_sizes[start_at];
	}
	
	offset = transferred-size_count;
	time(&starttime);
	
#ifdef WIN32
	HANDLE statusthread;
	statusthread = CreateThread(NULL, 0, &NetworkReceiver::startStatusThread, this, 0, NULL);
#elif defined(__linux__) || defined(__APPLE__)
	pthread_t statusthread;
	pthread_create(&statusthread, NULL, &this->startStatusThread, this);
	pthread_detach(statusthread);
#endif
	
	int64_t remaining;
	for (int64_t i=start_at; i < file_count; i++)
	{
		int file_location_len = strlen(save_directory) + strlen(file_names[i]) + 2;
		char* file_location = (char*)malloc(file_location_len);
		snprintf(file_location, file_location_len, "%s/%s", save_directory, file_names[i]);
		
		// receive the file
		fstream ofs(file_location, ios::out | ios::in | ios::binary);
		
		if (ofs && i == start_at)
		{
#if defined(__linux__) || defined(__APPLE__)
			truncate(file_location, offset);
#elif defined(WIN32)
			ofs.close();
			HANDLE file = CreateFile((LPCTSTR)file_location,
									 GENERIC_WRITE,
									 FILE_SHARE_WRITE,
									 NULL,
									 OPEN_EXISTING,
									 FILE_ATTRIBUTE_NORMAL,
									 NULL);
			
			SetFilePointer(file, offset, 0, FILE_BEGIN);
			SetEndOfFile(file);
			CloseHandle(file);
			ofs.open(file_location, ios::out | ios::binary);
#else
#error Not implemented on this platform
#endif
			remaining = file_sizes[i]-offset;
		}
		else
		{
			offset = 0;
			remaining = file_sizes[i];
			ofs.close();
			ofs.open(file_location, ios::out | ios::binary);
		}

		free(file_location);
		file_location = NULL;
		
		int64_t recvsize;
		if (UDT::ERROR == (recvsize = UDT::recvfile(recv_socket, ofs, offset, remaining)))
		{
			cout << "error\trecvfile\t" << UDT::getlasterror().getErrorMessage() << endl;
			return 1;
		}
		
		offset = 0;
		ofs.close();
	}
	
	time_t endtime;
	time(&endtime);
	
	if (UDT::ERROR == UDT::send(recv_socket, (char*)&endtime, sizeof(endtime), 0))
	{
		cout << "error\tsend\t" << UDT::getlasterror().getErrorMessage() << endl;
		return 1;
	}
	
	recv_finished = true;
	UDT::TRACEINFO trace;
	UDT::perfmon(recv_socket, &trace);
	
	double current_speed;
	current_speed = trace.mbpsRecvRate/8;
	double overall_speed;
	overall_speed = (double)trace.pktFileBytesRecvd/(1024*1024*(double)(endtime-starttime));
	double guesstimated_speed;
	guesstimated_speed = (current_speed+(overall_speed*2))/3;
	
	cout << "finished\t" << current_speed << "\t" << overall_speed << "\t" << guesstimated_speed << "\t" << endtime-starttime;
	cout << "\t" << 100 << "\t" << trace.msRTT << "\t" << trace.pktRecvTotal << "\t" << trace.pktRcvLossTotal << "\t";
	cout << total_size << "\t" << 0 << endl;
	
	recv_finished = true;
	
#if defined(__linux__) || defined(__APPLE__)
	sleep(1);
#elif defined(WIN32)
	Sleep(1000);
#else
#error Not implemented on your platform
#endif

	if (recv_socket)
		UDT::close(recv_socket);
	if (file_names)
		free(file_names);
	if (file_sizes)
		free(file_sizes);
	
	UDT::cleanup();
	return 0;
}

#ifdef WIN32
DWORD NetworkReceiver::startStatusThread(LPVOID obj)
#elif (__linux__) || (__APPLE__)
void* NetworkReceiver::startStatusThread(void* obj)
#else
#error not defined for this platform
#endif
{
	reinterpret_cast<NetworkReceiver *>(obj)->statusThread();
	return NULL;
}

void NetworkReceiver::statusThread()
{
	UDT::TRACEINFO trace;
	time_t curtime;
	
	const int millisecondsToSleep = 500;
	
#ifdef WIN32
	Sleep(millisecondsToSleep);
#elif defined(__linux__) || defined(__APPLE__)
	usleep(millisecondsToSleep*1000);
#else
#error not defined for this platform
#endif
	
	while(!recv_finished)
	{
		time(&curtime);
		UDT::perfmon(recv_socket, &trace);
		
		int64_t total_transferred = transferred+trace.pktFileBytesRecvd;
		double current_speed = trace.mbpsRecvRate/8;
		double overall_speed = (double)trace.pktFileBytesRecvd/(1024*1024*(double)(curtime-starttime));
		double guesstimated_speed = current_speed;
		
		cout << "receiving\t" << current_speed << "\t" << overall_speed << "\t" << guesstimated_speed << "\t" << curtime-starttime;
		cout << "\t" << (total_transferred*100)/total_size << "\t" << trace.msRTT << "\t" << trace.pktRecvTotal << "\t" << trace.pktRcvLossTotal;
		cout << "\t" << total_transferred << "\t" << (total_size-total_transferred)/(guesstimated_speed*1024*1024) << endl;
		
#ifdef WIN32
		Sleep(millisecondsToSleep);
#elif defined(__linux__) || defined(__APPLE__)
		usleep(millisecondsToSleep*1000);
#else
#error not defined for this platform
#endif
	}
}

#ifdef WIN32
DWORD NetworkReceiver::startInputThread(LPVOID obj)
#elif defined(__linux__) || defined(__APPLE__)
void* NetworkReceiver::startInputThread(void* obj)
#else
#error not defined for this platform
#endif
{
	reinterpret_cast<NetworkReceiver *>(obj)->inputThread();
	return NULL;
}

void NetworkReceiver::inputThread()
{
	// Check for commands being input from stdin
	string command;
#ifndef WIN32
	pollfd cinfd[1];
	cinfd[0].fd = fileno(stdin);
	cinfd[0].events = POLLIN;
#else
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
#endif
	while (!recv_finished)
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

void NetworkReceiver::processCommand(char* command, size_t len)
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
		recv_finished = true;
		UDT::close(recv_socket);
		exit(0);
	}
}

void NetworkReceiver::setMaxSpeed(int64_t new_speed)
{
	ZUDTCC* cchandle = NULL;
	int size;
	UDT::getsockopt(recv_socket, 0, UDT_CC, &cchandle, &size);
	cchandle->setBW(new_speed);
}
