/*
   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "driver/twai.h"
#include "cJSON.h"
#include "common.h"

static const char *TAG = "PUB";

static EventGroupHandle_t s_mqtt_event_group;

#define MQTT_CONNECTED_BIT BIT0

extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_twai_tx;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
			xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
	return ESP_OK;
}

void mqtt_pub_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Start Subscribe Broker:%s", CONFIG_BROKER_URL);

    esp_mqtt_client_config_t mqtt_cfg = {
        //.host = "192.168.1.42",
    	.uri = CONFIG_BROKER_URL,
        .event_handle = mqtt_event_handler,
        .client_id = "publish",
    };

    s_mqtt_event_group = xEventGroupCreate();
    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
    esp_mqtt_client_start(mqtt_client);
    xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connect to MQTT Server");

	MQTT_t mqttBuf;
    while (1) {

    				xQueueReceive(xQueue_mqtt_tx, &mqttBuf, portMAX_DELAY);
					if (mqttBuf.topic_type == PUBLISH) {

						#ifdef CONFIG_PRINT_CAN_FRAME_INFO
						ESP_LOGI(TAG, "TOPIC=[%s]", mqttBuf.topic);
						for(int i=0;i<mqttBuf.data_len;i++) {
							ESP_LOGI(TAG, "DATA=%x", mqttBuf.data[i]);
						}
						#endif


						EventBits_t EventBits = xEventGroupGetBits(s_mqtt_event_group);
						//ESP_LOGI(TAG, "EventBits=%x", EventBits);
						if (EventBits & MQTT_CONNECTED_BIT) {

							cJSON *root;
							root = cJSON_CreateObject();
							cJSON_AddNumberToObject(root, "PGN", mqttBuf.PGN);
							cJSON_AddNumberToObject(root, "LENGTH", mqttBuf.data_len);
							cJSON_AddNumberToObject(root, "B0", mqttBuf.data[0]);
							cJSON_AddNumberToObject(root, "B1", mqttBuf.data[1]);
							cJSON_AddNumberToObject(root, "B2", mqttBuf.data[2]);
							cJSON_AddNumberToObject(root, "B3", mqttBuf.data[3]);
							cJSON_AddNumberToObject(root, "B4", mqttBuf.data[4]);
							cJSON_AddNumberToObject(root, "B5", mqttBuf.data[5]);
							cJSON_AddNumberToObject(root, "B6", mqttBuf.data[6]);
							cJSON_AddNumberToObject(root, "B7", mqttBuf.data[7]);

							char *mqtt_json_string = cJSON_Print(root);

							#ifdef CONFIG_PRINT_CAN_FRAME_INFO
							ESP_LOGI(TAG, "my_json_string\n%s",mqtt_json_string);

							for (int i = 0; i < strlen(mqtt_json_string); ++i) {
								printf("%c", mqtt_json_string[i]);
							}
							#endif

							//esp_mqtt_client_publish(mqtt_client,mqttBuf.topic,mqtt_json_string,strlen(mqtt_json_string),1,0);
							esp_mqtt_client_publish(mqtt_client,"CAN/engine",mqtt_json_string,strlen(mqtt_json_string),1,0);
							cJSON_Delete(root);

							//esp_mqtt_client_publish(mqtt_client, mqttBuf.topic, mqttBuf.data, mqttBuf.data_len, 1, 0);

						} else {
							ESP_LOGE(TAG, "NOT CONNECTED TO MQTT BROKER ");
						}
    			}

    } // end while

    // Never reach here
    ESP_LOGI(TAG, "Task Delete");
    esp_mqtt_client_stop(mqtt_client);
    vTaskDelete(NULL);
}
