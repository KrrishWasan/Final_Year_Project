#pragma once

// Sets screen brightness as a percentage (0-100). Returns true on success.
bool SetScreenBrightness(int brightnessPercent);

// Gets current screen brightness as a percentage (0-100).
// outSuccess is set to false if the query failed (e.g. unsupported hardware).
int GetScreenBrightness(bool &outSuccess);