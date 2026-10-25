// wifi_control.cpp
#include "wifi_control.h"
#include <windows.h>
#include <wlanapi.h>
#include <iostream>
#include <sstream>

#pragma comment(lib, "Wlanapi.lib")

// ===========================================================
// Radio state control
// ===========================================================

bool SetWifiRadioState(bool turnOn)
{
    HANDLE hClient = nullptr;
    DWORD dwCurVersion = 0;

    DWORD dwResult = WlanOpenHandle(2, nullptr, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[SetWifiRadioState] WlanOpenHandle failed: " << dwResult << "\n";
        return false;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    dwResult = WlanEnumInterfaces(hClient, nullptr, &pIfList);
    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[SetWifiRadioState] WlanEnumInterfaces failed: " << dwResult << "\n";
        WlanCloseHandle(hClient, nullptr);
        return false;
    }

    if (pIfList->dwNumberOfItems == 0)
    {
        std::cerr << "[SetWifiRadioState] No WLAN interfaces found on this system.\n";
        WlanFreeMemory(pIfList);
        WlanCloseHandle(hClient, nullptr);
        return false;
    }

    GUID interfaceGuid = pIfList->InterfaceInfo[0].InterfaceGuid;

    WLAN_PHY_RADIO_STATE radioState = {};
    radioState.dwPhyIndex = 0;
    radioState.dot11SoftwareRadioState =
        turnOn ? dot11_radio_state_on : dot11_radio_state_off;
    radioState.dot11HardwareRadioState = dot11_radio_state_unknown;

    dwResult = WlanSetInterface(
        hClient,
        &interfaceGuid,
        wlan_intf_opcode_radio_state,
        sizeof(WLAN_PHY_RADIO_STATE),
        &radioState,
        nullptr);

    WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, nullptr);

    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[SetWifiRadioState] WlanSetInterface failed: " << dwResult << "\n";
        return false;
    }

    return true;
}

bool GetWifiRadioState(bool &outSuccess)
{
    outSuccess = false;
    HANDLE hClient = nullptr;
    DWORD dwCurVersion = 0;

    DWORD dwResult = WlanOpenHandle(2, nullptr, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[GetWifiRadioState] WlanOpenHandle failed: " << dwResult << "\n";
        return false;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    dwResult = WlanEnumInterfaces(hClient, nullptr, &pIfList);
    if (dwResult != ERROR_SUCCESS || pIfList->dwNumberOfItems == 0)
    {
        std::cerr << "[GetWifiRadioState] No interfaces found or enum failed.\n";
        if (pIfList)
            WlanFreeMemory(pIfList);
        WlanCloseHandle(hClient, nullptr);
        return false;
    }

    GUID interfaceGuid = pIfList->InterfaceInfo[0].InterfaceGuid;

    PWLAN_RADIO_STATE pRadioState = nullptr;
    DWORD dwOutSize = 0;
    WLAN_OPCODE_VALUE_TYPE opcodeValueType;

    dwResult = WlanQueryInterface(
        hClient,
        &interfaceGuid,
        wlan_intf_opcode_radio_state,
        nullptr,
        &dwOutSize,
        (PVOID *)&pRadioState,
        &opcodeValueType);

    WlanFreeMemory(pIfList);

    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[GetWifiRadioState] WlanQueryInterface failed: " << dwResult << "\n";
        WlanCloseHandle(hClient, nullptr);
        return false;
    }

    bool isOn = false;
    if (pRadioState->dwNumberOfPhys > 0)
    {
        isOn = (pRadioState->PhyRadioState[0].dot11SoftwareRadioState == dot11_radio_state_on);
    }

    WlanFreeMemory(pRadioState);
    WlanCloseHandle(hClient, nullptr);

    outSuccess = true;
    return isOn;
}

// ===========================================================
// Network scan & connect
// ===========================================================

static std::string SsidToString(const DOT11_SSID &ssid)
{
    return std::string(reinterpret_cast<const char *>(ssid.ucSSID), ssid.uSSIDLength);
}

std::vector<std::tuple<std::string, int, bool>> ListAvailableNetworks(bool &outSuccess)
{
    outSuccess = false;
    std::vector<std::tuple<std::string, int, bool>> results;

    HANDLE hClient = nullptr;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(2, nullptr, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[ListAvailableNetworks] WlanOpenHandle failed: " << dwResult << "\n";
        return results;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    dwResult = WlanEnumInterfaces(hClient, nullptr, &pIfList);
    if (dwResult != ERROR_SUCCESS || pIfList->dwNumberOfItems == 0)
    {
        std::cerr << "[ListAvailableNetworks] No WLAN interfaces found.\n";
        if (pIfList)
            WlanFreeMemory(pIfList);
        WlanCloseHandle(hClient, nullptr);
        return results;
    }

    GUID interfaceGuid = pIfList->InterfaceInfo[0].InterfaceGuid;
    WlanFreeMemory(pIfList);

    WlanScan(hClient, &interfaceGuid, nullptr, nullptr, nullptr);
    Sleep(2000);

    PWLAN_AVAILABLE_NETWORK_LIST pNetList = nullptr;
    dwResult = WlanGetAvailableNetworkList(
        hClient, &interfaceGuid, 0, nullptr, &pNetList);

    if (dwResult != ERROR_SUCCESS || !pNetList)
    {
        std::cerr << "[ListAvailableNetworks] WlanGetAvailableNetworkList failed: " << dwResult << "\n";
        WlanCloseHandle(hClient, nullptr);
        return results;
    }

    for (DWORD i = 0; i < pNetList->dwNumberOfItems; ++i)
    {
        const WLAN_AVAILABLE_NETWORK &net = pNetList->Network[i];
        std::string ssid = SsidToString(net.dot11Ssid);
        if (ssid.empty())
            continue;

        int signalQuality = static_cast<int>(net.wlanSignalQuality);
        bool isSecured = (net.bSecurityEnabled != FALSE);

        results.emplace_back(ssid, signalQuality, isSecured);
    }

    WlanFreeMemory(pNetList);
    WlanCloseHandle(hClient, nullptr);

    outSuccess = true;
    return results;
}

static std::wstring BuildProfileXml(const std::string &ssid, const std::string &password, bool isOpen)
{
    std::wstring wssid(ssid.begin(), ssid.end());
    std::wstringstream xml;

    xml << L"<?xml version=\"1.0\"?>"
        << L"<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
        << L"<name>" << wssid << L"</name>"
        << L"<SSIDConfig><SSID><name>" << wssid << L"</name></SSID></SSIDConfig>"
        << L"<connectionType>ESS</connectionType>"
        << L"<connectionMode>auto</connectionMode>"
        << L"<MSM><security>"
        << L"<authEncryption>"
        << L"<authentication>" << (isOpen ? L"open" : L"WPA2PSK") << L"</authentication>"
        << L"<encryption>" << (isOpen ? L"none" : L"AES") << L"</encryption>"
        << L"<useOneX>false</useOneX>"
        << L"</authEncryption>";

    if (!isOpen)
    {
        std::wstring wpass(password.begin(), password.end());
        xml << L"<sharedKey>"
            << L"<keyType>passPhrase</keyType>"
            << L"<protected>false</protected>"
            << L"<keyMaterial>" << wpass << L"</keyMaterial>"
            << L"</sharedKey>";
    }

    xml << L"</security></MSM></WLANProfile>";

    return xml.str();
}

bool ConnectToWifi(const std::string &ssid, const std::string &password)
{
    HANDLE hClient = nullptr;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(2, nullptr, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[ConnectToWifi] WlanOpenHandle failed: " << dwResult << "\n";
        return false;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    dwResult = WlanEnumInterfaces(hClient, nullptr, &pIfList);
    if (dwResult != ERROR_SUCCESS || pIfList->dwNumberOfItems == 0)
    {
        std::cerr << "[ConnectToWifi] No WLAN interfaces found.\n";
        if (pIfList)
            WlanFreeMemory(pIfList);
        WlanCloseHandle(hClient, nullptr);
        return false;
    }

    GUID interfaceGuid = pIfList->InterfaceInfo[0].InterfaceGuid;
    WlanFreeMemory(pIfList);

    bool isOpen = password.empty();
    std::wstring profileXml = BuildProfileXml(ssid, password, isOpen);

    DWORD reasonCode = 0;
    dwResult = WlanSetProfile(
        hClient, &interfaceGuid,
        0,
        profileXml.c_str(),
        nullptr,
        TRUE,
        nullptr,
        &reasonCode);

    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[ConnectToWifi] WlanSetProfile failed: " << dwResult
                  << " (reason code: " << reasonCode << ")\n";
        WlanCloseHandle(hClient, nullptr);
        return false;
    }

    std::wstring wssid(ssid.begin(), ssid.end());

    WLAN_CONNECTION_PARAMETERS connParams = {};
    connParams.wlanConnectionMode = wlan_connection_mode_profile;
    connParams.strProfile = wssid.c_str();
    connParams.dot11BssType = dot11_BSS_type_infrastructure;
    connParams.dwFlags = 0;

    dwResult = WlanConnect(hClient, &interfaceGuid, &connParams, nullptr);

    WlanCloseHandle(hClient, nullptr);

    if (dwResult != ERROR_SUCCESS)
    {
        std::cerr << "[ConnectToWifi] WlanConnect failed: " << dwResult << "\n";
        return false;
    }

    return true;
}