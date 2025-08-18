//
// Created by lsk on 6/27/25.
//

#ifndef NETWORK_PROVIDER_WIFI_H
#define NETWORK_PROVIDER_WIFI_H

#include "network.h"
#include "sdkconfig.h"

/**
 * Wi-Fi Implementation to the WebSocket Communication
 */

/**
 * Get the network_provider structure
 */
struct network_provider* wifi_network_provider();

/*
 * THESE FUNCTIONS ARE PRIVATE TO THIS MODULE AND SHOULD NOT BE ACCESSED OUTSIDE network_provider.c
 * Access them through network_provider structure.
 */

/*
 * Up to now, the Wi-Fi password and SSID are configured in Kconfig, and compiled into the firmware.
 * In the future, user should be able to change them via his mobile.
 */

/**
 * Initialization Wi-Fi network provider and connect to Wi-Fi
 */
static int wifi_network_provider_init();

/**
 * Set callback to process incoming data
 */
static void wifi_network_provider_set_on_data_callback(void *data, size_t *size);

/**
 * Send data to socket
 * @return If the sending process is successful, returns bytes sent.
 *         If the sending process failed, return a negative value that represents the error (Defined in network.h).
 */
static int wifi_network_provider_send(void *data, size_t *size);

#endif //NETWORK_PROVIDER_WIFI_H
