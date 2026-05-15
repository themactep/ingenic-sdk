#ifndef __MXU3_H__
#define __MXU3_H__

#define VR0      0
#define VR1      1
#define VR2      2
#define VR3      3
#define VR4      4
#define VR5      5
#define VR6      6
#define VR7      7
#define VR8      8
#define VR9      9
#define VR10     10
#define VR11     11
#define VR12     12
#define VR13     13
#define VR14     14
#define VR15     15
#define VR16     16
#define VR17     17
#define VR18     18
#define VR19     19
#define VR20     20
#define VR21     21
#define VR22     22
#define VR23     23
#define VR24     24
#define VR25     25
#define VR26     26
#define VR27     27
#define VR28     28
#define VR29     29
#define VR30     30
#define VR31     31

#define VS0      0
#define VS1      1
#define VS2      2
#define VS3      3

#define VWR0     0
#define VWR1     1
#define VWR2     2
#define VWR3     3
#define VWR4     4
#define VWR5     5
#define VWR6     6
#define VWR7     7
#define VWR8     8
#define VWR9     9
#define VWR10    10
#define VWR11    11
#define VWR12    12
#define VWR13    13
#define VWR14    14
#define VWR15    15
#define VWR16    16
#define VWR17    17
#define VWR18    18
#define VWR19    19
#define VWR20    20
#define VWR21    21
#define VWR22    22
#define VWR23    23
#define VWR24    24
#define VWR25    25
#define VWR26    26
#define VWR27    27
#define VWR28    28
#define VWR29    29
#define VWR30    30
#define VWR31    31

//shift
#define SLLI(fmt, vrd, vrs, imm)                                        \
    do{                                                                 \
        __asm__ __volatile__("mxu3_slli"#fmt" %0, %1, %2"               \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((imm))        \
            );                                                          \
    }while(0)
#define SLL(fmt, vrd, vrs, vrp)                                 \
    do{                                                         \
        __asm__ __volatile__("mxu3_sll"#fmt" %0, %1, %2"        \
                             :                                  \
                             :"i"((vrd)), "i"((vrs)),"i"((vrp)) \
            );                                                  \
    }while(0)

#define SRA(fmt, vrd, vrs, vrp)                                 \
    do{                                                         \
        __asm__ __volatile__("mxu3_sra"#fmt" %0, %1, %2"        \
                             :                                  \
                             :"i"((vrd)), "i"((vrs)),"i"((vrp)) \
            );                                                  \
    }while(0)

#define SRAI(fmt, vrd, vrs, imm)                                        \
    do{                                                                 \
        __asm__ __volatile__("mxu3_srai"#fmt" %0, %1, %2"               \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((imm))        \
            );                                                          \
    }while(0)

#define SRAR(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_srar"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)), "i"((vrs)),"i"((vrp)) \
            );                                                  \
    }while(0)

#define SRARI(fmt, vrd, vrs, imm)                                       \
    do{                                                                 \
        __asm__ __volatile__("mxu3_srari"#fmt" %0, %1, %2"              \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((imm))        \
            );                                                          \
    }while(0)

#define SRL(fmt, vrd, vrs, vrp)                                 \
    do{                                                         \
        __asm__ __volatile__("mxu3_srl"#fmt" %0, %1, %2"        \
                             :                                  \
                             :"i"((vrd)), "i"((vrs)),"i"((vrp)) \
            );                                                  \
    }while(0)

#define SRLI(fmt, vrd, vrs, imm)                                        \
    do{                                                                 \
        __asm__ __volatile__("mxu3_srli"#fmt" %0, %1, %2"               \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((imm))        \
            );                                                          \
    }while(0)

#define SRLR(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_srlr"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)), "i"((vrs)),"i"((vrp)) \
            );                                                  \
    }while(0)

#define SRLRI(fmt, vrd, vrs, imm)                                       \
    do{                                                                 \
        __asm__ __volatile__("mxu3_srlri"#fmt" %0, %1, %2"              \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((imm))        \
            );                                                          \
    }while(0)

//load/store
#define LU(fmt, vrd, n, base)                                                  \
    do{                                                                        \
        __asm__ __volatile__("mxu3_lu"#fmt" %1[%2], %0"                        \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n))                             \
                             :"memory");                                       \
    }while(0)

#define LA(fmt, vrd, n, base, offset)                                          \
    do{                                                                        \
        __asm__ __volatile__("mxu3_la"#fmt" %0[%1], %3(%2)"                    \
                             :                                                 \
                             :"i"((vrd)), "i"((n)), "d"((base)), "i"((offset)) \
                             :"memory");                                       \
    }while(0)

#define SU(fmt, vrp, n, base)                                                  \
    do{                                                                        \
        __asm__ __volatile__("mxu3_su"#fmt" %1[%2], %0"                        \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n))                             \
                             :"memory");                                       \
    }while(0)

#define SA(fmt, vrd, n, base, offset)                                          \
    do{                                                                        \
        __asm__ __volatile__("mxu3_sa"#fmt" %0[%1], %3(%2)"                    \
                             :                                                 \
                             :"i"((vrd)), "i"((n)), "d"((base)), "i"((offset)) \
                             :"memory");                                       \
    }while(0)

#define LUO(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_luo"#x""#s_fmt" %1[%2], %3, %0"             \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p))                   \
                             :"memory");                                       \
    }while(0)

#define LAO(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_lao"#x""#s_fmt" %0[%1], %3, %2"             \
                             :                                                 \
                             :"i"((vrd)), "i"((n)),"d"((base)), "i"((p))       \
                             :"memory");                                       \
    }while(0)

#define SUO(x, g_fmt, vrp, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_suo"#x""#g_fmt" %1[%2], %3, %0"             \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p))                   \
                             :"memory");                                       \
    }while(0)

#define SAO(x, g_fmt, vrp, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_sao"#x""#g_fmt" %0[%1], %3, %2"             \
                             :                                                 \
                             :"i"((vrp)), "i"((n)),"d"((base)), "i"((p))       \
                             :"memory");                                       \
    }while(0)

#define LUW(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_luw"#x""#s_fmt" %1[%2], %3, %0"             \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p))                   \
                             :"memory");                                       \
    }while(0)

#define LAW(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_law"#x""#s_fmt" %0[%1], %3, %2"             \
                             :                                                 \
                             :"i"((vrd)), "i"((n)),"d"((base)), "i"((p))       \
                             :"memory");                                       \
    }while(0)

#define LUD(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_lud"#x""#s_fmt" %1[%2], %3, %0"             \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p))                   \
                             :"memory");                                       \
    }while(0)

#define LAD(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_lad"#x""#s_fmt" %0[%1], %3, %2"             \
                             :                                                 \
                             :"i"((vrd)), "i"((n)),"d"((base)), "i"((p))       \
                             :"memory");                                       \
    }while(0)

#define SUD(x, g_fmt, vrp, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_sud"#x""#g_fmt" %1[%2], %3, %0"             \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p))                   \
                             :"memory");                                       \
    }while(0)

#define SAD(x, g_fmt, vrp, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_sad"#x""#g_fmt" %0[%1], %3, %2"             \
                             :                                                 \
                             :"i"((vrp)), "i"((n)),"d"((base)), "i"((p))       \
                             :"memory");                                       \
    }while(0)

#define LUQ(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_luq"#x""#s_fmt" %1[%2], %3, %0"             \
                             :"+r"((base))                                     \
                             :"i"((vrd)),"i"((n)),"i"((p))                     \
                             :"memory");                                       \
    }while(0)

#define LAQ(x, s_fmt, vrd, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_laq"#x""#s_fmt" %0[%1], %3, %2"             \
                             :                                                 \
                             :"i"((vrd)),"i"((n)),"d"((base)),"i"((p))         \
                             :"memory");                                       \
    }while(0)

#define SUQ(x, g_fmt, vrp, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_suq"#x""#g_fmt" %1[%2], %3, %0"             \
                             :"+r"((base))                                     \
                             :"i"((vrp)),"i"((n)), "i"((p))                    \
                             :"memory");                                       \
    }while(0)

#define SAQ(x, g_fmt, vrp, n, base ,p)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_saq"#x""#g_fmt" %0[%1], %3, %2"             \
                             :                                                 \
                             :"i"((vrp)),"i"((n)),"d"((base)),"i"((p))         \
                             :"memory");                                       \
    }while(0)

// load/store macro instr
#define LU_ADD(fmt, vrd, n, base, inc)                                         \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lu"#fmt" %1[%2], %0 \n"                     \
                             "addu %0, %0, %3 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "r"((inc))                 \
                             :"memory");                                       \
    }while(0)

#define LA_ADD(fmt, vrd, n, base, offset, inc)                                 \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_la"#fmt" %1[%2], %3(%0) \n"                 \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((offset)), "r"((inc))  \
                             :"memory");                                       \
    }while(0)

#define SU_ADD(fmt, vrp, n, base, inc)                                         \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_su"#fmt" %1[%2], %0 \n"                     \
                             "addu %0, %0, %3 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "r"((inc))                 \
                             :"memory");                                       \
    }while(0)

#define SA_ADD(fmt, vrd, n, base, offset, inc)                                 \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sa"#fmt" %1[%2], %3(%0) \n"                 \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((offset)), "r"((inc))  \
                             :"memory");                                       \
    }while(0)

#define LUO_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_luo"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAO_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lao"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUO_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_suo"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAO_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sao"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LUW_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_luw"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAW_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_law"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUW_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_suw"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAW_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_saw"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LUD_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lud"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAD_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lad"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUD_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sud"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAD_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sad"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LUQ_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_luq"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAQ_ADD(x, s_fmt, vrd, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_laq"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUQ_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_suq"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAQ_ADD(x, g_fmt, vrp, n, base , p, inc)                               \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_saq"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addu %0, %0, %4 \n"                              \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LU_ADDI(fmt, vrd, n, base, inc)                                        \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lu"#fmt" %1[%2], %0 \n"                     \
                             "addiu %0, %0, %3 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((inc))                 \
                             :"memory");                                       \
    }while(0)

#define LA_ADDI(fmt, vrd, n, base, offset, inc)                                \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_la"#fmt" %1[%2], %3(%0) \n"                 \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((offset)), "i"((inc))  \
                             :"memory");                                       \
    }while(0)

#define SU_ADDI(fmt, vrp, n, base, inc)                                        \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_su"#fmt" %1[%2], %0 \n"                     \
                             "addiu %0, %0, %3 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((inc))                 \
                             :"memory");                                       \
    }while(0)

#define SA_ADDI(fmt, vrd, n, base, offset, inc)                                \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sa"#fmt" %1[%2], %3(%0) \n"                 \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((offset)), "i"((inc))  \
                             :"memory");                                       \
    }while(0)

#define LUO_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_luo"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "r"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAO_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lao"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUO_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_suo"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAO_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sao"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LUW_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_luw"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAW_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_law"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUW_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_suw"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAW_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_saw"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LUD_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lud"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAD_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_lad"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUD_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sud"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAD_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_sad"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LUQ_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_luq"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define LAQ_ADDI(x, s_fmt, vrd, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_laq"#x""#s_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrd)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SUQ_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_suq"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

#define SAQ_ADDI(x, g_fmt, vrp, n, base , p, inc)                              \
    do{                                                                        \
        __asm__ __volatile__(".set noreorder \n"                               \
                             "mxu3_saq"#x""#g_fmt" %1[%2], %3, %0 \n"          \
                             "addiu %0, %0, %4 \n"                             \
                             ".set reorder \n"                                 \
                             :"+r"((base))                                     \
                             :"i"((vrp)), "i"((n)), "i"((p)), "i"((inc))       \
                             :"memory");                                       \
    }while(0)

//shuffle
#define GT_FMT(fmt, vrd, vrp)                                                  \
    do{                                                                        \
        __asm__ __volatile__("mxu3_gt"#fmt" %0, %1"                            \
                             :                                                 \
                             :"i"((vrd)),"i"((vrp))                            \
            );                                                                 \
    }while(0)

#define GT_N_FMT(n, fmt, vrd, m, vrp, pos)                                     \
    do{                                                                        \
        __asm__ __volatile__("mxu3_gt"#n""#fmt" %0[%1], %2, %3"                \
                             :                                                 \
                             :"i"((vrd)),"i"((m)),"i"((vrp)),"i"((pos))        \
            );                                                                 \
    }while(0)

#define EXT_FMT_P(s, fmt, p, vrd, vrp)                                         \
    do{                                                                        \
        __asm__ __volatile__("mxu3_ext"#s""#fmt""#p" %0, %1"                   \
                             :                                                 \
                             :"i"((vrd)),"i"((vrp))                            \
            );                                                                 \
    }while(0)

#define EXT_FMT_P_ZP(fmt, p, zp, vrd, vrp, w)                                  \
    do{                                                                        \
        __asm__ __volatile__("mxu3_extu"#fmt""#p""#zp" %0, %1, %2"             \
                             :                                                 \
                             :"i"((vrd)),"i"((vrp)),"i"((w))                   \
            );                                                                 \
    }while(0)

#define EXTU3BW(vrd, vrp)                                                      \
    do{                                                                        \
        __asm__ __volatile__("mxu3_extu3bw %0, %1"                             \
                             :                                                 \
                             :"i"((vrd)),"i"((vrp))                            \
            );                                                                 \
    }while(0)

#define REPI(fmt, vrd, vrp, n)                                                 \
    do{                                                                        \
        __asm__ __volatile__("mxu3_repi"#fmt" %0, %1[%2]"                      \
                             :                                                 \
                             :"i"((vrd)),"i"((vrp)),"i"((n))                   \
            );                                                                 \
    }while(0)

#define MOVW(vrd, m, vrp, n)                                                   \
    do{                                                                        \
        __asm__ __volatile__("mxu3_movw %0[%1], %2[%3]"                        \
                             :                                                 \
                             :"i"((vrd)),"i"((m)),"i"((vrp)),"i"((n))          \
            );                                                                 \
    }while(0)

#define MOV(fmt, vrd, m, vrp, n)                                               \
    do{                                                                        \
        __asm__ __volatile__("mxu3_mov"#fmt" %0[%1], %2[%3]"                   \
                             :                                                 \
                             :"i"((vrd)),"i"((m)),"i"((vrp)),"i"((n))          \
            );                                                                 \
    }while(0)

#define SHUF(fmt, ngroup, vrd, vrp)                             \
    do{                                                         \
        __asm__ __volatile__("mxu3_shuf"#fmt""#ngroup" %0, %1"  \
                             :                                  \
                             :"i"((vrd)),"i"((vrp))             \
            );                                                  \
    }while(0)

#define GSHUFW(vrd, vrs, vrp, m)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_gshufw %0, %1, %2, %3"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp),"i"((m)))  \
            );                                                  \
    }while(0)

#define GSHUFWB(vrd, vrs, vrp)                                  \
    do{                                                         \
        __asm__ __volatile__("mxu3_gshufwb %0, %1, %2"          \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define GSHUFB(vrd, vrs, vrp)                                   \
    do{                                                         \
        __asm__ __volatile__("mxu3_gshufb %0, %1, %2"           \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define ILVE(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_ilve"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define ILVO(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_ilvo"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define BSHLI(vrd, vrp, imm)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_bshli %0, %1, %2"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrp)),"i"((imm))  \
            );                                                  \
    }while(0)

#define BSHRI(vrd, vrp, imm)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_bshri %0, %1, %2"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrp)),"i"((imm))  \
            );                                                  \
    }while(0)

#define BSHR(vrd, vrs, vrp)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_bshr %0, %1, %2"             \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define BSHL(vrd, vrs, vrp)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_bshl %0, %1, %2"             \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

//integer arithmetic==============================
#define ADD(fmt, vrd, vrs, vrp)                                         \
    do{                                                                 \
        __asm__ __volatile__("mxu3_add"#fmt" %0, %1, %2"                \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))        \
            );                                                          \
    }while(0)

#define SUB(fmt, vrd, vrs, vrp)                                         \
    do{                                                                 \
        __asm__ __volatile__("mxu3_sub"#fmt" %0, %1, %2"                \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))        \
            );                                                          \
    }while(0)

#define MUL(fmt, vrd, vrs, vrp)                                         \
    do{                                                                 \
        __asm__ __volatile__("mxu3_mul"#fmt" %0, %1, %2"                \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))        \
            );                                                          \
    }while(0)

#define WADDS(fmt, p, vrd, vrs, vrp)                                    \
    do{                                                                 \
        __asm__ __volatile__("mxu3_wadds"#fmt""#p" %0, %1, %2"          \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))        \
            );                                                          \
    }while(0)

#define WADDU(fmt, p, vrd, vrs, vrp)                                    \
    do{                                                                 \
        __asm__ __volatile__("mxu3_waddu"#fmt""#p" %0, %1, %2"          \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))        \
            );                                                          \
    }while(0)

#define WSUBS(fmt, p, vrd, vrs, vrp)                                    \
    do{                                                                 \
        __asm__ __volatile__("mxu3_wsubs"#fmt""#p" %0, %1, %2"          \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))        \
            );                                                          \
    }while(0)

#define WSUBU(fmt, p, vrd, vrs, vrp)                                    \
    do{                                                                 \
        __asm__ __volatile__("mxu3_wsubu"#fmt""#p" %0, %1, %2"          \
                             :                                          \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))        \
            );                                                          \
    }while(0)

#define SRSUM(seg, fmt, vsd, m, vrs)                            \
    do{                                                         \
        __asm__ __volatile__("mxu3_sr"#seg"sum"#fmt" %0[%1], %2"\
                             :                                  \
                             :"i"((vsd)),"i"((m)),"i"((vrs))    \
            );                                                  \
    }while(0)

#define ABS(fmt, vrd, vrs)                                      \
    do{                                                         \
        __asm__ __volatile__("mxu3_abs"#fmt" %0, %1"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrs))             \
            );                                                  \
    }while(0)

#define SMUL(fmt, p, vrd, vrs, vrp)                             \
    do{                                                         \
        __asm__ __volatile__("mxu3_smul"#fmt""#p" %0, %1, %2"   \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define UMUL(fmt, p, vrd, vrs, vrp)                             \
    do{                                                         \
        __asm__ __volatile__("mxu3_umul"#fmt""#p" %0, %1, %2"   \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define WSMUL(fmt, p, vrd, vrs, vrp)                            \
    do{                                                         \
        __asm__ __volatile__("mxu3_wsmul"#fmt""#p" %0, %1, %2"  \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define WUMUL(fmt, p, vrd, vrs, vrp)                            \
    do{                                                         \
        __asm__ __volatile__("mxu3_wumul"#fmt""#p" %0, %1, %2"  \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define MLAW(vsd, vrs, vrp)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_mlaw %0, %1, %2"             \
                             :                                  \
                             :"i"((vsd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define MLSW(vsd, vrs, vrp)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_mlsw %0, %1, %2"             \
                             :                                  \
                             :"i"((vsd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define SMLAH(p, vsd, vrs, vrp)                                 \
    do{                                                         \
        __asm__ __volatile__("mxu3_smlah"#p" %0, %1, %2"        \
                             :                                  \
                             :"i"((vsd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define SMLSH(p, vsd, vrs, vrp)                                 \
    do{                                                         \
        __asm__ __volatile__("mxu3_smlsh"#p" %0, %1, %2"        \
                             :                                  \
                             :"i"((vsd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define WSMLAH(p, vsd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_wsmlah"#p" %0, %1, %2"       \
                             :                                  \
                             :"i"((vsd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define WSMLSH(p, vsd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_wsmlsh"#p" %0, %1, %2"       \
                             :                                  \
                             :"i"((vsd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define MAXU(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_maxu"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define SRMAC(seg, fmt, vsd, m, vrs, vrp, n)                                   \
    do{                                                                        \
        __asm__ __volatile__("mxu3_sr"#seg"mac"#fmt" %0[%1], %2, %3[%4]"       \
                             :                                                 \
                             :"i"((vsd))"i"((m)),"i"((vrs))"i"((vrp)),"i"((n)) \
            );                                                                 \
    }while(0)

#define SMAC(seg, fmt, vsd, m, vrs, vrp)                                       \
    do{                                                                        \
        __asm__ __volatile__("mxu3_s"#seg"mac"#fmt" %0[%1], %2, %3"            \
                             :                                                 \
                             :"i"((vsd))"i"((m)),"i"((vrs))"i"((vrp))          \
            );                                                                 \
    }while(0)

#define MAXA(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_maxa"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define MAXS(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_maxs"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define MINA(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_mina"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define MINS(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_mins"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define MINU(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_minu"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define SATSS(fmt, tgt, vrd, vrs)                               \
    do{                                                         \
        __asm__ __volatile__("mxu3_satss"#fmt""#tgt" %0, %1"    \
                             :                                  \
                             :"i"((vrd)),"i"((vrs))             \
            );                                                  \
    }while(0)

#define SATSU(fmt, tgt, vrd, vrs)                               \
    do{                                                         \
        __asm__ __volatile__("mxu3_satsu"#fmt""#tgt" %0, %1"    \
                             :                                  \
                             :"i"((vrd)),"i"((vrs))             \
            );                                                  \
    }while(0)

#define SATUU(fmt, tgt, vrd, vrs)                               \
    do{                                                         \
        __asm__ __volatile__("mxu3_satuu"#fmt""#tgt" %0, %1"    \
                             :                                  \
                             :"i"((vrd)),"i"((vrs))             \
            );                                                  \
    }while(0)

#define TOC(fmt, vsd, vrs)                              \
    do{                                                 \
        __asm__ __volatile__("mxu3_toc"#fmt" %0, %1"    \
                             :                          \
                             :"i"((vsd)),"i"((vrs))     \
            );                                          \
    }while(0)

//bitwise===============
#define ANDV(vrd, vrs, vrp)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_andv %0, %1, %2"             \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define ANDNV(vrd, vrs, vrp)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_andnv %0, %1, %2"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define ANDIB(vrd, vrs, imm)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_andib %0, %1, %2"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((imm))  \
            );                                                  \
    }while(0)

#define ORV(vrd, vrs, vrp)                                      \
    do{                                                         \
        __asm__ __volatile__("mxu3_orv %0, %1, %2"              \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define ORNV(vrd, vrs, vrp)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_ornv %0, %1, %2"             \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define ORIB(vrd, vrs, imm)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_orib %0, %1, %2"             \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((imm))  \
            );                                                  \
    }while(0)

#define XORV(vrd, vrs, vrp)                                     \
    do{                                                         \
        __asm__ __volatile__("mxu3_xorv %0, %1, %2"             \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define XORNV(vrd, vrs, vrp)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_xornv %0, %1, %2"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define XORIB(vrd, vrs, imm)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_xorib %0, %1, %2"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((imm))  \
            );                                                  \
    }while(0)

#define BSELV(vrd, vrs, vrp)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_bselv %0, %1, %2"            \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

//register load and misc
#define MTCPUW(rd,vwrp)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_mtcpuw %0, %1"       \
                             :"+r"((rd))                \
                             :"i"((vwrp))               \
            );                                          \
    }while(0)

#define MFCPUW(vwrd,rs)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_mfcpuw %0, %1"       \
                             :                          \
                             :"i"((vwrd)), "r"((rs))    \
            );                                          \
    }while(0)

#define SUMZ(vsd)                               \
    do{                                         \
        __asm__ __volatile__("mxu3_sumz %0"     \
                             :                  \
                             :"i"((vsd))        \
            );                                  \
    }while(0)

#define MFSUMZ(vrd, vsd)                                \
    do{                                                 \
        __asm__ __volatile__("mxu3_mfsumz %0, %1"       \
                             :                          \
                             :"i"((vrd)),"i"((vsd))     \
            );                                          \
    }while(0)

#define MFSUM(vrd, vss)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_mfsum %0, %1"        \
                             :                          \
                             :"i"((vrd)),"i"((vss))     \
            );                                          \
    }while(0)

#define MTSUM(vsd, vrs)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_mtsum %0, %1"        \
                             :                          \
                             :"i"((vsd)),"i"((vrs))     \
            );                                          \
    }while(0)

#define LIW(vrd, imm)                                   \
    do{                                                 \
        __asm__ __volatile__("mxu3_liw %0, %1"          \
                             :                          \
                             :"i"((vrd)),"i"((imm))     \
            );                                          \
    }while(0)

#define LIWH(vrd, imm)                                  \
    do{                                                 \
        __asm__ __volatile__("mxu3_liwh %0, %1"         \
                             :                          \
                             :"i"((vrd)),"i"((imm))     \
            );                                          \
    }while(0)

#define LIH(vrd, imm)                                   \
    do{                                                 \
        __asm__ __volatile__("mxu3_lih %0, %1"          \
                             :                          \
                             :"i"((vrd)),"i"((imm))     \
                             :"memory");                \
    }while(0)

#define CMVW(vrd, vrs, vrp, imm)                                          \
    do{                                                                   \
        __asm__ __volatile__("mxu3_cmvw %0, %1, %2, %3"                   \
                             :                                            \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp)),"i"((imm)) \
            );                                                            \
    }while(0)

#define PMAP(fmt,vrd,vrs,vrp)                                   \
    do{                                                         \
        __asm__ __volatile__("mxu3_pmap"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

//other
#define GSHUFWH(vrd, vrs, vrp)                                  \
    do{                                                         \
        __asm__ __volatile__("mxu3_gshufwh %0, %1, %2"          \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define GSHUFH(vrd, vrs, vrp)                                   \
    do{                                                         \
        __asm__ __volatile__("mxu3_gshufh %0, %1, %2"           \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define EXTU(fmt, p, vrd, vrp, w)                               \
    do{                                                         \
        __asm__ __volatile__("mxu3_extu"#fmt""#p" %0, %1, %2"   \
                             :                                  \
                             :"i"((vrd)),"i"((vrp)),"i"((w))    \
            );                                                  \
    }while(0)

#define CLTS(fmt, vrd, vrs, vrp)                                \
    do{                                                         \
        __asm__ __volatile__("mxu3_clts"#fmt" %0, %1, %2"       \
                             :                                  \
                             :"i"((vrd)),"i"((vrs)),"i"((vrp))  \
            );                                                  \
    }while(0)

#define CLEZ(fmt, vrd, vrs)                                    \
    do{                                                         \
        __asm__ __volatile__("mxu3_clez"#fmt" %0, %1"           \
                             :                                  \
                             :"i"((vrd)),"i"((vrs))      \
            );                                                  \
    }while(0)

#define GATHER(small, fmt, vrd, vrp)                            \
    do{                                                         \
        __asm__ __volatile__("mxu3_gt"#small""#fmt" %0, %1"     \
                             :                                  \
                             :"i"((vrd)),"i"((vrp))             \
            );                                                  \
    }while(0)

#define ADDIW(vwrd, vwrp, imm)                                          \
    do{                                                                 \
        __asm__ __volatile__("mxu3_addiw %0, %1, %2"                    \
                             :                                          \
                             :"i"((vwrd)),"i"((vwrp)),"i"((imm))        \
            );                                                          \
    }while(0)

#define ADDRW(vwrd, vwrp)                                               \
    do{                                                                 \
        __asm__ __volatile__("mxu3_addrw %0, %1"                        \
                             :                                          \
                             :"i"((vwrd)),"i"((vwrp))                   \
            );                                                          \
    }while(0)

#define LIWR(vwrd, imm)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_liwr %0, %1"         \
                             :                          \
                             :"i"((vwrd)),"i"((imm))    \
            );                                          \
    }while(0)

//neural networks acelerate
#define NNRWR(vwrp, imm)                                \
    do{                                                 \
        __asm__ __volatile__("mxu3_nnrwr %0, %1"        \
                             :                          \
                             :"i"((vwrp)),"i"((imm))    \
            );                                          \
    }while(0)

#define NNRRD(vrd, imm)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_nnrrd %0, %1"        \
                             :                          \
                             :"i"((vrd)),"i"((imm))     \
            );                                          \
    }while(0)

#define NNDWR(vrp, imm)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_nndwr %0, %1"        \
                             :                          \
                             :"i"((vrp)),"i"((imm))     \
            );                                          \
    }while(0)

#define NNDRD(vrd, imm)                                 \
    do{                                                 \
        __asm__ __volatile__("mxu3_nndrd %0, %1"        \
                             :                          \
                             :"i"((vrd)),"i"((imm))     \
            );                                          \
    }while(0)

#define NNCMD(imm)                                      \
    do{                                                 \
        __asm__ __volatile__("mxu3_nncmd %0"            \
                             :                          \
                             :"i"((imm))                \
            );                                          \
    }while(0)

#define NNMAC(vwrp, imm)                                \
    do{                                                 \
        __asm__ __volatile__("mxu3_nnmac %0, %1"        \
                             :                          \
                             :"i"((vwrp)), "i"((imm))   \
            );                                          \
    }while(0)

//===============

//mxu3 floating point
#define FADDW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_faddw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FSUBW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fsubw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FMULW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fmulw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FCMULRW(vrd, vrs, vrp)                                      \
    do{                                                             \
        __asm__ __volatile__("mxu3_fcmulrw %0, %1, %2"              \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FCMULIW(vrd, vrs, vrp)                                      \
    do{                                                             \
        __asm__ __volatile__("mxu3_fcmuliw %0, %1, %2"              \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FCADDW(vrd, vrs, vrp)                                       \
    do{                                                             \
        __asm__ __volatile__("mxu3_fcaddw %0, %1, %2"               \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FXAS(fmt, vrd, vrp)                                         \
    do{                                                             \
        __asm__ __volatile__("mxu3_fxas"#fmt" %0, %1"               \
                             :                                      \
                             :"i"((vrd)), "i"((vrp))                \
            );                                                      \
    }while(0)

#define FMAXW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fmaxw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FMAXAW(vrd, vrs, vrp)                                       \
    do{                                                             \
        __asm__ __volatile__("mxu3_fmaxaw %0, %1, %2"               \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FMINW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fminw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FMINAW(vrd, vrs, vrp)                                       \
    do{                                                             \
        __asm__ __volatile__("mxu3_fminaw %0, %1, %2"               \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FCLASSW(vrd, vrs)                                           \
    do{                                                             \
        __asm__ __volatile__("mxu3_fclassw %0, %1"                  \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

#define FCEQW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fceqw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FCLEW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fclew %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FCLTW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fcltw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FCORW(vrd, vrs, vrp)                                        \
    do{                                                             \
        __asm__ __volatile__("mxu3_fcorw %0, %1, %2"                \
                             :                                      \
                             :"i"((vrd)), "i"((vrs)), "i"((vrp))    \
            );                                                      \
    }while(0)

#define FFSIW(vrd, vrs)                                             \
    do{                                                             \
        __asm__ __volatile__("mxu3_ffsiw %0, %1"                    \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

#define FFUIW(vrd, vrs)                                             \
    do{                                                             \
        __asm__ __volatile__("mxu3_ffuiw %0, %1"                    \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

#define FTSIW(vrd, vrs)                                             \
    do{                                                             \
        __asm__ __volatile__("mxu3_ftsiw %0, %1"                    \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

#define FTUIW(vrd, vrs)                                             \
    do{                                                             \
        __asm__ __volatile__("mxu3_ftuiw %0, %1"                    \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

#define FRINTW(vrd, vrs)                                            \
    do{                                                             \
        __asm__ __volatile__("mxu3_frintw %0, %1"                   \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

#define FTRUNCSW(vrd, vrs)                                          \
    do{                                                             \
        __asm__ __volatile__("mxu3_ftruncsw %0, %1"                 \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

#define FTRUNCUW(vrd, vrs)                                          \
    do{                                                             \
        __asm__ __volatile__("mxu3_ftruncuw %0, %1"                 \
                             :                                      \
                             :"i"((vrd)), "i"((vrs))                \
            );                                                      \
    }while(0)

/* IU Instructions */
#define i_rdhwr(src0)                                               \
(    {unsigned long _dst_;                                          \
     __asm__ __volatile__ ("rdhwr\t%0,$%1":"=r"(_dst_):"i"(src0));  \
     _dst_;} )

/*for palladium*/
#define PALLADIUM_WAVE() __asm__ __volatile__("ehb")

#endif//__MXU3_H__
