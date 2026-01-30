/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsp_common.h"
#include <stdarg.h>

#define TIE_LOG_ENABLED 1

#if (CONFIG_IDF_TARGET_ESP32S3)

esp_err_t tie_log(int n_regs, ...)
{

#if !TIE_LOG_ENABLED
    return ESP_OK;
#else

    va_list list;
    va_start(list, n_regs);

    uint32_t reg_128_bits[4] = {0, 0, 0, 0};
    int reg_code;

    for (int i = 0; i < n_regs; i++) {
        reg_code = va_arg(list, int);

        // ACCX register
        if ( reg_code == 'a') {
            asm volatile("rur.accx_0 %0" : "=a" (reg_128_bits[0]));
            asm volatile("rur.accx_1 %0" : "=a" (reg_128_bits[1]));
            printf("ACCX - %02x %08x", (unsigned int)reg_128_bits[1], (unsigned int)reg_128_bits[0]);
            printf(" --- %llu\n", (long long unsigned)reg_128_bits[1] << 32 | (unsigned int)reg_128_bits[0]);
        }

        // SAR:_BYTE register
        else if ( reg_code == 's') {
            asm volatile("rur.sar_byte %0" : "=a" (reg_128_bits[0]));
            printf("SAR_BYTE - %d\n", (unsigned int)reg_128_bits[0]);
        }

        // Q0 - Q7 registers
        else if ((reg_code >= 0) && (reg_code <= 7)) {
            switch (reg_code) {
            case 0 : {
                asm volatile("ee.movi.32.a q0, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q0, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q0, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q0, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q0");
                break;
            }
            case 1 : {
                asm volatile("ee.movi.32.a q1, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q1, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q1, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q1, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q1");
                break;
            }
            case 2 : {
                asm volatile("ee.movi.32.a q2, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q2, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q2, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q2, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q2");
                break;
            }
            case 3 : {
                asm volatile("ee.movi.32.a q3, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q3, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q3, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q3, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q3");
                break;
            }
            case 4 : {
                asm volatile("ee.movi.32.a q4, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q4, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q4, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q4, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q4");
                break;
            }
            case 5 : {
                asm volatile("ee.movi.32.a q5, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q5, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q5, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q5, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q5");
                break;
            }
            case 6 : {
                asm volatile("ee.movi.32.a q6, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q6, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q6, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q6, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q6");
                break;
            }
            case 7 : {
                asm volatile("ee.movi.32.a q7, %0, 0" : "=a" (reg_128_bits[0]));
                asm volatile("ee.movi.32.a q7, %0, 1" : "=a" (reg_128_bits[1]));
                asm volatile("ee.movi.32.a q7, %0, 2" : "=a" (reg_128_bits[2]));
                asm volatile("ee.movi.32.a q7, %0, 3" : "=a" (reg_128_bits[3]));
                printf("Q7");
                break;
            }
            }

            printf(" - 0x%08X %08X %08X %08X  ---  ",   (unsigned int)reg_128_bits[3],       (unsigned int)reg_128_bits[2], (unsigned int)reg_128_bits[1], (unsigned int)reg_128_bits[0]);
            printf("%u %u   %u %u   %u %u   %u %u\n",   (unsigned int)reg_128_bits[3] >> 16, (unsigned int)reg_128_bits[3] & 0x0000FFFF,
                   (unsigned int)reg_128_bits[2] >> 16, (unsigned int)reg_128_bits[2] & 0x0000FFFF,
                   (unsigned int)reg_128_bits[1] >> 16, (unsigned int)reg_128_bits[1] & 0x0000FFFF,
                   (unsigned int)reg_128_bits[0] >> 16, (unsigned int)reg_128_bits[0] & 0x0000FFFF);
        } else {
            printf("Bad register code");
        }
    }
    printf("------------------------------------------------------------------------------------\n");

    return ESP_OK;
#endif //TIE_LOG_ENABLED
}

#endif // CONFIG_IDF_TARGET_ESP32S3
