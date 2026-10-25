#pragma once

// Sets system volume as a percentage (0-100). Returns true on success.
bool SetSystemVolume(int volumePercent);

// Gets current system volume as a percentage (0-100).
// outSuccess is set to false if the query failed.
int GetSystemVolume(bool &outSuccess);

// Sets mute state. Returns true on success.
bool SetSystemMute(bool mute);

// Gets current mute state.
// outSuccess is set to false if the query failed.
bool GetSystemMute(bool &outSuccess);