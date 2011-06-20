/*
 *  network_helper.cpp
 *  NetworkHelper
 *
 *  Created by zac on 6/02/11.
 *  Copyright 2011 Caffeinated Mind Inc. All rights reserved.
 *
 */
#include "holepoke/sender.h"
#include "holepoke/receiver.h"

class senderDelegate: public holepoke::SenderDelegate
{
public:
	void gotID(const std::string & identifier)
	{
		std::cout << "peerid\t" << identifier.size() << "\t" << identifier << std::endl;
	}
};

class receiverDelegate : public holepoke::ReceiverDelegate
{
public:
	void invalidID()
	{
		std::cout << "error" << std::endl;
	}
};