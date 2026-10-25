// brightness_control.cpp
#include "brightness_control.h"
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <iostream>

#pragma comment(lib, "wbemuuid.lib")

// ---------------------------------------------------------
// Helper: connects to ROOT\WMI namespace and returns
// an IWbemServices pointer. Caller must Release() it.
// Returns false and logs on failure.
// ---------------------------------------------------------
static bool ConnectToWmiNamespace(IWbemLocator **ppLocator, IWbemServices **ppServices)
{
    HRESULT hr = CoCreateInstance(
        CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *)ppLocator);
    if (FAILED(hr))
    {
        std::cerr << "[WMI] Failed to create IWbemLocator: " << hr << "\n";
        return false;
    }

    hr = (*ppLocator)->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, ppServices);
    if (FAILED(hr))
    {
        std::cerr << "[WMI] ConnectServer failed: " << hr << "\n";
        (*ppLocator)->Release();
        *ppLocator = nullptr;
        return false;
    }

    hr = CoSetProxyBlanket(
        *ppServices,
        RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE);
    if (FAILED(hr))
    {
        std::cerr << "[WMI] CoSetProxyBlanket failed: " << hr << "\n";
        (*ppServices)->Release();
        (*ppLocator)->Release();
        *ppServices = nullptr;
        *ppLocator = nullptr;
        return false;
    }

    return true;
}

// ---------------------------------------------------------
// Sets a fresh COM security context. Safe to call once per
// process; if already initialized this may return an error
// we can ignore (RPC_E_TOO_LATE).
// ---------------------------------------------------------
static void InitComSecurityOnce()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    HRESULT hr = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);

    // RPC_E_TOO_LATE means security was already initialized elsewhere (fine to ignore)
    if (FAILED(hr) && hr != RPC_E_TOO_LATE)
    {
        std::cerr << "[WMI] CoInitializeSecurity warning: " << hr << "\n";
    }
}

bool SetScreenBrightness(int brightnessPercent)
{
    if (brightnessPercent < 0)
        brightnessPercent = 0;
    if (brightnessPercent > 100)
        brightnessPercent = 100;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hrInit);
    InitComSecurityOnce();

    IWbemLocator *pLocator = nullptr;
    IWbemServices *pServices = nullptr;
    if (!ConnectToWmiNamespace(&pLocator, &pServices))
    {
        if (comInitializedHere)
            CoUninitialize();
        return false;
    }

    // Get the WmiMonitorBrightnessMethods class
    IEnumWbemClassObject *pEnumerator = nullptr;
    HRESULT hr = pServices->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM WmiMonitorBrightnessMethods"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnumerator);

    if (FAILED(hr) || !pEnumerator)
    {
        std::cerr << "[SetScreenBrightness] ExecQuery failed: " << hr << "\n";
        pServices->Release();
        pLocator->Release();
        if (comInitializedHere)
            CoUninitialize();
        return false;
    }

    IWbemClassObject *pInstance = nullptr;
    ULONG returned = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pInstance, &returned);
    pEnumerator->Release();

    if (FAILED(hr) || returned == 0 || !pInstance)
    {
        std::cerr << "[SetScreenBrightness] No brightness-capable display found.\n";
        pServices->Release();
        pLocator->Release();
        if (comInitializedHere)
            CoUninitialize();
        return false;
    }

    // Get the InstanceName property to target the exact instance in the method call
    VARIANT vInstanceName;
    VariantInit(&vInstanceName);
    pInstance->Get(L"InstanceName", 0, &vInstanceName, nullptr, nullptr);

    // Build the input parameters object for WmiSetBrightness
    IWbemClassObject *pClass = nullptr;
    hr = pServices->GetObject(_bstr_t(L"WmiMonitorBrightnessMethods"), 0, nullptr, &pClass, nullptr);
    if (FAILED(hr) || !pClass)
    {
        std::cerr << "[SetScreenBrightness] GetObject(class) failed: " << hr << "\n";
        VariantClear(&vInstanceName);
        pInstance->Release();
        pServices->Release();
        pLocator->Release();
        if (comInitializedHere)
            CoUninitialize();
        return false;
    }

    IWbemClassObject *pInParamsDef = nullptr;
    pClass->GetMethod(L"WmiSetBrightness", 0, &pInParamsDef, nullptr);

    IWbemClassObject *pInParams = nullptr;
    pInParamsDef->SpawnInstance(0, &pInParams);

    VARIANT vTimeout;
    VariantInit(&vTimeout);
    vTimeout.vt = VT_UI4;
    vTimeout.ulVal = 0; // instant
    pInParams->Put(L"Timeout", 0, &vTimeout, 0);

    VARIANT vBrightness;
    VariantInit(&vBrightness);
    vBrightness.vt = VT_UI1;
    vBrightness.bVal = static_cast<BYTE>(brightnessPercent);
    pInParams->Put(L"Brightness", 0, &vBrightness, 0);

    // Execute the method on the specific instance (by InstanceName path)
    _bstr_t path = _bstr_t(L"WmiMonitorBrightnessMethods.InstanceName='") + vInstanceName.bstrVal + _bstr_t(L"'");

    IWbemClassObject *pOutParams = nullptr;
    hr = pServices->ExecMethod(
        path, _bstr_t(L"WmiSetBrightness"), 0, nullptr, pInParams, &pOutParams, nullptr);

    // Cleanup
    if (pOutParams)
        pOutParams->Release();
    pInParams->Release();
    pInParamsDef->Release();
    pClass->Release();
    VariantClear(&vTimeout);
    VariantClear(&vBrightness);
    VariantClear(&vInstanceName);
    pInstance->Release();
    pServices->Release();
    pLocator->Release();
    if (comInitializedHere)
        CoUninitialize();

    if (FAILED(hr))
    {
        std::cerr << "[SetScreenBrightness] ExecMethod failed: " << hr << "\n";
        return false;
    }
    return true;
}

int GetScreenBrightness(bool &outSuccess)
{
    outSuccess = false;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hrInit);
    InitComSecurityOnce();

    IWbemLocator *pLocator = nullptr;
    IWbemServices *pServices = nullptr;
    if (!ConnectToWmiNamespace(&pLocator, &pServices))
    {
        if (comInitializedHere)
            CoUninitialize();
        return 0;
    }

    IEnumWbemClassObject *pEnumerator = nullptr;
    HRESULT hr = pServices->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM WmiMonitorBrightness"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnumerator);

    if (FAILED(hr) || !pEnumerator)
    {
        std::cerr << "[GetScreenBrightness] ExecQuery failed: " << hr << "\n";
        pServices->Release();
        pLocator->Release();
        if (comInitializedHere)
            CoUninitialize();
        return 0;
    }

    IWbemClassObject *pInstance = nullptr;
    ULONG returned = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pInstance, &returned);
    pEnumerator->Release();

    if (FAILED(hr) || returned == 0 || !pInstance)
    {
        std::cerr << "[GetScreenBrightness] No brightness-capable display found.\n";
        pServices->Release();
        pLocator->Release();
        if (comInitializedHere)
            CoUninitialize();
        return 0;
    }

    VARIANT vBrightness;
    VariantInit(&vBrightness);
    hr = pInstance->Get(L"CurrentBrightness", 0, &vBrightness, nullptr, nullptr);

    int brightness = 0;
    if (SUCCEEDED(hr))
    {
        brightness = vBrightness.bVal; // CurrentBrightness is uint8, already 0-100
        outSuccess = true;
    }
    else
    {
        std::cerr << "[GetScreenBrightness] Get(CurrentBrightness) failed: " << hr << "\n";
    }

    VariantClear(&vBrightness);
    pInstance->Release();
    pServices->Release();
    pLocator->Release();
    if (comInitializedHere)
        CoUninitialize();

    return brightness;
}