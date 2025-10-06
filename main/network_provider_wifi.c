//
// Created by lsk on 6/27/25.
//
#include "sdkconfig.h"
#include "network_provider_wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/sys.h"
#include "esp_transport_tcp.h"
#include "esp_netif.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

/*
 * --- PRIVATE FUNCTIONS ---
 */

/**
 * Initialization Wi-Fi network provider and connect to Wi-Fi
 */
int wifi_network_provider_init();

/**
 * Set callback to process incoming data
 */
void wifi_network_provider_set_on_data_callback(void *data, size_t *size);

/**
 * Send data to socket
 * @return If the sending process is successful, returns bytes sent.
 *         If the sending process failed, return a negative value that represents the error (Defined in network.h).
 */
int wifi_network_provider_send(void *data, size_t *size);

/**
 * Wi-Fi event handler
 */
void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/**
 * Flag of whether the Wi-Fi is ready
 */
volatile bool wifi_ready = 0;

/*
 * --- IMPLEMENTATIONS ---
 */

