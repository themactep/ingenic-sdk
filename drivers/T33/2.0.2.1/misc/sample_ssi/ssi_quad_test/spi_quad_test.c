#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "spi_test.h"



void *var_addr = NULL;

#define jz_sfc_readl(n) (*(unsigned long *)(var_addr + n))
#define jz_sfc_writel(x, n) (*(unsigned long *)(var_addr + n) = x)
#define debug() printf("__%d Line   %s\n", __LINE__, __func__)
// #define debug()

#define CONFIG_SPI_QUAD

unsigned int sfc_quad_mode = 0;
int mode = 6;


int set_base_addr(void)
{
    int mem_fd;
    int size = 0x2000;

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0)
    {
        printf("open /dev/mem error!\n");
        return -1;
    }

    var_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, SFC1_BASE);
    if (var_addr == MAP_FAILED)
    {
        printf("mmap fail\n");
        close(mem_fd);
        return -1;
    }

#if 0
	pbint = (unsigned long)var_addr + PBINT_OFFSET;
	pbmsk = (unsigned long)var_addr + PBMSK_OFFSET;
	pbpat1 = (unsigned long)var_addr + PBPAT1_OFFSET;
	pbpat0 = (unsigned long)var_addr + PBPAT0_OFFSET;

	//GPIO状态设置
	//mode_0
	(*(unsigned long *)(pbint + 0x8)) = 0xc0;
	(*(unsigned long *)(pbmsk + 0x4)) = 0xc0;
	(*(unsigned long *)(pbpat1 + 0x8)) = 0xc0;
	(*(unsigned long *)(pbpat0 + 0x8)) = 0xc0;

	while(1)
	{
		(*(unsigned long *)(pbpat0 + 0x4)) = 0xc0;
		(*(unsigned long *)(pbpat0 + 0x8)) = 0xc0;
	}

	// gpio_set(pbpat0);
#endif
    return 0;
}

void sfc_dump_reg()
{
    //return ;
    int i = 0;
    printf("SFC_GLB                 :0x%08lx\n", jz_sfc_readl(SFC_GLB));
    printf("SFC_GLB1                :0x%08lx\n", jz_sfc_readl(SFC_GLB1));
    printf("SFC_DEV_CONF            :0x%08lx\n", jz_sfc_readl(SFC_DEV_CONF));
    printf("SFC_DEV_STA_RT          :0x%08lx\n", jz_sfc_readl(SFC_DEV_STA_RT));
    printf("SFC_DEV1_STA_RT         :0x%08lx\n", jz_sfc_readl(SFC_DEV1_STA_RT));
    printf("SFC_DEV_STA_MSK         :0x%08lx\n", jz_sfc_readl(SFC_DEV_STA_MSK));
    printf("SFC_TRAN_LEN            :0x%08lx\n", jz_sfc_readl(SFC_TRAN_LEN));
    for (i = 0; i < 6; i++)
        printf("SFC_TRAN_CONF(%d)        :0x%08lx\n", i, jz_sfc_readl(SFC_TRAN_CONF(i)));
    for (i = 0; i < 6; i++)
        printf("SFC_TRAN_CONF1(%d)       :0x%08lx\n", i, jz_sfc_readl(SFC_TRAN_CONF1(i)));
    for (i = 0; i < 6; i++)
        printf("SFC_DEV_ADDR(%d)         :0x%08lx\n", i, jz_sfc_readl(SFC_DEV_ADDR(i)));
    printf("SFC_MEM_ADDR            :0x%08lx\n", jz_sfc_readl(SFC_MEM_ADDR));
    printf("SFC_TRIG                :0x%08lx\n", jz_sfc_readl(SFC_TRIG));
    printf("SFC_SR                  :0x%08lx\n", jz_sfc_readl(SFC_SR));
    printf("SFC_SCR                 :0x%08lx\n", jz_sfc_readl(SFC_SCR));
    printf("SFC_INTC                :0x%08lx\n", jz_sfc_readl(SFC_INTC));
    printf("SFC_FSM                 :0x%08lx\n", jz_sfc_readl(SFC_FSM));
    printf("SFC_CGE                 :0x%08lx\n", jz_sfc_readl(SFC_CGE));
}

void sfc_transfer_direction(int value)
{
    unsigned int tmp;
    if (value == 0)
    {
        tmp = jz_sfc_readl(SFC_GLB);
        tmp &= ~GLB_TRAN_DIR;
        jz_sfc_writel(tmp, SFC_GLB);
    }
    else
    {
        tmp = jz_sfc_readl(SFC_GLB);
        tmp |= GLB_TRAN_DIR;
        jz_sfc_writel(tmp, SFC_GLB);
    }

    tmp = jz_sfc_readl(SFC_GLB);
    tmp &= ~(GLB_DES_EN | GLB_CDT_EN);
    jz_sfc_writel(tmp, SFC_GLB);
}

void sfc_set_length(int value)
{
    jz_sfc_writel(value, SFC_TRAN_LEN);
}

void sfc_set_addr_length(int channel, unsigned int value)
{
    unsigned int tmp;
    tmp = jz_sfc_readl(SFC_TRAN_CONF(channel));
    tmp &= ~(TRAN_CONF0_ADDR_WIDTH_MSK);
    tmp |= (value << TRAN_CONF0_ADDR_WIDTH_OFFSET);
    jz_sfc_writel(tmp, SFC_TRAN_CONF(channel));
}

void sfc_cmd_en(int channel, unsigned int value)
{
    if (value == 1)
    {
        unsigned int tmp;
        tmp = jz_sfc_readl(SFC_TRAN_CONF(channel));
        tmp |= TRAN_CONF0_CMD_EN;
        jz_sfc_writel(tmp, SFC_TRAN_CONF(channel));
    }
    else
    {
        unsigned int tmp;
        tmp = jz_sfc_readl(SFC_TRAN_CONF(channel));
        tmp &= ~TRAN_CONF0_CMD_EN;
        jz_sfc_writel(tmp, SFC_TRAN_CONF(channel));
    }
}

void sfc_data_en(int channel, unsigned int value)
{
    if (value == 1)
    {
        unsigned int tmp;
        tmp = jz_sfc_readl(SFC_TRAN_CONF(channel));
        tmp |= TRAN_CONF0_DATEEN;
        jz_sfc_writel(tmp, SFC_TRAN_CONF(channel));
    }
    else
    {
        unsigned int tmp;
        tmp = jz_sfc_readl(SFC_TRAN_CONF(channel));
        tmp &= ~TRAN_CONF0_DATEEN;
        jz_sfc_writel(tmp, SFC_TRAN_CONF(channel));
    }
}

void sfc_write_cmd(int channel, unsigned int value)
{
    unsigned int tmp;
    tmp = jz_sfc_readl(SFC_TRAN_CONF(channel));
    tmp &= ~TRAN_CONF0_CMD_MSK;
    tmp |= value;
    jz_sfc_writel(tmp, SFC_TRAN_CONF(channel));
}

void sfc_dev_addr(int channel, unsigned int value)
{
    jz_sfc_writel(value, SFC_DEV_ADDR(channel));
}

void sfc_dev_addr_plus(int channel, unsigned int value)
{
    jz_sfc_writel(value, SFC_DEV_ADDR_PLUS(channel));
}

void sfc_dev_addr_dummy_bytes(int channel, unsigned int value)
{
    unsigned int tmp;
    tmp = jz_sfc_readl(SFC_TRAN_CONF(channel));
    tmp &= ~TRAN_CONF0_DMYBITS_MSK;
    tmp |= (value << TRAN_CONF0_DMYBITS_OFFSET);
    jz_sfc_writel(tmp, SFC_TRAN_CONF(channel));
}

void sfc_set_mode(int channel, int value)
{
    unsigned int tmp;
    /*T40 modify SFC_TRAN_CONF->SFC_TRAN_CONF1*/
    tmp = jz_sfc_readl(SFC_TRAN_CONF1(channel));
    tmp &= ~(TRAN_CONF1_TRAN_MODE_MSK);
    tmp |= (value << TRAN_CONF1_TRAN_MODE_OFFSET);
    jz_sfc_writel(tmp, SFC_TRAN_CONF1(channel));
	printf("*****************sfc quad read %08lx\n", jz_sfc_readl(SFC_TRAN_CONF1(channel)));
}

void sfc_set_transfer(struct jz_sfc *hw, int dir)
{
    /* step1. 设置读或者写 */
    if (dir == 1)
        sfc_transfer_direction(GLB_TRAN_DIR_WRITE);
    else
        sfc_transfer_direction(GLB_TRAN_DIR_READ);


    /* step2.  设置传输字节数*/
    sfc_set_length(hw->len);

    /* step3. 设置addr长度 */
    sfc_set_addr_length(0, hw->addr_len);
    // sfc_set_addr_length(0, 0);

    /* step4. 设置是否使用cmd */
    sfc_cmd_en(0, 0);
    // sfc_cmd_en(0, 0x1);

    /* step5. 设置是否传输数据 */
    sfc_data_en(0, hw->daten);

    /* step6. 配置传输的cmd */
    // sfc_write_cmd(0, hw->cmd);
    // sfc_write_cmd(0, 0);

    /* step7. 配置传输的addr */
    sfc_dev_addr(0, hw->addr);
    // sfc_dev_addr(0, 0);

    /* step8. 配置传输的高位addr */
    sfc_dev_addr_plus(0, hw->addr_plus);
    // sfc_dev_addr_plus(0, 0);

    /* step9. 配置所需的dummy cycles */
    sfc_dev_addr_dummy_bytes(0, hw->dummy_byte);

    /* step10. 配置传输的 SPI 模式 */
    // sfc_set_mode(0,hw->sfc_mode);
#ifdef CONFIG_SPI_QUAD
    sfc_set_mode(0, 5);
#else
    sfc_set_mode(0, 1);
#endif
}

void sfc_send_cmd(unsigned char *cmd, unsigned int len, unsigned int addr, unsigned addr_len, unsigned dummy_byte, unsigned int daten, unsigned char dir)
{
    struct jz_sfc sfc;
    unsigned int reg_tmp = 0;
    sfc.cmd = *cmd;
    sfc.addr_len = addr_len;
    // sfc.addr = ((*addr << 16) &0x00ff0000) | ((*(addr + 1) << 8)&0x0000ff00) |((*(addr + 2))&0xff);
    sfc.addr = addr;
    sfc.addr_plus = 0;
    sfc.dummy_byte = dummy_byte;
    sfc.daten = daten;
    sfc.len = len;

    debug();

    if ((daten == 1) && (addr_len != 0))
    {
        sfc.sfc_mode = mode;
        debug();
    }
    else
    {
        sfc.sfc_mode = 0;
    }
    debug();
    sfc_set_transfer(&sfc, dir);
    debug();
    jz_sfc_writel(TRIG_FLUSH, SFC_TRIG);//清空fifo
    jz_sfc_writel(TRIG_START, SFC_TRIG);//开始传输

    sfc_dump_reg();

    /*this must judge the end status*/
    // debug();
    if (daten == 0)
    {
        reg_tmp = jz_sfc_readl(SFC_SR);
        // printf("SR %x\n", reg_tmp);
        while (!(reg_tmp & END))
        {
            reg_tmp = jz_sfc_readl(SFC_SR);
        }
        if ((jz_sfc_readl(SFC_SR)) & END)
            jz_sfc_writel(CLR_END, SFC_SCR);
    }
    debug();
}

static int sfc_write_data(unsigned int *data, unsigned int length)
{
    unsigned int tmp_len = 0;
    unsigned int fifo_num = 0;
    unsigned int i;
    unsigned int reg_tmp = 0;
    unsigned int len = (length + 3) / 4;

    // debug();

    // reg_tmp = jz_sfc_readl(SFC_SR);
    // printf("SFC_SR %x\n", reg_tmp);
    // jz_sfc_writel(TRIG_START, SFC_TRIG);

    while (1)
    {
        reg_tmp = jz_sfc_readl(SFC_SR);
        // printf("SFC_SR %x\n", reg_tmp);
        //fifo 中的空余数量大于等于阈值（32字节），表示可以写
        if (reg_tmp & TRAN_REQ)
        {
            //清除状态
            jz_sfc_writel(CLR_TREQ, SFC_SCR);

            //若要写的长度大于32字节则只写32字节，否则写剩余的内容
            if ((len - tmp_len) > THRESHOLD)
                fifo_num = THRESHOLD;
            else
            {
                fifo_num = len - tmp_len;
            }

            for (i = 0; i < fifo_num; i++)
            {
                //每次写4字节，写32个
                printf("data:%d,cnt:%d\n",*data,tmp_len);
                jz_sfc_writel(*data, SFC_RM_DR);
                data++;
                tmp_len++;
            }
        }
        if (tmp_len == len)
            break;
    }
    debug();
    sfc_dump_reg();

    reg_tmp = jz_sfc_readl(SFC_SR);
    while (!(reg_tmp & END))
    {
        reg_tmp = jz_sfc_readl(SFC_SR);
    }
    debug();

    if ((jz_sfc_readl(SFC_SR)) & END)
        jz_sfc_writel(CLR_END, SFC_SCR);
    debug();

    return 0;
}

static int sfc_read_data(unsigned int *data, unsigned int length)
{
    unsigned int tmp_len = 0;
    unsigned int fifo_num = 0;
    unsigned int i;
    unsigned int reg_tmp = 0;
    unsigned int len = (length + 3) / 4;
    debug();
    // jz_sfc_writel(TRIG_START, SFC_TRIG);
    while (1)
    {
        reg_tmp = jz_sfc_readl(SFC_SR);
        if (reg_tmp & RECE_REQ)
        {
            jz_sfc_writel(CLR_RREQ, SFC_SCR);
            if ((len - tmp_len) > THRESHOLD)
                fifo_num = THRESHOLD;
            else
            {
                fifo_num = len - tmp_len;
            }

            for (i = 0; i < fifo_num; i++)
            {
                *data = jz_sfc_readl(SFC_RM_DR);
                printf("data : %x\n", *data);
                data++;
                tmp_len++;
            }
        }
        if (tmp_len == len)
            break;
    }
    debug();
    reg_tmp = jz_sfc_readl(SFC_SR);
    printf("SFC_SR %x\n", reg_tmp);
    while (!(reg_tmp & END))
    {
        reg_tmp = jz_sfc_readl(SFC_SR);
    }
    debug();
    if ((jz_sfc_readl(SFC_SR)) & END)
        jz_sfc_writel(CLR_END, SFC_SCR);

    return 0;
}


int ssi_init(void)
{
    unsigned int tmp, i;

#ifdef CONFIG_SPI_QUAD
    sfc_quad_mode = 1;
#endif

    // tmp = THRESHOLD << GLB_THRESHOLD_OFFSET; //set FIFO threshold = 31. (unit: word)
    // jz_sfc_writel(tmp, SFC_GLB);

    // tmp = DEV_CONF_CEDL | DEV_CONF_HOLDDL | DEV_CONF_WPDL;
    // jz_sfc_writel(tmp, SFC_DEV_CONF);

    // /* low power consumption */
    // jz_sfc_writel(0, SFC_CGE); //CLK EN

    tmp = jz_sfc_readl(SFC_GLB);
    // tmp &= ~(GLB_TRAN_DIR | GLB_OP_MODE); // TRAN_DIR 传输方向   OP_MODE DMA/PIO mode 表示从spiflash中读，slave模式
    tmp &= ~GLB_OP_MODE; // TRAN_DIR 传输方向   OP_MODE DMA/PIO mode 表示写数据到从机，slave模式

    tmp &= ~(1<<14 | 1<< 15);
    tmp &= ~GLB_WP_EN; // T40 modify
    // tmp |= GLB_WP_EN;
    jz_sfc_writel(tmp, SFC_GLB);

    tmp = jz_sfc_readl(SFC_DEV_CONF);
    tmp &= ~(DEV_CONF_CMD_TYPE | DEV_CONF_CPHA | DEV_CONF_CPOL | DEV_CONF_SMP_DELAY_MSK |
             DEV_CONF_THOLD_MSK | DEV_CONF_TSETUP_MSK | DEV_CONF_TSH_MSK);
    /*T40 ADD STA_ENDIAN_MSB*/
    // tmp |= (DEV_CONF_CEDL | DEV_CONF_HOLDDL | DEV_CONF_WPDL | 1 << DEV_CONF_SMP_DELAY_OFFSET | STA_ENDIAN_MSB);
    tmp |= (DEV_CONF_CEDL | DEV_CONF_HOLDDL | DEV_CONF_WPDL /* | DEV_CONF_CPOL */);
    tmp |= (0 << 31) | (1 << 3) | (1 << 4); //bit31: 0:LSB 1:MSB
#ifdef CONFIG_SPI_QUAD
    tmp |= DEV_CONF_SMP_DELAY_135 << 16; // T41 SFC reg function
    tmp |= 0x2 << 5;                     // T41 SFC reg function
#endif
    jz_sfc_writel(tmp, SFC_DEV_CONF);

    for (i = 0; i < 6; i++)
    {
        if(0 == i){
            jz_sfc_writel(jz_sfc_readl(SFC_TRAN_CONF(i)) & (~(TRAN_CONF0_PHASE_FORMAT)), SFC_TRAN_CONF(i));
            jz_sfc_writel(jz_sfc_readl(SFC_TRAN_CONF1(i)) & (~(TRAN_CONF1_TRAN_MODE_MSK)), SFC_TRAN_CONF1(i)); // tran mode
            jz_sfc_writel(((1<<18) |(2 << 16) | (5<<4)), SFC_TRAN_CONF1(i));//MSB
            // jz_sfc_writel(((0<<18) |(2 << 16) | (5<<4)), SFC_TRAN_CONF1(i));    //LSB
        } else {
            jz_sfc_writel(0, SFC_TRAN_CONF(i));
            jz_sfc_writel(0, SFC_TRAN_CONF1(i)); // tran mode
        }

    }

    // jz_sfc_writel((CLR_END | CLR_TREQ | CLR_RREQ | CLR_OVER | CLR_UNDR), SFC_INTC);
    jz_sfc_writel(0, SFC_INTC);

    //jz_sfc_writel(0, SFC_CGE);
    //jz_sfc_writel((1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5), SFC_CGE);
    jz_sfc_writel(0x3f, SFC_CGE);

    tmp = jz_sfc_readl(SFC_GLB);
    tmp &= ~(GLB_THRESHOLD_MSK);
    // tmp |= 0x4 << 7;//4.22 默认设置4
    tmp &= ~(GLB_BURST_MD_MSK); // T40 ADD
    tmp |= (THRESHOLD << GLB_THRESHOLD_OFFSET);
    jz_sfc_writel(tmp, SFC_GLB);

    tmp = jz_sfc_readl(SFC_GLB1);
    tmp |= 1 << 3;
    jz_sfc_writel(tmp, SFC_GLB1);
    return 0;
}

int read_id()
{
    unsigned char cmd[4], len = 1;
    // unsigned char tmp = 0,data[128] = {0};
    unsigned int dat = 0;

    cmd[0] = 0x05;
    debug();
    sfc_send_cmd(&cmd[0], len, 0, 0, 0, 1, 0);
    usleep(25000);
    debug();

    sfc_read_data(&dat, len);
    debug();
    dat &= 0xff;
	printf("0x%02x\n", dat);

    // for (tmp = 0; tmp < len; tmp++)
    // {
    //     printf("%x  ", data[tmp]);
    // }

    printf("\n");
    return 0;
}

static int sfc_gpio_init(void)
{
#if 0
    system("devmem 0x10012018 32 0xf87ff800");//gpioc,中断清除寄存器,pc口这里对应的是ssi_slv
    system("devmem 0x10012028 32 0xf87ff800");
    system("devmem 0x10012038 32 0xf87ff800");
    system("devmem 0x10012048 32 0xf87ff800");
#else
    system("devmem 0x10012018 32 0");//gpioc,中断清除寄存器,pc口这里对应的是ssi_slv
    system("devmem 0x10012028 32 0x8180003f");
    system("devmem 0x10012038 32 0x9f8000ff");
    system("devmem 0x10012048 32 0");
#endif
    return 0;
}


#define DATA_LEN 128
int main(int argc, char *argv[])
{
    int ret = 0;
    unsigned char cmd[4];
    unsigned int data[DATA_LEN] = {0};

    cmd[0] = 0x66;
    cmd[1] = 0;//0x77;

    for(ret = 0; ret < DATA_LEN; ret++)
    {
        data[ret] = ret;
    }

    ret = set_base_addr();
    if (ret)
    {
        printf("set_base_addr error!\n");
        return -1;
    }

    debug();
    // sfc_gpio_init(); //gpio的初始化，先放到设备树中

    ssi_init();
    sfc_dump_reg();
    // ssi_mastere_init();
    debug();

    // read_id();
    // sfc_send_cmd(&cmd[0], 0, 0, 0, 0, 0, 1);


    sfc_send_cmd(&cmd[1], DATA_LEN, 0, 0, 0, 1, 1);
    debug();
    sfc_write_data(data, DATA_LEN);
    debug();

    // sfc_send_cmd(&cmd[1], DATA_LEN, 0, 0, 0, 1, 0);
    // sfc_read_data(&data, DATA_LEN);

    return 0;
}
