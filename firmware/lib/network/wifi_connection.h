#pragma once
#include <stdbool.h>

// Configures the ESP8266 module and connects to the given access point.
// Disables echo, sets station mode, sets single-connection mode,
// and joins the network with the provided credentials.
// Returns true on success, false if any step fails.
// The caller is responsible for error handling (e.g. halting or retrying).
bool wifi_configure(const char *ssid, const char *password);

// Polls the ESP8266 for a DHCP-assigned station IP address.
// Retries up to 20 times with 1-second intervals (20-second maximum wait).
// Returns true once an IP is confirmed, false if no IP is assigned in time.
bool wifi_wait_for_ip(void);

// Queries and logs the current ESP8266 connection status to the console.
// Used for diagnostics only – does not affect program state.
void wifi_log_status(void);