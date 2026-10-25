#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/string.h>
#include "wifi_control.h"
#include "volume_control.h"

namespace nb = nanobind;
using namespace nb::literals;

int add(int a, int b) { return a + b; }

std::pair<bool, bool> get_wifi_state_py()
{
    bool success = false;
    bool isOn = GetWifiRadioState(success);
    return {success, isOn};
}

std::pair<bool, std::vector<std::tuple<std::string, int, bool>>> list_networks_py()
{
    bool success = false;
    auto networks = ListAvailableNetworks(success);
    return {success, networks};
}

std::pair<bool, int> get_volume_py()
{
    bool success = false;
    int vol = GetSystemVolume(success);
    return {success, vol};
}

std::pair<bool, bool> get_mute_py()
{
    bool success = false;
    bool isMuted = GetSystemMute(success);
    return {success, isMuted};
}

NB_MODULE(example, m)
{
    m.def("add", &add);

    m.def("set_wifi_radio_state", &SetWifiRadioState);
    m.def("get_wifi_radio_state", &get_wifi_state_py);

    m.def("list_available_networks", &list_networks_py,
          "Returns (success, [(ssid, signal_percent, is_secured), ...]).");
    m.def("connect_to_wifi", &ConnectToWifi,
          "ssid"_a, "password"_a = "",
          "Connects to a WiFi network. Leave password empty for open networks.");

    m.def("set_system_volume", &SetSystemVolume);
    m.def("get_system_volume", &get_volume_py);

    m.def("set_system_mute", &SetSystemMute);
    m.def("get_system_mute", &get_mute_py);
}