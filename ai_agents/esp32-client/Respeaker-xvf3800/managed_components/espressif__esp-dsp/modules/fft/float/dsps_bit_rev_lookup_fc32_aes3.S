// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License. 

#include "dsps_fft2r_platform.h"

#if (dsps_bit_rev_lookup_fc32_ae32_enabled == 1)
#if (dsps_fft2r_fc32_aes3_enabled == 1)

// This is matrix multipliction function for ESP32 processor.
	.text
	.align  4
	.global dsps_bit_rev_lookup_fc32_aes3
	.type   dsps_bit_rev_lookup_fc32_aes3,@function

dsps_bit_rev_lookup_fc32_aes3: 
//esp_err_t dsps_bit_rev_lookup_fc32_aes3(float *data, int reverse_size, uint16_t *reverse_tab)

	entry	a1, 16
// data - a2
// reverse_size - a3
// reverse_tab - a4
    loopnez     a3, .__loop_end_radix2_reorder_lookup_table
        l16ui   a5, a4, 0   // Load first addr shift
        l16ui   a6, a4, 2   // Load second addr shift
        addi    a4, a4, 4   // Table addr update

        add.n   a5, a5, a2
        add.n   a6, a6, a2

        EE.LDF.64.IP f0, f2, a5, 0
        EE.LDF.64.IP f1, f3, a6, 0

        EE.STF.64.IP f0, f2, a6, 0
        EE.STF.64.IP f1, f3, a5, 0

.__loop_end_radix2_reorder_lookup_table:

	movi.n	a2, 0 // return status ESP_OK
	retw.n

#endif // dsps_fft2r_fc32_aes3_enabled
#endif // dsps_bit_rev_lookup_fc32_ae32_enabled