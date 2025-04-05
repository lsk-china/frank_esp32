#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include <string.h> // for handling strings
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/i2s_types_legacy.h>
#include "esp_wifi.h" // esp_wifi_init functions and wifi operations
#include "esp_log.h" // for showing logs
#include "esp_event.h" // for wifi event
#include "nvs_flash.h" // non volatile storage
#include "lwip/sys.h" // system applications for light weight ip apps
#include "esp_transport_tcp.h"
#include "esp_netif.h"
#include "driver/i2s.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "driver/i2c.h"

/*
 * Global configurations and handles.
 */

// Logging
#define TAG "Frank's App"

// Tasks
// Greater stack depth to prevent SOF
#define RECORD_TASK_STACK_SIZE (1024 * 8)
#define PLAY_TASK_STACK_SIZE   (1024 * 10)
#define CLEAR_TASK_STACK_SIZE  (1024 * 4)

// Network
#define WIFI_SSID     ""
#define WIFI_PASS     ""
#define SERVER_IP     "192.168.1.8"
#define SEND_PORT     27012
#define LISTEN_PORT   27011
#define COMMAND_PORT  27013
TaskHandle_t receive_task_handle;

// Buttons and interrupts
TaskHandle_t task_handle;
/*
 * The INPUT_GPIO_MASK is an uint64_t integer, in which each bit represents a GPIO on the device.
 * For exp, here I shift 1 to right by 40 bits, so the 40th bit of INPUT_GPIO_MASK is set to 1,
 *  meaning that I'm going to perform operations on GPIO40, which is connected to the button that
 *  controls the microphone.
 * If you hope to initialize some new pins, just perform or operates, to set the corresponding to 1,
 *  marking that GPIO pin is to be operated.
 */
#define SPEAK_GPIO_MASK (1ULL << 40)         // Pin 40 for speak
#define CLEAR_GPIO_MASK (1ULL << 39)         // Pin 39 for clear chat history
#define SPEAK_GPIO      GPIO_NUM_40
#define CLEAR_GPIO      GPIO_NUM_39
#define SPEAK_INTR_ID   0
#define CLEAR_INTR_ID   1

// I2S devices (microphone and amplifier)
#define WITH_I2S
#define RECORD_RATE    8000
#define PLAY_RATE      32000
#define RECORD_SD_PIN  GPIO_NUM_6
#define RECORD_WS_PIN  GPIO_NUM_4
#define RECORD_SCK_PIN GPIO_NUM_5
#define PLAY_SD_PIN    GPIO_NUM_7
#define PLAY_WS_PIN    GPIO_NUM_16
#define PLAY_SCK_PIN   GPIO_NUM_15
#define RECORD_I2S_NUM I2S_NUM_0
#define PLAY_I2S_NUM   I2S_NUM_1
#define READ_BUF_SIZE  2048  // Don't set too high before PSRAM problem is solved....
#define WRITE_BUF_SIZE 2048
#define DMA_BUFFER_CNT 8
#define DMA_BUFFER_LEN 1024

// OLED Configurations
#define WITH_OLED
#define LCD_PIXEL_CLOCK_HZ              (400*1000)
#define LCD_PIN_SDA                     41      // SDA connect to GPIO41
#define LCD_PIN_SCL                     42      // SCL connect to GPIO42
#define LCD_PIN_RST                     (-1)    // No such reset pin...
#define I2C_HW_ADDR                     0x3C    // I2C address for SSD1306 is 0x27
#define LCD_H_RES                       128     // Size for the screen
#define LCD_V_RES                       32
#define I2C_BUS_PORT                    0       // Use I2C controller 0
lv_disp_t *oled_handle;
LV_FONT_DECLARE(lxwk);                        // Declare the font
LV_FONT_DECLARE(lv_font_simsun_16_cjk);
LV_FONT_DECLARE(lxwk_common_20)
lv_style_t style;                               // Font handle


/*
 * Function declarations
 *
 * Make functions able to call functions that are defined after them,
 * and make the code more reasonable (?
 */
static void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void wifi_connection(void);
void IRAM_ATTR gpio_isr_handler(void* arg);
void i2s_initialization(void);
void oled_initialization(void);
void oled_draw_text(const char* text);
void oled_clear_screen(void);
void record_task(void *arg);
void do_record(void);
void do_play(void* arg);
void clear_history_task(void *arg);

/*
 * Functions to connect to Wi-Fi, copied from
 *   https://medium.com/@fatehsali517/how-to-connect-esp32-to-wifi-using-esp-idf-iot-development-framework-d798dc89f0d6
 * We don't need to know how it works. Just using it is fine ;-)
 */
static void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi connecting..");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "WiFi connected.");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WiFi disconnected.");
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_connection()
{
    esp_netif_init(); // network interdace initialization
    esp_event_loop_create_default(); // responsible for handling and dispatching events
    esp_netif_create_default_wifi_sta(); // sets up necessary data structs for wifi station interface
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    // sets up wifi wifi_init_config struct with default values
    esp_wifi_init(&wifi_initiation); // wifi initialised with dafault wifi_initiation
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    // creating event handler register for wifi
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    // creating event handler register for ip event
    wifi_config_t wifi_configuration = {
        // struct wifi_config_t var wifi_configuration
        .sta = {
            .ssid = "",
            .password = ""
            /* we are sending a const char of ssid and password which we will strcpy in following line so leaving it blank */
        } // also this part is used if you donot want to use Kconfig.projbuild
    };
    strcpy((char*)wifi_configuration.sta.ssid, WIFI_SSID); // copy chars from hardcoded configs to struct
    strcpy((char*)wifi_configuration.sta.password, WIFI_PASS);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration); // setting up configs when event ESP_IF_WIFI_STA
    esp_wifi_start(); // start connection with configurations provided in funtion
    esp_wifi_set_mode(WIFI_MODE_STA); // station mode selected
    esp_wifi_connect(); // connect with saved ssid and pass
}

/*
 * Following code handles the button input.
 * Deepseek suggest me to do this magic. God knows why it has to be such
 *   complex to work... The simple version in examples is not working.
 * What happens is basically:
 *   1. I installed an ISR thread, where GPIO interrupts are handled.
 *   2. When an falling edge occurs at GPIO38, the code running at the ISR
 *      will launch the record_task task to perform record operation.
 * What is "debouncing"?
 *   Due to the mechanical structure of the button we are using, when we
 *   press the button, the button is actually not perfectly connected.
 *   Instead, it goes through a period when the level quickly flips between
 *   high and low, which could be detected by our CPU.
 *   This phenomenon is called bouncing, which usually lasts 10-20ms. In
 *   order to tackle this issue, we just need to wait 20ms, so that CPU
 *   won't detect the fake press and release signals.
 */

void IRAM_ATTR gpio_isr_handler(void* arg) {
    // Dispatch interrupt to handler tasks
    int source = (int) arg; // As arg is just an integer, so we don't need to dereference
    switch (source)
    {
    case SPEAK_INTR_ID:
        xTaskCreate(record_task, "record_task", RECORD_TASK_STACK_SIZE, NULL, 5, NULL);
        break;
    case CLEAR_INTR_ID:
        xTaskCreate(clear_history_task, "clear_history_task", CLEAR_TASK_STACK_SIZE, NULL, 5, NULL);
        break;
    default:
        // Won't happen, just to satisfy clang-tidy's complaint
    }
}

void record_task(void *arg)
{
    vTaskDelay(20 / portTICK_PERIOD_MS); // Debouncing
    if (!gpio_get_level(SPEAK_GPIO))
    {
        ESP_LOGI(TAG, "recording start");
        do_record();
        ESP_LOGI(TAG, "recording end");
    }
    vTaskDelete(NULL);                  // Must exit explicitly via vTaskDelete.
}

/*
 * I2S devices initialization
 */

void i2s_initialization(void)
{
#ifdef WITH_I2S
    // Recoder setup
    i2s_config_t i2s_config_record = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = RECORD_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,            // Only record on left channel
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = DMA_BUFFER_CNT,
        .dma_buf_len = DMA_BUFFER_LEN,
        .use_apll = false,
    };
    i2s_driver_install(RECORD_I2S_NUM, &i2s_config_record, 0, NULL);
    i2s_pin_config_t record_pin_config = {
        .bck_io_num = RECORD_SCK_PIN,
        .ws_io_num = RECORD_WS_PIN,
        .data_in_num = RECORD_SD_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin(RECORD_I2S_NUM, &record_pin_config);

    // Player setup
    i2s_config_t i2s_config_play = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = PLAY_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,   // Play both left and right channels
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_CNT,
        .dma_buf_len = DMA_BUFFER_LEN,
        .tx_desc_auto_clear = true,
        .use_apll = false,
        .fixed_mclk = 0
    };
    i2s_driver_install(PLAY_I2S_NUM, &i2s_config_play, 0, NULL);
    i2s_pin_config_t play_pin_config = {
        .bck_io_num = PLAY_SCK_PIN,
        .ws_io_num = PLAY_WS_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE,
        .data_out_num = PLAY_SD_PIN,
    };
    i2s_set_pin(PLAY_I2S_NUM, &play_pin_config);
#endif
}

/*
 * I2C OLED Functions
 */
void oled_initialization(void)
{
#ifdef WITH_OLED
    // Create the i2c bus handle
    ESP_LOGI(TAG, "Initializing OLED...");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = LCD_PIN_SDA,
        .scl_io_num = LCD_PIN_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));
    // Install the SSD1306 driver
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = I2C_HW_ADDR,
        .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,               // According to SSD1306 datasheet
        .lcd_cmd_bits = 8,                      // According to SSD1306 datasheet
        .lcd_param_bits = 8,                    // According to SSD1306 datasheet
        .dc_bit_offset = 6,                     // According to SSD1306 datasheet
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = LCD_PIN_RST,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
    // OLED Initialization
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    // LVGL Initialization
    // LVGL is a graphical library. We use it to draw texts.
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    oled_handle = lvgl_port_add_disp(&disp_cfg);
    lv_disp_set_rotation(oled_handle, LV_DISP_ROT_NONE);  // No rotation
    ESP_LOGI(TAG, "OLED Initialized");

    // Load the font
    lv_style_init(&style);
    lv_style_set_text_font(&style, &lxwk_common_20);
#endif
}

/*
 * Show text on the OLED
 */
void oled_draw_text(const char* text)
{
#ifdef WITH_OLED
    if (lvgl_port_lock(0)) // Lock the mutex due to the LVGL APIs are not thread-safe
    {
        lv_obj_t *scr = lv_disp_get_scr_act(oled_handle);
        lv_obj_t *label = lv_label_create(scr);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); // Circular scroll
        lv_obj_add_style(label, &style, LV_STATE_DEFAULT);
        lv_label_set_text(label, text);
        lv_obj_set_width(label, oled_handle->driver->hor_res);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
        // Release the mutex
        lvgl_port_unlock();
    }
#endif
}

/*
 * Clear the OLED
 */
void oled_clear_screen(void)
{
#ifdef WITH_OLED
    lv_obj_clean(lv_scr_act());
#endif
}

/*
 * Record Logic
 */
void do_record(void)
{
    // Allocate buffer
    uint8_t *read_buf = (uint8_t *)calloc(1, READ_BUF_SIZE);

    // Connect to server
    struct sockaddr_in dest_addr;
    inet_pton(AF_INET, SERVER_IP, &dest_addr.sin_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SEND_PORT);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Socket creation failed: %d", errno);
        return;
    }
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Socket connected");

    // Record voice and send
    while (!gpio_get_level(SPEAK_GPIO)) // Record if button is not released.
    {
        size_t bytes_read;
#ifdef WITH_I2S
        esp_err_t ret = i2s_read(
            RECORD_I2S_NUM,
            read_buf,
            READ_BUF_SIZE,
            &bytes_read,
            portMAX_DELAY          // No timeout.
        );
        if (ret != ESP_OK) continue;
#else
        bytes_read = 0;
#endif
        if (send(sock, read_buf, bytes_read, 0) < 0)
        {
            ESP_LOGE(TAG, "Socket send failed: %d", errno);
        }
    }

    // Shutdown the socket and clean up
    shutdown(sock, 0);
    close(sock);
    free(read_buf);

    // clear screen and prompt user to wait for resp
    oled_clear_screen();
    oled_draw_text("Thinking...");
}

/*
 * Receive and play audio logic
 */
void do_play(void *arg)
{
    // Initialize the socket

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;                 // IPv4
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen requests from any host
    dest_addr.sin_port = htons(LISTEN_PORT);        // Listen on LISTEN_PORT
    // Create the socket
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Socket creation failed: %d", errno);
        vTaskDelete(NULL);                          // Quit this task
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // Bind the socket to 0.0.0.0:LISTEN_PORT
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket bind failed: %d", errno);
        // Do some cleanup and exit...
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    // Listen to requests
    err = listen(listen_sock, 1);          // There should not be two clients connect to it at the same time
                                                    // So set backlog to 1. (Reject extra requests)
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket listen failed: %d", errno);
        // Do some cleanup and exit...
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Listening on port %d", LISTEN_PORT);

    // Allocate the receiving buffer
    uint8_t *recv_buf = (uint8_t *) calloc(1, 1024);
    size_t bytes_read;

    // Main loop, handles the requests here
    while (1)
    {
        // Accept a connection, and get the ip (though unnecessary)
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&client_addr, &len);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Socket accept failed: %d", errno);
            continue;
        }
        // First read the type of incoming data (whether text or audio)
        char type;
        read(sock, &type, sizeof(type));
        if (type == 'a')    // Audio
        {
            size_t bytes_written ; // Dummy variable
            int16_t *audio_buffer = (int16_t *)recv_buf;
#ifdef WITH_I2S
            i2s_zero_dma_buffer(PLAY_I2S_NUM);
            i2s_stop(PLAY_I2S_NUM);
            i2s_start(PLAY_I2S_NUM);
#endif
            // Download audio and play
            while ((bytes_read = read(sock, recv_buf, 1024)) > 0)
            {
#ifdef WITH_I2S
                for (int i = 0; i < bytes_read/2; i++) {
                    // Gain control for each sample
                    float sample = audio_buffer[i] * 0.8f; // Reduce volume
                    audio_buffer[i] = (int16_t)sample;
                }
                int ret = i2s_write(
                    PLAY_I2S_NUM,
                    recv_buf,
                    bytes_read,
                    &bytes_written,
                    portMAX_DELAY
                );
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "I2S write error: %d", ret);
                    break;
                }
                // Wait 1ms to sync
                vTaskDelay(1 / portTICK_PERIOD_MS);
#endif
            }
#ifdef WITH_I2S
            // Cleanup
            i2s_zero_dma_buffer(PLAY_I2S_NUM);
#endif
        } else {            // Text
            read(sock, recv_buf, 512);
            oled_clear_screen();
            oled_draw_text((char *) recv_buf);
        }
    }
}

/*
 * Clear chat history logic
 */

void clear_history_task(void *arg)
{
    ESP_LOGI(TAG, "Sending clear history cmd");
    // Connect to server
    struct sockaddr_in dest_addr;
    inet_pton(AF_INET, SERVER_IP, &dest_addr.sin_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(COMMAND_PORT);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Socket creation failed: %d", errno);
        return;
    }
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        return;
    }
    ESP_LOGI(TAG, "Socket connected");

    // Send cmd
    char * command = "clear history";
    send(sock, command, strlen(command), 0);

    // Disconnect
    shutdown(sock, 0);
    close(sock);
    ESP_LOGI(TAG, "Clear history cmd sent");
}

void app_main(void)
{
    nvs_flash_init(); // needed to save wifi config.
    wifi_connection();
    vTaskDelay(1000 / portTICK_PERIOD_MS); // wait for wifi (?

    // Initialize i2s devices
    i2s_initialization();

    // Initialize the OLED
    oled_initialization();

    /*
     * Here we initialize the GPIOs.
     * Mode is input, pull up enabled (since we connected the button to GND),
     * pull down disabled, interrupt is triggered at a falling edge
     */
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = SPEAK_GPIO_MASK;
    gpio_config(&io_conf);
    // Since we use the same configuration for clear pin, just replace the mask is enough
    io_conf.pin_bit_mask = CLEAR_GPIO_MASK;
    gpio_config(&io_conf);

    // Initialize GPIO interrupt
    gpio_install_isr_service(0);
    gpio_intr_enable(SPEAK_GPIO);
    gpio_intr_enable(CLEAR_GPIO);
    // The third argument will be passed to "arg" of gpio_isr_handler
    // We use it to distinguish the source pin of the event
    gpio_isr_handler_add(SPEAK_GPIO, gpio_isr_handler, (void *) SPEAK_INTR_ID);
    gpio_isr_handler_add(CLEAR_GPIO, gpio_isr_handler, (void *) CLEAR_INTR_ID);

    // Run play audio task
    xTaskCreate(do_play, "play", PLAY_TASK_STACK_SIZE, NULL, 10, NULL);
}
