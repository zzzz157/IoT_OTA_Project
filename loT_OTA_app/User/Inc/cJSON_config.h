#ifndef CJSON_CONFIG_H
#define CJSON_CONFIG_H

#include <stdint.h>

#define CJSON_POOL_SIZE 	2048
#define CJSON_POOL_WORDS 	(CJSON_POOL_SIZE/4)

void reset_cjson_pool();
void My_cJSON_Hook_Init();


#endif