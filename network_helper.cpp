/*
 *  network_helper.cpp
 *  NetworkHelper
 *
 *  Created by zac on 5/16/11.
 *  Copyright 2011 Caffeinated Mind Inc. All rights reserved.
 *
 */

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "network_sender.h"
#include "network_receiver.h"

#ifdef __linux__
#include <bsd/bsd.h>
#endif

using namespace std;

int main(int argc, char* argv[])
{
	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 's')
	{
		int count = atoi(argv[4]);

		if ( count < 0 )
		{
			cerr << "Invalid filename count " << count << ". Should be >= 0" << endl;
			exit(1);
		}

		const char** file_array = (const char**)malloc(sizeof(char*)*count);
		
		for (int i=0; i < count; i++)
		{
			file_array[i] = strdup(argv[5+i]);
		}
		
		NetworkSender* sender = new NetworkSender(atoi(argv[2]), atoi(argv[3]), count, file_array);
		exit(sender->startSend());
	}
	else if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'r')
	{
		NetworkReceiver* receiver = new NetworkReceiver(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), argv[6]);
		exit(receiver->startReceive());
	}
	else
	{
		//const char* filename = "D:\\BigFiles\\Archive.rar";
		const char* filename = "/Users/zac/Desktop/random.out";
		NetworkSender* sender = new NetworkSender(0, -1, 1, &filename);
		exit(sender->startSend());
		
//		char* ipaddress = (char*)malloc(16);
//		strlcpy(ipaddress, "192.168.0.101", 15);
//		char* dirname = (char*)malloc(18);
//		strlcpy(dirname, "/tmp/", 17);
//		NetworkReceiver* receiver = new NetworkReceiver(ipaddress, 9000, -1, 50, dirname);
//		return receiver->startReceive();
		
//		cout << "Usage: networkhelper [-s server_port filename] [-r sender_ip sender_port save_directory]" << endl;
//		return 0;
	}
}
