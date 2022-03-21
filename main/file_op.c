
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "FILE_OP_TASK";


char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

esp_err_t build_table(TOPIC_t **topics, char *file, int16_t *ntopic)
{
	ESP_LOGI(TAG, "build_table file=%s", file);
	char line[1024];
	int _ntopic = 0;


	//read file to get the file size
	FILE* f = fopen(file, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}

	while (1){
		if ( fgets(line, sizeof(line) ,f) == 0 ) break;
		ESP_LOGI(TAG, "line=[%s]", line);
		_ntopic++;
	}
	fclose(f);


   // read file to push items into array


	ESP_LOGI(TAG, "build_table _ntopic=%d", _ntopic);

	*topics = calloc(_ntopic, sizeof(TOPIC_t));
	if (*topics == NULL) {
		ESP_LOGE(TAG, "Error allocating memory for topic");
		return ESP_ERR_NO_MEM;
	}

	FILE* f2 = fopen(file, "r");
	if (f2 == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}
	int index = 0;

	char** tokens;

	while (1){
		if ( fgets(line, sizeof(line) ,f2) == 0 ) break;
		ESP_LOGI(TAG, "line=[%s]", line);

		// put trailing character instead of new line
		char* pos = strchr(line, '\n');
		if (pos) {
			*pos = '\0';
		}

		//split line with delimeter
		char* tmp = strdup(line);

        tokens = str_split(tmp, ',');
		free(tmp);
		//printf("field pgn =[%s]\n", *(tokens + 0));
		//printf("field topic =[%s]\n", *(tokens + 1));

		// get values from splitted line
		int  pgn = atoi( *(tokens + 0));
		char* topic =  *(tokens + 1);
		int topic_len=strlen(topic);


		// assign values
		(*topics+index)->pgn = pgn;
		(*topics+index)->topic = topic;
		(*topics+index)->topic_len= topic_len;
		index++;
	}
	fclose(f2);

	*ntopic = index;
	return ESP_OK;
}

esp_err_t sub_table(TOPIC_t **topics, int16_t *ntopic)
{

	int _ntopic = 1;
	int index = 0;

	ESP_LOGI(TAG, "subscribe table _ntopic=%d", _ntopic);

	*topics = calloc(_ntopic, sizeof(TOPIC_t));
	if (*topics == NULL) {
		ESP_LOGE(TAG, "Error allocating memory for topic");
		return ESP_ERR_NO_MEM;
	}

	// assign values
	(*topics+index)->pgn = 0x00000001; //don't care now.
	(*topics+index)->topic = "/CAN/getlogs";
	(*topics+index)->topic_len= 12;
	*ntopic = 1;

	return ESP_OK;

}



