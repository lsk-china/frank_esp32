#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "network.h"
#include "proto/packet.pb-c.h"
#include <pb_encode.h>
#include <string.h>
#include <lwip/pbuf.h>

/*
 * --- PRIVATE FUNCTIONS AND FIELDS ---
 */

/**
 * Starts a new task to handle incoming data.
 * This function should be running in the task of network_provider
 * @param data data
 * @param size size of data
 */
void on_data(void *data, size_t size);

/**
 * Actually handles the data.
 * This function should be running in a new task dedicated for this packet.
 * @param pvParameters a data_size_bundle structure
 */
void process_data_task(void *pvParameters);

/**
 * Send a packet to server
 * @param packet Packet to be sent
 */
void do_send_packet(Packet packet);

// Callbacks
on_text_packet_callback_t on_text_packet;
on_audio_packet_callback_t on_audio_packet;

// Network provider
struct network_provider *current_provider;

// Tag for logging
const char *TAG = "network";

// Used to pass data between tasks
struct data_size_bundle_t
{
    void *data;
    size_t size;
};

// Convert command_t to Commands
Commands CMD_T_TO_COMMANDS[] = {COMMANDS__COMMANDS_CLEAR_HISTORY};

/*
 * --- IMPLEMENTATIONS ---
 */

void set_on_text_packet(on_text_packet_callback_t callback) {
    on_text_packet = callback;
}

void set_on_audio_packet(on_audio_packet_callback_t callback) {
    on_audio_packet = callback;
}

void initialize_network(struct network_provider *provider)
{
    current_provider = provider;
    provider->set_on_data_callback(on_data);
}

void on_data(void *data, size_t size)
{
    // Will be freed later in the processing task.
    struct data_size_bundle_t *bundle = malloc(sizeof(struct data_size_bundle_t));
    xTaskCreate(process_data_task,
        "hdl_pkt",
        CONFIG_DEFAULT_TASK_STACK_SIZE,
        bundle,
        10,
        NULL
    );
}

void process_data_task(void *pvParameters)
{
    struct data_size_bundle_t *bundle = pvParameters;
    void *data = bundle->data;
    size_t size = bundle->size;
    // Attempt to deserialize the incoming data
    Packet *packet = packet__unpack(NULL, size, data);
    if (!packet)
    {
        ESP_LOGE(TAG, "Failed to deserialize");
        goto cleanup;
    }

    // Distribute data to corresponding handler
    switch (packet->head->type)
    {
    case PACKET__BODY_TEXT:
        Text *text = packet->text;
        on_text_packet(text->text);
        break;
    case PACKET__BODY_AUDIO:
        Audio *audio = packet->audio;
        void* audio_data = audio->data.data;
        size_t audio_data_size = audio->data.len;
        int sample_rate = audio->sample_rate;
        int channels = audio->channels;
        struct audio_metadata metadata = {
            .length = audio_data_size,
            .channels = channels,
            .sample_rate = sample_rate,
            .bits_per_sample = CONFIG_AUDIO_PLAY_BPS
        };
        on_audio_packet(audio_data, metadata);
        break;
    default:
        ESP_LOGE(TAG, "Unexpected packet type");
    }
    // Do some cleanup.
cleanup:
    free(bundle);
    packet__free_unpacked(packet, NULL);
    vTaskDelete(NULL);
}

void do_send_packet(Packet packet)
{
    // Serialize
    size_t len = packet__get_packed_size(&packet);
    void *buffer = malloc(len);
    packet__pack(&packet, buffer);

    // Send to network_provider
    current_provider->send(buffer, len);

    // Cleanup
    free(buffer);
}

void send_voice_to_server(void *data, struct audio_metadata metadata)
{
    // Pack audio data
    Audio audio = AUDIO__INIT;
    audio.sample_rate = metadata.sample_rate;
    audio.channels = metadata.channels;
    audio.data.data = data;
    audio.data.len = metadata.length;

    // Pack packet
    Packet packet = PACKET__INIT;
    packet.body_case = PACKET__BODY_AUDIO;
    packet.head = malloc(sizeof(Packet__Head));
    packet.head->type = MESSAGE_TYPE__MESSAGE_TYPE_AUDIO;
    packet.head->version = CONFIG_MAX_PACKET_VERSION;
    packet.audio = (Audio *) malloc(sizeof(Audio));
    memcpy(&audio, packet.audio, sizeof(audio));

    // Send
    do_send_packet(packet);

    // cleanup
    free(packet.head);
    free(packet.audio);
}

void send_command_to_server(command_t command)
{
    // Do nearly the same thing as the previous function.
    Command cmd = COMMAND__INIT;
    cmd.command = CMD_T_TO_COMMANDS[command]; // Converts command_t to Commands.

    Packet packet = PACKET__INIT;
    packet.body_case = PACKET__BODY_TEXT;
    packet.head = malloc(sizeof(Packet__Head));
    packet.head->type = MESSAGE_TYPE__MESSAGE_TYPE_COMMAND;
    packet.head->version = CONFIG_MAX_PACKET_VERSION;
    packet.command = malloc(sizeof(Command));
    memcpy(packet.command, &cmd, sizeof(Command));

    do_send_packet(packet);

    free(packet.head);
    free(packet.command);
}