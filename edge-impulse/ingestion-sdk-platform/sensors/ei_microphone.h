/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EI_MICROPHONE_H
#define EI_MICROPHONE_H

/* Include ----------------------------------------------------------------- */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * I2S microphone pin configuration (compatible with INMP441 and similar).
 * Override these defines in your board configuration if needed.
 *
 * INMP441 wiring:
 *   INMP441 SCK  -> GPIO defined by EI_MIC_I2S_SCK
 *   INMP441 WS   -> GPIO defined by EI_MIC_I2S_WS
 *   INMP441 SD   -> GPIO defined by EI_MIC_I2S_SD
 *   INMP441 L/R  -> GND (selects left channel)
 *   INMP441 VDD  -> 3.3V
 *   INMP441 GND  -> GND
 */
#ifndef EI_MIC_I2S_SCK
#define EI_MIC_I2S_SCK  26   /* Bit clock (BCLK / SCK) */
#endif
#ifndef EI_MIC_I2S_WS
#define EI_MIC_I2S_WS   32   /* Word select / L-R clock (WS / LRCLK) */
#endif
#ifndef EI_MIC_I2S_SD
#define EI_MIC_I2S_SD   33   /* Serial data input (SD / DOUT) */
#endif

/**
 * Two-stage gain for INMP441 → int16 conversion.
 *
 * Stage 1 – EI_MIC_GAIN_SHIFT (right-shift)
 *   INMP441 puts 24-bit audio in bits[31:8] of the 32-bit I2S word,
 *   so the raw int32 can reach ±2^31. Shifting by 16 maps the 24-bit
 *   full-scale value exactly into the int16 range (±32767) with no risk
 *   of integer overflow or wrap-around.  Do NOT reduce below 16.
 *
 * Stage 2 – EI_MIC_GAIN_MUL (integer multiplier, applied after shift)
 *   Compensates for the INMP441's -26 dBFS sensitivity. A multiplier of
 *   8 brings normal conversational speech (~65 dB SPL at 0.5 m) to a
 *   comfortable recording level. Saturation clamping prevents clipping.
 *   Increase if audio is too quiet; decrease if peaks still clip.
 */
#ifndef EI_MIC_GAIN_SHIFT
#define EI_MIC_GAIN_SHIFT  16
#endif
#ifndef EI_MIC_GAIN_MUL
#define EI_MIC_GAIN_MUL    8
#endif

/* Function prototypes ----------------------------------------------------- */
bool ei_microphone_inference_start(uint32_t n_samples, float interval_ms);

bool ei_microphone_sample_start(void);
bool ei_microphone_inference_record(void);
bool ei_microphone_inference_is_recording(void);
void ei_microphone_inference_reset_buffers(void);
int ei_microphone_inference_get_data(size_t offset, size_t length, float *out_ptr);
bool ei_microphone_inference_end(void);

int i2s_init(uint32_t sampling_rate);
int i2s_deinit(void);

#endif