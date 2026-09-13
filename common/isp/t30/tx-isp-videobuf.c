#include <txx-funcs.h>

#define ISP_MEM_MAX_NUM 20
struct isp_mem_buffer {
	bool used;
	bool busy;
	struct isp_mem_buffer *prev;
	struct isp_mem_buffer *next;
	unsigned int addr;
	unsigned int size;
};

struct isp_mem_manager {
	unsigned int ispmembase;
	unsigned int ispmemsize;
	unsigned int usedsize;
	struct isp_mem_buffer bufs[ISP_MEM_MAX_NUM];
	struct isp_mem_buffer *first;
	struct mutex mlock;
};

static struct isp_mem_manager ispmem;

static struct isp_mem_buffer *find_new_buffer(void)
{
	int index = 0;
	while(index < ISP_MEM_MAX_NUM){
		if(ispmem.bufs[index].busy == false){
			memset(&ispmem.bufs[index], 0, sizeof(struct isp_mem_buffer));
			ispmem.bufs[index].busy = true;
			break;
		}
		index++;
	}
	return index >= ISP_MEM_MAX_NUM ? NULL : &ispmem.bufs[index];
}

void isp_mem_init(void)
{
	memset(&ispmem, 0, sizeof(ispmem));
	private_get_isp_priv_mem(&ispmem.ispmembase, &ispmem.ispmemsize);
	/*printk("addr = 0x%08x, size = 0x%08x\n", ispmem.ispmembase, ispmem.ispmemsize);*/
	private_mutex_init(&ispmem.mlock);
	ispmem.first = find_new_buffer();
	ispmem.first->used = false;
	ispmem.first->prev = NULL;
	ispmem.first->next = NULL;
	ispmem.first->addr = ispmem.ispmembase;
	ispmem.first->size = ispmem.ispmemsize;
}

unsigned int isp_malloc_buffer(unsigned int size)
{
	unsigned int algn = 0;
	struct isp_mem_buffer *buf = NULL;
	struct isp_mem_buffer *new = NULL;
	if(ispmem.ispmembase == 0 || size == 0)
		return 0;
	/* 4k aligned */
	algn = (size + 4095) & (~(4095));

	private_mutex_lock(&ispmem.mlock);
	buf = ispmem.first;
	while(buf){
		if(buf->used == false && buf->size >= algn){
			if(algn < buf->size){
				new = find_new_buffer();
				if(!new){
					private_mutex_unlock(&ispmem.mlock);
					return 0;
				}
				new->addr = buf->addr+algn;
				new->size = buf->size - algn;

				new->prev = buf;
				if(buf->next){
					buf->next->prev = new;
				}
				new->next = buf->next;
				buf->next = new;
			}
			buf->size = algn;
			buf->used = true;
			break;
		}
		buf = buf->next;
	}

	private_mutex_unlock(&ispmem.mlock);

	/*printk("##### %s %d  addr = 0x%08x #####\n", __func__,__LINE__, buf->addr);*/
	return buf ? buf->addr : 0;
}

void isp_free_buffer(unsigned int addr)
{
	struct isp_mem_buffer *buf = NULL;
	struct isp_mem_buffer *next = NULL;

	/*printk("##### %s %d  addr = 0x%08x #####\n", __func__,__LINE__, addr);*/
	private_mutex_lock(&ispmem.mlock);
	buf = ispmem.first;
	while(buf){
		if(buf->used == true && buf->addr == addr){
			buf->used = false;
			break;
		}
		buf = buf->next;
	}

	buf = ispmem.first;
	while(buf){
		if(buf->used == false){
			if(buf->next && buf->next->used == false && (buf->addr + buf->size == buf->next->addr)){
				next = buf->next;
				buf->size = buf->size + next->size;

				buf->next = next->next;
				if(next->next)
					next->next->prev = buf;
				/* free node */
				memset(next, 0, sizeof(*next));
				next->busy = false;
				continue;
			}
		}
		buf = buf->next;
	}

	private_mutex_unlock(&ispmem.mlock);
}

