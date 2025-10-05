#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "network.h"
#include "proto/packet.pb-c.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include <string.h>

// when CONFIG_FREERTOS_HZ undefined
#ifndef CONFIG_FREERTOS_HZ
#define CONFIG_FREERTOS_HZ 100
#endif

// when CONFIG_LOG_MAXIMUM_LEVEL undefined
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_ERROR
#endif

// Network task handle
TaskHandle_t task_handle;

// Callbacks
on_text_packet_callback_t on_text_packet;
on_audio_packet_callback_t on_audio_packet;

// Network provider
static struct network_provider *current_provider;

// Tag for logging
static const char *TAG = "network";

// Used to pass data between tasks
struct data_size_bundle_t
{
    void *data;
    size_t size;
};

/**
 * Network task function
 * @param pvParameters the network provider
 */
void network_task(void *pvParameters);

/**
 * Handle incoming packet based on its type
 * @param packet the received packet
 */
void handle_packet(Packet *packet);

/**
 * Handle text packet
 * @param text the text packet
 */
void handle_text_packet(Text *text);

/**
 * Handle audio packet
 * @param audio the audio packet
 */
void handle_audio_packet(Audio *audio);

/**
 * Handle command packet
 * @param command the command packet
 */
void handle_command_packet(Command *command);

// // Buffer for receiving data
// #define RECEIVE_BUFFER_SIZE 1024
// static uint8_t receive_buffer[RECEIVE_BUFFER_SIZE];
// static size_t receive_buffer_pos = 0;
//
// void start_network_task(struct network_provider *provider) {
//     current_provider = provider;
//
//     // Create message queues
//     queue_send_to_network = xQueueCreate(10, sizeof(network_send_message_t));
//     queue_send_to_main = xQueueCreate(10, sizeof(Packet*));
//
//     // Create network task
//     xTaskCreate(network_task, "network_task", NETWORK_TASK_STACK_SIZE, provider, NETWORK_TASK_PRIORITY, &task_handle);
// }
//
// void network_task(void *pvParameters) {
//     struct network_provider *provider = (struct network_provider *)pvParameters;
//
//     // Initialize provider
//     int result = provider->connect_to_server();
//     if (result != 0) {
//         ESP_LOGE(TAG, "Failed to connect to server: %d", result);
//         vTaskDelete(NULL);
//         return;
//     }
//
//     ESP_LOGI(TAG, "Connected to server successfully");
//
//     // Set data callback
//     provider->set_on_data_callback(NULL); // Not used in this implementation
//
//     network_send_message_t send_msg;
//
//     while (1) {
//         // Check for messages to send
//         if (xQueueReceive(queue_send_to_network, &send_msg, 0) == pdTRUE) {
//             // Serialize and send packet
//             uint8_t buffer[1024];
//             pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
//
//             if (pb_encode(&stream, , send_msg.packet)) {
//                 provider->send(buffer, stream.bytes_written);
//             } else {
//                 ESP_LOGE(TAG, "Failed to encode packet");
//             }
//
//             // Free the packet memory
//             pb_release(Packet_fields, send_msg.packet);
//             free(send_msg.packet);
//         }
//
//         // Try to receive data
//         size_t bytes_received = 0;
//         // This is a simplified implementation - in a real implementation,
//         // you would read from the socket until it's closed or a complete message is received
//         // For now, we'll simulate receiving data
//         // bytes_received = provider->receive(receive_buffer + receive_buffer_pos, RECEIVE_BUFFER_SIZE - receive_buffer_pos);
//
//         if (bytes_received > 0) {
//             receive_buffer_pos += bytes_received;
//
//             // Try to decode packet
//             pb_istream_t istream = pb_istream_from_buffer(receive_buffer, receive_buffer_pos);
//             Packet *packet = malloc(sizeof(Packet));
//             if (packet == NULL) {
//                 ESP_LOGE(TAG, "Failed to allocate memory for packet");
//                 receive_buffer_pos = 0;
//                 continue;
//             }
//
//             // Initialize packet
//             memset(packet, 0, sizeof(Packet));
//
//             if (pb_decode(&istream, Packet_fields, packet)) {
//                 // Successfully decoded, handle the packet
//                 handle_packet(packet);
//                 // Reset buffer
//                 receive_buffer_pos = 0;
//             } else {
//                 // Failed to decode, might be incomplete data
//                 ESP_LOGW(TAG, "Failed to decode packet, keeping data in buffer");
//                 // If buffer is full, reset it
//                 if (receive_buffer_pos >= RECEIVE_BUFFER_SIZE) {
//                     receive_buffer_pos = 0;
//                 }
//             }
//
//             free(packet);
//
//             free(packet);
//         }
//
//         // Small delay to prevent busy loop
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }
//
// void handle_packet(Packet *packet) {
//     switch (packet->head.type) {
//         case MessageType_MESSAGE_TYPE_TEXT:
//             handle_text_packet(&packet->body.text);
//             break;
//         case MessageType_MESSAGE_TYPE_AUDIO:
//             handle_audio_packet(&packet->body.audio);
//             break;
//         case MessageType_MESSAGE_TYPE_COMMAND:
//             handle_command_packet(&packet->body.command);
//             break;
//         default:
//             ESP_LOGW(TAG, "Unknown packet type: %d", packet->head.type);
//             break;
//     }
// }
//
// void handle_text_packet(Text *text) {
//     if (on_text_packet != NULL) {
//         // For simplicity, we assume the text is null-terminated
//         // In a real implementation, you would need to handle the callback properly
//         // according to nanopb's callback mechanism
//         on_text_packet(NULL); // Placeholder
//     }
// }
//
// void handle_audio_packet(Audio *audio) {
//     if (on_audio_packet != NULL) {
//         struct audio_metadata metadata = {
//             .sample_rate = audio->sample_rate,
//             .channels = audio->channels
//         };
//         on_audio_packet(NULL, metadata); // Placeholder
//     }
// }
//
// void handle_command_packet(Command *command) {
//     // Handle command packet
//     ESP_LOGI(TAG, "Received command: %d", command->command);
// }
//
// void send_voice_to_server(void *data, struct audio_metadata metadata) {
//     Packet *packet = malloc(sizeof(Packet));
//     if (packet == NULL) {
//         ESP_LOGE(TAG, "Failed to allocate memory for packet");
//         return;
//     }
//
//     // Initialize packet
//     memset(packet, 0, sizeof(Packet));
//
//     // Set header
//     packet->head.version = 1;
//     packet->head.type = MessageType_MESSAGE_TYPE_AUDIO;
//
//     // Set audio body
//     packet->which_body = Packet_audio_tag;
//     packet->body.audio.sample_rate = metadata.sample_rate;
//     packet->body.audio.channels = metadata.channels;
//     // In a real implementation, you would set up the callback for the audio data
//     // packet->body.audio.data.funcs.encode = &audio_data_encode_callback;
//     // packet->body.audio.data.arg = data;
//
//     // Send to network task
//     network_send_message_t msg = {
//         .packet = packet
//     };
//
//     if (xQueueSend(queue_send_to_network, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
//         ESP_LOGE(TAG, "Failed to send packet to network task");
//         pb_release(Packet_fields, packet);
//         free(packet);
//     }
// }
//
// void send_command_to_server(command_t command) {
//     Packet *packet = malloc(sizeof(Packet));
//     if (packet == NULL) {
//         ESP_LOGE(TAG, "Failed to allocate memory for packet");
//         return;
//     }
//
//     // Initialize packet
//     memset(packet, 0, sizeof(Packet));
//
//     // Set header
//     packet->head.version = 1;
//     packet->head.type = MessageType_MESSAGE_TYPE_COMMAND;
//
//     // Set command body
//     packet->which_body = Packet_command_tag;
//     packet->body.command.command = (Commands)(command + 1); // Adjust for protobuf enum
//
//     // Send to network task
//     network_send_message_t msg = {
//         .packet = packet
//     };
//
//     if (xQueueSend(queue_send_to_network, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
//         ESP_LOGE(TAG, "Failed to send packet to network task");
//         pb_release(Packet_fields, packet);
//         free(packet);
//     }
// }

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

static void on_data(void *data, size_t size)
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

static void process_data_task(void *pvParameters)
{
    struct data_size_bundle_t *bundle = pvParameters;
    void *data = bundle->data;
    size_t size = bundle->size;
    // Attempt to deserialize the incoming data
    pb_istream_t stream = pb_istream_from_buffer(data, size);
    Packet *packet = (Packet *) malloc(sizeof(Packet));
    if (!packet)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for packet");
        goto cleanup;
    }

    if (!pb_decode(&stream, &packet__descriptor, packet))
    {
        ESP_LOGE(TAG, "Failed to deserialize packet");
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
        on_audio_packet(audio, metadata);
        break;
    default:
        ESP_LOGE(TAG, "Unexpected packet type");
    }
    // Do some cleanup.
cleanup:
    free(bundle);
    if (packet)
    {
        free(packet);
    }
    vTaskDelete(NULL);
}

void send_voice_to_server(void *data, struct audio_metadata *metadata)
{

}