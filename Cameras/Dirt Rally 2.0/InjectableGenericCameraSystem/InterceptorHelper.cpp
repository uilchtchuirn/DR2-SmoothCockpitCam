////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part of Injectable Generic Camera System
// Copyright(c) 2020-2025, Frans Bouma
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
#include "InterceptorHelper.h"
#include "GameConstants.h"
#include "GameImageHooker.h"
#include "MessageHandler.h"
#include "CameraManipulator.h"
#include "Globals.h"

using namespace std;

//--------------------------------------------------------------------------------------------------------------------------------
// external asm functions
extern "C" {
    void cameraStructInterceptor();
    void cameraWriteInjection1();
    void cameraWriteInjection2();
    void cameraWriteInjection3();
    void cameraWriteInjection4();
    void cameraWriteInjection5();
	void carPositionInterceptor();
}

// external addresses used in asm.
extern "C" {
    uint8_t* _cameraStructInterceptionContinue = nullptr;
    uint8_t* _cameraWriteInjection1Continue = nullptr;
    uint8_t* _cameraWriteInjection2Continue = nullptr;
    uint8_t* _cameraWriteInjection3Continue = nullptr;
    uint8_t* _cameraWriteInjection4Continue = nullptr;
    uint8_t* _cameraWriteInjection5Continue = nullptr;
	uint8_t* _fovWriteInjection1Continue = nullptr;
	uint8_t* _fovWriteInjection2Continue = nullptr;
	uint8_t* _fovWriteInjection3Continue = nullptr;
	uint8_t* _fovWriteInjection4Continue = nullptr;
	uint8_t* g_fovAbsoluteAddress = nullptr;
	uint8_t* _carPositionInjectionContinue = nullptr;
	uint8_t* _hudToggleInjectionContinue = nullptr;
	uint8_t* _timescaleInjectionContinue = nullptr;
	uint8_t* _dofInjectionContinue = nullptr;
}

namespace IGCS::GameSpecific
{

    // Helper function for initialization hooks that need error checking
    bool tryInitHook(map<string, AOBBlock>& aobBlocks, const string& key,
        const function<void(AOBBlock&)>& operation)
    {
        try {
            if (aobBlocks.find(key) != aobBlocks.end()) {
                operation(aobBlocks[key]);
                return true;
            }
            MessageHandler::logError("Block '%s' not found in AOBBlocks map", key.c_str());
            return false;
        }
        catch (const exception& e) {
            MessageHandler::logError("Error in operation for block '%s': %s", key.c_str(), e.what());
            return false;
        }
    }

    // AOB pattern constants - definitions moved here from function body for better organization
    const struct {
        string active_camera_address = "4C 8B A1 F0 38 04 00";
    	string cam_write_injection1= "0F C6 D2 27 F3 0F 10 D1 0F C6 D2 27 0F 29 12 48";
        string cam_write_injection2 = "0F 29 03 F3 0F 5C 4B 70";
        string cam_write_injection3 = "0F 29 43 10 0F 5C 73 50";
		string cam_write_injection4 = "0F C6 D2 27 0F 29 56 50 0F";
		string cam_write_injection5 = "F3 0F 10 5C 24 58 0F 14 D8 0F";
        string gameplay_nops1 = "E8 0F 06 01 00 8B 83 F4 00 00 00"; //5 nops
		string gameplay_nops2 = "E8 DF 1B 01 00"; //5 nops
		string gameplay_nops3 = "E8 DF 34 01 00"; //5 nops
		string gameplay_nops4 = "FF 90 E8 00 00 00 40 84 ED"; //6 nops
		string gameplay_nops5 = "E8 BD 64 02 00"; //5 nops
        string absolute_fov = "F3 0F 59 0D | 00 9F 97 00";
		string fov_write_nop = "F3 0F 11 47 70 8B 43 18"; // 5 nops
		string fov_write_nop2 = "F3 0F 11 43 70 F3 0F 59 0D 00 9F 97 00"; //5 nops
        string collision_nop1 = "75 35 F3 0F 10 83 08 0A 00 00"; //2 nops
		string collision_nop2 = "77 0F C7 83 08 0A 00 00 00 00 00 00"; //2 nops
		string car_position_injection = "F3 0F 10 99 B0 02 00 00 ";
		string focusloss_nop = "48 8D 05 19 EB EB 00 C3 CC"; //7 nops
		string hud_toggle_injection = "48 8B 81 70 01 00 00 41 0F 10 40 10";
		string dof_injection = "F3 0F 10 81 AC 07 00 00";
    } aob;

    bool InterceptorHelper::initializeAOBBlocks(const LPBYTE hostImageAddress, DWORD hostImageSize, map<string, AOBBlock>& aobBlocks)
    {
        // Initialize all AOB blocks with their patterns
        aobBlocks[ACTIVE_CAMERA_ADDRESS_INTERCEPT] = AOBBlock(ACTIVE_CAMERA_ADDRESS_INTERCEPT, aob.active_camera_address, 1);
    	aobBlocks[CAM_WRITE1] = AOBBlock(CAM_WRITE1, aob.cam_write_injection1, 1);
        aobBlocks[CAM_WRITE2] = AOBBlock(CAM_WRITE2, aob.cam_write_injection2, 1);
		aobBlocks[CAM_WRITE3] = AOBBlock(CAM_WRITE3, aob.cam_write_injection3, 1);
		aobBlocks[CAM_WRITE4] = AOBBlock(CAM_WRITE4, aob.cam_write_injection4, 1);
		aobBlocks[CAM_WRITE5] = AOBBlock(CAM_WRITE5, aob.cam_write_injection5, 1);
		aobBlocks[GAMEPLAY_NOP1] = AOBBlock(GAMEPLAY_NOP1, aob.gameplay_nops1, 1);
		aobBlocks[GAMEPLAY_NOP2] = AOBBlock(GAMEPLAY_NOP2, aob.gameplay_nops2, 1);
		aobBlocks[GAMEPLAY_NOP3] = AOBBlock(GAMEPLAY_NOP3, aob.gameplay_nops3, 1);
		aobBlocks[GAMEPLAY_NOP4] = AOBBlock(GAMEPLAY_NOP4, aob.gameplay_nops4, 1);
		aobBlocks[GAMEPLAY_NOP5] = AOBBlock(GAMEPLAY_NOP5, aob.gameplay_nops5, 1);
		aobBlocks[FOV_ABS] = AOBBlock(FOV_ABS, aob.absolute_fov, 1);
		aobBlocks[FOV_WRITE_NOP] = AOBBlock(FOV_WRITE_NOP, aob.fov_write_nop, 1);
        aobBlocks[FOV_WRITE_NOP2] = AOBBlock(FOV_WRITE_NOP2, aob.fov_write_nop2, 1);
		aobBlocks[COLLISION_NOP1] = AOBBlock(COLLISION_NOP1, aob.collision_nop1, 1);
		aobBlocks[COLLISION_NOP2] = AOBBlock(COLLISION_NOP2, aob.collision_nop2, 1);
		aobBlocks[CAR_POSITION_INJECTION] = AOBBlock(CAR_POSITION_INJECTION, aob.car_position_injection, 1);
		aobBlocks[FOCUS_LOSS_NOP] = AOBBlock(FOCUS_LOSS_NOP, aob.focusloss_nop, 1);
		aobBlocks[HUD_TOGGLE_INJECTION] = AOBBlock(HUD_TOGGLE_INJECTION, aob.hud_toggle_injection, 1);
		aobBlocks[DOF_INJECTION] = AOBBlock(DOF_INJECTION, aob.dof_injection, 1);

        // Scan for all patterns
        bool result = true;
        for (auto& [key, block] : aobBlocks)
        {
            const bool scanResult = block.scan(hostImageAddress, hostImageSize);
            if (!scanResult) {
                MessageHandler::logError("Failed to find pattern for block '%s'", key.c_str());
            }
            result &= scanResult; 
        }

        if (result) {
            MessageHandler::logLine("All interception offsets found successfully.");
        }
        else {
            MessageHandler::logError("One or more interception offsets weren't found: tools aren't compatible with this game's version.");
        }
        return result;
    }

    bool InterceptorHelper::setCameraStructInterceptorHook(map<string, AOBBlock>& aobBlocks)
    {
        return tryInitHook(aobBlocks, ACTIVE_CAMERA_ADDRESS_INTERCEPT, [](AOBBlock& block) {
                GameImageHooker::setHook(&block, 0x12, &_cameraStructInterceptionContinue, &cameraStructInterceptor);
            });
    }

	bool InterceptorHelper::setPostCameraStructHooks(map<string, AOBBlock>& aobBlocks)
	{
		bool result = true;
		result &= tryInitHook(aobBlocks, CAM_WRITE1,
			[](AOBBlock& block) {
				GameImageHooker::setHook(&block, 0xF, &_cameraWriteInjection1Continue, &cameraWriteInjection1);
			});
		result &= tryInitHook(aobBlocks, CAM_WRITE2,
			[](AOBBlock& block) {
				GameImageHooker::setHook(&block, 0x16, &_cameraWriteInjection2Continue, &cameraWriteInjection2);
			});
		result &= tryInitHook(aobBlocks, CAM_WRITE3,
			[](AOBBlock& block) {
				GameImageHooker::setHook(&block, 0xF, &_cameraWriteInjection3Continue, &cameraWriteInjection3);
			});
		result &= tryInitHook(aobBlocks, CAM_WRITE4,
			[](AOBBlock& block) {
				GameImageHooker::setHook(&block, 0x13, &_cameraWriteInjection4Continue, &cameraWriteInjection4);
			});
		result &= tryInitHook(aobBlocks, CAM_WRITE5,
			[](AOBBlock& block) {
				GameImageHooker::setHook(&block, 0x12, &_cameraWriteInjection5Continue, &cameraWriteInjection5);
			});
		result &= tryInitHook(aobBlocks, CAR_POSITION_INJECTION,
			[](AOBBlock& block) {
				GameImageHooker::setHook(&block, 0x10, &_carPositionInjectionContinue, &carPositionInterceptor);
			});
		return result;
	}

    void InterceptorHelper::getAbsoluteAddresses(map<string, AOBBlock>& aobBlocks)
    {
        g_fovAbsoluteAddress = Utils::calculateAbsoluteAddress(&aobBlocks[FOV_ABS], 4);


    }

    bool InterceptorHelper::cameraSetup(map<string, AOBBlock>& aobBlocks, bool enabled, GameAddressData& addressData)
    {
        try {
            // apply replay and gameplay nops
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP1], 11, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP2], 3, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP3], 4, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP4], 4, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP5], 6, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP6], 4, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP7], 5, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP8], 6, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP9], 3, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP10], 11, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP11], 4, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP12], 9, enabled);
			//Utils::toggleNOPState(aobBlocks[REPLAY_NOP13], 4, enabled);
			Utils::toggleNOPState(aobBlocks[GAMEPLAY_NOP1], 5, enabled);
			Utils::toggleNOPState(aobBlocks[GAMEPLAY_NOP2], 5, enabled);
			Utils::toggleNOPState(aobBlocks[GAMEPLAY_NOP3], 5, enabled);
			Utils::toggleNOPState(aobBlocks[GAMEPLAY_NOP4], 6, enabled);
			Utils::toggleNOPState(aobBlocks[GAMEPLAY_NOP5], 5, enabled);
			// apply fov write nops
			//Utils::toggleNOPState(aobBlocks[FOV_WRITE_NOP], 5, enabled);
			//Utils::toggleNOPState(aobBlocks[FOV_WRITE_NOP2], 5, enabled);
            // apply collision nops
			Utils::toggleNOPState(aobBlocks[COLLISION_NOP1], 2, enabled);
			Utils::toggleNOPState(aobBlocks[COLLISION_NOP2], 2, enabled);

            return true;
        }
        catch (const exception& e) {
            MessageHandler::logError("Failed to set up camera: %s", e.what());
            return false;
        }
    }

    bool InterceptorHelper::toolsInit(map<string, AOBBlock>& aobBlocks)
    {
	    try
	    {
			//This function can be extended to initialize additional tools or patches
			Utils::toggleNOPState(aobBlocks[FOCUS_LOSS_NOP], 7, true);
	    }
	    catch (const exception& e)
	    {
			MessageHandler::logError("Failed to initialise pre-system ready instructions: %s", e.what());
			return false;
	    }


  //  	// Change conditional jump to unconditional jump
  //      uint8_t unconditionalJump[] = { 0xEB };
  //      Utils::saveBytesWrite(aobBlocks[WINDOWFOCUS_JMP], 1, unconditionalJump, true);
    	return true;
    }

}