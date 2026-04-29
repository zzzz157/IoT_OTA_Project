#include "cJSON_config.h"
#include "cJSON.h"

/* static storage pool */
static uint32_t cjson_pool_words[CJSON_POOL_WORDS];
static uint8_t* const cjson_pool = (uint8_t*)cjson_pool_words;
static size_t cjson_pool_offset = 0;

/* allocate */
static void* my_cjson_malloc(size_t size)
{
	size = (size + 3) & ~3;
	if (cjson_pool_offset + size <= CJSON_POOL_SIZE)
	{
		void* ptr = &cjson_pool[cjson_pool_offset];
		cjson_pool_offset+=size;
		return ptr;
	}
	return NULL;
}
/* free */
static void my_cjson_free(void* ptr)
{
	
}
void reset_cjson_pool()
{
    cjson_pool_offset = 0;
}
/* init */
void My_cJSON_Hook_Init()
{
    cJSON_Hooks hooks;
    hooks.malloc_fn = my_cjson_malloc;
    hooks.free_fn = my_cjson_free;
    cJSON_InitHooks(&hooks);
}


