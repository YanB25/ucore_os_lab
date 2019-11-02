#ifndef __LIBS_LOGGING_H__
#define __LIBS_LOGGING_H__

#ifndef __LIBS_DEBUG_CONFIG_
#define __LIBS_DEBUG_CONFIG_

    /* component identity for logging */
    #define MMU "MMU"
    #define PMM "PMM"
    #define VMM "VMM"
    #define SWAP "SWAP" 
    #define KERNEL "KERNEL" 

    /* setting logging level */
    /* set the global logging level */
    #define LOGGING_LEVEL KDEBUG
    /* set the logging level for MMU */
    #define MMU_LOGGING          KWARNING
    #define PMM_LOGGING          KWARNING
    #define VMM_LOGGING          KDEBUG
    #define SWAP_LOGGING         KDEBUG
    #define KERNEL_LOGGING       KDEBUG

#endif

#include <defs.h>

extern const char* LOGGING_LEVEL_PRT[];
extern const uint8_t __logging_raw_cnt_mg_flg__;
/* Get Logging Level Promt */
#define GLLP(L) (LOGGING_LEVEL_PRT[L])

#define KDEBUG        0x0
#define KINFO         0x1
#define KWARNING      0x2
#define KERROR        0x3
#define KPANIC        0x4
#define KDISABLED_LOG 0xf

#define RAW_LOGGING { const uint8_t __logging_raw_cnt_mg_flg__=1;
#define ENDR }

#define __LOGGING_PRINT_PROMT(LEVEL, ID) \
    cprintf("\n[%s] %s:%u - %s\n%s: ", LEVEL, __FILE__, __LINE__, __func__, ID);

#define logf(LEVEL, ID, fmt, ...)                                                                          \
    do {                                                                                                   \
        if (LEVEL >= LOGGING_LEVEL && LEVEL >= ID##_LOGGING) {                                          \
            if (!__logging_raw_cnt_mg_flg__) {                                               \
                __LOGGING_PRINT_PROMT(GLLP(LEVEL), ID)                                  \
            } else {                                                                    \
                cprintf("\t");                                                           \
            }                                                                             \
            cprintf(fmt, ##__VA_ARGS__);                                                                   \
        }                                                                                                  \
    } while(0)

#define debugf(ID, fmt, ...)                                                                \
    do {                                                                                    \
        if (KDEBUG >= LOGGING_LEVEL && KDEBUG >= ID##_LOGGING) {                           \
            if (!__logging_raw_cnt_mg_flg__) {                                               \
                __LOGGING_PRINT_PROMT(GLLP(KDEBUG), ID)                                     \
            } else {                                                                    \
                cprintf("\t");                                                           \
            }                                                                             \
            cprintf(fmt, ##__VA_ARGS__);                                                    \
        }                                                                                   \
    } while(0)

#define infof(ID, fmt, ...)                                                                \
    do {                                                                                    \
        if (KINFO >= LOGGING_LEVEL && KINFO >= ID##_LOGGING) {                                          \
            if (!__logging_raw_cnt_mg_flg__) {                       \
                __LOGGING_PRINT_PROMT(GLLP(KINFO), ID)                                     \
            } else {                                                                    \
                cprintf("\t");                                                           \
            }                                                                             \
            cprintf(fmt, ##__VA_ARGS__);                                                    \
        }                                                                                   \
    } while(0)

#define warnf(ID, fmt, ...)                                                                \
    do {                                                                                    \
        if (KWARNING >= LOGGING_LEVEL && KWARNING >= ID##_LOGGING) {                                          \
            if (!__logging_raw_cnt_mg_flg__) {                       \
                __LOGGING_PRINT_PROMT(GLLP(KWARNING), ID)                                     \
            } else {                                                                    \
                cprintf("\t");                                                           \
            }                                                                             \
            cprintf(fmt, ##__VA_ARGS__);                                                    \
        }                                                                                   \
    } while(0)

#define errorf(ID, fmt, ...)                                                                \
    do {                                                                                    \
        if (KERROR >= LOGGING_LEVEL && KERROR >= ID##_LOGGING) {                                          \
            if (!__logging_raw_cnt_mg_flg__) {                       \
                __LOGGING_PRINT_PROMT(GLLP(KERROR), ID)                                     \
            } else {                                                                    \
                cprintf("\t");                                                           \
            }                                                                             \
            cprintf(fmt, ##__VA_ARGS__);                                                    \
        }                                                                                   \
    } while(0)

#define panicf(ID, fmt, ...)                                                                \
    do {                                                                                    \
        if (KPANIC >= LOGGING_LEVEL && KPANIC >= ID##_LOGGING) {                                          \
            if (!__logging_raw_cnt_mg_flg__) {                                              \
                __LOGGING_PRINT_PROMT(GLLP(KPANIC), ID)                                     \
            } else {                                                                    \
                cprintf("\t");                                                           \
            }                                                                             \
            cprintf(fmt, ##__VA_ARGS__);                                                    \
            panic("PANIC: SEE MESSAGE ABOVE");                                              \
        }                                                                                   \
    } while(0)

#endif /* !__LIBS__LOGGING_H__ */