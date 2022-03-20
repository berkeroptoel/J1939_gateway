
#ifndef MAIN_COMMON_H_
#define MAIN_COMMON_H_

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define	PUBLISH		100
#define	SUBSCRIBE	200


typedef struct {
	int16_t topic_type; //To MQTT broker
	int16_t topic_len; //To MQTT broker
	char topic[64]; //To MQTT broker
	uint32_t PGN; // from TWAI
	int16_t data_len; //from TWAI
	char data[64]; //from TWAI
} MQTT_t;

typedef struct {
	uint32_t pgn;
	char * topic;
	int16_t topic_len;
} TOPIC_t;

TOPIC_t *publish;
int16_t	npublish;
TOPIC_t *subscribe;
int16_t	nsubscribe;


#endif /* MAIN_COMMON_H_ */
