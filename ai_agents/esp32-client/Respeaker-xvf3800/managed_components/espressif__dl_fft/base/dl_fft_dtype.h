#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// union to simplify access to the 16 bit data
typedef union dl_sc16_u {
    struct {
        int16_t re;
        int16_t im;
    };
    uint32_t data;
} dl_sc16_t;

typedef union dl_fc32_u {
    struct {
        float re;
        float im;
    };
    uint64_t data;
} dl_fc32_t;

#ifdef __cplusplus
}
#endif
