//
// Created by lsk on 6/27/25.
//

#ifndef NETWORK_PROVIDER_WIFI_H
#define NETWORK_PROVIDER_WIFI_H

#include "network.h"

/**
 * Wi-Fi Implementation to the WebSocket Communication
 * --- TODO ---
 * Up to now, the Wi-Fi password and SSID are configured in Kconfig, and compiled into the firmware.
 * In the future, user should be able to change them via his mobile.
 */

/**
 * Get the network_provider structure.
 * @return the network_provider structure
 */
struct network_provider* wifi_network_provider();

#endif //NETWORK_PROVIDER_WIFI_H
