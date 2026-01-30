/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <malloc.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_dsp.h"

#include "dsps_mem.h"
#include "dsp_tests.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_task_wdt.h"

#define CORNERS_CPY_SET_COUNT       200
#define MEMCPY_REPORT_LEN           100
#define MEMSET_REPORT_LEN           50
#define CALL_REPEAT_COUNT           1000
#define TEST_PINNED_NUM_TASKS       2
#define TEST_PINNED_NUM_ITERS       2
#define CPY_REPEAT_COUNT            500
#define CPY_ITERS                   40
#define AREA_LENGTH                 1024

static const char *TAG = "dsps_mem_access";

/*
Test functionality of the memcpy and memset functions optimized for esp32s3

Requires: esp32s3

Purpose:
    - Test that esp32s3 optimized memcpy and memset have the same functionality as the original memcpy and memset

Procedure:
    - Create 4 arrays, 2 source arrays (aligned and unaligned) and 2 destination arrays (aligned and unaligned)
    - Initialize the destination arrays to 0, fill the source arrays with non-zero values
    - Copy the desired length of content from the source array to the destination array using memcpy
    - Compare the content of the destination array with the content of the source array
    - Initialize the destination arrays to 0
    - Repeat the 3 above steps for different copy lengths (especially corner conditions like copy 0, 1, 2... and N, N -1, N - 2.... bytes)
      and following arrays alignments
        - destination array 16-byte aligned, source array 16-byte aligned
        - destination array unaligned,       source array 16-byte aligned
        - destination array 16-byte aligned, source array unaligned
        - destination array unaligned,       source array unaligned
    - Set the desired length of the destination array using memset
    - Compare the content of the destination array with the set constant
    - Initialize the destination arrays to 0
    - Repeat the 3 above steps for different set lengths (especially corner conditions like copy 0, 1, 2... and N, N -1, N - 2.... bytes)
      and both alignments of the destination array (16-byte aligned or unaligned)
    - Free the dynamic array
*/

TEST_CASE("dsps_memcpy_memset_aes3_functionality", "[dsps]")
{
    const size_t arr_len = 1024;
    const uint8_t set_val = 0xaa;
    const size_t full_count = arr_len;
    const size_t canary_bytes = 16;                     // canary bytes to check a possibe overflow
    const unsigned int align_combinations_cpy = 4;      // source and destination arrays aligned or unaligned combinations
    const unsigned int align_combinations_set = 2;      // destination array aligned or unaligned

    uint8_t *arr_dest_align = (uint8_t *)memalign(16, (arr_len + canary_bytes) * sizeof(uint8_t));
    uint8_t *arr_src_align  = (uint8_t *)memalign(16, arr_len * sizeof(uint8_t));

    uint8_t *arr_dest_unalign = (uint8_t *)malloc((arr_len + canary_bytes) * sizeof(uint8_t));
    uint8_t *arr_src_unalign  = (uint8_t *)malloc(arr_len * sizeof(uint8_t));
    uint8_t *arr_dest = NULL, *arr_src = NULL;

    for (int i = 0; i < arr_len; i++) {
        ((uint8_t *)arr_src_align)[i] = (uint8_t)i;
        ((uint8_t *)arr_src_unalign)[i] = (uint8_t)i;
    }

    // canary bytes
    for (int i = arr_len; i < (arr_len + canary_bytes); i++) {
        ((uint8_t *)arr_dest_align)[i] = 0;
        ((uint8_t *)arr_dest_unalign)[i] = 0;
    }

    // aes3 memcpy functionality
    for (int align = 0; align < align_combinations_cpy; align++) {                   // alinged and unaligned arrays test loop

        size_t byte_count[2] = {0, full_count - CORNERS_CPY_SET_COUNT};         // amount of bytes to be copied

        switch (align) {
        case 0:                             // both 16-byte aligned
            arr_src = arr_src_align;
            arr_dest = arr_dest_align;
            break;

        case 1:                             // destination unaligned, source aligned
            arr_src = arr_src_align;
            arr_dest = arr_dest_unalign;
            break;

        case 2:                             // source unaligned, destination aligned
            arr_src = arr_src_unalign;
            arr_dest = arr_dest_align;
            break;

        case 3:                             // both unaligned
            arr_src = arr_src_unalign;
            arr_dest = arr_dest_unalign;
            break;

        default:                            // default - both aligned
            arr_src = arr_src_align;
            arr_dest = arr_dest_align;
            break;
        }

        for (int var = 0; var < 2; var++) {                                     // test conrner conditions
            for (int j = 0; j < CORNERS_CPY_SET_COUNT; j++) {                   // mem_set from 1 to CORNERS_CPY_SET_COUNT
                //         from (full_count - CORNERS_CPY_SET_COUNT + 1) to full_count
                for (int i = 0; i < full_count; i++) {                          // Destination array initializing
                    ((uint8_t *)arr_dest)[i] = 0;
                }

                dsps_memcpy((void *)arr_dest, (void *)arr_src, ++byte_count[var]);

                TEST_ASSERT_EQUAL_UINT8_ARRAY(arr_src, arr_dest, byte_count[var]);
                if (byte_count[var] < arr_len) {
                    TEST_ASSERT_EACH_EQUAL_UINT8(0, &arr_dest[byte_count[var]], (arr_len - byte_count[var]));
                }
                TEST_ASSERT_EACH_EQUAL_UINT8(0, &arr_dest[arr_len], canary_bytes);
            }
        }
    }

    // aes3 memset functionality
    for (int align = 0; align < align_combinations_set; align++ ) {             // alinged and unaligned arrays test loop

        size_t byte_count[2] = {0, full_count - CORNERS_CPY_SET_COUNT};         // amount of bytes to be copied
        if (!align) {
            arr_dest = arr_dest_align;
        } else {
            arr_dest = arr_dest_unalign;
        }

        for (int var = 0; var < 2; var++) {                                     // test conrner conditions
            for (int j = 0; j < CORNERS_CPY_SET_COUNT; j++) {                   // mem_set from 1 to CORNERS_CPY_SET_COUNT
                //         from (full_count - CORNERS_CPY_SET_COUNT + 1) to full_count
                for (int i = 0; i < full_count; i++) {                          // Destination array initializing
                    ((uint8_t *)arr_dest)[i] = 0;
                }

                dsps_memset((void *)arr_dest, set_val, ++byte_count[var]);

                TEST_ASSERT_EACH_EQUAL_UINT8(set_val, arr_dest, byte_count[var]);
                if (byte_count[var] < arr_len) {
                    TEST_ASSERT_EACH_EQUAL_UINT8(0, &arr_dest[byte_count[var]], (arr_len - byte_count[var]));
                }
                TEST_ASSERT_EACH_EQUAL_UINT8(0, &arr_dest[arr_len], canary_bytes);
            }
        }
    }

    free(arr_dest_align);
    free(arr_src_align);
    free(arr_dest_unalign);
    free(arr_src_unalign);
}


/*
Test micro-benchmark of the memcpy and memset functions optimized for esp32s3 and esp32

Requires: esp32s3

Purpose:
    - Test how fast the esp32s3 optimized memcpy and memset are compared to the esp32 optimized memcpy and memset

Procedure:
    - Create 2 unaligned arrays, source and destination array
    - Copy the content of the source array to the destination array using esp32s3 memcpy N times, while counting CPU cycles
    - Copy the content of the source array to the destination array using esp32 memcpy N times, while counting CPU cycles
    - Set the destination array using esp32s3 memcpy N times, while counting CPU cycles
    - Set the destination array using esp32 memcpy N times, while counting CPU cycles
    - Calculate benchmarks
    - Free both arrays
*/

TEST_CASE("dsps_memcpy_memset_aes3_benchmark", "[dsps]")
{
    const size_t area_len = AREA_LENGTH;                // full length of the area (in bytes)
    const size_t full_count = sizeof(uint8_t) * area_len;
    const uint8_t set_val = 0xee;                       // constant value, the destination array will be set with

    uint8_t *arr_src = (uint8_t *)malloc(area_len * sizeof(uint8_t));
    uint8_t *arr_dest = (uint8_t *)malloc(area_len * sizeof(uint8_t));

    // Memcpy benchmark
    const unsigned int start_aes3_memcpy = dsp_get_cpu_cycle_count();
    for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
        dsps_memcpy((void *)arr_dest, (void *)arr_src, full_count);
    }
    const unsigned int end_aes3_memcpy = dsp_get_cpu_cycle_count();

    const unsigned int start_ae32_memcpy = dsp_get_cpu_cycle_count();
    for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
        memcpy((void *)arr_dest, (void *)arr_src, full_count);
    }
    const unsigned int end_ae32_memcpy = dsp_get_cpu_cycle_count();

    const float aes3_cycles_memcpy = ((float)(end_aes3_memcpy - start_aes3_memcpy)) / CALL_REPEAT_COUNT;
    const float ae32_cycles_memcpy = ((float)(end_ae32_memcpy - start_ae32_memcpy)) / CALL_REPEAT_COUNT;

    ESP_LOGI(TAG, "Micro benchmark of memcpy for unaligned array of %"PRIu32" bytes", (uint32_t)full_count);
    ESP_LOGI(TAG, "Not-optimized cycles = %.2f", ae32_cycles_memcpy);
    ESP_LOGI(TAG, "S3  optimized cycles  = %.2f", aes3_cycles_memcpy);

    // Memset benchmark
    const unsigned int start_aes3_memset = dsp_get_cpu_cycle_count();
    for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
        dsps_memset((void *)arr_dest, set_val, full_count);
    }
    const unsigned int end_aes3_memset = dsp_get_cpu_cycle_count();

    const unsigned int start_ae32_memset = dsp_get_cpu_cycle_count();
    for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
        memset((void *)arr_dest, set_val, full_count);
    }
    const unsigned int end_ae32_memset = dsp_get_cpu_cycle_count();

    const float ae32_cycles_memset = ((float)(end_ae32_memset - start_ae32_memset)) / CALL_REPEAT_COUNT;
    const float aes3_cycles_memset = ((float)(end_aes3_memset - start_aes3_memset)) / CALL_REPEAT_COUNT;

    ESP_LOGI(TAG, "Micro benchmark of memset for unaligned array of %"PRIu32" bytes", (uint32_t)full_count);
    ESP_LOGI(TAG, "Not-optimized cycles = %.2f", ae32_cycles_memset);
    ESP_LOGI(TAG, "S3  optimized cycles  = %.2f", aes3_cycles_memset);

    free(arr_src);
    free(arr_dest);
}


/*
Test micro-benchmark of the memcpy optimized for esp32s3 and esp32 and print a comparison report for copy lengths from
1 to 200 bytes, where the difference between the two memcpys is not unanimous

Requires: esp32s3

Purpose:
    - Test how fast the esp32s3 optimized memcpy is to the esp32 optimized memcpy

Procedure:
    - Create 2 aligned arrays, source and destination array
    - Copy the content of the source array to the destination array using esp32s3 memcpy N times, while counting CPU cycles
    - Copy the content of the source array to the destination array using esp32 memcpy N times, while counting CPU cycles
    - Calculate benchmarks and save the result
    - Repeat the 3 above steps for different copy lengths (from 1 to 200 bytes)
      and following arrays alignments
        - destination array 16-byte aligned, source array 16-byte aligned
        - destination array unaligned,       source array 16-byte aligned
        - destination array 16-byte aligned, source array unaligned
        - destination array unaligned,       source array unaligned
    - Print table of results
    - Free dynamic arrays
*/
TEST_CASE("dsps_memcpy_benchmark_report", "[dsps]")
{
    unsigned int start_count, end_count;
    const unsigned int align_combinations = 4;      // source and destination arrays aligned or unaligned combinations
    const int32_t arr_len = 256;

    uint8_t *arr_dest = (uint8_t *)memalign(16, arr_len * sizeof(uint8_t));
    uint8_t *arr_src  = (uint8_t *)memalign(16, arr_len * sizeof(uint8_t));
    uint8_t *arr_dest_align = NULL, *arr_src_align = NULL;

    uint16_t **result_aes3 = (uint16_t **)malloc(align_combinations * sizeof(uint16_t *));    // 2D arrays result_aes3[align_combinations][MEMCPY_REPORT_LEN]
    uint16_t **result_ae32 = (uint16_t **)malloc(align_combinations * sizeof(uint16_t *));    // 2D arrays result_ae32[align_combinations][MEMCPY_REPORT_LEN]

    for (int i = 0; i < align_combinations; i++) {
        result_aes3[i] = (uint16_t *)malloc(MEMCPY_REPORT_LEN * sizeof(uint16_t));
        result_ae32[i] = (uint16_t *)malloc(MEMCPY_REPORT_LEN * sizeof(uint16_t));
    }

    for (int iter = 0; iter < align_combinations; iter++) {
        switch (iter) {
        case 0:                             // both 16-byte aligned
            arr_dest_align = arr_dest;
            arr_src_align = arr_src;
            break;

        case 1:                             // destination unaligned, source aligned
            arr_dest_align = arr_dest + 1;
            arr_src_align = arr_src;
            break;

        case 2:                             // source unaligned, destination aligned
            arr_dest_align = arr_dest;
            arr_src_align = arr_src + 1;
            break;

        case 3:                             // both unaligned
            arr_dest_align = arr_dest + 1;
            arr_src_align = arr_src + 1;
            break;

        default:                            // default - both aligned
            arr_dest_align = arr_dest;
            arr_src_align = arr_src;
            break;
        }

        for (int cpy_amount = 1; cpy_amount <= MEMCPY_REPORT_LEN; cpy_amount++) {

            start_count = dsp_get_cpu_cycle_count();
            for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
                dsps_memcpy((void *)arr_dest_align, (void *)arr_src_align, cpy_amount);
            }
            end_count = dsp_get_cpu_cycle_count();
            result_aes3[iter][cpy_amount - 1] = ((uint16_t)((end_count - start_count) / CALL_REPEAT_COUNT));

            start_count = dsp_get_cpu_cycle_count();
            for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
                memcpy((void *)arr_dest_align, (void *)arr_src_align, cpy_amount);
            }
            end_count = dsp_get_cpu_cycle_count();
            result_ae32[iter][cpy_amount - 1] = ((uint16_t)((end_count - start_count) / CALL_REPEAT_COUNT));
        }
    }

    ESP_LOGI(TAG, "Cycle counts for aligned/unaligned source/destination array using default xtensa memcpy and s3 optimized memcpy");
    printf("\n\tdest aligned \tdest unaligned\tdest   aligned\tdest unaligned\n");
    printf(  "\tsrc  aligned \tsrc    aligned\tsrc  unaligned\tsrc  unaligned\n\n");
    printf(  "byte \taes3    ae32\taes3    ae32\taes3    ae32\taes3    ae32\n");

    for (int i = 0; i < MEMCPY_REPORT_LEN; i++) {
        printf("%d\t", i + 1);

        for (int j = 0; j < align_combinations; j++) {
            printf(" %d\t", result_aes3[j][i]);
            printf(" %d\t", result_ae32[j][i]);
        }
        putchar('\n');
    }

    for (int i = 0; i < MEMCPY_REPORT_LEN; i++) {
        for (int j = 0; j < align_combinations; j++) {
            TEST_ASSERT_GREATER_OR_EQUAL((result_ae32[j][i]) / 4, result_aes3[j][i]);
        }
    }

    free(arr_dest);
    free(arr_src);
    free(result_ae32);
    free(result_aes3);
}

/*
Test micro-benchmark of the memset optimized for esp32s3 and esp32 and print a comparison report for set lengths from
1 to 200 bytes, where the difference between the two memsets is not unanimous

Requires: esp32s3

Purpose:
    - Test how fast the esp32s3 optimized memset is compared to the esp32 optimized memset

Procedure:
    - Create 1 aligned array - destination array
    - Set the destination array using esp32s3 memcpy N times, while counting CPU cycles
    - Set the destination array using esp32 memcpy N times, while counting CPU cycles
    - Calculate benchmarks and save the result
    - Repeat the 3 above steps for different copy lengths (from 1 to 200 bytes)
      and both destination arrays alignments (16-byte aligned and unaligned)
    - Print table of results
    - Free dynamic arrays
*/
TEST_CASE("dsps_memset_benchmark_report", "[dsps]")
{
    unsigned int start_count, end_count;
    const unsigned int align_combinations = 2;  // destination arrays aligned or unaligned
    const int32_t arr_len = 256;
    const uint8_t set_val = 0xaa;

    uint8_t *arr_dest = (uint8_t *)memalign(16, arr_len * sizeof(uint8_t));
    uint8_t *arr_dest_align = NULL;

    uint16_t **result_aes3 = (uint16_t **)malloc(align_combinations * sizeof(uint16_t *));    // 2D arrays result_aes3[align_combinations][MEMSET_REPORT_LEN]
    uint16_t **result_ae32 = (uint16_t **)malloc(align_combinations * sizeof(uint16_t *));    // 2D arrays result_ae32[align_combinations][MEMSET_REPORT_LEN]

    for (int i = 0; i < align_combinations; i++) {
        result_aes3[i] = (uint16_t *)malloc(MEMSET_REPORT_LEN * sizeof(uint16_t));
        result_ae32[i] = (uint16_t *)malloc(MEMSET_REPORT_LEN * sizeof(uint16_t));
    }

    for (int iter = 0; iter < align_combinations; iter++) {

        if (iter == 0) {
            arr_dest_align = arr_dest;          // destination 16-byte aligned
        } else {
            arr_dest_align = arr_dest + 1;      // destination unaligned
        }

        for (int set_amount = 1; set_amount <= MEMSET_REPORT_LEN; set_amount++) {
            start_count = dsp_get_cpu_cycle_count();
            for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
                dsps_memset((void *)arr_dest_align, set_val, set_amount);
            }
            end_count = dsp_get_cpu_cycle_count();
            result_aes3[iter][set_amount - 1] = ((uint16_t)((end_count - start_count) / CALL_REPEAT_COUNT));

            start_count = dsp_get_cpu_cycle_count();
            for (int j = 0; j < CALL_REPEAT_COUNT; j++) {
                memset((void *)arr_dest_align, set_val, set_amount);
            }
            end_count = dsp_get_cpu_cycle_count();
            result_ae32[iter][set_amount - 1] = ((uint16_t)((end_count - start_count) / CALL_REPEAT_COUNT));
        }
    }

    ESP_LOGI(TAG, "Cycle counts for aligned/unaligned destination array using default xtensa memcpy and s3 optimized memcpy");
    printf("\n\tdest aligned \tdest unaligned\n\n");
    printf(  "byte \taes3    ae32\taes3    ae32\n");

    for (int i = 0; i < MEMSET_REPORT_LEN; i++) {
        printf("%d\t", i + 1);

        for (int j = 0; j < align_combinations; j++) {
            printf(" %d\t", result_aes3[j][i]);
            printf(" %d\t", result_ae32[j][i]);
        }
        putchar('\n');
    }

    for (int i = 0; i < MEMSET_REPORT_LEN; i++) {
        for (int j = 0; j < align_combinations; j++) {
            TEST_ASSERT_GREATER_OR_EQUAL((result_ae32[j][i]) / 8, result_aes3[j][i]);
        }
    }

    free(arr_dest);
    free(result_ae32);
    free(result_aes3);
}

/*
Test micro-benchmark of the memcpy and memset functions optimized for esp32s3, with task switching

Requires: esp32s3

Purpose:
    - Test how fast the esp32s3 optimized memcpy and memset are while  running memset and memcpy in multiple tasks

Procedure:
    - Create 4 tasks - 2 tasks per each core. Tasks are pinned to cores and all the tasks are the same.
    - Run the memcpy micro-benchmark routine (from the previous test case) in each of the tasks.
    - Start all the tasks simultaneously
    - Wait for the tasks to complete, then delete the tasks
    - Get the benchmark result
    - Repeat all the above steps with memset, instead of memcpy
    - Free the created dynamic arrays
*/

typedef struct {
    SemaphoreHandle_t semaphore;
    uint8_t *arr_src;
    uint8_t *arr_dest;
    uint8_t set_val;
    size_t area_len;
    uint32_t mean_val_cpy;
    uint32_t mean_val_set;
} test_context_benchmark_t;


static void pinned_task_benchmark_memcpy(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    test_context_benchmark_t *context = (test_context_benchmark_t *)arg;
    long unsigned int cycles_acc = 0;
    unsigned int start_memcpy_count, end_memcpy_count;

    for (int j = 0; j < CPY_ITERS; j++) {
        start_memcpy_count = dsp_get_cpu_cycle_count();
        for (int i = 0; i < CPY_REPEAT_COUNT; i++) {
            dsps_memcpy((void *)context->arr_dest, (void *)context->arr_src, context->area_len);
        }
        end_memcpy_count = dsp_get_cpu_cycle_count();
        cycles_acc += (end_memcpy_count - start_memcpy_count);
        vTaskDelay(1);  // Block to cause a context switch, forcing the TIE context to be saved
    }

    context->mean_val_cpy += (uint32_t)((cycles_acc / CPY_REPEAT_COUNT) / CPY_ITERS);

    // Indicate done and wait to be deleted
    xSemaphoreGive(context->semaphore);
    vTaskSuspend(NULL);
}


static void pinned_task_benchmark_memset(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    test_context_benchmark_t *context = (test_context_benchmark_t *)arg;
    long unsigned int cycles_acc = 0;
    unsigned int start_memset_count, end_memset_count;

    for (int j = 0; j < CPY_ITERS; j++) {
        start_memset_count = dsp_get_cpu_cycle_count();
        for (int i = 0; i < CPY_REPEAT_COUNT; i++) {
            dsps_memset((void *)context->arr_dest, context->set_val, context->area_len);
        }
        end_memset_count = dsp_get_cpu_cycle_count();
        cycles_acc += (end_memset_count - start_memset_count);
        vTaskDelay(1);  // Block to cause a context switch, forcing the TIE context to be saved
    }

    context->mean_val_set += (uint32_t)((cycles_acc / CPY_REPEAT_COUNT) / CPY_ITERS);

    // Indicate done and wait to be deleted
    xSemaphoreGive(context->semaphore);
    vTaskSuspend(NULL);
}


TEST_CASE("dsps_memset_memcpy_context_switch_benchmark", "[dsps]")
{
    test_context_benchmark_t test_context;
    char task_name[10];

    test_context.semaphore = xSemaphoreCreateCounting(configNUM_CORES * TEST_PINNED_NUM_TASKS, 0);
    test_context.area_len = (size_t)AREA_LENGTH;
    test_context.arr_dest = (uint8_t *)malloc(AREA_LENGTH * sizeof(uint8_t));
    test_context.arr_src = (uint8_t *)malloc(AREA_LENGTH * sizeof(uint8_t));
    test_context.set_val = 0xab;
    test_context.mean_val_cpy = 0;
    test_context.mean_val_set = 0;

    static void (*pinned_functions[2])(void *);
    pinned_functions[0] = pinned_task_benchmark_memcpy;
    pinned_functions[1] = pinned_task_benchmark_memset;

    TEST_ASSERT_NOT_EQUAL(NULL, test_context.semaphore);

    for (int iter = 0; iter < TEST_PINNED_NUM_ITERS; iter++) {
        TaskHandle_t task_handles[configNUM_CORES][TEST_PINNED_NUM_TASKS];

        // Create test tasks for each core
        for (int i = 0; i < configNUM_CORES; i++) {
            for (int j = 0; j < TEST_PINNED_NUM_TASKS; j++) {
                sprintf(task_name, "task %d-%d", i, j);
                TEST_ASSERT_EQUAL(pdTRUE, xTaskCreatePinnedToCore(pinned_functions[iter], task_name, 4096,
                                  &test_context, 10, &task_handles[i][j], i));
            }
        }

        // Start the created tasks simultaneously
        for (int i = 0; i < configNUM_CORES; i++) {
            for (int j = 0; j < TEST_PINNED_NUM_TASKS; j++) {
                xTaskNotifyGive(task_handles[i][j]);
            }
        }

        // Wait for the tasks to complete
        for (int i = 0; i < configNUM_CORES * TEST_PINNED_NUM_TASKS; i++) {
            xSemaphoreTake(test_context.semaphore, portMAX_DELAY);
        }

        // Delete the tasks
        for (int i = 0; i < configNUM_CORES; i++) {
            for (int j = 0; j < TEST_PINNED_NUM_TASKS; j++) {
                vTaskDelete(task_handles[i][j]);
            }
        }

        vTaskDelay(10); // Short delay to allow idle task to be free task memory and TIE contexts
    }

    vSemaphoreDelete(test_context.semaphore);
    free(test_context.arr_dest);
    free(test_context.arr_src);

    const uint32_t iterations = (uint32_t)(configNUM_CORES * TEST_PINNED_NUM_TASKS * CPY_REPEAT_COUNT * CPY_ITERS);
    const uint32_t copy_mean_val =  (uint32_t)(test_context.mean_val_cpy / (configNUM_CORES * TEST_PINNED_NUM_TASKS));
    const uint32_t set_mean_val = (uint32_t)(test_context.mean_val_set / (configNUM_CORES * TEST_PINNED_NUM_TASKS));

    printf("\nOut of %"PRIu32" iterations, array len of %"PRIu32" bytes\n", iterations, (uint32_t)AREA_LENGTH);
    printf("Memcpy cycles = %"PRIu32"\n", copy_mean_val);
    printf("Memset cycles = %"PRIu32"\n", set_mean_val);
}


/*
Test context switching for the TIE disabled and enabled

Requires: esp32s3

Purpose:
    - Compare context switching between the tasks when TIE (esp32s3 instruction extension) is enabled and disabled to
      see what is the switching time overhead for the TIE enabled

Procedure:
    - Create a timer, 1000 ms is used for this test, but the exact time is not crucial
    - Create 4 tasks - 2 tasks per each core. Tasks are pinned to cores and all the tasks are the same
    - Start the created tasks simultaneously, start the timer
    - A task executes a single assembler instruction from the TIE, to induce the context switch
    - As soon, as the instruction is executed, a context switch occurs
    - A counter counts number or context switcher within the timer interval specified by the timer
    - Wait for the timer to expire and terminate the tasks
    - Get the number of task switches and delete all the tasks
    - Repeat the 7 above steps with the created tasks executing a single generic Xtensa assembler instruction,
      instead of the TIE instruction to get the switching overhead
*/

static bool timer_expired = false;
static TimerHandle_t one_shot_timer = NULL;

typedef struct {
    SemaphoreHandle_t semaphore;
    uint32_t switch_count_tie_on;
    uint32_t switch_count_tie_off;
} test_context_timing_t;

// Taks pinned to a core, executing TIE instruction
static void pinned_task_tie_on(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    test_context_timing_t *context = (test_context_timing_t *)arg;
    vTaskDelay(1);

    while (!timer_expired) {
        asm volatile("ee.zero.q q0");
        context->switch_count_tie_on++;
        taskYIELD();            // Block to cause a context switch, forcing the TIE context to be saved
    }
    xSemaphoreGive(context->semaphore);
    vTaskSuspend(NULL);
}

// Taks pinned to a core, executing generic Xtensa instruction
static void pinned_task_tie_off(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    test_context_timing_t *context = (test_context_timing_t *)arg;
    vTaskDelay(1);

    while (!timer_expired) {
        asm volatile("nop");
        context->switch_count_tie_off++;
        taskYIELD();            // Block to cause a context switch, forcing the context to be saved
    }

    xSemaphoreGive(context->semaphore);
    vTaskSuspend(NULL);
}

static void context_switch_timer_callback(TimerHandle_t xTimer)
{
    timer_expired = true;
}


TEST_CASE("dsps_TIE_context_switch_timing", "[dsps]")
{
    test_context_timing_t test_context;
    const TickType_t timer_period_ms = 1000;
    char task_name[10];

    test_context.semaphore = xSemaphoreCreateCounting(configNUM_CORES * TEST_PINNED_NUM_TASKS, 0);
    test_context.switch_count_tie_off = 0;
    test_context.switch_count_tie_on = 0;
    TEST_ASSERT_NOT_EQUAL(NULL, test_context.semaphore);

    static void (*pinned_functions[2])(void *);
    pinned_functions[0] = pinned_task_tie_on;
    pinned_functions[1] = pinned_task_tie_off;

    one_shot_timer = xTimerCreate("timer", pdMS_TO_TICKS(timer_period_ms), pdFALSE, (void *)0, context_switch_timer_callback);

    for (int iter = 0; iter < TEST_PINNED_NUM_ITERS; iter++) {
        timer_expired = false;
        TaskHandle_t task_handles[configNUM_CORES][TEST_PINNED_NUM_TASKS];

        // Create test tasks for each core
        for (int i = 0; i < configNUM_CORES; i++) {
            for (int j = 0; j < TEST_PINNED_NUM_TASKS; j++) {
                sprintf(task_name, "task %d-%d", i, j);
                TEST_ASSERT_EQUAL(pdTRUE, xTaskCreatePinnedToCore(pinned_functions[iter], task_name, 4096,
                                  &test_context, 1, &task_handles[i][j], i));
            }
        }

        // Start the created tasks simultaneously
        for (int i = 0; i < configNUM_CORES; i++) {
            for (int j = 0; j < TEST_PINNED_NUM_TASKS; j++) {
                xTaskNotifyGive(task_handles[i][j]);
            }
        }
        xTimerStart(one_shot_timer, portMAX_DELAY);
        vTaskDelay(1);

        // Wait for the tasks to complete
        for (int i = 0; i < configNUM_CORES * TEST_PINNED_NUM_TASKS; i++) {
            xSemaphoreTake(test_context.semaphore, portMAX_DELAY);
        }

        // Delete the tasks
        for (int i = 0; i < configNUM_CORES; i++) {
            for (int j = 0; j < TEST_PINNED_NUM_TASKS; j++) {
                vTaskDelete(task_handles[i][j]);
            }
        }
        vTaskDelay(10); // Short delay to allow idle task to be free task memory and TIE contexts
    }

    vSemaphoreDelete(test_context.semaphore);

    printf("\nContext switching count within %"PRIu32" ms nterval\n", (uint32_t)timer_period_ms);
    printf("TIE enabled  %"PRIu32"\n", test_context.switch_count_tie_on);
    printf("TIE disabled %"PRIu32"\n", test_context.switch_count_tie_off);

    float overhead = (((float)test_context.switch_count_tie_off / (float)test_context.switch_count_tie_on) * 100) - 100;
    printf("Switch overhead %.2f %%\n", overhead);
}
