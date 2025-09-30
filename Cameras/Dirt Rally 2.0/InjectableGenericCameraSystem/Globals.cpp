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
#include "Globals.h"

#include "Console.h"
#include "GameConstants.h"
#include "NamedPipeManager.h"
#include "Input.h"
#include "Gamepad.h"
#include "Config.h"

//--------------------------------------------------------------------------------------------------------------------------------
// data shared with asm functions. This is allocated here, 'C' style and not in some datastructure as passing that to 
// MASM is rather tedious. 
extern "C" {
	uint8_t g_cameraEnabled = 0;
	uint8_t g_hudEnabled = 1;
	double g_timescaleValue = 1.0;
}


namespace IGCS
{

	Globals::Globals()
	{
		initializeKeyBindings();
	}


	Globals::~Globals()
	{
	}


	Globals &Globals::instance()
	{
		static Globals theInstance;
		return theInstance;
	}

	
	//void Globals::handleSettingMessage(uint8_t payload[], DWORD payloadLength)
	//{
	//	_settings.setValueFromMessage(payload, payloadLength);
	//	
	//}

	//for handling actions with a payload (like hotsampling)
	//void Globals::handleActionPayload(uint8_t payload[], DWORD payloadLength)
	//{
	//	_settings.setValueFromAction(payload, payloadLength);

	//}

	void Globals::handleKeybindingMessage(uint8_t payload[], DWORD payloadLength)
	{
		if (payloadLength < 7)
		{
			IGCS::Console::WriteError("Error: payloadLength < 7, returning early");
			return;
		}

		ActionType idOfBinding = static_cast<ActionType>(payload[1]);
		uint8_t keyCodeByte = payload[2];
		bool altPressed = payload[3] == 0x01;
		bool ctrlPressed = payload[4] == 0x01;
		bool shiftPressed = payload[5] == 0x01;
		bool isGamepadButton = payload[6] == 0x01;

		// Determine which ActionData to update
		const InputSource inputType = isGamepadButton ? InputSource::Gamepad : InputSource::Keyboard;

		ActionData* toUpdate = (inputType == InputSource::Keyboard) ?
			getActionData(idOfBinding) : getGamePadActionData(idOfBinding);

		if (nullptr == toUpdate)
		{
			MessageHandler::logError("Key Binding Action Data could not be found - binding was not changed");
			return;
		}

		// Get key code to use - start with current value as default
		int keyCode = toUpdate->getKeyCode();

		// For gamepad buttons, convert ID to XInput mask if valid
		if (isGamepadButton)
		{
			if (const auto xinputMask = Gamepad::idToXInputMask(keyCodeByte); xinputMask.has_value()) keyCode = xinputMask.value();
			else MessageHandler::logError("Invalid gamepad button ID provided, no update to keyCode");
		}
		else keyCode = keyCodeByte;

		toUpdate->update(keyCode, altPressed, ctrlPressed, shiftPressed, isGamepadButton);
	}


	// Keep the original method unchanged
	ActionData* Globals::getActionData(ActionType type)
	{
		if (_keyBindingPerActionType.count(type) != 1)
		{
			return nullptr;
		}
		return _keyBindingPerActionType.at(type);
	}

	// New method to get alternate binding
	ActionData* Globals::getGamePadActionData(ActionType type)
	{
		if (_gamepadKeyBindingPerActionType.count(type) != 1)
		{
			return nullptr;
		}
		return _gamepadKeyBindingPerActionType.at(type);
	}

	// Check if any binding for this action is active
	bool Globals::isAnyBindingActivated(ActionType type, bool altCtrlOptional)
	{
		ActionData* primaryBinding = getActionData(type);
		ActionData* altBinding = getGamePadActionData(type);

		// Check primary binding
		if (primaryBinding && primaryBinding->isActive(altCtrlOptional))
		{
			return true;
		}

		// Check alternate binding
		if (altBinding && altBinding->isActive(altCtrlOptional))
		{
			return true;
		}

		return false;
	}
	

	void Globals::initializeKeyBindings()
	{
		// initialize the bindings with defaults. First the features which are always supported.
		_keyBindingPerActionType[ActionType::CameraEnable] = new ActionData("CameraEnable", IGCS_KEY_CAMERA_ENABLE, false, false, false);
		// Gamepad bindings
		_gamepadKeyBindingPerActionType[ActionType::CameraEnable] = new ActionData("CameraEnableGP", IGCS::Config::get().cameraEnableGamepadMask, false, false, false, true, InputSource::Gamepad);

	}
}