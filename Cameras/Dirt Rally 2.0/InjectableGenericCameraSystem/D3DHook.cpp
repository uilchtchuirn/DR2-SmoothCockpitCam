// ReSharper disable CppTooWideScope
// ReSharper disable CppUnreachableCode
// ReSharper disable CppRedundantBooleanExpressionArgument
#include "stdafx.h"
#include "D3DHook.h"
#include "Globals.h"
#include "MessageHandler.h"
#include "CameraManipulator.h"
#include "MinHook.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "Utils.h"
#include "DummyWindowHelper.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace IGCS {

    //==================================================================================================
    // Static Member Initialization
    //==================================================================================================
    D3DHook::Present_t D3DHook::_originalPresent = nullptr;
    D3DHook::ResizeBuffers_t D3DHook::_originalResizeBuffers = nullptr;
    D3DHook::OMSetRenderTargets_t D3DHook::_originalOMSetRenderTargets = nullptr;

	//==================================================================================================
	// Getters, setters and other methods
	//==================================================================================================

    void D3DHook::queueInitialization() {
        // Set a flag indicating initialization is needed
        _needsInitialization = true;

    }

    // New method to check if initialization is complete
    bool D3DHook::isFullyInitialized() const {
        return _isInitialized && _renderTargetInitialized && !_needsInitialization;
    }

     //==================================================================================================
    // Singleton Implementation
    //==================================================================================================
    D3DHook& D3DHook::instance() {
        static D3DHook instance;
        return instance;
    }

    //==================================================================================================
    // Constructor & Destructor
    //==================================================================================================

    D3DHook::D3DHook()
    {
    }

    D3DHook::~D3DHook() {
        cleanUp();
    }

    //==================================================================================================
    // Public Interface Methods
    //==================================================================================================
    bool D3DHook::initialize() {
        if (_isInitialized) {
            return true;
        }

        DummyWindowHelper dummyWnd;

        // Create a dummy device and swap chain to get the Present address
        IDXGISwapChain* pTempSwapChain = nullptr;
        ID3D11Device* pTempDevice = nullptr;
        ID3D11DeviceContext* pTempContext = nullptr;

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = 1;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = dummyWnd.hwnd();
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
            &swapChainDesc, &pTempSwapChain, &pTempDevice, nullptr, &pTempContext
        );

        if (FAILED(hr)) {
            MessageHandler::logError("D3DHook::initialize: Failed to create temporary device/swapchain");
            return false;
        }

        // Get the Present function address from the temp swap chain's vftable
        void** vftable = *reinterpret_cast<void***>(pTempSwapChain);
        void* presentAddress = vftable[8]; // Present is at index 8
        //void* resizeBuffersAddress = vftable[13]; // ResizeBuffers is at index 13

        // Hook Present first


        // Create the Present hook
        MH_STATUS status = MH_CreateHook(
            presentAddress,
            &D3DHook::hookedPresent,
            reinterpret_cast<void**>(&_originalPresent)
        );

        if (status != MH_OK) {
            MessageHandler::logError("D3DHook::initialize: Failed to create Present hook, error: %d", status);
            pTempContext->Release();
            pTempDevice->Release();
            pTempSwapChain->Release();
            return false;
        }

        // Enable the Present hook
        status = MH_EnableHook(presentAddress);
        if (status != MH_OK) {
            MessageHandler::logError("D3DHook::initialize: Failed to enable Present hook, error: %d", status);
            pTempContext->Release();
            pTempDevice->Release();
            pTempSwapChain->Release();
            return false;
        }

        MessageHandler::logLine("Successfully hooked Direct3D 11 Present function");

        // Clean up temporary objects - we don't need them anymore
        pTempContext->Release();
        pTempDevice->Release();
        pTempSwapChain->Release();

        _isInitialized = true;
        return true;
    }

    // New method to perform the actual initialization
    // This should be called from a controlled point in your main loop
    void D3DHook::performQueuedInitialization() {
        // Only proceed if initialization is needed
        if (!_needsInitialization) {
            return;
        }

        // Use lock to prevent race conditions
        std::lock_guard<std::mutex> lock(_resourceMutex);

        // Double-check after acquiring lock
        if (!_needsInitialization) {
            return;
        }

        MessageHandler::logDebug("D3DHook::performQueuedInitialization: Starting initialization");

        // Setup hook for depth buffer detection if needed
        if (!_hookingOMSetRenderTargets) {
            MessageHandler::logLine("Hooking OMSetRenderTargets...");
            setupOMSetRenderTargetsHook();
        }

        // Mark initialization as complete
        _needsInitialization = false;
        _isInitialized = true;
        _renderTargetInitialized = true;

        MessageHandler::logDebug("D3DHook::performQueuedInitialization: Initialization complete");
    }

    //==================================================================================================
    // Resource Management Methods
    //==================================================================================================
    void D3DHook::cleanUp() {
        static bool cleanupDone = false;
        if (cleanupDone) {
            return;
        }

        if (!_isInitialized) {
            cleanupDone = true;
            return;
        }

        // 1. First, disable all hooks
        if (_hookingOMSetRenderTargets && _originalOMSetRenderTargets && _pLastContext) {
            void** vtbl = *reinterpret_cast<void***>(_pLastContext);
            if (vtbl) {
                void* originalFunc = vtbl[33];
                MH_DisableHook(originalFunc);
                MH_RemoveHook(originalFunc);
            }
            _hookingOMSetRenderTargets = false;
        }

        // Disable Present and ResizeBuffers hooks
        if (_originalPresent) {
            MH_DisableHook(_originalPresent);
            MH_RemoveHook(_originalPresent);
        }

        if (_originalResizeBuffers) {
            MH_DisableHook(_originalResizeBuffers);
            MH_RemoveHook(_originalResizeBuffers);
        }

        // 2. Clean up all resources
        cleanupAllResources();

        // 3. Cleanup MinHook
        MH_Uninitialize();

        cleanupDone = true;


    }

	void D3DHook::cleanupAllResources() {
        std::lock_guard<std::mutex> lock(_resourceMutex);

        // Helper macro for safe release
		#define SAFE_RELEASE(obj) if(obj) { (obj)->Release(); (obj) = nullptr; }



        // 1. Release Shader Resources
        SAFE_RELEASE(_simpleVertexShader);
        SAFE_RELEASE(_simplePixelShader);
        SAFE_RELEASE(_coloredPixelShader);
        SAFE_RELEASE(_simpleInputLayout);

        // 2. Release Buffer Resources
        SAFE_RELEASE(_constantBuffer);
        SAFE_RELEASE(_colorBuffer);

        // 6. Release Rendering State Resources
        SAFE_RELEASE(_pBlendState);
        SAFE_RELEASE(_pDepthStencilState);
        SAFE_RELEASE(_pRasterizerState);

        // 7. Release Core D3D Instance Resources
        SAFE_RELEASE(_pRenderTargetView);
        SAFE_RELEASE(_pContext);
        SAFE_RELEASE(_pDevice);
        SAFE_RELEASE(_pSwapChain);

        // 8. Release Static D3D Resources
        SAFE_RELEASE(_pLastRTV);
        SAFE_RELEASE(_pLastContext);
        SAFE_RELEASE(_pLastDevice);

        // 10. Clean up Depth Buffer Resources
        // Release all DSVs
        for (auto& dsv : _detectedDepthStencilViews) {
            SAFE_RELEASE(dsv);
        }
        _detectedDepthStencilViews.clear();

        // Release all depth textures
        for (auto& tex : _detectedDepthTextures) {
            SAFE_RELEASE(tex);
        }
        _detectedDepthTextures.clear();
        _depthTextureDescs.clear();

        // 11. Reset Variables and Flags
        _currentDepthBufferIndex = 0;
        _useDetectedDepthBuffer = false;
        _renderTargetInitialized = false;
        _isInitialized = false;
        _windowWidth = 0;
        _windowHeight = 0;

        // 13. Reset Atomic Flags
        _needsInitialization.store(false, std::memory_order_release);
        _isChangingMode.store(false, std::memory_order_release);
        _resourcesNeedUpdate.store(false, std::memory_order_release);

		#undef SAFE_RELEASE

    }

    //==================================================================================================
    // Hook Functions
    //==================================================================================================
    HRESULT STDMETHODCALLTYPE D3DHook::hookedPresent(IDXGISwapChain* pSwapChain, const UINT SyncInterval, const UINT Flags)
    {
        // Device acquisition - if we don't have a device yet
        if (!instance()._pLastDevice && pSwapChain) {
            bool needsInit = false;
            {
                std::lock_guard<std::mutex> lock(instance()._resourceMutex);
                if (!instance()._pLastDevice) { // Double-check after acquiring lock
                    ID3D11Device* pDevice = nullptr;
                    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice)))) {
                        // Set static members
                        instance()._pLastDevice = pDevice;
                        pDevice->GetImmediateContext(&instance()._pLastContext);

                        // Set instance members
                        D3DHook& hook = instance();
                        hook._pDevice = pDevice;
                        hook._pSwapChain = pSwapChain;
                        pSwapChain->AddRef();
                        pDevice->GetImmediateContext(&hook._pContext);

                        needsInit = true;
                    }
                }
            }

            if (needsInit) {
                instance().queueInitialization();
            }
        }

        // Call the original Present
        HRESULT hr = _originalPresent(pSwapChain, SyncInterval, Flags);

        // After presenting, increment epoch to mark end of the frame
        instance()._frameEpoch.fetch_add(1, std::memory_order_relaxed);

        // Cache the backbuffer texture if we don't have it yet
        if (instance()._pSwapChain && instance()._pBackBufferTex == nullptr) {
            ID3D11Texture2D* backbuf = nullptr;
            if (SUCCEEDED(instance()._pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuf)))) {
                instance()._pBackBufferTex = backbuf; // hold a ref returned by GetBuffer
            }
        }

        return hr;
    }


    void STDMETHODCALLTYPE D3DHook::hookedOMSetRenderTargets(
        ID3D11DeviceContext* pContext,
        const UINT NumViews,
        ID3D11RenderTargetView* const* ppRenderTargetViews,
        ID3D11DepthStencilView* pDepthStencilView)
    {
        // Prevent recursion
        static thread_local bool inOurCode = false;
        if (inOurCode) return;
        inOurCode = true;

        // Call original exactly once
        _originalOMSetRenderTargets(pContext, NumViews, ppRenderTargetViews, pDepthStencilView);

        // Only act on the immediate context and when system is active
        bool shouldUpdate = false;
        if (pContext && Globals::instance().systemActive()) {
            if (pContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE) {

                // Identify if RTV[0] is the backbuffer
                if (NumViews > 0 && ppRenderTargetViews && ppRenderTargetViews[0] && instance()._pBackBufferTex) {
                    ID3D11Resource* r = nullptr;
                    ppRenderTargetViews[0]->GetResource(&r);
                    if (r) {
                        ID3D11Texture2D* tex = nullptr;
                        HRESULT qhr = r->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex));
                        r->Release();

                        if (SUCCEEDED(qhr) && tex) {
                            // Compare pointer identity to the cached backbuffer
                            if (tex == instance()._pBackBufferTex) {
                                shouldUpdate = true;
                            }
                            tex->Release();
                        }
                    }
                }
            }
        }

        if (shouldUpdate) {
            // Once-per-frame guard tied to Present’s epoch
            static thread_local uint64_t s_lastEpoch = 0;
            const uint64_t epoch = instance()._frameEpoch.load(std::memory_order_relaxed);
            if (s_lastEpoch != epoch) {
                System::instance().updateFrame();
                s_lastEpoch = epoch;
            }
        }

        inOurCode = false;
    }

    //==================================================================================================
    // Depth Buffer Management
    //==================================================================================================

    void D3DHook::setupOMSetRenderTargetsHook() {
        // Early return if already hooked or context is null
        if (_hookingOMSetRenderTargets || !_pLastContext) {
            return;
        }

        MessageHandler::logDebug("D3DHook::setupOMSetRenderTargetsHook: Setting up hook to capture actual game depth buffers");

        // Get the virtual table pointer
        void** vtbl = *reinterpret_cast<void***>(_pLastContext);

        // OMSetRenderTargets is at index 33 in the ID3D11DeviceContext vtable
        void* originalFunc = vtbl[33];

        // Create the hook
        MH_STATUS status = MH_CreateHook(
            originalFunc,
            &D3DHook::hookedOMSetRenderTargets,
            reinterpret_cast<void**>(&_originalOMSetRenderTargets)
        );

        if (status != MH_OK) {
            MessageHandler::logError("D3DHook::setupOMSetRenderTargetsHook: Failed to create hook, error: %d", status);
            return;
        }

        // Enable the hook
        status = MH_EnableHook(originalFunc);
        if (status != MH_OK) {
            MessageHandler::logError("D3DHook::setupOMSetRenderTargetsHook: Failed to enable hook, error: %d", status);
            return;
        }

        // Mark as hooked
        _hookingOMSetRenderTargets = true;
        MessageHandler::logDebug("D3DHook::setupOMSetRenderTargetsHook: Successfully hooked OMSetRenderTargets");
    }

    void D3DHook::validateAndAcquireDeviceContext()
    {
        if (!_pLastDevice || !_pLastContext || !_pSwapChain)
        {
            MessageHandler::logDebug("D3DHook: Validating and reacquiring device/context on camera enable.");
            if (_pSwapChain)
            {
                ID3D11Device* pDevice = nullptr;
                if (SUCCEEDED(_pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice))))
                {
                    _pLastDevice = pDevice;
                    pDevice->GetImmediateContext(&_pLastContext);
                }
            }
        }
    }
}