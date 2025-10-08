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
#include "System.h"
#include "Globals.h"
#include "Defaults.h"
#include "GameConstants.h"
#include "Gamepad.h"
#include "CameraManipulator.h"
#include "InterceptorHelper.h"
#include "InputHooker.h"
#include "input.h"
#include "MinHook.h"
#include "NamedPipeManager.h"
#include "MessageHandler.h"
#include "Console.h"
#include "GameImageHooker.h"
#include "WindowHook.h"
#include "Utils.h"
#include <Xinput.h>
#include "DirectInputPad.h"
#include "Config.h"

namespace IGCS
{
	using namespace IGCS::GameSpecific;
	static IGCS::DirectInputPad* s_directInput = nullptr;

	System::System():
		_igcscacheData(),
		_originalData(),
		_hostImageAddress(nullptr),
		_hostImageSize(0),
		_lastFrameTime(),
		_frequency(),
		_useFixedDeltaTime(false),
		_fixedDeltaValue(0.0167f)
	{
	}

	System::~System()
	{
		D3DHook::instance().cleanUp();
	}

	void System::start(LPBYTE hostBaseAddress, DWORD hostImageSize)
	{
		Globals::instance().systemActive(true);
		Globals::instance().storeCurrentaobBlock(&_aobBlocks);
		Globals::instance().storeCurrentdeltaT(&_deltaTime);
				
		_hostImageAddress = (LPBYTE)hostBaseAddress;
		_hostImageSize = hostImageSize;
		filesystem::path hostExeFilenameAndPath = Utils::obtainHostExeAndPath();
		_hostExeFilename = hostExeFilenameAndPath.stem();
		_hostExePath = hostExeFilenameAndPath.parent_path();

		// Initialize timing options
		_useFixedDeltaTime = false;  // Toggle this to use fixed or real delta time
		_fixedDeltaValue = 1.0f / 120.0f;  // Fixed delta time value at 60 FPS

		// Initialize the high-precision timer only for delta time calculation
		QueryPerformanceFrequency(&_frequency);
		QueryPerformanceCounter(&_lastFrameTime);

		initialize();    // will block till camera is found
		mainLoop();
	}

	void System::shutdown()
	{
		// Prevent re-entrancy
		static std::atomic_bool shuttingDown{ false };
		bool expected = false;
		if (!shuttingDown.compare_exchange_strong(expected, true)) {
			return; // already shutting down
		}

		// Tell the rest of the code to stop doing work
		Globals::instance().systemActive(false);

		// 1) Disable our input hooks first (so the game can tear down input cleanly)
		try {
			InputHooker::setXInputHook(false);
		}
		catch (...) { /* swallow */ }

		// 2) Unhook/cleanup window-related hooks
		try {
			WindowHook::cleanup();
		}
		catch (...) { /* swallow */ }

		// 3) Tear down D3D resources and Present detours
		try {
			D3DHook::instance().cleanUp();
		}
		catch (...) { /* swallow */ }

		// 4) As a final safety net, disable all MinHook hooks if initialized
		//    This prevents any dangling detours from calling back into our DLL during host teardown.
		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
	}

	// Core loop of the system
	void System::mainLoop()
	{
		
		if (D3DHook::instance().needsInitialization())
			D3DHook::instance().performQueuedInitialization();

		if (RUN_IN_HOOKED_PRESENT)
		{
			// If RUN_IN_HOOKED_PRESENT is true, we need to run the main loop in the hooked present function.
			// so we just log this and let the hooked present deal with it
			MessageHandler::logLine("DirectX Hook Enabled: Running frame update in hooked present");
		}
	}


	// updates the data and camera for a frame 
	void System::updateFrame()
	{
		//updateDeltaTime();
		CameraManipulator::cacheGameAddresses(_addressData);
		validateAddresses(); //needed in dirt 2
		cameraStateProcessor();
		handleUserInput();
		
	}

	void System::validateAddresses()
	{
		isCameraStructValid = CameraManipulator::validateCameraMemory();
		isPlayerStructValid = CameraManipulator::validatePLayerPositionMemory();
	}

	void System::cameraStateProcessor()
	{
		if (!g_cameraEnabled)
			return;

		static auto& Camera = Camera::instance();
		Camera.updateCamera();
	}

	void System::updateDeltaTime()
	{

		// Use real-time calculation (original behavior)
		LARGE_INTEGER currentFrameTime;
		QueryPerformanceCounter(&currentFrameTime);
		_deltaTime = static_cast<float>(currentFrameTime.QuadPart - _lastFrameTime.QuadPart) / static_cast<float>(_frequency.QuadPart);
		_lastFrameTime = currentFrameTime;
		//MessageHandler::logLine("Delta time: %.6f", _deltaTime);

	}
	
	void System::handleUserInput()
	{

		if (!_cameraStructFound)
		{
			// camera not found yet, can't proceed.
			return;
		}

		// Keep existing XInput update for other controls
		Globals::instance().gamePad().update();

		// DirectInput (wheel buttons)
		if (!s_directInput)
		{
			s_directInput = new IGCS::DirectInputPad(); // intentionally leaked on process exit
			s_directInput->initialize(Globals::instance().mainWindowHandle());
		}
		else if (!s_directInput->isInitialized())
		{
			s_directInput->initialize(Globals::instance().mainWindowHandle());
		}

		// Guard update – if DI fails during shutdown, update() simply no-ops
		s_directInput->update();

		// Toggle on configured DirectInput button index (default 13 from config)
		const int diIndex = IGCS::Config::get().directInputToggleButtonIndex;
		const bool diToggle = s_directInput->isButtonJustPressed(diIndex);

		// Also allow existing keybinding to toggle (optional)
		const bool toggleRequested = diToggle || Input::isActionActivated(ActionType::CameraEnable);

		if (toggleRequested)
		{
			if (g_cameraEnabled)
			{
				// it's going to be disabled, make sure things are alright when we give it back to the host
				CameraManipulator::restoreGameCameraData(_originalData);
				Globals::instance().cameraMovementLocked(false);
				InterceptorHelper::cameraSetup(_aobBlocks, false, _addressData);
			}
			else
			{
				// it's going to be enabled, so cache the original values before we enable it so we can restore it afterwards
				CameraManipulator::cacheGameCameraData(_originalData);
				InterceptorHelper::cameraSetup(_aobBlocks, true, _addressData);
				Camera::instance().prepareCamera();
			}
			g_cameraEnabled ^= 1;
			Camera::instance().toggleFixedCameraMount();
		}

		if (!g_cameraEnabled)
		{
			// camera is disabled. We simply disable all input to the camera movement, by returning now.
			return;
		}


		if (Input::isActionActivated(ActionType::ToggleFixedCameraMount))
		{
			Camera::instance().toggleFixedCameraMount();

		}

	}

	void System::checkDXHookRequired()
	{
		// Check if D3D hooking is disabled via shared memory
		wchar_t memName[64];
		DWORD processId = GetCurrentProcessId();
		swprintf_s(memName, L"IGCS_D3D_DISABLED_%lu", processId);

		if (HANDLE hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, memName); hMapFile != nullptr) {
			// Shared memory exists - D3D hooking should be disabled
			//MessageHandler::logLine("DirectX hooking disabled via shared memory flag");
			// Set the setting value to ensure consistency
			//Globals::instance().settings().d3ddisabled = 1;
			CloseHandle(hMapFile);
		}
	}

	// Initializes system. Will block till camera struct is found.
	void System::initialize()
	{
		MH_Initialize();
		checkDXHookRequired();

		// first grab the window handle
		Globals::instance().mainWindowHandle(Utils::findMainWindow(GetCurrentProcessId()));
		HWND hWnd = Globals::instance().mainWindowHandle();

		Input::registerRawInput();
		//Initialise hooks (D3D, window etc)
		initializeHooks();

		blocksInit = InterceptorHelper::initializeAOBBlocks(_hostImageAddress, _hostImageSize, _aobBlocks);
		InterceptorHelper::getAbsoluteAddresses(_aobBlocks); //return if needed
		cameraStructInit = InterceptorHelper::setCameraStructInterceptorHook(_aobBlocks);
		waitForCameraStructAddresses();		// blocks till camera is found.
		postCameraStructInit = InterceptorHelper::setPostCameraStructHooks(_aobBlocks);

		// camera struct found, init our own camera object now and hook into game code which uses camera.
		_cameraStructFound = true;
		Camera::instance().resetAngles();

		//apply any code changes now
		InterceptorHelper::toolsInit(_aobBlocks);
		_deltaTime = 0.0f;

	}

	// Initialise hooks here
	void System::initializeHooks()
	{
		// Initialize hooks here
		if (!D3DHook::instance().initialize())
			MessageHandler::logError("Failed to initialize D3D hook");
		InputHooker::setXInputHook(true);

	}


	// Waits for the interceptor to pick up the camera struct address. Should only return if address is found 
	void System::waitForCameraStructAddresses()
	{
		MessageHandler::logLine("Waiting for camera struct interception...");
		while(!CameraManipulator::isCameraFound())
		{
			handleUserInput();
			Sleep(500);
		}
		//MessageHandler::addNotification("Camera found.");
	}
		


	void System::toggleInputBlockState()
	{
		//MessageHandler::addNotification(Globals::instance().toggleInputBlocked() ? "Input to game blocked" : "Input to game enabled");
	}
}
