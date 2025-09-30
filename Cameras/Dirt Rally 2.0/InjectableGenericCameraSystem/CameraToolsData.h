#pragma once
#include "stdafx.h"
#include <DirectXMath.h>

namespace IGCS
{
	using namespace DirectX;

	struct CameraToolsData
	{
		uint8_t cameraEnabled;					// 1 is enabled 0 is not enabled
		uint8_t cameraMovementLocked;			// 1 is camera movement is locked, 0 is camera movement isn't locked.
		uint8_t reserved1;
		uint8_t reserved2;
		float fov;								// in degrees
		XMFLOAT3 coordinates;					// camera coordinates (x, y, z)
		XMFLOAT4 lookQuaternion;					// camera look quaternion qx, qy, qz, qw
		XMFLOAT3 rotationMatrixUpVector;			// up vector from the rotation matrix calculated from the look quaternion. 
		XMFLOAT3 rotationMatrixRightVector;		// right vector from the rotation matrix calculated from the look quaternion. 
		XMFLOAT3 rotationMatrixForwardVector;	// forward vector from the rotation matrix calculated from the look quaternion. 
		float pitch;							// in radians
		float yaw;
		float roll;
	};

}