#pragma once
#include <string>
#include <vector>
#include <tuple>

// ---- Radio state control ----

// Turns the WiFi radio on/off. Returns true on success.
bool SetWifiRadioState(bool turnOn);

// Queries current radio state.
// outSuccess is set false if the query itself failed.
bool GetWifiRadioState(bool &outSuccess);

// ---- Network scan & connect ----

// Returns list of (ssid, signal_quality_percent, is_secured) for
// currently visible WiFi networks. outSuccess=false on failure.
std::vector<std::tuple<std::string, int, bool>> ListAvailableNetworks(bool &outSuccess);

// Connects to a WiFi network by SSID. If the network requires a
// password, pass it in `password` (ignored for open networks).
// Builds/overwrites a WLAN profile for this SSID using WPA2-PSK/AES.
// Returns true if the connect request was issued successfully
// (does NOT guarantee association succeeded).
bool ConnectToWifi(const std::string &ssid, const std::string &password);