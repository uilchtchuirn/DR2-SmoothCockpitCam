////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part of Injectable Generic Camera System (refactored 2025)
////////////////////////////////////////////////////////////////////////////////////////////////////////
// ================== Camera.cpp ==================
#include "stdafx.h"
#include "Camera.h"
#include "GameConstants.h"
#include "Globals.h"
#include "CameraManipulator.h"
#include <windows.h> // For QueryPerformanceCounter, QueryPerformanceFrequency
#include <DirectXMath.h>
#include "Config.h"

using namespace DirectX;

namespace IGCS
{

    static XMVECTOR previousPlayerPosVec = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    static int unchangedPlayerPositionFrames = 0;

    // Helper: returns true if the position vectors are "the same" within a small epsilon.
    bool isStationary(const XMVECTOR& posA, const XMVECTOR& posB, float epsilon = 0.0001f)
    {
        XMVECTOR diff = XMVectorSubtract(posA, posB);
        XMVECTOR lenSq = XMVector3LengthSq(diff);
        float distanceSq = 0.0f;
        XMStoreFloat(&distanceSq, lenSq);
        return distanceSq < (epsilon * epsilon);
    }

    // --------------------------------------------- Quaternion / movement helpers -------------------------------------
    XMVECTOR Camera::calculateLookQuaternion() noexcept
    {
        const XMVECTOR q = Utils::generateEulerQuaternion(getRotation());
        XMStoreFloat4(&_toolsQuaternion, q);
        _toolsMatrix = XMMatrixRotationQuaternion(q);
        return q;
    }

    // -----------------------------------------------------------------------------------------------------------------
    XMFLOAT3 Camera::calculateNewCoords(const XMFLOAT3& currentCoords, FXMVECTOR lookQ) noexcept
    {
        const XMVECTOR curr = XMLoadFloat3(&currentCoords);
        const XMVECTOR dir = XMLoadFloat3(&_direction);
        const XMVECTOR newDir = XMVectorAdd(curr, XMVector3Rotate(dir, lookQ));

        XMFLOAT3 result;
        XMStoreFloat3(&result, newDir);
        XMStoreFloat3(&_toolsCoordinates, newDir);
        return result;
    }

    // ---------------------------------------------- Movement state helpers -------------------------------------------
    void Camera::resetMovement() noexcept
    {
        _movementOccurred = false;
        _direction = { 0,0,0 };
    }

    void Camera::resetTargetMovement() noexcept
    {
        _movementOccurred = false;
        _targetdirection = { 0,0,0 };
    }

    void Camera::resetAngles() noexcept
    {
        setPitch(INITIAL_PITCH_RADIANS);
        setRoll(INITIAL_ROLL_RADIANS);
        setYaw(INITIAL_YAW_RADIANS);
        setTargetPitch(INITIAL_PITCH_RADIANS);
        setTargetRoll(INITIAL_ROLL_RADIANS);
        setTargetYaw(INITIAL_YAW_RADIANS);
    }

    // --------------------------------------------- Direct angle setters ---------------------------------------------
    void Camera::setPitch(float angle) noexcept { _pitch = clampAngle(angle); }
    void Camera::setYaw(float angle) noexcept { _yaw = clampAngle(angle); }
    void Camera::setRoll(float angle) noexcept { _roll = clampAngle(angle); }
    float Camera::clampAngle(float angle) noexcept
    {
        while (angle > XM_PI) { angle -= XM_2PI; }
        while (angle < -XM_PI) { angle += XM_2PI; }
        return angle;
    }

    // --------------------------------------------- FOV helpers ------------------------------------------------------
    void Camera::initFOV() noexcept
    {
        if (_fov <= 0.01f)
        {
            _fov = IGCS::GameSpecific::CameraManipulator::getCurrentFoV();
            _targetfov = _fov;
        }
    }

    void Camera::changeFOV(float amount) noexcept
    {
        _targetfov = Utils::rangeClamp(_targetfov + amount, 0.01f, 3.0f);
    }

    void Camera::setFoV(float fov, bool fullreset) noexcept
    {
        if (fullreset)
            _fov = _targetfov = fov;
        else
            _targetfov = fov;
    }

    void Camera::toggleFixedCameraMount() noexcept
    {
        if (!_fixedCameraMountEnabled)
        {
            // Enabling fixed mount - capture current relative offset
            captureCurrentRelativeOffset();
        }
        else
        {
            // Disabling fixed mount - adjust target rotations to account for negation constants
            _targetpitch = NEGATE_PITCH ? -_pitch : _pitch;
            _targetyaw = NEGATE_YAW ? -_yaw : _yaw;
            _targetroll = NEGATE_ROLL ? -_roll : _roll;
        }
        _fixedCameraMountEnabled = !_fixedCameraMountEnabled;
    }

    void Camera::captureCurrentRelativeOffset() noexcept
    {
        // Get current camera position and rotation
        // Camera position is loaded directly from class member _toolsCoordinate below
        const XMFLOAT3 cameraRotation = getRotation();

        // Get player position and rotation
        const XMVECTOR playerPosVec = GameSpecific::CameraManipulator::getCurrentPlayerPosition();
        const XMVECTOR playerRotVec = GameSpecific::CameraManipulator::getCurrentPlayerRotation();

        // Calculate relative position in player's local coordinate system
        const XMVECTOR camPosVec = XMLoadFloat3(&_toolsCoordinates);
        const XMVECTOR worldOffset = XMVectorSubtract(camPosVec, playerPosVec);

        // Transform world offset to player's local space (inverse transform)
        const XMMATRIX playerRotMatrix = XMMatrixRotationQuaternion(playerRotVec);
        const XMMATRIX invPlayerRotMatrix = XMMatrixTranspose(playerRotMatrix);  // Inverse of rotation matrix is its transpose
        const XMVECTOR localOffset = XMVector3Transform(worldOffset, invPlayerRotMatrix);

        // Store the local offset in fixed mount position offset
        XMStoreFloat3(&_fixedMountPositionOffset, localOffset);

        // Calculate and store relative rotation as quaternion
        const XMVECTOR cameraRotVec = generateEulerQuaternion(cameraRotation, MULTIPLICATION_ORDER, false, false, false);

        // Calculate relative rotation: camera = relative * player
        // So: relative = camera * inverse(player)
        const XMVECTOR invPlayerRot = XMQuaternionInverse(playerRotVec);
        _fixedMountRelativeRotation = XMQuaternionMultiply(cameraRotVec, invPlayerRot);
    };

    // --------------------------------------------- Camera update ----------------------------------------------------
    void Camera::updateCamera() noexcept
    {
        _hasValidLookAtTarget = false;

        // Get player transform data
        const XMVECTOR playerPosVec = GameSpecific::CameraManipulator::getCurrentPlayerPosition();
        const XMVECTOR playerRotVec = GameSpecific::CameraManipulator::getCurrentPlayerRotation();

        //if (DirectX::XMVector3Equal(playerPosVec, previousPlayerPosVec))
        //{
        //    unchangedPlayerPositionFrames++;
        //    if (unchangedPlayerPositionFrames == 1) {
        //        MessageHandler::logLine("Player position unchanged for more than one frame in updateCamera!");
        //    }
        //    if (unchangedPlayerPositionFrames % 100 == 0) {
        //        DirectX::XMFLOAT3 pos;
        //        DirectX::XMStoreFloat3(&pos, playerPosVec);
        //        MessageHandler::logLine("Player position stuck for %d frames at %.4f, %.4f, %.4f", unchangedPlayerPositionFrames, pos.x, pos.y, pos.z);
        //    }
        //}
        //else
        //{
        //    unchangedPlayerPositionFrames = 0;
        //    DirectX::XMFLOAT3 pos;
        //    DirectX::XMStoreFloat3(&pos, playerPosVec);
        //    MessageHandler::logDebug("Player position changed: %.4f, %.4f, %.4f", pos.x, pos.y, pos.z);
        //}

        //previousPlayerPosVec = playerPosVec;

        // Apply fixed mount transformation
        // Direct quaternion and position calculation (most efficient)
        // This avoids matrix multiplication entirely

        // Calculate camera position: player position + rotated offset
        const XMVECTOR localOffset = XMLoadFloat3(&_fixedMountPositionOffset);
        const XMVECTOR worldOffset = XMVector3Rotate(localOffset, playerRotVec);
        const XMVECTOR newCameraPos = XMVectorAdd(playerPosVec, worldOffset);

        // Store position directly
        XMStoreFloat3(&_toolsCoordinates, newCameraPos);

        // Calculate camera rotation: relative rotation * player rotation
        const XMVECTOR targetCameraRotQuat = XMQuaternionMultiply(_fixedMountRelativeRotation, playerRotVec);
        const float blend = IGCS::Config::get().blend;
        _smoothedCameraQuat = XMQuaternionSlerp(_smoothedCameraQuat, targetCameraRotQuat, blend);
        // Store quaternion directly
        XMStoreFloat4(&_toolsQuaternion, _smoothedCameraQuat);

        // Update camera data in game memory
        GameSpecific::CameraManipulator::updateCameraDataInGameData();

        // Get final position and rotation
        XMFLOAT3 finalPosition = _toolsCoordinates;
        XMVECTOR finalRotation = XMLoadFloat4(&_toolsQuaternion);

        // Update tools quaternion with final rotation
        XMStoreFloat4(&_toolsQuaternion, finalRotation);

        // Write final values to game memory
        GameSpecific::CameraManipulator::writeNewCameraValuesToGameData(finalPosition, finalRotation);

    }

    // --------------------------------------------- Bridging ---------------------------------------------------------
    // Uses axis inversion settings, applies negation constants to target values
    // Does not apply axis inversion to current angles
    void Camera::setAllRotation(DirectX::XMFLOAT3 eulers) noexcept
    {
        // We need to apply negation constants to target values only
        _targetpitch = (NEGATE_PITCH ? -eulers.x : eulers.x);
        _targetyaw = (NEGATE_YAW ? -eulers.y : eulers.y);
        _targetroll = (NEGATE_ROLL ? -eulers.z : eulers.z);

        // We don't need to apply negation constants to current angles as they do not experience interpolation
        _pitch = eulers.x;
        _yaw = eulers.y;
        _roll = eulers.z;

        initFOV();
    }

    //--------------------------- Camera Prep
    void Camera::prepareCamera() noexcept
    {
        // Initialize internal position from game memory
        _toolsCoordinates = GameSpecific::CameraManipulator::getCurrentCameraCoords();
        // Set camera fov to game fov
        //setFoV(GameSpecific::CameraManipulator::getCurrentFoV(), true);
        // Set initial values
        setAllRotation(GameSpecific::CameraManipulator::getEulers());
        // Reset direction and target direction
        //resetDirection();
        // Initialize FOV
        //initFOV();
    }

} // namespace IGCS
