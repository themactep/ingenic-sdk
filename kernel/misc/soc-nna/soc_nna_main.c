#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>


#include <linux/fs.h>
#include <linux/uaccess.h>

#include "soc_nna_common.h"

#define CPU_SIMULATOR
#define CMD_LINE "/proc/cmdline"
#define SOC_NNA_VERSION "H20230310"

#ifdef CONFIG_SOC_T41
    #define SOC_NNA_BORD "T41"
#elif defined(CONFIG_SOC_T40)
    #define SOC_NNA_BORD "T40"
#else
    #define SOC_NNA_BORD "Unknown"
#endif

static int nna_clk = 200000000;
module_param(nna_clk, int, S_IRUGO);
MODULE_PARM_DESC(nna_clk, "nna clock");

static uint32_t  num_all = 0;
struct buf{
	uint32_t version_buf;
	uint32_t nmem_extension_buf;
	uint32_t nmem_paddr;
	uint32_t nmem_size;
};
struct buf buf;
struct buf buf_comparison;

#define NNADMA_VA_2_PA(vaddr, paddr) \
do {                                \
    __asm__ __volatile__ (          \
        ".set push\n"               \
        ".set noreorder\n"          \
        ".set mips32r2\n"           \
        " ll %0, 0(%1)\n"           \
        " rdhwr %0,$4\n"            \
        ".set reorder\n"            \
        ".set pop\n"                \
        :"=r"(paddr)                \
        :"r"(vaddr)                 \
        :                           \
    );                              \
} while(0)

struct soc_nna {
    char                name[16];
    struct miscdevice   mdev;       /* miscdevice */
    struct device       *dev;
    int                 refcnt;
    void __iomem        *iomem;
    dma_addr_t          dmamem;
#ifndef CPU_SIMULATOR
    struct clk          *clk;
    struct clk          *clk_gate;
#endif
    struct mutex        mlock;
    struct list_head    memory_list;
    struct kmem_cache   *memory_cache;

    nna_dma_des_info_t  des_info[2];
};

struct soc_nna_memory_cache {
    struct list_head    list;
    struct soc_nna_buf  buf;
};

int soc_nna_open(struct inode *inode, struct file *file)
{
    struct miscdevice *mdev = file->private_data;
    struct soc_nna *pnna = list_entry(mdev, struct soc_nna, mdev);
    bool b_first_open = false;
	unsigned int cp0_status = 0;

	mutex_lock(&pnna->mlock);
#ifdef CONFIG_SOC_T41
	__asm__ volatile(
			".set push		\n\t"
			".set mips32r2	\n\t"
			"sync			\n\t"
			"lw $0,0(%0)	\n\t"
			::"r" (0xa0000000));
	*((volatile unsigned int *)(0xb3012038)) = 0x88404002;
	__asm__ volatile(
			".set push		\n\t"
			".set mips32r2	\n\t"
			"sync			\n\t"
			"lw $0,0(%0)	\n\t"
			::"r" (0xa0000000));
#endif
	if (pnna->refcnt++ == 0) {
		b_first_open = true;
	}
	mutex_unlock(&pnna->mlock);

	if (b_first_open) {
#ifndef CPU_SIMULATOR
        clk_enable(pnna->clk);
        clk_enable(pnna->clk_gate);
#endif
    }

    __asm__ volatile (" li   $t8, 0xffffffff \n"
                      " mtc0 $t8, $7, 0		\n"
					  " mfc0 $t8, $12, 0	\n"
					  " li $t9, 0x40000000 \n"
					  " or	$t8, $t8, $t9 \n"
					  " mtc0 $t8, $12, 0	\n"
					  " mfc0 %0, $12, 0	\n"
                      :"=r" (cp0_status):);
    /*printk("-------%s(%d): cp0_status=0x%x------\n", __func__, __LINE__, cp0_status);*/

    /*dev_info(pnna->mdev.this_device, "%s(%d) [%d:%d]: open sucess, refcnt=%d\n", __func__, __LINE__, current->tgid, current->pid, pnna->refcnt);*/

    printk("%s: open sucess\n", __func__);

    return 0;
}

int soc_nna_release(struct inode *inode, struct file *file)
{
    struct miscdevice *mdev = file->private_data;
    struct soc_nna *pnna = list_entry(mdev, struct soc_nna, mdev);
    bool b_last_release = false;

    mutex_lock(&pnna->mlock);
    if ((pnna->refcnt > 0) && (--pnna->refcnt == 0)) {
        b_last_release = true;
    }
    mutex_unlock(&pnna->mlock);

    if (b_last_release) {
        struct soc_nna_memory_cache *pelem = NULL;
        struct list_head *pos = NULL, *n = NULL;
        mutex_lock(&pnna->mlock);
        list_for_each_safe(pos, n, &pnna->memory_list) {
            pelem = list_entry(pos, struct soc_nna_memory_cache, list);
            list_del(pos);
            dma_free_coherent(pnna->mdev.this_device, pelem->buf.size, pelem->buf.vaddr, (dma_addr_t)pelem->buf.paddr);
            kmem_cache_free(pnna->memory_cache, pelem);
        }
        mutex_unlock(&pnna->mlock);
#ifndef CPU_SIMULATOR
        clk_disable(pnna->clk);
        clk_disable(pnna->clk_gate);
#endif
    }

    dev_info(pnna->mdev.this_device, "%s(%d) [%d:%d]: sucess\n", __func__, __LINE__, current->tgid, current->pid);

    return 0;
}

static loff_t soc_nna_llseek(struct file *file, loff_t offset, int origin)
{
    //TODO
    return 0;
}

static ssize_t soc_nna_read(struct file *file, char *buf, size_t size, loff_t *offset)
{
    //TODO
    return 0;
}

static ssize_t soc_nna_write(struct file *file, const char *buf, size_t size, loff_t *offset)
{
    //TODO
    return 0;
}

static unsigned int soc_nna_poll(struct file *file, struct poll_table_struct *poll_table)
{
    //TODO
    return 0;
}

static long soc_nna_malloc(struct soc_nna *pnna, long usr_arg)
{
    long ret = 0;
    struct soc_nna_buf buf;
    struct soc_nna_memory_cache *pelem = NULL;
    void *page = NULL, *endpage = NULL;
	unsigned int cp0_status = 0;

    __asm__ volatile (" li   $t8, 0xffffffff \n"
                      " mtc0 $t8, $7, 0		\n"
					  " mfc0 $t8, $12, 0	\n"
					  " li $t9, 0x40000000 \n"
					  " or	$t8, $t8, $t9 \n"
					  " mtc0 $t8, $12, 0	\n"
					  " mfc0 %0, $12, 0	\n"
                      :"=r" (cp0_status):);
    /*printk("-------%s(%d): cp0_status=0x%x------\n", __func__, __LINE__, cp0_status);*/


	if (copy_from_user(&buf, (void *)usr_arg, sizeof(buf))) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_from_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		ret = -EFAULT;
        goto err_copy_from_user;
	}

    pelem = kmem_cache_alloc(pnna->memory_cache, GFP_ATOMIC);
    if (!pelem) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:kmem_cache_alloc failed\n", __func__, __LINE__, current->tgid, current->pid);
		ret = -ENOMEM;
        goto err_kmem_cache_alloc;
    }

    buf.size = PAGE_ALIGN(buf.size);
    buf.vaddr = dma_alloc_coherent(pnna->mdev.this_device, buf.size, (dma_addr_t *)&buf.paddr, GFP_KERNEL);
    if (!buf.vaddr) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:dma_alloc_coherent failed\n", __func__, __LINE__, current->tgid, current->pid);
		ret = -ENOMEM;
        goto err_dma_alloc_coherent;
    }

    endpage = buf.vaddr + buf.size;
	for (page = buf.vaddr; page < endpage; page += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page));
	}

    memcpy(&pelem->buf, &buf, sizeof(buf));

    if (copy_to_user((void *)usr_arg, &buf, sizeof(buf))) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_to_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		ret = -EFAULT;
        goto err_copy_to_user;
    }

    mutex_lock(&pnna->mlock);
    list_add_tail(&pelem->list, &pnna->memory_list);
    mutex_unlock(&pnna->mlock);

    dev_info(pnna->mdev.this_device, "%s(%d) [%d:%d]:malloc success, buf.vaddr=%p, buf.paddr=%p, buf.size=0x%x\n", __func__, __LINE__, current->tgid, current->pid, buf.vaddr, buf.paddr, buf.size);

    return 0;

err_copy_to_user:
    dma_free_coherent(pnna->mdev.this_device, buf.size, buf.vaddr, (dma_addr_t)buf.paddr);
err_dma_alloc_coherent:
    kmem_cache_free(pnna->memory_cache, pelem);
err_kmem_cache_alloc:
err_copy_from_user:
    return ret;
}

static long soc_nna_free(struct soc_nna *pnna, long usr_arg)
{
    long ret = -1;
    struct soc_nna_buf buf;
    struct soc_nna_memory_cache *pelem = NULL;
	struct list_head *pos = NULL, *n = NULL;

	if (copy_from_user(&buf, (void *)usr_arg, sizeof(buf))) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_from_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		return -EFAULT;
	}

    mutex_lock(&pnna->mlock);
	list_for_each_safe(pos, n, &pnna->memory_list) {
		pelem = list_entry(pos, struct soc_nna_memory_cache, list);
        if ((pelem->buf.vaddr == buf.vaddr) && (pelem->buf.paddr == buf.paddr)) {
            list_del(pos);
            dma_free_coherent(pnna->mdev.this_device, pelem->buf.size, pelem->buf.vaddr, (dma_addr_t)pelem->buf.paddr);
            kmem_cache_free(pnna->memory_cache, pelem);
            dev_info(pnna->mdev.this_device, "%s(%d) [%d:%d]:free success, buf.vaddr=%p, buf.paddr=%p, buf.size=0x%x\n", __func__, __LINE__, current->tgid, current->pid, buf.vaddr, buf.paddr, buf.size);
            ret = 0;
            break;
        }
	}
    mutex_unlock(&pnna->mlock);

    return ret;
}

long soc_nna_flushcache(struct soc_nna *pnna, long usr_arg)
{
	struct flush_cache_info info;

	if (copy_from_user(&info, (void *)usr_arg, sizeof(info))) {
		printk("copy frame user failed\n");
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_from_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		return -EFAULT;
	}
	if((info.addr == 0) || (info.len == 0))
	{
		printk("addr or len zero\n");
		return -1;
	}
	num_all += info.len;
//	printk("info.addr = %x, info.len = %x, num_all = %x\n", info.addr, info.len, num_all);
	dma_cache_sync(NULL, (void *)info.addr, info.len, info.dir);

    /*dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:flush cache success\n", __func__, __LINE__, current->tgid, current->pid);*/

    return 0;
}

static void soc_nna_analysis_des(struct soc_nna *pnna, unsigned int st_idx, unsigned int cmd_cnt, nna_dma_cmd_t *d_va_cmd, nna_dma_des_info_t *des_info)
{
    int i = 0, j = 0;
    int c64kcnt = 0;
    unsigned int remain_bytes = 0, trans_bytes = 0, last_des = 0, cur_trans_bytes = 0, total_bytes = 0;
    unsigned int d_pa_st_addr = 0, o_pa_st_addr = 0, o_pa_mlc_addr = 0, o_pa_mlc_end_addr = 0;
    nna_dma_cmd_t *pcmd = NULL;
    unsigned int widx = 0, des_num = 0, chain_st_idx = st_idx;

    memset(des_info, 0, sizeof(nna_dma_des_info_t));

    for (i = st_idx; i < cmd_cnt; i++) {
        pcmd = d_va_cmd + i;
        c64kcnt = ((pcmd->data_bytes - 1) >> 16) + 1;
        remain_bytes = pcmd->data_bytes;
        NNADMA_VA_2_PA(pcmd->d_va_st_addr, d_pa_st_addr);
        NNADMA_VA_2_PA(pcmd->o_va_st_addr, o_pa_st_addr);
        NNADMA_VA_2_PA(pcmd->o_va_mlc_addr, o_pa_mlc_addr);
        o_pa_mlc_end_addr = o_pa_mlc_addr + pcmd->o_mlc_bytes;

        for (j = 0; j < c64kcnt; j++) {
            last_des = (j == (c64kcnt - 1)) ? 1 : 0;
            trans_bytes = last_des ? remain_bytes : 65536;

            if ((o_pa_st_addr + trans_bytes) <= o_pa_mlc_end_addr) {
                des_info->des_data[widx++] = ((((last_des && !pcmd->des_link) ? DES_CFG_END : DES_CFG_LINK) << DES_CFG_FLAG) & DES_CFG_FLAG_MASK)
                    | (((((unsigned long long int)trans_bytes >> SOC_NNA_ADDR_ALIGN_BIT) - 1ULL) << DES_DATA_LEN) & DES_DATA_LEN_MASK)
                    | ((((unsigned long long int)o_pa_st_addr >> SOC_NNA_ADDR_ALIGN_BIT) << DES_ORAM_ADDR) & DES_ORAM_ADDR_MASK)
                    | ((((unsigned long long int)d_pa_st_addr >> SOC_NNA_ADDR_ALIGN_BIT) << DES_DDR_ADDR) & DES_DDR_ADDR_MASK);
                d_pa_st_addr += trans_bytes;
                o_pa_st_addr += trans_bytes;
                des_num++;
            } else {
                cur_trans_bytes = o_pa_mlc_end_addr - o_pa_st_addr;
                des_info->des_data[widx++] = ((DES_CFG_LINK << DES_CFG_FLAG) & DES_CFG_FLAG_MASK)
                    | (((((unsigned long long int)cur_trans_bytes >> SOC_NNA_ADDR_ALIGN_BIT) - 1ULL) << DES_DATA_LEN) & DES_DATA_LEN_MASK)
                    | ((((unsigned long long int)o_pa_st_addr >> SOC_NNA_ADDR_ALIGN_BIT) << DES_ORAM_ADDR) & DES_ORAM_ADDR_MASK)
                    | ((((unsigned long long int)d_pa_st_addr >> SOC_NNA_ADDR_ALIGN_BIT) << DES_DDR_ADDR) & DES_DDR_ADDR_MASK);

                des_info->des_data[widx++] = ((((last_des && !pcmd->des_link) ? DES_CFG_END : DES_CFG_LINK) << DES_CFG_FLAG) & DES_CFG_FLAG_MASK)
                    | (((((unsigned long long int)(trans_bytes - cur_trans_bytes) >> SOC_NNA_ADDR_ALIGN_BIT) - 1ULL) << DES_DATA_LEN) & DES_DATA_LEN_MASK)
                    | ((((unsigned long long int)o_pa_mlc_addr >> SOC_NNA_ADDR_ALIGN_BIT) << DES_ORAM_ADDR) & DES_ORAM_ADDR_MASK)
                    | ((((unsigned long long int)(d_pa_st_addr + cur_trans_bytes) >> SOC_NNA_ADDR_ALIGN_BIT) << DES_DDR_ADDR) & DES_DDR_ADDR_MASK);
                d_pa_st_addr += trans_bytes;
                o_pa_st_addr = o_pa_mlc_addr + (trans_bytes - cur_trans_bytes);
                des_num += 2;
            }
            remain_bytes -= trans_bytes;
        }

        total_bytes += pcmd->data_bytes;
        if (!pcmd->des_link) {
            des_info->des_num[des_info->chain_num] = des_num;
            des_info->total_bytes[des_info->chain_num] = total_bytes;
            des_info->chain_st_idx[des_info->chain_num] = chain_st_idx;
            des_info->chain_num++;
            des_num = 0;
            total_bytes = 0;
            chain_st_idx = i + 1;
        }
    }
}

static void soc_nna_update_des(struct soc_nna *pnna, unsigned int *d_va_chn, des_gen_result_t *des_rslt)
{
    nna_dma_des_info_t *des_info = pnna->des_info;
    int des_remain = 2048; //(16 * 1024) / sizeof(unsigned long long int);
    int chnidx = 0, desidx = 0, destotal_chain = 0, rdidx = 0, wridx = 0;
    int maxchnnum = des_info[0].chain_num > des_info[1].chain_num ? des_info[0].chain_num : des_info[1].chain_num;
    unsigned long long int *vdma = (unsigned long long int *)pnna->dmamem;
    memset(des_rslt, 0, sizeof(des_gen_result_t));

    for (chnidx = 0; chnidx < maxchnnum; chnidx++) {
        destotal_chain = des_info[0].des_num[chnidx] + des_info[1].des_num[chnidx] + 2;
        if (destotal_chain > des_remain) {
            des_rslt->rcmd_st_idx = des_info[0].chain_st_idx[chnidx];
            des_rslt->wcmd_st_idx = des_info[1].chain_st_idx[chnidx];
            des_rslt->dma_chn_num = chnidx;
            des_rslt->finish = 0;
            return;
        }

        /* rd chain */
        *d_va_chn++ = desidx;
        *(vdma + desidx++) = ((DES_CFG_CNT << DES_CFG_FLAG) & DES_CFG_FLAG_MASK)
            | ((des_info[0].total_bytes[chnidx] << DES_TOTAL_BYTES) & DES_TOTAL_BYTES_MASK);
        memcpy(vdma + desidx, &(des_info[0].des_data[rdidx]), 8 * des_info[0].des_num[chnidx]);
        desidx += des_info[0].des_num[chnidx];
        rdidx += des_info[0].des_num[chnidx];

        /* wr chain */
        *d_va_chn++ = desidx;
        *(vdma + desidx++) = ((DES_CFG_CNT << DES_CFG_FLAG) & DES_CFG_FLAG_MASK)
            | ((des_info[1].total_bytes[chnidx] << DES_TOTAL_BYTES) & DES_TOTAL_BYTES_MASK);
        memcpy(vdma + desidx, &(des_info[1].des_data[wridx]), 8 * des_info[1].des_num[chnidx]);
        desidx += des_info[1].des_num[chnidx];
        wridx += des_info[1].des_num[chnidx];

        des_remain -= destotal_chain;
    }

    des_rslt->dma_chn_num = maxchnnum;
    des_rslt->finish = 1;
}

long soc_nna_setup_des(struct soc_nna *pnna, long usr_arg)
{
    long ret = 0;
    nna_dma_cmd_set_t cmd_set;
    nna_dma_cmd_t *d_va_cmd = NULL, *d_pa_cmd = NULL;
    unsigned int d_pa_chn = 0;
    unsigned int *d_va_chn = NULL;

	if (copy_from_user(&cmd_set, (void *)usr_arg, sizeof(nna_dma_cmd_set_t))) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_from_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		ret = -EFAULT;
        goto err_copy_from_user;
	}

    mutex_lock(&pnna->mlock);

    NNADMA_VA_2_PA((unsigned long)cmd_set.d_va_cmd, d_pa_cmd);      //convert user space vaddr to paddr
    d_va_cmd = phys_to_virt((unsigned long)d_pa_cmd);               //map paddr to kernel space vaddr to be used by kernel

    NNADMA_VA_2_PA((unsigned long)cmd_set.d_va_chn, d_pa_chn);      //convert user space vaddr to paddr
    d_va_chn = phys_to_virt((unsigned long)d_pa_chn);               //map paddr to kernel space vaddr to be used by kernel

    soc_nna_analysis_des(pnna, cmd_set.rd_cmd_st_idx, cmd_set.rd_cmd_cnt, d_va_cmd, &(pnna->des_info[0]));
    soc_nna_analysis_des(pnna, cmd_set.wr_cmd_st_idx, cmd_set.wr_cmd_cnt, d_va_cmd + cmd_set.rd_cmd_cnt, &(pnna->des_info[1]));

    soc_nna_update_des(pnna, d_va_chn, &cmd_set.des_rslt);
    mutex_unlock(&pnna->mlock);

	if (copy_to_user((void *)usr_arg, &cmd_set, sizeof(nna_dma_cmd_set_t))) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_to_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		ret = -EFAULT;
        goto err_copy_to_user;
	}

    return 0;

err_copy_to_user:
err_copy_from_user:
    return ret;
}

long soc_nna_rdchn_start(struct soc_nna *pnna, long usr_arg)
{
    dma_addr_t dma_addr = 0;

	if (copy_from_user(&dma_addr, (void *)usr_arg, sizeof(dma_addr_t))) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_from_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		return -EFAULT;
	}

    soc_nna_writel(pnna, NNA_DMA_RCFG, ((dma_addr << RCFG_DES_ADDR) & RCFG_DES_ADDR_MASK) | (1 << RCFG_START));

    return 0;
}

long soc_nna_wrchn_start(struct soc_nna *pnna, long usr_arg)
{
    dma_addr_t dma_addr = 0;

	if (copy_from_user(&dma_addr, (void *)usr_arg, sizeof(dma_addr_t))) {
        dev_err(pnna->mdev.this_device, "%s(%d) [%d:%d]:copy_from_user failed\n", __func__, __LINE__, current->tgid, current->pid);
		return -EFAULT;
	}

    soc_nna_writel(pnna, NNA_DMA_WCFG, ((dma_addr << WCFG_DES_ADDR) & WCFG_DES_ADDR_MASK) | (1 << WCFG_START));

    return 0;
}
int soc_nna_version(struct soc_nna *pnna, long usr_arg)
{
	long ret = 0;
#ifdef CONFIG_SOC_T41
	uint32_t soc_version_buf = 0x00000041;
	memcpy(&buf_comparison, (int *)usr_arg, sizeof(uint32_t));
#elif defined(CONFIG_SOC_T40)
	uint32_t soc_version_buf = 0x20220525;
#endif
	if (copy_from_user(&buf, (void __user *)usr_arg, sizeof(struct buf))) {
		printk("copy_from_user nmem_extension_size error %d\n", __LINE__);
		ret = -EFAULT;
	}

	buf.version_buf = soc_version_buf;
	if (copy_to_user((void __user *)usr_arg, &buf, sizeof(struct buf))) {
		printk("copy_to_user version error %d\n", __LINE__);
		ret = -EFAULT;
	}
	return ret;
}

static long soc_nna_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = -1;
    struct miscdevice *mdev = file->private_data;
    struct soc_nna *pnna = list_entry(mdev, struct soc_nna, mdev);

	switch (cmd) {
        case IOCTL_SOC_NNA_MALLOC:
            ret = soc_nna_malloc(pnna, arg);
            break;
        case IOCTL_SOC_NNA_FREE:
            ret = soc_nna_free(pnna, arg);
            break;
        case IOCTL_SOC_NNA_FLUSHCACHE:
            ret = soc_nna_flushcache(pnna, arg);
            break;
        case IOCTL_SOC_NNA_SETUP_DES:
            ret = soc_nna_setup_des(pnna, arg);
            break;
        case IOCTL_SOC_NNA_RDCH_START:
            ret = soc_nna_rdchn_start(pnna, arg);
            break;
        case IOCTL_SOC_NNA_WRCH_START:
            ret = soc_nna_wrchn_start(pnna, arg);
			break;
		case IOCTL_SOC_NNA_VERSION:
			ret = soc_nna_version(pnna, arg);
            break;
        default:
            dev_err(mdev->this_device, "%s(%d) [%d:%d]: unsupport cmd=0x%x\n", __func__, __LINE__, current->tgid, current->pid, cmd);
            return -1;
    }

    return ret;
}

/**
 * According to mips memory architecture, the user space range is [0x00000000 ~ 0x800000000], but the io memory is [0x10000000 to 0x20000000];
 * the io memory should be mapped to uncached.
 * Because nna dma descript memory [0x12500000 ~ 0x12504000 ] and oram memory [0x12600000 ~ 0x12700000] is data communication memory, so mmaps
 * they to uncached accelerated
 */
#if 1

static int get_nmem_info(uint32_t * nmem_addr, uint32_t * nmem_size)
{
	int ret;
    loff_t pos;
    mm_segment_t fs;
	char buf[512] = "";
	char *p = NULL;
	char *retptr;

	struct file *fb = filp_open(CMD_LINE, O_RDONLY, 0644);
	if(fb == NULL) {
		printk("open file (%s) error\n", CMD_LINE);
		return -1;
	}

    fs = get_fs();
    set_fs(KERNEL_DS);

    pos = 0;
	ret = vfs_read(fb, buf, sizeof(buf), &pos);
	if(ret <= 0) {
		printk("fread (%s) error\n", CMD_LINE);
		return -1;
	}

	filp_close(fb, NULL);
    set_fs(fs);

#if 1
	p = strstr(buf, "nmem");
	if(p == NULL) {
		printk("fread (%s) error\n", CMD_LINE);
		return -1;
	}

	*nmem_size = memparse(p + 5, &retptr);

	if (*retptr == '@')
		*nmem_addr = memparse(retptr + 1, NULL);
#endif
	return 0;
}

#endif
static int soc_nna_mmap(struct file *file, struct vm_area_struct *vma)
{
	uint32_t nmem_addr = 0;
	uint32_t nmem_size = 0;
	uint32_t n = 0, set_oram_nums = 0;
    struct miscdevice *mdev = file->private_data;
	unsigned long paddr_start = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long paddr_end = paddr_start + vma->vm_end - vma->vm_start;
#ifdef CONFIG_SOC_T40
	unsigned int oram_size_cmd_t = 0, oram_real_size = 0;
	unsigned int l2cache_size = 0;
#endif
	uint32_t mmap_type = 0;
	uint32_t extend = 0;


	/* 设置 pfn的属性 */
	vma->vm_flags |= VM_IO;
	get_nmem_info(&nmem_addr, &nmem_size);

	/* 根据地址区分地址的类型 */
	if(paddr_start == 0x12620000){
		mmap_type = 1;//oram
		extend = 0;
	}else if(paddr_start == 0x12500000){
		mmap_type = 2; //descram
		extend = 0;
	}else if((paddr_start < 0x12620000 ) && (paddr_end > 0x12620000)) {
		mmap_type = 1; //oram
		extend = 1;
	} else {
		mmap_type = 3;// nmem
		if(paddr_start == nmem_addr)
		{
			extend = 0;
		} else {
			extend = 1;
		}
	}
#ifdef CONFIG_SOC_T40
	/* l2cache_size + ORAM = 1*1024*1024 这里是获取l2cache size，根据l2cache size来得到ORAM size */
	oram_size_cmd_t = *((volatile unsigned int *)(0xb2200060));
	switch((oram_size_cmd_t & 0x1c00) >> 10)
	{
		case 1:
			l2cache_size = 128;
			oram_real_size = 1024 - l2cache_size;
			break;
		case 2:
			l2cache_size = 256;
			oram_real_size = 1024 - l2cache_size;
			break;
		case 3:
			l2cache_size = 512;
			oram_real_size = 1024 - l2cache_size;
			break;
		case 4:
			l2cache_size = 1024;
			oram_real_size = 1024 - l2cache_size;
			break;
		default:
			break;
	}
#endif
	/* 根据不同虚拟地址的类型来设置属性 */
	if(mmap_type != 3){
		pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
		pgprot_val(vma->vm_page_prot) |= _CACHE_UNCACHED_ACCELERATED;
	}else{
		pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
		pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_WB_WA;
	}

	/* 根据上层传过来的地址，来判断需不需要扩充映射的虚拟地址的大小，extent=0 不需要扩充映射；extent=1 扩充映射 */
	if (extend == 0){
		if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
			dev_err(mdev->this_device, "%s(%d) [%d:%d]: io_remap_pfn_range failed\n", __func__, __LINE__, current->tgid, current->pid);
			return -EAGAIN;
		}
	}else {
		unsigned int real_paddr = 0x12620000;
#ifdef CONFIG_SOC_T41
		unsigned int real_size = 384 * 1024;//目前T41的oram_size 为 384 *1024
#elif defined(CONFIG_SOC_T40)
		unsigned int real_size = oram_real_size * 1024;//896 * 1024
#endif
		unsigned int before_s = 0;
		unsigned int after_s = 0;
		if(mmap_type !=1)//oram
		{
			if(buf_comparison.version_buf == 0)
			{
				real_size = nmem_size;
				real_paddr = nmem_addr;
			}else{
				real_size = buf.nmem_size;
				real_paddr = buf.nmem_paddr;
			}
		}
			/*根据上层传过来扩充映射内存的大小和ORAM size，来做处理，每次映射的区域不能超过实际的物理大小，不改变物理地址，根据虚拟地址偏移多次映射来达到扩充虚拟地址的目的 */
			before_s = real_paddr - paddr_start;
			set_oram_nums = before_s / real_size;
			set_oram_nums += before_s%real_size ? 1 : 0;
			for(n = 0 ; n < set_oram_nums; n++){
				if (io_remap_pfn_range(vma, vma->vm_start + n * real_size, real_paddr>>PAGE_SHIFT, (before_s % real_size)&&(n == (set_oram_nums -1)) ? before_s % real_size : real_size , vma->vm_page_prot)) {
					dev_err(mdev->this_device, "%s(%d) [%d:%d]: io_remap_pfn_range failed\n", __func__, __LINE__, current->tgid, current->pid);
					return -EAGAIN;
				}
			}
			if (io_remap_pfn_range(vma, vma->vm_start + before_s, real_paddr>>PAGE_SHIFT, real_size, vma->vm_page_prot)) {
				dev_err(mdev->this_device, "%s(%d) [%d:%d]: io_remap_pfn_range failed\n", __func__, __LINE__, current->tgid, current->pid);
				return -EAGAIN;
			}
			after_s = paddr_end - real_paddr - real_size;
			set_oram_nums = after_s / real_size;
			set_oram_nums += after_s % real_size ? 1 : 0;
			for(n = 0; n < set_oram_nums ; n++){
				if (io_remap_pfn_range(vma, vma->vm_start+ before_s + real_size * (n + 1) , real_paddr>>PAGE_SHIFT,(after_s % real_size)&&(n == (set_oram_nums -1)) ? after_s % real_size : real_size, vma->vm_page_prot)) {
					dev_err(mdev->this_device, "%s(%d) [%d:%d]: io_remap_pfn_range failed\n", __func__, __LINE__, current->tgid, current->pid);
					return -EAGAIN;
				}
			}
	}
	return 0;
}

static const struct file_operations soc_nna_fops = {
	.owner          = THIS_MODULE,
	.open           = soc_nna_open,
	.release        = soc_nna_release,
	.llseek         = soc_nna_llseek,
	.read           = soc_nna_read,
	.write          = soc_nna_write,
	.poll           = soc_nna_poll,
	.unlocked_ioctl	= soc_nna_unlocked_ioctl,
	.mmap           = soc_nna_mmap,
};

static int soc_nna_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct soc_nna *pnna = NULL;
    struct resource *res = NULL;
    unsigned int oram_clk = 0;

    pnna = (struct soc_nna *)kzalloc(sizeof(struct soc_nna), GFP_KERNEL);
    if (!pnna) {
        dev_err(&pdev->dev, "kzalloc struct soc_nna failed\n");
        ret = -ENOMEM;
        goto err_kzalloc_soc_nna;
    }

    ret = snprintf(pnna->name, sizeof(pnna->name), "%s", pdev->name);
    if (ret < 0) {
        dev_err(&pdev->dev, "snprintf soc_nna name failed\n");
        ret = -EINVAL;
        goto err_snprintf_name;
    }

    pnna->mdev.minor = MISC_DYNAMIC_MINOR;
    pnna->mdev.name = pnna->name;
    pnna->mdev.fops = &soc_nna_fops;
    pnna->dev = &pdev->dev;

    mutex_init(&pnna->mlock);

    INIT_LIST_HEAD(&pnna->memory_list);
    pnna->memory_cache = kmem_cache_create(pnna->name, sizeof(struct soc_nna_memory_cache), 0, SLAB_HWCACHE_ALIGN, NULL);
    if (!pnna->memory_cache) {
        printk("%s:kmem_cache_create failed\n", __func__);
        ret = -ENOMEM;
        goto err_kmem_cache_create;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "no iomem resource\n");
        ret = -ENXIO;
        goto err_get_iomem_resource;
    }

    pnna->iomem = ioremap(res->start, resource_size(res));
    if (!pnna->iomem) {
        dev_err(&pdev->dev, "ioremap iomem resource failed\n");
        ret = -EPERM;
        goto err_ioremap_iomem;
    }

    res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
    if (!res) {
        dev_err(&pdev->dev, "no dmamem resource\n");
        ret = -ENXIO;
        goto err_get_dmamem_resource;
    }

    pnna->dmamem = (dma_addr_t)ioremap(res->start, resource_size(res));
    if (!pnna->iomem) {
        dev_err(&pdev->dev, "ioremap dmamem resource failed\n");
        ret = -EPERM;
        goto err_ioremap_dmamem;
    }

#ifndef CPU_SIMULATOR
    pnna->clk = clk_get(&pdev->dev, "cgu_nna");
    if (IS_ERR(pnna->clk)) {
        dev_err(&pdev->dev, "get cgu nna clk failed\n");
        ret = PTR_ERR(pnna->clk);
        goto err_clk_get_cgu_nna;
    }
    clk_set_rate(pnna->clk, nna_clk);

    pnna->clk_gate = clk_get(&pdev->dev, "nna");
    if (IS_ERR(pnna->clk_gate)) {
        dev_err(&pdev->dev, "get gate nna clk failed\n");
        ret = PTR_ERR(pnna->clk_gate);
        goto err_clk_get_gate_nna;
    }
#endif

    platform_set_drvdata(pdev, pnna);

    ret = misc_register(&pnna->mdev);
    if (ret < 0) {
        dev_err(&pdev->dev, "register misc device failed\n");
        goto err_misc_register;
    }

    oram_clk = *(volatile unsigned int*)0xb2200060;
    *(volatile unsigned int *)0xb2200060 = oram_clk | (1 << 5);
    printk("@@@@ soc nna probe sucess (Board: %s, Version: %s) @@@\n", SOC_NNA_BORD, SOC_NNA_VERSION);

    return 0;

err_misc_register:
#ifndef CPU_SIMULATOR
    clk_put(pnna->clk_gate);
err_clk_get_gate_nna:
    clk_put(pnna->clk);
err_clk_get_cgu_nna:
#endif
    iounmap((void *)pnna->dmamem);
err_ioremap_dmamem:
err_get_dmamem_resource:
    iounmap(pnna->iomem);
err_ioremap_iomem:
err_get_iomem_resource:
    kmem_cache_destroy(pnna->memory_cache);
err_kmem_cache_create:
err_snprintf_name:
    kfree(pnna);
err_kzalloc_soc_nna:
    return ret;
}

static int __exit soc_nna_remove(struct platform_device *pdev)
{
    struct soc_nna *pnna = platform_get_drvdata(pdev);
    if (pnna) {
        misc_deregister(&pnna->mdev);
#ifndef CPU_SIMULATOR
        clk_put(pnna->clk_gate);
        clk_put(pnna->clk);
#endif
        iounmap((void *)pnna->dmamem);
        iounmap(pnna->iomem);
        kmem_cache_destroy(pnna->memory_cache);
        kfree(pnna);
    }

	return 0;
}

#ifdef CONFIG_PM
static int soc_nna_suspend(struct device *pdev)
{
    //TODO
	return 0;
}

static int soc_nna_resume(struct device *pdev)
{
    //TODO
	return 0;
}

static const struct dev_pm_ops soc_nna_pm_ops = {
    .suspend = soc_nna_suspend,
    .resume = soc_nna_resume,
};
#endif

static struct platform_driver soc_nna_driver = {
    .probe = soc_nna_probe,
    .remove = soc_nna_remove,
    .driver = {
        .name = "soc-nna",
        .owner = THIS_MODULE,
#ifdef CONFIG_PM
        .pm = &soc_nna_pm_ops,
#endif
    },
};

static int __init soc_nna_init(void)
{
    int ret = 0;

    ret = platform_device_register(&soc_nna_device);
    if (ret < 0) {
        printk("%s platform_device_register failed:ret=0x%x\n", __func__, ret);
        goto err_platform_device_register;
    }

	ret = platform_driver_register(&soc_nna_driver);
	if(ret){
        printk("%s platform_driver_register failed:ret=0x%x\n", __func__, ret);
        goto err_platform_driver_register;
	}

    printk("%s success\n", __func__);

    return 0;

err_platform_driver_register:
    platform_device_unregister(&soc_nna_device);
err_platform_device_register:
	return ret;
}

static void __exit soc_nna_exit(void)
{
	platform_driver_unregister(&soc_nna_driver);
	platform_device_unregister(&soc_nna_device);
    printk("%s success\n", __func__);
}

module_init(soc_nna_init);
module_exit(soc_nna_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Justin <pengtao.kang@ingenic.com>");
MODULE_DESCRIPTION("Ingenic soc nna driver");
MODULE_ALIAS("platform:soc-nna");
MODULE_VERSION("20190724a");
