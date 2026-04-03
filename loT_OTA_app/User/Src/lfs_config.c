#include "lfs.h"
#include "W25Q64.h"
#include "FreeRTOS.h"
#include "semphr.h"
SemaphoreHandle_t lfs_mutex = NULL;

static W25Q64_t* lfs_w25q_t=NULL;
/* register w25q dev */
static void lfs_w25q_init()
{
	if(lfs_mutex==NULL) lfs_mutex=xSemaphoreCreateMutex();
	lfs_w25q_t=&W25QHandle_t;
	lfs_w25q_t->Init(lfs_w25q_t);
}
/* lfs take mutex */
int lfs_lock(const struct lfs_config *c)
{
	xSemaphoreTake(lfs_mutex, portMAX_DELAY);
	return 0;
}
/* lfs give mutex */
int lfs_unlock(const struct lfs_config *c)
{
	xSemaphoreGive(lfs_mutex);
	return 0;
}
/* register lfs read */
int my_block_dev_read(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size)
{
	if(lfs_w25q_t==NULL) lfs_w25q_init();
	uint32_t addr = LFS_FLASH_OFFSET + (block * c->block_size) + off;
	if(lfs_w25q_t->ReadDatas(lfs_w25q_t,addr,(uint8_t*)buffer,size)==0)
	{
		return LFS_ERR_IO;
	}
	return LFS_ERR_OK;
}
/* register lfs write */
int my_block_dev_prog(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size)
{
	if(lfs_w25q_t==NULL) lfs_w25q_init();
	uint32_t addr = LFS_FLASH_OFFSET + (block * c->block_size) + off;
	if(lfs_w25q_t->WritePage(lfs_w25q_t,addr,(uint8_t*)buffer,size)==0)
	{
		return LFS_ERR_IO;
	}
	return LFS_ERR_OK;
}
/* register lfs erase */
int my_block_dev_erase(const struct lfs_config *c, lfs_block_t block)
{
	if(lfs_w25q_t==NULL) lfs_w25q_init();
	uint32_t addr = LFS_FLASH_OFFSET + (block * c->block_size);
	if(lfs_w25q_t->SectorErase(lfs_w25q_t,addr)==0)
	{
		return LFS_ERR_IO;
	}
	return LFS_ERR_OK;
}
/* register lfs sync */
int my_block_dev_sync(const struct lfs_config *c)
{
	return LFS_ERR_OK;
}

/* lfs's flash config */
const struct lfs_config lfs_cfg={
	.read=my_block_dev_read,
	.prog=my_block_dev_prog,
	.erase=my_block_dev_erase,
	.sync=my_block_dev_sync,
	
#ifdef LFS_THREADSAFE
	.lock=lfs_lock,
	.unlock=lfs_unlock,
#endif
	
	.read_size=W25Q64_PAGE_LEN,
	.prog_size=W25Q64_PAGE_LEN,
	.block_count=(W25Q64_TOTAL_SIZE-LFS_FLASH_OFFSET)/W25Q64_Sector_LEN,
	.block_cycles=500,
	.cache_size=W25Q64_PAGE_LEN,
	.block_size=W25Q64_Sector_LEN,
	.lookahead_size = 16,
};
