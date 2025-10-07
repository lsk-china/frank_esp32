//
// Created by lsk on 6/27/25.
//

#ifndef NETWORK_H
#define NETWORK_H

#include "hardware.h"

#define NETWORK_TASK_STACK_SIZE 4096
#define NETWORK_TASK_PRIORITY 5

/**
 * The callback function to process incoming data.
 * @param data the incoming data
 * @param size the size of the data
 */
typedef void (*on_packet_callback_t)(void *data, size_t size);

/**
 * The callback function to process a text packet
 * @param text text sent by the server
 */
typedef void (*on_text_packet_callback_t)(char *text);

/**
 *	The callback function to process a audio packet
 *	@param data the PCM data
 *	@param metadata the metadata
 */
typedef void (*on_audio_packet_callback_t)(void *data, struct audio_metadata metadata);

/**
 * Defines an implementation to the communication
 */
struct network_provider {
	/**
	 * The identity of the implementation.
	 */
	int id;

	/**
	 * Function to initialize the implementation, and connect to the WebSocket server.
	 * @return a number indicates the result of this operation
	 */
	int (*connect_to_server)(void);

	/**
	 * Send data to the socket.
	 * @return If data is sent successfully, returns bytes sent.
	 *         If the process is failed, returns a number indicates the reason.
	 * @param data the data to be sent.
	 * @param size the size of the data
	 */
	int (*send)(void *data, size_t size);

	/**
	 * Set the callback to process incoming data
	 * @param callback the callback function
	 */
	void (*set_on_data_callback)(on_packet_callback_t callback);
};

/**
 * Possible commands, should be in consist with enum Commands in the proto file.
 */
typedef enum
{
	/**
	 * Clear the chat history
	 */
	COMMAND_CLEAR_HISTORY = 0,
} command_t;

/**
 * Start the network task, with the network implementation
 * @param provider the communication implementation
 */
void initialize_network(struct network_provider *provider);

/**
 * Send a Audio packet with recorded voice to the server.
 * @param data the PCM data
 * @param metadata the metadata of the audio
 * @return 0, if packet is successfully sent.
 *		   A negative indicating the error, if packet is not sent.
 */
int send_voice_to_server(void *data, struct audio_metadata metadata);

/**
 * Send a command to the server
 * @param command the command
 * @return 0, if packet is successfully sent.
 *		   A negative indicating the error, if packet is not sent.
 */
int send_command_to_server(command_t command);

/**
 * Set the callback to process a text packet
 * @param callback the callback
 */
void set_on_text_packet(on_text_packet_callback_t callback);

/**
 * Set the callback to process a audio packet
 * @param callback the callback
 */
void set_on_audio_packet(on_audio_packet_callback_t callback);

#endif // NETWORK_H