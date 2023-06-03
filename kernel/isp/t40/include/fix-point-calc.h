#ifndef __FIX_POINT_CALC_H__
#define __FIX_POINT_CALC_H__

#include <linux/types.h>

extern uint16_t label_line2log[216];
extern uint16_t value_line2log[216];
extern uint16_t value_log2line[122];

uint64_t fix_point_add(uint32_t pointpos, uint64_t a, uint64_t b);
uint64_t fix_point_sub(uint32_t pointpos, uint64_t a, uint64_t b);
uint64_t fix_point_mult2(uint32_t pointpos, uint64_t a, uint64_t b);
uint64_t fix_point_mult3(uint32_t pointpos, uint64_t a, uint64_t b, uint64_t c);
uint64_t fix_point_div(uint32_t pointpos, uint64_t a, uint64_t b);

uint32_t fix_point_add_32(uint32_t pointpos, uint32_t a, uint32_t b);
uint32_t fix_point_sub_32(uint32_t pointpos, uint32_t a, uint32_t b);
uint32_t fix_point_mult2_32(uint32_t pointpos, uint32_t a, uint32_t b);
uint32_t fix_point_mult3_32(uint32_t pointpos, uint32_t a, uint32_t b, uint32_t c);
uint32_t fix_point_div_32(uint32_t pointpos, uint32_t a, uint32_t b);

uint64_t fix_point_add_64(uint32_t pointpos, uint64_t a, uint64_t b);
uint64_t fix_point_sub_64(uint32_t pointpos, uint64_t a, uint64_t b);
uint64_t fix_point_mult2_64(uint32_t pointpos, uint64_t a, uint64_t b);
uint64_t fix_point_mult3_64(uint32_t pointpos, uint64_t a, uint64_t b, uint64_t c);
uint64_t fix_point_div_64(uint32_t pointpos, uint64_t a, uint64_t b);

uint32_t tisp_math_exp2(uint32_t val, const unsigned char shift_in, const unsigned char shift_out);
uint32_t tisp_log2_fixed_to_fixed(const uint32_t val, const int in_fix_point, const uint8_t out_fix_point);
uint32_t tisp_log2_fixed_to_fixed_64(uint64_t val, int32_t in_fix_point, uint8_t out_fix_point);

uint32_t tisp_simple_intp(uint32_t x_int, uint32_t x_fra, uint32_t *y_array);
uint8_t tisp_simple_intp_int8(uint32_t x_int, uint32_t x_fra, uint8_t *y_array);
uint16_t tisp_simple_intp_int16(uint32_t x_int, uint32_t x_fra, uint16_t *y_array);

#endif
