#ifndef __ORAM_MEM_H__
#define __ORAM_MEM_H__
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

extern int oram_memory_init (void *mp, unsigned int mp_size);

extern void oram_memory_deinit (void);

extern void *oram_malloc (unsigned int size);

extern void oram_free (void *addr);

extern void *oram_calloc (unsigned int size, unsigned int n);

extern void *oram_realloc (void *addr, unsigned int size);

extern void *oram_memalign (unsigned int align, unsigned int size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
