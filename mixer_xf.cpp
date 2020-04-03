/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "mixer_xf.h"
#include "utility/dspinst.h"

#if defined(KINETISK)
#define MULTI_UNITYGAIN 65536

static void applyGainConst(int16_t *in0, const int16_t *in1)
{
  // in0 is destination
  uint32_t *dst = (uint32_t *)in0;
  const uint32_t *src = (uint32_t *)in1;
  const uint32_t *end = (uint32_t *)(in0 + AUDIO_BLOCK_SAMPLES);

  do {
    uint32_t tmp32 = *src++; // read 2 samples from *data
    int32_t val1 = signed_multiply_32x16b(MULTI_UNITYGAIN/2, tmp32);
    int32_t val2 = signed_multiply_32x16t(MULTI_UNITYGAIN/2, tmp32);
    val1 = signed_saturate_rshift(val1, 16, 0);
    val2 = signed_saturate_rshift(val2, 16, 0);
    tmp32 = pack_16b_16b(val2, val1);
    uint32_t tmp32b = *dst;
    val1 = signed_multiply_32x16b(MULTI_UNITYGAIN/2, tmp32b);
    val2 = signed_multiply_32x16t(MULTI_UNITYGAIN/2, tmp32b);
    val1 = signed_saturate_rshift(val1, 16, 0);
    val2 = signed_saturate_rshift(val2, 16, 0);
    tmp32b = pack_16b_16b(val2, val1);
    *dst++ = signed_add_16_and_16(tmp32, tmp32b);
  } while (dst < end);
}

static void applyGainXF(int16_t *in0, const int16_t *in1, const int16_t *ctrl)
{
  // in0 is destination
  uint32_t       *in0_32 = (uint32_t *)in0;
  const uint32_t *in1_32 = (uint32_t *)in1;
  const int32_t  *ctrl32 = (int32_t *)ctrl;
  const uint32_t *end   = (uint32_t *)(in0 + AUDIO_BLOCK_SAMPLES);

  int32_t mult = signed_multiply_accumulate_32x16b(MULTI_UNITYGAIN/2,*ctrl32++,1);
  do {
    uint32_t tmp32 = *in0_32;
    int32_t val1 = signed_multiply_32x16b(MULTI_UNITYGAIN-mult, tmp32);
    int32_t val2 = signed_multiply_32x16t(MULTI_UNITYGAIN-mult, tmp32);
    val1 = signed_saturate_rshift(val1, 16, 0);
    val2 = signed_saturate_rshift(val2, 16, 0);
    tmp32 = pack_16b_16b(val2, val1);
    uint32_t tmp32b = *in1_32++;
    val1 = signed_multiply_32x16b(mult, tmp32b);
    val2 = signed_multiply_32x16t(mult, tmp32b);
    val1 = signed_saturate_rshift(val1, 16, 0);
    val2 = signed_saturate_rshift(val2, 16, 0);
    tmp32b = pack_16b_16b(val2, val1);
    *in0_32++ = signed_add_16_and_16(tmp32, tmp32b);
  } while (in0_32 < end);
}

#elif defined(KINETISL)
#define MULTI_UNITYGAIN 256

// code for KINETISL, expect bugs!

static void applyGainConst(int16_t *in0, const int16_t *in1)
{
  // in0 is destination
  const int16_t *end = in0 + AUDIO_BLOCK_SAMPLES;
  
  do {
    int32_t val = *in0 * (MULTI_UNITYGAIN/2);
    *in0 = signed_saturate_rshift(val, 16, 0);
    val = *in0 + ((*in1++ * (MULTI_UNITYGAIN/2)) >> 8); // overflow possible??
    *in0++ = signed_saturate_rshift(val, 16, 0);
  } while (in0 < end);
}

static void applyGainXF(int16_t *in0, const int16_t *in1, const int16_t* ctrl)
{
  // in0 is destination
  const int16_t *end = in0 + AUDIO_BLOCK_SAMPLES;
  
  do {
    int32_t val = *in0 * *ctrl;
    *in0 = signed_saturate_rshift(val, 16, 0);
    val = *in0 + ((*in1++ * (*ctrl++)) >> 8); // overflow possible??
    *in0++ = signed_saturate_rshift(val, 16, 0);
  } while (in0 < end);
}

#endif

void AudioMixerXF::update(void)
{
  audio_block_t *in0=receiveWritable(0), *in1, *in2;
  
  if (in0) {
    in1=receiveReadOnly(1);
    if (in1) {
      in2=receiveReadOnly(2);
      if (in2) {
        // do crossfade
        applyGainXF(in0->data, in1->data, in2->data);
        release(in2);
      }
      else {
        // mix 0.5,0.5 if no control input
        applyGainConst(in0->data, in1->data);
      }
      release(in1);
    }
    transmit(in0);
    release(in0);
  }
}


