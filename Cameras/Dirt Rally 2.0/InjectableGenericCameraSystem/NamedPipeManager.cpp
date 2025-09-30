////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part of Injectable Generic Camera System
// Copyright(c) 2020, Frans Bouma
// All rights reserved.
// https://github.com/FransBouma/InjectableGenericCameraSystem
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NamedPipeManager.h"
#include "Defaults.h"
#include "Console.h"
#include <string>
#include "Globals.h"
#include "InputHooker.h"
#include "CameraManipulator.h"

namespace IGCS
{
	// Workaround for having a method as the actual thread func. See: https://stackoverflow.com/a/1372989
	//static DWORD WINAPI staticListenerThread(LPVOID lpParam)
	//{
	//	// our message handling will be very fast so we'll do this on the listener thread, no need to offload that to yet another thread yet.
	//	auto This = (NamedPipeManager*)lpParam;
	//	return This->listenerThread();
	//}

	NamedPipeManager::NamedPipeManager(): _clientToDllPipe(nullptr), _clientToDllPipeConnected(false), _dllToClientPipe(nullptr), _dllToClientPipeConnected(false)
	{
	}
		
	NamedPipeManager::~NamedPipeManager()
	= default;

	NamedPipeManager& NamedPipeManager::instance()
	{
		static NamedPipeManager theInstance;
		return theInstance;
	}

	void NamedPipeManager::writeTextPayload(const std::string& messageText, MessageType typeOfMessage) const
	{
		if (!_dllToClientPipeConnected)
		{
			return;
		}

		uint8_t payload[IGCS_MAX_MESSAGE_SIZE];
		payload[0] = uint8_t(typeOfMessage);
		strncpy_s((char*)(&payload[1]), IGCS_MAX_MESSAGE_SIZE-2, messageText.c_str(), messageText.length());
		DWORD numberOfBytesWritten;
		WriteFile(_dllToClientPipe, payload, static_cast<DWORD>(messageText.length() + 1), &numberOfBytesWritten, nullptr);
	}

	void NamedPipeManager::writeMessage(const std::string& messageText)
	{
		writeMessage(messageText, false, false);
	}

	void NamedPipeManager::writeMessage(const std::string& messageText, bool isError)
	{
		writeMessage(messageText, isError, false);
	}

	void NamedPipeManager::writeMessage(const std::string& messageText, bool isError, bool isDebug)
	{
		MessageType typeOfMessage = MessageType::NormalTextMessage;
		if(isError)
		{
			typeOfMessage = MessageType::ErrorTextMessage;
		}
		else
		{
			if(isDebug)
			{
				typeOfMessage = MessageType::DebugTextMessage;
			}
		}
		writeTextPayload(messageText, typeOfMessage);
	}

}
