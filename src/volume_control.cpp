// volume_control.cpp
#include "volume_control.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>

#pragma comment(lib, "Ole32.lib")

// ---------------------------------------------------------
// Helper: acquires the IAudioEndpointVolume interface for
// the current default audio playback device.
// Caller must Release() the returned pointer and call
// CoUninitialize() after use (handled by caller wrapper below).
// ---------------------------------------------------------
static HRESULT GetVolumeInterface(IAudioEndpointVolume **ppVolume)
{
    *ppVolume = nullptr;

    IMMDeviceEnumerator *pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);
    if (FAILED(hr))
        return hr;

    IMMDevice *pDevice = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    pEnumerator->Release();
    if (FAILED(hr))
        return hr;

    hr = pDevice->Activate(
        __uuidof(IAudioEndpointVolume), CLSCTX_ALL,
        nullptr, (void **)ppVolume);
    pDevice->Release();

    return hr;
}

bool SetSystemVolume(int volumePercent)
{
    if (volumePercent < 0)
        volumePercent = 0;
    if (volumePercent > 100)
        volumePercent = 100;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // hrInit can be S_FALSE if COM was already initialized on this thread — that's fine.
    bool comInitializedHere = SUCCEEDED(hrInit);

    IAudioEndpointVolume *pVolume = nullptr;
    HRESULT hr = GetVolumeInterface(&pVolume);
    if (FAILED(hr) || !pVolume)
    {
        std::cerr << "[SetSystemVolume] Failed to get volume interface: " << hr << "\n";
        if (comInitializedHere)
            CoUninitialize();
        return false;
    }

    float scalar = static_cast<float>(volumePercent) / 100.0f;
    hr = pVolume->SetMasterVolumeLevelScalar(scalar, nullptr);

    pVolume->Release();
    if (comInitializedHere)
        CoUninitialize();

    if (FAILED(hr))
    {
        std::cerr << "[SetSystemVolume] SetMasterVolumeLevelScalar failed: " << hr << "\n";
        return false;
    }
    return true;
}

int GetSystemVolume(bool &outSuccess)
{
    outSuccess = false;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hrInit);

    IAudioEndpointVolume *pVolume = nullptr;
    HRESULT hr = GetVolumeInterface(&pVolume);
    if (FAILED(hr) || !pVolume)
    {
        std::cerr << "[GetSystemVolume] Failed to get volume interface: " << hr << "\n";
        if (comInitializedHere)
            CoUninitialize();
        return 0;
    }

    float scalar = 0.0f;
    hr = pVolume->GetMasterVolumeLevelScalar(&scalar);

    pVolume->Release();
    if (comInitializedHere)
        CoUninitialize();

    if (FAILED(hr))
    {
        std::cerr << "[GetSystemVolume] GetMasterVolumeLevelScalar failed: " << hr << "\n";
        return 0;
    }

    outSuccess = true;
    return static_cast<int>(scalar * 100.0f + 0.5f); // round to nearest int
}

bool SetSystemMute(bool mute)
{
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hrInit);

    IAudioEndpointVolume *pVolume = nullptr;
    HRESULT hr = GetVolumeInterface(&pVolume);
    if (FAILED(hr) || !pVolume)
    {
        std::cerr << "[SetSystemMute] Failed to get volume interface: " << hr << "\n";
        if (comInitializedHere)
            CoUninitialize();
        return false;
    }

    hr = pVolume->SetMute(mute, nullptr);

    pVolume->Release();
    if (comInitializedHere)
        CoUninitialize();

    if (FAILED(hr))
    {
        std::cerr << "[SetSystemMute] SetMute failed: " << hr << "\n";
        return false;
    }
    return true;
}

bool GetSystemMute(bool &outSuccess)
{
    outSuccess = false;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hrInit);

    IAudioEndpointVolume *pVolume = nullptr;
    HRESULT hr = GetVolumeInterface(&pVolume);
    if (FAILED(hr) || !pVolume)
    {
        std::cerr << "[GetSystemMute] Failed to get volume interface: " << hr << "\n";
        if (comInitializedHere)
            CoUninitialize();
        return false;
    }

    BOOL isMuted = FALSE;
    hr = pVolume->GetMute(&isMuted);

    pVolume->Release();
    if (comInitializedHere)
        CoUninitialize();

    if (FAILED(hr))
    {
        std::cerr << "[GetSystemMute] GetMute failed: " << hr << "\n";
        return false;
    }

    outSuccess = true;
    return isMuted != FALSE;
}