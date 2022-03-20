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
#include "esp_spiffs.h"
#include "mqtt_client.h"
#include "driver/twai.h"
#include "common.h"

static const char *TAG = "SUB";

static EventGroupHandle_t s_mqtt_event_group;

#define MQTT_CONNECTED_BIT BIT0

extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_twai_tx;

extern TOPIC_t *subscribe;
extern int16_t nsubscribe;


static QueueHandle_t xQueueSubscribe;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
			//esp_mqtt_client_subscribe(mqtt_client, CONFIG_SUB_TOPIC, 0);
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
			ESP_LOGI(TAG, "TOPIC=%.*s\r", event->topic_len, event->topic);
			ESP_LOGI(TAG, "DATA=%.*s\r", event->data_len, event->data);
			MQTT_t mqttBuf;
			mqttBuf.topic_type = SUBSCRIBE;
			mqttBuf.topic_len = event->topic_len;
			for(int i=0;i<event->topic_len;i++) {
				mqttBuf.topic[i] = event->topic[i];
				mqttBuf.topic[i+1] = 0;
			}
			mqttBuf.data_len = event->data_len;
			for(int i=0;i<event->data_len;i++) {
				mqttBuf.data[i] = event->data[i];
			}
			xQueueSend(xQueueSubscribe, &mqttBuf, 0);
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

void mqtt_sub_task(void *pvParameters)
{
	ESP_LOGI(TAG, "Start Subscribe Broker:%s", CONFIG_BROKER_URL);

	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = CONFIG_BROKER_URL,
		.event_handle = mqtt_event_handler,
		.client_id = "subscribe",
	};

	xQueueSubscribe = xQueueCreate( 10, sizeof(MQTT_t) );
	configASSERT( xQueueSubscribe );

	s_mqtt_event_group = xEventGroupCreate();
	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
	esp_mqtt_client_start(mqtt_client);
	xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
	ESP_LOGI(TAG, "Connect to MQTT Server");


	for(int index=0;index<nsubscribe;index++) {
		ESP_LOGI(TAG, "subscribe[%d] topic=[%s]", index, subscribe[index].topic);
		esp_mqtt_client_subscribe(mqtt_client, subscribe[index].topic, 0);
	}

	MQTT_t mqttBuf;
	while (1) {
		xQueueReceive(xQueueSubscribe, &mqttBuf, portMAX_DELAY);
		ESP_LOGI(TAG, "type=%d", mqttBuf.topic_type);

		if (mqttBuf.topic_type != SUBSCRIBE) continue;
		ESP_LOGI(TAG, "TOPIC=%.*s\r", mqttBuf.topic_len, mqttBuf.topic);
		ESP_LOGI(TAG, "TOPIC=[%s]", mqttBuf.topic);
		for(int i=0;i<mqttBuf.data_len;i++) {
			ESP_LOGI(TAG, "DATA=0x%x", mqttBuf.data[i]);
		}

		for(int index=0;index<nsubscribe;index++) {

		    FILE* f2 = fopen("/spiffs/CANlogs.txt", "r");
		    if (f2 == NULL) {
		        ESP_LOGE(TAG, "Failed to open CANlogs file for reading");
		        return;
		    }

		    MQTT_t Logss;

		    while(fread(&Logss, sizeof(MQTT_t), 1, f2))
		    	//printf ("roll_no = %d name = %s\n", Logss.topic_type, Logss.topic);
		         printf ("%d, %d, %s, %d, %d, %s", Logss.PGN,Logss.data_len,Logss.data,Logss.topic_len,Logss.topic_type,Logss.topic);

		     // close file
		     fclose (f2);
		     ESP_LOGI(TAG,"File closed\n");

			//change here
//			if (strcmp(subscribe[index].pgn, mqttBuf.PGN) == 0)
//			ESP_LOGI(TAG, "LOGs will be transmitted to the broker");
//			else
//			ESP_LOGI(TAG, "LOGs will not be transmitted to the broker");

		}


	} // end while

	// Never reach here
	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);
}
