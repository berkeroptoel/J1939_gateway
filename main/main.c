
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "driver/twai.h" // Update from V4.2
#include "driver/gpio.h"
#include "protocol_examples_common.h"
#include "common.h"
#include "cJSON.h"

static const char *TAG = "CANJ1939";

//WIFI
#define EXAMPLE_ESP_MAXIMUM_RETRY 3
int s_retry_num = 0; //why static? --removed
EventGroupHandle_t s_wifi_event_group; //why static? --removed

esp_err_t build_table(TOPIC_t **topics, char *file, int16_t *ntopic);
esp_err_t sub_table(TOPIC_t **topics, int16_t *ntopic);

//TWAI
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(22, 21, TWAI_MODE_NORMAL);


QueueHandle_t xQueue_mqtt_tx;
QueueHandle_t xQueue_twai_tx;


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    	ESP_LOGI(TAG, "Start event for connect to the AP");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num = 0;
            ESP_LOGI(TAG, "Couldn't connect to the AP with %d times trying", EXAMPLE_ESP_MAXIMUM_RETRY);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
}

bool wifi_init_sta(void)
{
	esp_err_t ret = ESP_FAIL;
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
			.ssid = CONFIG_EXAMPLE_WIFI_SSID,
			.password = CONFIG_EXAMPLE_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
		 .threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};

    esp_wifi_set_ps(WIFI_PS_NONE); //Disable WIFI power save mode, Fast OTA
    //esp_wifi_set_ps(WIFI_PS_MIN_MODEM); //NO Power saving, in case of BT, WIFI has to go down

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

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
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
				CONFIG_EXAMPLE_WIFI_SSID, CONFIG_EXAMPLE_WIFI_PASSWORD);
		ret = ESP_OK;
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
				CONFIG_EXAMPLE_WIFI_SSID, CONFIG_EXAMPLE_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	/* The event will not be processed after unregister */
	//ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	//ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	//vEventGroupDelete(s_wifi_event_group);
	return ret;
}


esp_err_t SPIFFS_init(char * partition_label, char * base_path)
{
	ESP_LOGI(TAG, "Initializing SPIFFS file system");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = base_path,
		.partition_label = partition_label,
		.max_files = 5,
		.format_if_mount_failed = true
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
	ESP_LOGI(TAG, "Mount SPIFFS filesystem");
	return ret;
}

void led_indicator(void *pvParameter)
{
	while(1)
	{
		gpio_set_level(25, 0);
		vTaskDelay( 250 / portTICK_PERIOD_MS );
		gpio_set_level(25, 1);
		vTaskDelay( 250 / portTICK_PERIOD_MS );

	}

	// Never reach here
	vTaskDelete(NULL);

}

void mqtt_pub_task(void *pvParameters);
void mqtt_sub_task(void *pvParameters);
void twai_task(void *pvParameters);
void wifi_reconnect_task(void *pvParameters);
void ota_check_task(void *pvParameters);

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // LED
    gpio_reset_pin(25);
    gpio_set_direction(25, GPIO_MODE_OUTPUT);

	// Mount SPIFFS
	char *partition_label = "storage";
	char *base_path = "/spiffs";
	ret = SPIFFS_init(partition_label, base_path);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "mountSPIFFS fail");
		while(1) { vTaskDelay(1); }
	}

	ret = build_table(&publish, "/spiffs/J1939_PGN_table.csv", &npublish);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "build publish table fail");
		while(1) { vTaskDelay(1); }
	}

	//Print publish topics
	for(int i=0;i<npublish;i++) {
		ESP_LOGI(pcTaskGetTaskName(0), "topics[%d]  pgn = %d pgn=0x%x topic=[%s] topic_len=%d",
		i,  (publish+i)->pgn, (publish+i)->pgn, (publish+i)->topic, (publish+i)->topic_len);
	}

	ret = sub_table(&subscribe,&nsubscribe);

	//Print subscribe topics
	for(int i=0;i<nsubscribe;i++) {
		ESP_LOGI(pcTaskGetTaskName(0), "topics[%d]  pgn = %d pgn=0x%x topic=[%s] topic_len=%d",
		i,  (subscribe+i)->pgn, (subscribe+i)->pgn, (subscribe+i)->topic, (subscribe+i)->topic_len);
	}


	// TWAI
	// Install and start TWAI driver
	ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
	ESP_LOGI(TAG, "Driver installed");
	ESP_ERROR_CHECK(twai_start());
	ESP_LOGI(TAG, "Driver started");


	// WiFi initialize
	ret = wifi_init_sta();
	if (ret != ESP_OK){
		ESP_LOGE(TAG, "WIFI NOT INIT");
		while(1) vTaskDelay(10);
	}


	// Create Queue
	xQueue_mqtt_tx = xQueueCreate( 100, sizeof(MQTT_t) );
	configASSERT( xQueue_mqtt_tx );
	xQueue_twai_tx = xQueueCreate( 10, sizeof(twai_message_t) );
	configASSERT( xQueue_twai_tx );


    //xTaskCreate(&ota_check_task, "ota_task", 1024 * 8, NULL, 5, NULL);
    //xTaskCreate(&wifi_reconnect_task, "wifi_task", 1024 * 8, NULL, 5, NULL);
    //xTaskCreate(led_indicator, "led_task", 1024, NULL, 1, NULL);
	xTaskCreate(mqtt_pub_task, "mqtt_pub", 1024*4, NULL, 2, NULL);
	xTaskCreate(mqtt_sub_task, "mqtt_sub", 1024*4, NULL, 2, NULL);
	xTaskCreate(twai_task, "twai_rx", 1024*6, NULL, 2, NULL);

}
