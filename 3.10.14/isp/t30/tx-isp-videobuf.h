#ifndef __TX_ISP_VIDEOBUF_H__
#define __TX_ISP_VIDEOBUF_H__

void isp_mem_init(void);
unsigned int isp_malloc_buffer(unsigned int size);
void isp_free_buffer(unsigned int addr);

#endif/* __TX_ISP_VIDEOBUF_H__ */
