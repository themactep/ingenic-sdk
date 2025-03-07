#ifndef __JZMXU_CORE_TYPES_C_H__
#define __JZMXU_CORE_TYPES_C_H__
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <stdint.h>
#include <math.h>

typedef signed char schar;
typedef unsigned char uchar;

#define MXU_PI   3.1415926535897932384626433832795

inline  int  mxuRound( double value )
{
    double intpart, fractpart;
    fractpart = modf(value, &intpart);
    if ((fabs(fractpart) != 0.5) || ((((int)intpart) % 2) != 0))
        return (int)(value + (value >= 0 ? 0.5 : -0.5));
    else
        return (int)intpart;
}

inline  int  mxuFloor( double value )
{
    int i = mxuRound(value);
    float diff = (float)(value - i);
    return i - (diff < 0);
}


inline  int  mxuCeil( double value )
{
    int i = mxuRound(value);
    float diff = (float)(i - value);
    return i + (diff < 0);
}

#define MXU_CN_MAX     512
#define MXU_CN_SHIFT   3
#define MXU_DEPTH_MAX  (1 << MXU_CN_SHIFT)

#define MXU_8U   0
#define MXU_8S   1
#define MXU_16U  2
#define MXU_16S  3
#define MXU_32S  4
#define MXU_32F  5
#define MXU_64F  6
#define MXU_USRTYPE1 7

#define MXU_MAT_DEPTH_MASK       (MXU_DEPTH_MAX - 1)
#define MXU_MAT_DEPTH(flags)     ((flags) & MXU_MAT_DEPTH_MASK)

#define MXU_MAKETYPE(depth,cn) (MXU_MAT_DEPTH(depth) + (((cn)-1) << MXU_CN_SHIFT))
#define MXU_MAKE_TYPE MXU_MAKETYPE

#define MXU_8UC1 MXU_MAKETYPE(MXU_8U,1)
#define MXU_8UC2 MXU_MAKETYPE(MXU_8U,2)
#define MXU_8UC3 MXU_MAKETYPE(MXU_8U,3)
#define MXU_8UC4 MXU_MAKETYPE(MXU_8U,4)
#define MXU_8UC(n) MXU_MAKETYPE(MXU_8U,(n))

#define MXU_8SC1 MXU_MAKETYPE(MXU_8S,1)
#define MXU_8SC2 MXU_MAKETYPE(MXU_8S,2)
#define MXU_8SC3 MXU_MAKETYPE(MXU_8S,3)
#define MXU_8SC4 MXU_MAKETYPE(MXU_8S,4)
#define MXU_8SC(n) MXU_MAKETYPE(MXU_8S,(n))

#define MXU_16UC1 MXU_MAKETYPE(MXU_16U,1)
#define MXU_16UC2 MXU_MAKETYPE(MXU_16U,2)
#define MXU_16UC3 MXU_MAKETYPE(MXU_16U,3)
#define MXU_16UC4 MXU_MAKETYPE(MXU_16U,4)
#define MXU_16UC(n) MXU_MAKETYPE(MXU_16U,(n))

#define MXU_16SC1 MXU_MAKETYPE(MXU_16S,1)
#define MXU_16SC2 MXU_MAKETYPE(MXU_16S,2)
#define MXU_16SC3 MXU_MAKETYPE(MXU_16S,3)
#define MXU_16SC4 MXU_MAKETYPE(MXU_16S,4)
#define MXU_16SC(n) MXU_MAKETYPE(MXU_16S,(n))

#define MXU_32SC1 MXU_MAKETYPE(MXU_32S,1)
#define MXU_32SC2 MXU_MAKETYPE(MXU_32S,2)
#define MXU_32SC3 MXU_MAKETYPE(MXU_32S,3)
#define MXU_32SC4 MXU_MAKETYPE(MXU_32S,4)
#define MXU_32SC(n) MXU_MAKETYPE(MXU_32S,(n))

#define MXU_32FC1 MXU_MAKETYPE(MXU_32F,1)
#define MXU_32FC2 MXU_MAKETYPE(MXU_32F,2)
#define MXU_32FC3 MXU_MAKETYPE(MXU_32F,3)
#define MXU_32FC4 MXU_MAKETYPE(MXU_32F,4)
#define MXU_32FC(n) MXU_MAKETYPE(MXU_32F,(n))

#define MXU_64FC1 MXU_MAKETYPE(MXU_64F,1)
#define MXU_64FC2 MXU_MAKETYPE(MXU_64F,2)
#define MXU_64FC3 MXU_MAKETYPE(MXU_64F,3)
#define MXU_64FC4 MXU_MAKETYPE(MXU_64F,4)
#define MXU_64FC(n) MXU_MAKETYPE(MXU_64F,(n))

#define MXU_AUTO_STEP  0x7fffffff
#define MXU_WHOLE_ARR  mxuSlice( 0, 0x3fffffff )

#define MXU_MAT_CN_MASK          ((MXU_CN_MAX - 1) << MXU_CN_SHIFT)
#define MXU_MAT_CN(flags)        ((((flags) & MXU_MAT_CN_MASK) >> MXU_CN_SHIFT) + 1)
#define MXU_MAT_TYPE_MASK        (MXU_DEPTH_MAX*MXU_CN_MAX - 1)
#define MXU_MAT_TYPE(flags)      ((flags) & MXU_MAT_TYPE_MASK)
#define MXU_MAT_CONT_FLAG_SHIFT  14
#define MXU_MAT_CONT_FLAG        (1 << MXU_MAT_CONT_FLAG_SHIFT)
#define MXU_IS_MAT_CONT(flags)   ((flags) & MXU_MAT_CONT_FLAG)
#define MXU_IS_CONT_MAT          MXU_IS_MAT_CONT
#define MXU_SUBMAT_FLAG_SHIFT    15
#define MXU_SUBMAT_FLAG          (1 << MXU_SUBMAT_FLAG_SHIFT)
#define MXU_IS_SUBMAT(flags)     ((flags) & MXU_MAT_SUBMAT_FLAG)

#define MXU_MAGIC_MASK       0xFFFF0000
#define MXU_MAT_MAGIC_VAL    0x42420000
#define MXU_TYPE_NAME_MAT    "jzmxu-matrix"

#define MXU_ELEM_SIZE(type) \
    (MXU_MAT_CN(type) << ((((sizeof(size_t)/4+1)*16384|0x3a50) >> MXU_MAT_DEPTH(type)*2) & 3))


#endif /*__JZMXU_CORE_TYPE_C_H__*/
