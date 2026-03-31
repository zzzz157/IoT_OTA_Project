#include "lfs.h"
#include "W25Q64.h"

static W25Q64_t* lfs_w26q_t=NULL;
/* register w25q dev */
static void lfs_w25q_init()
{
	lfs_w26q_t=&W25QHandle_t;
	lfs_w26q_t->Init(lfs_w26q_t);
}
/* register lfs read */
int my_block_dev_read(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size)
{
	if(lfs_w26q_t==NULL) lfs_w25q_init();
	if(lfs_w26q_t->ReadDatas(lfs_w26q_t,c->block_size*block+off,(uint8_t*)buffer,size)==0)
	{
		return LFS_ERR_IO;
	}
	return LFS_ERR_OK;
}
/* register lfs write */
int my_block_dev_prog(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size)
{
	if(lfs_w26q_t==NULL) lfs_w25q_init();
	if(lfs_w26q_t->WritePage(lfs_w26q_t,c->block_size*block+off,(uint8_t*)buffer,size)==0)
	{
		return LFS_ERR_IO;
	}
	return LFS_ERR_OK;
}
/* register lfs erase */
int my_block_dev_erase(const struct lfs_config *c, lfs_block_t block)
{
	if(lfs_w26q_t==NULL) lfs_w25q_init();
	if(lfs_w26q_t->SectorErase(lfs_w26q_t,block*c->block_size)==0)
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
	
	.read_size=W25Q64_PAGE_LEN,
	.prog_size=W25Q64_PAGE_LEN,
	.block_count=2048,
	.block_cycles=500,
	.cache_size=W25Q64_PAGE_LEN,
	.block_size=W25Q64_Sector_LEN,
	.lookahead_size = 16,
};
