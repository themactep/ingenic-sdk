#ifndef __DDR_MEM_H__
#define __DDR_MEM_H__
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

extern int ddr_memory_init (void *mp, unsigned int mp_size);

extern void ddr_memory_deinit (void);

extern void *ddr_malloc (unsigned int size);

extern void ddr_free (void *addr);

extern void *ddr_calloc (unsigned int size, unsigned int n);

extern void *ddr_realloc (void *addr, unsigned int size);

extern void *ddr_memalign (unsigned int align, unsigned int size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
