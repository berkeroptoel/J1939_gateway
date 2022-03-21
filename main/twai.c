/* TWAI Network Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/twai.h" // Update from V4.2
#include "common.h"

static const char *TAG = "TWAI";

extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_twai_tx;

extern TOPIC_t *publish;
extern int16_t npublish;

void twai_task(void *pvParameters)
{
	ESP_LOGI(TAG,"task start");

	twai_message_t rx_msg;
	MQTT_t mqttBuf;
	mqttBuf.topic_type = PUBLISH;


#ifdef CONFIG_MODE_TRIGGER_SEND_MQTT

    FILE* f1 = fopen("/spiffs/CANlogs.txt", "a");
    if (f1 == NULL) {
        ESP_LOGE(TAG, "Failed to open CANlogs file for appending");
        return;
    }

#endif

	while (1) {
		//esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(1));
		esp_err_t ret = twai_receive(&rx_msg, portMAX_DELAY);
		if (ret == ESP_OK) {

			int ext = rx_msg.flags & 0x01; //Standart? Extended?
			int rtr = rx_msg.flags & 0x02; //Remote request

			if (ext == 0) {
				printf("Standard ID: 0x%03x", rx_msg.identifier);
			} else {
				printf("Extended ID: 0x%08x", rx_msg.identifier);
			}
			printf(" DLC: %d  Data: ", rx_msg.data_length_code);

			if (rtr == 0) {
				for (int i = 0; i < rx_msg.data_length_code; i++) {
					printf("0x%02x ", rx_msg.data[i]);
				}
			} else {
				printf("REMOTE REQUEST FRAME");

			}
			printf("\n");


			for(int index=0;index<npublish;index++) {

				if (publish[index].pgn == rx_msg.identifier) {
					ESP_LOGI(TAG, "publish[%d]  pgn=0x%x topic=[%s] topic_len=%d",
					index, publish[index].pgn, publish[index].topic, publish[index].topic_len);

					mqttBuf.PGN = rx_msg.identifier;
					mqttBuf.topic_len = publish[index].topic_len;
					for(int i=0;i<mqttBuf.topic_len;i++) {
						mqttBuf.topic[i] = publish[index].topic[i];
						mqttBuf.topic[i+1] = 0;
					}
					if (rtr == 0) {
						mqttBuf.data_len = rx_msg.data_length_code;
					} else {
						mqttBuf.data_len = 0;
					}
					for(int i=0;i<mqttBuf.data_len;i++) {
						mqttBuf.data[i] = rx_msg.data[i];
					}


					#ifdef CONFIG_MODE_FAST_SEND_MQTT

						if (xQueueSend(xQueue_mqtt_tx, &mqttBuf, portMAX_DELAY) != pdPASS) {
							ESP_LOGE(TAG, "xQueueSend Fail");
						}

					#endif


					#ifdef CONFIG_MODE_TRIGGER_SEND_MQTT

						fwrite(&mqttBuf,sizeof(MQTT_t),1,f1);
						fclose(f1);
						ESP_LOGI(TAG,"File closed\n");

					#endif

				}
			}

		} else if (ret == ESP_ERR_TIMEOUT) {


		} else {
			ESP_LOGE(TAG, "twai_receive Fail %s", esp_err_to_name(ret));
		}
	}

	// Never reach here
	vTaskDelete(NULL);
}

