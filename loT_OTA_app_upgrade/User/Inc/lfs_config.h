#ifndef LFS_CONFIG_H
#define LFS_CONFIG_H

#include "FreeRTOS.h"
#include "task.h"

#define LFS_MALLOC(size)  pvPortMalloc(size)
#define LFS_FREE(ptr)     vPortFree(ptr)

extern const struct lfs_config lfs_cfg;

#endif