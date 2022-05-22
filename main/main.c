#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include <stdio.h>
#include "rc522.h"
#include "servo.h"
#include "DisplayUI.h"
#include "SNTP_local.h"
//#include "stepper.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
//#include "driver/stepper.h"

#include "cJSON.h"

const int16_t SDA_PIN = GPIO_NUM_21;
const int16_t SCL_PIN = GPIO_NUM_22;

static const char *TAG = "RFID";
static uint8_t _Mode = 0;
volatile bool _ChangeUI = false;
volatile bool _cardRequest = false;
volatile bool check = true;
volatile bool request = true;
volatile bool openDoor = false;
char *TAG_SNTP;
char strftime_buf[64];
Time _now;



// #define WIFI_SSID "KTMT - SinhVien"
// #define WIFI_PASS "sinhvien"
#define WIFI_SSID "ManDat"
#define WIFI_PASS "12345678"
#define MAXIMUM_RETRY 5
// #define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
// #define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
// RTC_DATA_ATTR static int boot_count = 0;

void SplitData(void);
void task_SNTP(void);
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool wifiConnect = false;
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
        wifiConnect = false;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifiConnect = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}
void subStr(const char *string, int start, int end, char *sub)
{
    int c = 0;
    while (c < end)
    {
        sub[c] = string[start + c];
        c++;
    }
    sub[c] = '\0';
}
void splitEvent(char *string, char *event, char *data)
{
    char *token = strtok(string, "\r\n");
    // loop through the string to extract all other tokens
    int count = 0;
    while (token != NULL && count < 2)
    {
        if (count == 0)
            subStr(token, 7, strlen(token), event);
        if (count == 1)
            subStr(token, 6, strlen(token), data);
        token = strtok(NULL, "\r\n");
        count++;
    }
    ESP_LOGI(TAG, "event ne: %s", event);
    ESP_LOGI(TAG, "data ne: %s", data);
}
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            if (evt->user_data)
            {
                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            }
            else
            {
                if (output_buffer == NULL)
                {
                    output_buffer = (char *)malloc(esp_http_client_get_content_length(evt->client));
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
            }
            output_len += evt->data_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL)
        {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}


void http_put_card_request(const char* cardID)
{
    char local_response_buffer[10] = {0};
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "cardID", cardID);
    const char *my_json_string = cJSON_Print(root);
    esp_http_client_config_t config = {     
       
        .url = "https://ceec-b2cd3-default-rtdb.asia-southeast1.firebasedatabase.app/cardRequest.json?auth=7xtpgyMSjViv3k9A2jHcs3VFDJHk06uPm1ynWuPE",
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,        // Pass address of local buffer to get response
        // .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, my_json_string, strlen(my_json_string));
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP PUT Status = %d, content_length = %d",  
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP PUT request failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "HTTP client ok");
    cJSON_Delete(root);
    esp_http_client_cleanup(client);
}
void http_put_door(const char* cardID)
{
    char local_response_buffer[10] = {0};
    // cJSON *root = cJSON_CreateObject();
    // cJSON_AddStringToObject(root, "cardID", cardID);
    // const char *my_json_string = cJSON_Print(root);
    // ESP_LOGI(TAG, "id: %s", cardID);
    esp_http_client_config_t config = {
       
        .url = "https://ceec-b2cd3-default-rtdb.asia-southeast1.firebasedatabase.app/door.json?auth=7xtpgyMSjViv3k9A2jHcs3VFDJHk06uPm1ynWuPE",
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,        // Pass address of local buffer to get response
        // .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    // esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, cardID, strlen(cardID));
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP PUT Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP PUT request failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "HTTP client ok 1");
    // cJSON_Delete(root);
    esp_http_client_cleanup(client);
}
bool http_check_card(char*cardID)
{
    bool returnVal = false;
    char local_response_buffer[100] = {0};
    
   
    char urlRequest[180] = "https://ceec-b2cd3-default-rtdb.asia-southeast1.firebasedatabase.app/users.json?auth=7xtpgyMSjViv3k9A2jHcs3VFDJHk06uPm1ynWuPE&orderBy=\"cardID\"&equalTo=\"";

    const char* temp = "\"";
    strcat(urlRequest, cardID);
    strcat(urlRequest, temp);
    ESP_LOGI(TAG, "vao card");
    ESP_LOGI(TAG, "vao card %s", urlRequest);
    esp_http_client_config_t config = {
        .url = "http://random.com",
        //.event_handler = _http_event_handler,
        .user_data = local_response_buffer,        // Pass address of local buffer to get response
        // .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_url(client, urlRequest);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if(esp_http_client_get_status_code(client)==200 && esp_http_client_get_content_length(client)>3)
            returnVal = true;
    } else {
        ESP_LOGE(TAG, "HTTP PUT request failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "HTTP client ok");
    esp_http_client_cleanup(client);
    return returnVal;
}
static void task_stream_card_request(void *ignore)
{
    int content_length = 0;
    esp_http_client_config_t config = {
       .url = "https://ceec-b2cd3-default-rtdb.asia-southeast1.firebasedatabase.app/cardRequest.json?auth=7xtpgyMSjViv3k9A2jHcs3VFDJHk06uPm1ynWuPE",

        .timeout_ms = 1000};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET Request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Accept", "text/event-stream");
    esp_http_client_set_redirection(client);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        }
        else
        {
            ESP_LOGE(TAG, "Start stream");
            esp_http_client_set_timeout_ms(client, 200);
            while (1)
            {

                char output_buffer[512] = "";
                char data[100], event[20];
                int data_read = esp_http_client_read(client, output_buffer, 512);
                if (data_read > 0)
                {
                    output_buffer[data_read] = 0;
                    splitEvent(output_buffer, event, data);
                    if (strcmp(data, "null"))
                    {   
                        ESP_LOGI(TAG, "no data except admin");
                        if(strlen(data)<25){
                        cJSON *root = cJSON_Parse(data);
                        char *state = cJSON_GetObjectItem(root, "data")->valuestring;
                        if (strcmp(state, "o") == 0)
                        {
                            _cardRequest = true;
                          //  openDoor=true;
                        }
                        ESP_LOGI(TAG, "http: %s", _cardRequest ? "true" : "false");
                        // gpio_set_level(2, 0);
                        // else if(strcmp(state, "f"))
                        // gpio_set_level(2, 1);
                        cJSON_Delete(root); 
                        }
                    }
                    else {
                        ESP_LOGI(TAG,"co data");
                    }
                }
            }
            esp_http_client_close(client);
        }
    }
      
}

void task_Display(void *pvParameters)
{
    SSD1306_t dev = *((SSD1306_t *)pvParameters);
    UI_ManualDisplay(&dev, _now, true);
   while (true)
   {
        if (_Mode == 0 && _ChangeUI){
      //  if (_Mode == 0){
            
          //  if(check && !_cardRequest && openDoor )
           if(check && !_cardRequest )
            {
            ESP_LOGI(TAG,"checking dunggg ne");
            ESP_LOGI(TAG,"checking mo cua web ne");
           
            UI_LockCommand(&dev, false);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            _ChangeUI = false;
            UI_ManualDisplay(&dev, _now, true);
           openDoor=false;
            }
            else if (!check && !_cardRequest)
            {
            ESP_LOGI(TAG,"checking saiii ne");
           
            UI_LockCommand(&dev,true);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            _ChangeUI = false;
            UI_ManualDisplay(&dev, _now, true);
            }
            if (!check && _cardRequest)
            {
             ESP_LOGI(TAG,"checking quet the k ne");
            UI_CheckingUser(&dev);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
             _ChangeUI = false;
            UI_ManualDisplay(&dev, _now, true);
            _cardRequest = false;
            }
        }
      UI_ManualDisplay(&dev, _now, false);
       
   }
    vTaskDelete(NULL);
}


void tag_handler(uint8_t *sn)
{ // serial number is always 5 bytes long
   // SSD1306_t dev = *((SSD1306_t *)pvParameter1);
    if (_Mode == 0)
    {       
      //  esp_sleep_enable_timer_wakeup(2000000);
        // rtc_gpio_isolate(GPIO_NUM_18);
       //gpio_set_level(25,1);
        char* temp = (char*)malloc(sizeof(char)*3);
        char val[11];
        ESP_LOGI(TAG, "Tag: %x %x %x %x %x",
                 sn[0], sn[1], sn[2], sn[3], sn[4]);
        for(int i=0; i<=4;i++){
            itoa(sn[i], temp, 16);
            strcat(val, temp);
        }
        ESP_LOGI(TAG,"ne %s", val);

        if (_cardRequest)
        {
            http_put_card_request(val);
            ESP_LOGI(TAG, "quet card");
           //  _cardRequest = false;    
        }   
          check = http_check_card(val);
        if (check){
                http_put_door("\"o\"");
               ESP_LOGI(TAG,"Hello User");
          //     UI_CheckingUser(&dev);

            }
           
        free(temp);
        _ChangeUI = true;
    }
    
}

static void http_stream(void *ignore)
{
    // while (1)
    // {
    /* code */
    // char *output_buffer = (char*)malloc(512*sizeof(char));
    // char output_buffer[512] = ""; // Buffer to store response of http request
    gpio_pad_select_gpio(2);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(2, GPIO_MODE_OUTPUT);
    int content_length = 0;
    bool blink = true;
    esp_http_client_config_t config = {
        //.url = "https://myproj-308409-default-rtdb.asia-southeast1.firebasedatabase.app/door.json?auth=SecretKey",
        .url = "https://ceec-b2cd3-default-rtdb.asia-southeast1.firebasedatabase.app/door.json?auth=7xtpgyMSjViv3k9A2jHcs3VFDJHk06uPm1ynWuPE",

        .timeout_ms = 1000};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET Request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Accept", "text/event-stream");
    esp_http_client_set_redirection(client);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        }
        else
        {
            ESP_LOGE(TAG, "Start stream");
            esp_http_client_set_timeout_ms(client, 200);
            while (1)
            {
                
                blink = !blink;
                char output_buffer[512] = "";
                char data[100], event[20];
                int data_read = esp_http_client_read(client, output_buffer, 512);
               // esp_http_client_close(client);
                 ESP_LOGI(TAG, "buffer ne: %s", output_buffer);
                if (data_read > 0)
                {
                    // ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                    //          esp_http_client_get_status_code(client),
                    //          esp_http_client_get_content_length(client));
                    // ESP_LOG_BUFFER_HEX(TAG, output_buffer, data_read);
                    output_buffer[data_read] = 0;
                    splitEvent(output_buffer, event, data);
                    ESP_LOGI(TAG, "sse: %s", output_buffer);
                    ESP_LOGI(TAG, "event ne: %s", event);
                    ESP_LOGI(TAG, "data: %s", data);
                    if(strcmp(data, "null")){
                        cJSON *root = cJSON_Parse(data);
                        char *state = cJSON_GetObjectItem(root,"data")->valuestring;
                        if(strcmp(state, "o"))
                        {
                                gpio_set_level(2, 0);       
                           //  ESP_LOGE(TAG, "Door open");
                                vTaskDelay(3000 / portTICK_PERIOD_MS);      
                                http_put_door("\"f\"");
                               // http_put_door("\"o\"");
                               // gpio_set_level(2, 1); 
                             //    gpio_set_level(2, 1);
                           //  Servo_Rotate(180);
                           //  openDoor=true;
                            //   vTaskDelay(5000 / portTICK_PERIOD_MS);
                            //     gpio_set_level(2, 0); 
                            //    http_put_door("\"f\"");
                                }
                        else if(strcmp(state, "f"))
                        {
                            gpio_set_level(2, 1);    
                         //    ESP_LOGE(TAG, "Door close");
                          //   Servo_Rotate(90);
                            //    vTaskDelay(5000 / portTICK_PERIOD_MS);
                            //     gpio_set_level(2, 0); 
                            //    http_put_door("\"o\"");
                            //    Servo_Rotate(180);
                        }
                    }
                }
                // else gpio_set_level(2, 1);
                // {
                //     ESP_LOGE(TAG, "Failed to read response");
                //     break;
                // }
            }
            //
            esp_http_client_close(client);
            // free(output);
        }
       
    
    }

    // free(output_buffer);
}

// static void sleep_mode()
// {
//    while (true) {
//         /* Wake up in 20 seconds, or when button is pressed */
//         esp_sleep_enable_timer_wakeup(2000000);
//        // esp_sleep_enable_gpio_wakeup();

//         /* Wait until GPIO goes high */
//         // if (gpio_get_level(button_gpio_num) == wakeup_level) {
//         //     printf("Waiting for GPIO%d to go high...\n", button_gpio_num);
//         //     do {
//         //         vTaskDelay(pdMS_TO_TICKS(10));
//         //     } while (gpio_get_level(button_gpio_num) == wakeup_level);
//         // }

//         printf("Entering light sleep\n");
//         /* To make sure the complete line is printed before entering sleep mode,
//          * need to wait until UART TX FIFO is empty:
//          */
//      //   uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);

//         /* Get timestamp before entering sleep */
//         int64_t t_before_us = esp_timer_get_time();
        
//         /* Enter sleep mode */
//         esp_light_sleep_start();
//         /* Execution continues here after wakeup */

//         /* Get timestamp after waking up from sleep */
//         int64_t t_after_us = esp_timer_get_time();

//         /* Determine wake up reason */
//         const char* wakeup_reason;
//         switch (esp_sleep_get_wakeup_cause()) {
//             case ESP_SLEEP_WAKEUP_TIMER:
//                 wakeup_reason = "timer";
//                 break;
//             case ESP_SLEEP_WAKEUP_GPIO:
//                 wakeup_reason = "pin";
//                 break;
//             default:
//                 wakeup_reason = "other";a
//                 break;
//         }

//         printf("Returned from light sleep, reason: %s, t=%lld ms, slept for %lld ms\n",
//                 wakeup_reason, t_after_us / 1000, (t_after_us - t_before_us) / 1000);
//     }
// }
void app_main(void)
{   
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init_sta();
 //   Servo_Init();
    // SSD1306_t dev;

    // UI_DisplayInit(&dev, SDA_PIN, SCL_PIN);
    
   // rtc_gpio_isolate(GPIO_NUM_18);
    // xTaskCreate(&task_Display, "Manual", 4096, &dev, 5, NULL);
     const rc522_start_args_t start_args = {
        .miso_io = 19,
        .mosi_io = 23,
        .sck_io = 18,
        .sda_io = 5,
        .callback = &tag_handler,
    };
    rc522_start(start_args);
    xTaskCreate(&task_stream_card_request, "task_stream_card_request", 8192, NULL, 0, NULL );
    xTaskCreate(&http_stream, "http_test_task", 8192, NULL, 0, NULL);
}