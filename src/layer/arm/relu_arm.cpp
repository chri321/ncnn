// Copyright 2017 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "relu_arm.h"

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

#include "arm_usability.h"
#include "cpu.h"

namespace ncnn {

ReLU_arm::ReLU_arm()
{
#if __ARM_NEON
    support_packing = true;
#if NCNN_ARM82
    support_fp16_storage = cpu_support_arm_asimdhp();
#endif
#endif // __ARM_NEON

#if NCNN_BF16
    support_bf16_storage = true;
#endif
}

int ReLU_arm::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
{
    int elembits = bottom_top_blob.elembits();

    if (elembits == 8)
        return forward_inplace_int8(bottom_top_blob, opt);

#if NCNN_ARM82
    if (support_fp16_storage && opt.use_fp16_storage && elembits == 16)
        return forward_inplace_fp16s(bottom_top_blob, opt);
#endif

#if NCNN_BF16
    if (opt.use_bf16_storage && elembits == 16)
        return forward_inplace_bf16s(bottom_top_blob, opt);
#endif

    int w = bottom_top_blob.w;
    int h = bottom_top_blob.h;
    int d = bottom_top_blob.d;
    int channels = bottom_top_blob.c;
    int elempack = bottom_top_blob.elempack;
    int size = w * h * d * elempack;

    if (slope == 0.f)
    {
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            float* ptr = bottom_top_blob.channel(q);

            int i = 0;
#if __ARM_NEON
            float32x4_t _zero = vdupq_n_f32(0.f);
            for (; i + 15 < size; i += 16)
            {
#if NCNN_GNU_INLINE_ASM
#if __aarch64__
                asm volatile(
                    "prfm   pldl1keep, [%0, #512]   \n"
                    "ld1    {v0.4s, v1.4s, v2.4s, v3.4s}, [%0] \n"
                    "fmax   v0.4s, v0.4s, %2.4s     \n"
                    "fmax   v1.4s, v1.4s, %2.4s     \n"
                    "fmax   v2.4s, v2.4s, %2.4s     \n"
                    "fmax   v3.4s, v3.4s, %2.4s     \n"
                    "st1    {v0.4s, v1.4s, v2.4s, v3.4s}, [%0], #64 \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_zero) // %2
                    : "memory", "v0", "v1", "v2", "v3");
#else  // __aarch64__
                asm volatile(
                    "pld        [%0, #512]      \n"
                    "vldm       %0, {d0-d7}     \n"
                    "vmax.f32   q0, q0, %q2     \n"
                    "vmax.f32   q1, q1, %q2     \n"
                    "vmax.f32   q2, q2, %q2     \n"
                    "vmax.f32   q3, q3, %q2     \n"
                    "vstm       %0!, {d0-d7}    \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_zero) // %2
                    : "memory", "q0", "q1", "q2", "q3");
#endif // __aarch64__
#else  // NCNN_GNU_INLINE_ASM
                float32x4_t _p0 = vld1q_f32(ptr);
                float32x4_t _p1 = vld1q_f32(ptr + 4);
                float32x4_t _p2 = vld1q_f32(ptr + 8);
                float32x4_t _p3 = vld1q_f32(ptr + 12);
                _p0 = vmaxq_f32(_p0, _zero);
                _p1 = vmaxq_f32(_p1, _zero);
                _p2 = vmaxq_f32(_p2, _zero);
                _p3 = vmaxq_f32(_p3, _zero);
                vst1q_f32(ptr, _p0);
                vst1q_f32(ptr + 4, _p1);
                vst1q_f32(ptr + 8, _p2);
                vst1q_f32(ptr + 12, _p3);
                ptr += 16;
#endif // NCNN_GNU_INLINE_ASM
            }
            for (; i + 7 < size; i += 8)
            {
                float32x4_t _p0 = vld1q_f32(ptr);
                float32x4_t _p1 = vld1q_f32(ptr + 4);
                _p0 = vmaxq_f32(_p0, _zero);
                _p1 = vmaxq_f32(_p1, _zero);
                vst1q_f32(ptr, _p0);
                vst1q_f32(ptr + 4, _p1);
                ptr += 8;
            }
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _ptr = vld1q_f32(ptr);
                _ptr = vmaxq_f32(_ptr, _zero);
                vst1q_f32(ptr, _ptr);
                ptr += 4;
            }
#endif // __ARM_NEON
            for (; i < size; i++)
            {
                *ptr = std::max(*ptr, 0.f);
                ptr++;
            }
        }
    }
    else
    {
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            float* ptr = bottom_top_blob.channel(q);

            int i = 0;
#if __ARM_NEON
            float32x4_t _zero = vdupq_n_f32(0.f);
            float32x4_t _slope = vdupq_n_f32(slope);
            for (; i + 15 < size; i += 16)
            {
#if NCNN_GNU_INLINE_ASM
#if __aarch64__
                asm volatile(
                    "prfm   pldl1keep, [%0, #512]   \n"
                    "ld1    {v0.4s, v1.4s, v2.4s, v3.4s}, [%0] \n"
                    "fcmle  v4.4s, v0.4s, #0        \n"
                    "fcmle  v5.4s, v1.4s, #0        \n"
                    "fcmle  v6.4s, v2.4s, #0        \n"
                    "fcmle  v7.4s, v3.4s, #0        \n"
                    "fmul   v8.4s, v0.4s, %2.4s     \n"
                    "fmul   v9.4s, v1.4s, %2.4s     \n"
                    "fmul   v10.4s, v2.4s, %2.4s    \n"
                    "fmul   v11.4s, v3.4s, %2.4s    \n"
                    "bit    v0.16b, v8.16b, v4.16b  \n"
                    "bit    v1.16b, v9.16b, v5.16b  \n"
                    "bit    v2.16b, v10.16b, v6.16b \n"
                    "bit    v3.16b, v11.16b, v7.16b \n"
                    "st1    {v0.4s, v1.4s, v2.4s, v3.4s}, [%0], #64 \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_slope) // %2
                    : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11");
#else  // __aarch64__
                asm volatile(
                    "pld        [%0, #512]      \n"
                    "vldm       %0, {d0-d7}     \n"
                    "vcle.f32   q4, q0, %q2     \n"
                    "vcle.f32   q5, q1, %q2     \n"
                    "vcle.f32   q6, q2, %q2     \n"
                    "vcle.f32   q7, q3, %q2     \n"
                    "vmul.f32   q8, q0, %q3     \n"
                    "vmul.f32   q9, q1, %q3     \n"
                    "vmul.f32   q10, q2, %q3    \n"
                    "vmul.f32   q11, q3, %q3    \n"
                    "vbit.32    q0, q8, q4      \n"
                    "vbit.32    q1, q9, q5      \n"
                    "vbit.32    q2, q10, q6     \n"
                    "vbit.32    q3, q11, q7     \n"
                    "vstm       %0!, {d0-d7}    \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_zero), // %2
                    "w"(_slope) // %3
                    : "memory", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9", "q10", "q11");
#endif // __aarch64__
#else  // NCNN_GNU_INLINE_ASM
                float32x4_t _p0 = vld1q_f32(ptr);
                float32x4_t _p1 = vld1q_f32(ptr + 4);
                float32x4_t _p2 = vld1q_f32(ptr + 8);
                float32x4_t _p3 = vld1q_f32(ptr + 12);
                uint32x4_t _lemask0 = vcleq_f32(_p0, _zero);
                uint32x4_t _lemask1 = vcleq_f32(_p1, _zero);
                uint32x4_t _lemask2 = vcleq_f32(_p2, _zero);
                uint32x4_t _lemask3 = vcleq_f32(_p3, _zero);
                float32x4_t _ps0 = vmulq_f32(_p0, _slope);
                float32x4_t _ps1 = vmulq_f32(_p1, _slope);
                float32x4_t _ps2 = vmulq_f32(_p2, _slope);
                float32x4_t _ps3 = vmulq_f32(_p3, _slope);
                _p0 = vbslq_f32(_lemask0, _ps0, _p0);
                _p1 = vbslq_f32(_lemask1, _ps1, _p1);
                _p2 = vbslq_f32(_lemask2, _ps2, _p2);
                _p3 = vbslq_f32(_lemask3, _ps3, _p3);
                vst1q_f32(ptr, _p0);
                vst1q_f32(ptr + 4, _p1);
                vst1q_f32(ptr + 8, _p2);
                vst1q_f32(ptr + 12, _p3);
                ptr += 16;
#endif // NCNN_GNU_INLINE_ASM
            }
            for (; i + 7 < size; i += 8)
            {
                float32x4_t _p0 = vld1q_f32(ptr);
                float32x4_t _p1 = vld1q_f32(ptr + 4);
                uint32x4_t _lemask0 = vcleq_f32(_p0, _zero);
                uint32x4_t _lemask1 = vcleq_f32(_p1, _zero);
                float32x4_t _ps0 = vmulq_f32(_p0, _slope);
                float32x4_t _ps1 = vmulq_f32(_p1, _slope);
                _p0 = vbslq_f32(_lemask0, _ps0, _p0);
                _p1 = vbslq_f32(_lemask1, _ps1, _p1);
                vst1q_f32(ptr, _p0);
                vst1q_f32(ptr + 4, _p1);
                ptr += 8;
            }
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _p = vld1q_f32(ptr);
                uint32x4_t _lemask = vcleq_f32(_p, _zero);
                float32x4_t _ps = vmulq_f32(_p, _slope);
                _p = vbslq_f32(_lemask, _ps, _p);
                vst1q_f32(ptr, _p);
                ptr += 4;
            }
#endif // __ARM_NEON
            for (; i < size; i++)
            {
                if (*ptr < 0)
                    *ptr *= slope;
                ptr++;
            }
        }
    }

    return 0;
}

#if NCNN_BF16
int ReLU_arm::forward_inplace_bf16s(Mat& bottom_top_blob, const Option& opt) const
{
    int w = bottom_top_blob.w;
    int h = bottom_top_blob.h;
    int d = bottom_top_blob.d;
    int channels = bottom_top_blob.c;
    int elempack = bottom_top_blob.elempack;
    int size = w * h * d * elempack;

    if (slope == 0.f)
    {
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            unsigned short* ptr = bottom_top_blob.channel(q);

            int i = 0;
#if __ARM_NEON
            float32x4_t _zero = vdupq_n_f32(0.f);
            for (; i + 15 < size; i += 16)
            {
#if NCNN_GNU_INLINE_ASM
#if __aarch64__
                asm volatile(
                    "prfm   pldl1keep, [%0, #256]   \n"
                    "ld1    {v0.4h, v1.4h, v2.4h, v3.4h}, [%0] \n"
                    "shll   v0.4s, v0.4h, #16       \n"
                    "shll   v1.4s, v1.4h, #16       \n"
                    "shll   v2.4s, v2.4h, #16       \n"
                    "shll   v3.4s, v3.4h, #16       \n"
                    "fmax   v0.4s, v0.4s, %2.4s     \n"
                    "fmax   v1.4s, v1.4s, %2.4s     \n"
                    "fmax   v2.4s, v2.4s, %2.4s     \n"
                    "fmax   v3.4s, v3.4s, %2.4s     \n"
                    "shrn   v0.4h, v0.4s, #16       \n"
                    "shrn   v1.4h, v1.4s, #16       \n"
                    "shrn   v2.4h, v2.4s, #16       \n"
                    "shrn   v3.4h, v3.4s, #16       \n"
                    "st1    {v0.4h, v1.4h, v2.4h, v3.4h}, [%0], #32 \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_zero) // %2
                    : "memory", "v0", "v1", "v2", "v3");
#else  // __aarch64__
                asm volatile(
                    "pld        [%0, #256]      \n"
                    "vld1.u16   {d4-d7}, [%0]   \n"
                    "vshll.u16  q0, d4, #16     \n"
                    "vshll.u16  q1, d5, #16     \n"
                    "vshll.u16  q2, d6, #16     \n"
                    "vshll.u16  q3, d7, #16     \n"
                    "vmax.f32   q0, q0, %q2     \n"
                    "vmax.f32   q1, q1, %q2     \n"
                    "vmax.f32   q2, q2, %q2     \n"
                    "vmax.f32   q3, q3, %q2     \n"
                    "vshrn.u32  d0, q0, #16     \n"
                    "vshrn.u32  d1, q1, #16     \n"
                    "vshrn.u32  d2, q2, #16     \n"
                    "vshrn.u32  d3, q3, #16     \n"
                    "vst1.u16   {d0-d3}, [%0]!  \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_zero) // %2
                    : "memory", "q0", "q1", "q2", "q3");
#endif // __aarch64__
#else  // NCNN_GNU_INLINE_ASM
                uint16x8_t _p = vld1q_u16(ptr);
                uint16x8_t _q = vld1q_u16(ptr + 8);
                float32x4_t _p0 = bfloat2float(vget_low_u16(_p));
                float32x4_t _p1 = bfloat2float(vget_high_u16(_p));
                float32x4_t _p2 = bfloat2float(vget_low_u16(_q));
                float32x4_t _p3 = bfloat2float(vget_high_u16(_q));
                _p0 = vmaxq_f32(_p0, _zero);
                _p1 = vmaxq_f32(_p1, _zero);
                _p2 = vmaxq_f32(_p2, _zero);
                _p3 = vmaxq_f32(_p3, _zero);
                _p = vcombine_u16(float2bfloat(_p0), float2bfloat(_p1));
                _q = vcombine_u16(float2bfloat(_p2), float2bfloat(_p3));
                vst1q_u16(ptr, _p);
                vst1q_u16(ptr + 8, _q);
                ptr += 16;
#endif // NCNN_GNU_INLINE_ASM
            }
            for (; i + 7 < size; i += 8)
            {
                uint16x8_t _p = vld1q_u16(ptr);
                float32x4_t _p0 = bfloat2float(vget_low_u16(_p));
                float32x4_t _p1 = bfloat2float(vget_high_u16(_p));
                _p0 = vmaxq_f32(_p0, _zero);
                _p1 = vmaxq_f32(_p1, _zero);
                _p = vcombine_u16(float2bfloat(_p0), float2bfloat(_p1));
                vst1q_u16(ptr, _p);
                ptr += 8;
            }
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _p = bfloat2float(vld1_u16(ptr));
                _p = vmaxq_f32(_p, _zero);
                vst1_u16(ptr, float2bfloat(_p));
                ptr += 4;
            }
#endif // __ARM_NEON
            for (; i < size; i++)
            {
                float v = bfloat16_to_float32(ptr[0]);
                if (v < 0.f)
                    ptr[0] = float32_to_bfloat16(0.f);
                ptr += 1;
            }
        }
    }
    else
    {
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            unsigned short* ptr = bottom_top_blob.channel(q);

            int i = 0;
#if __ARM_NEON
            float32x4_t _zero = vdupq_n_f32(0.f);
            float32x4_t _slope = vdupq_n_f32(slope);
            for (; i + 15 < size; i += 16)
            {
#if NCNN_GNU_INLINE_ASM
#if __aarch64__
                asm volatile(
                    "prfm   pldl1keep, [%0, #256]   \n"
                    "ld1    {v0.4h, v1.4h, v2.4h, v3.4h}, [%0] \n"
                    "shll   v0.4s, v0.4h, #16       \n"
                    "shll   v1.4s, v1.4h, #16       \n"
                    "shll   v2.4s, v2.4h, #16       \n"
                    "shll   v3.4s, v3.4h, #16       \n"
                    "fcmle  v4.4s, v0.4s, #0        \n"
                    "fcmle  v5.4s, v1.4s, #0        \n"
                    "fcmle  v6.4s, v2.4s, #0        \n"
                    "fcmle  v7.4s, v3.4s, #0        \n"
                    "fmul   v8.4s, v0.4s, %2.4s     \n"
                    "fmul   v9.4s, v1.4s, %2.4s     \n"
                    "fmul   v10.4s, v2.4s, %2.4s    \n"
                    "fmul   v11.4s, v3.4s, %2.4s    \n"
                    "bit    v0.16b, v8.16b, v4.16b  \n"
                    "bit    v1.16b, v9.16b, v5.16b  \n"
                    "bit    v2.16b, v10.16b, v6.16b \n"
                    "bit    v3.16b, v11.16b, v7.16b \n"
                    "shrn   v0.4h, v0.4s, #16       \n"
                    "shrn   v1.4h, v1.4s, #16       \n"
                    "shrn   v2.4h, v2.4s, #16       \n"
                    "shrn   v3.4h, v3.4s, #16       \n"
                    "st1    {v0.4h, v1.4h, v2.4h, v3.4h}, [%0], #32 \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_slope) // %2
                    : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11");
#else  // __aarch64__
                asm volatile(
                    "pld        [%0, #256]      \n"
                    "vld1.u16   {d4-d7}, [%0]   \n"
                    "vshll.u16  q0, d4, #16     \n"
                    "vshll.u16  q1, d5, #16     \n"
                    "vshll.u16  q2, d6, #16     \n"
                    "vshll.u16  q3, d7, #16     \n"
                    "vcle.f32   q4, q0, %q2     \n"
                    "vcle.f32   q5, q1, %q2     \n"
                    "vcle.f32   q6, q2, %q2     \n"
                    "vcle.f32   q7, q3, %q2     \n"
                    "vmul.f32   q8, q0, %q3     \n"
                    "vmul.f32   q9, q1, %q3     \n"
                    "vmul.f32   q10, q2, %q3    \n"
                    "vmul.f32   q11, q3, %q3    \n"
                    "vbit.32    q0, q8, q4      \n"
                    "vbit.32    q1, q9, q5      \n"
                    "vbit.32    q2, q10, q6     \n"
                    "vbit.32    q3, q11, q7     \n"
                    "vshrn.u32  d0, q0, #16     \n"
                    "vshrn.u32  d1, q1, #16     \n"
                    "vshrn.u32  d2, q2, #16     \n"
                    "vshrn.u32  d3, q3, #16     \n"
                    "vst1.u16   {d0-d3}, [%0]!  \n"
                    : "=r"(ptr) // %0
                    : "0"(ptr),
                    "w"(_zero), // %2
                    "w"(_slope) // %3
                    : "memory", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9", "q10", "q11");
#endif // __aarch64__
#else  // NCNN_GNU_INLINE_ASM
                uint16x8_t _p = vld1q_u16(ptr);
                uint16x8_t _q = vld1q_u16(ptr + 8);
                float32x4_t _p0 = bfloat2float(vget_low_u16(_p));
                float32x4_t _p1 = bfloat2float(vget_high_u16(_p));
                float32x4_t _p2 = bfloat2float(vget_low_u16(_q));
                float32x4_t _p3 = bfloat2float(vget_high_u16(_q));
                uint32x4_t _lemask0 = vcleq_f32(_p0, _zero);
                uint32x4_t _lemask1 = vcleq_f32(_p1, _zero);
                uint32x4_t _lemask2 = vcleq_f32(_p2, _zero);
                uint32x4_t _lemask3 = vcleq_f32(_p3, _zero);
                float32x4_t _ps0 = vmulq_f32(_p0, _slope);
                float32x4_t _ps1 = vmulq_f32(_p1, _slope);
                float32x4_t _ps2 = vmulq_f32(_p2, _slope);
                float32x4_t _ps3 = vmulq_f32(_p3, _slope);
                _p0 = vbslq_f32(_lemask0, _ps0, _p0);
                _p1 = vbslq_f32(_lemask1, _ps1, _p1);
                _p2 = vbslq_f32(_lemask2, _ps2, _p2);
                _p3 = vbslq_f32(_lemask3, _ps3, _p3);
                _p = vcombine_u16(float2bfloat(_p0), float2bfloat(_p1));
                _q = vcombine_u16(float2bfloat(_p2), float2bfloat(_p3));
                vst1q_u16(ptr, _p);
                vst1q_u16(ptr + 8, _q);
                ptr += 16;
#endif // NCNN_GNU_INLINE_ASM
            }
            for (; i + 7 < size; i += 8)
            {
                uint16x8_t _p = vld1q_u16(ptr);
                float32x4_t _p0 = bfloat2float(vget_low_u16(_p));
                float32x4_t _p1 = bfloat2float(vget_high_u16(_p));
                uint32x4_t _lemask0 = vcleq_f32(_p0, _zero);
                uint32x4_t _lemask1 = vcleq_f32(_p1, _zero);
                float32x4_t _ps0 = vmulq_f32(_p0, _slope);
                float32x4_t _ps1 = vmulq_f32(_p1, _slope);
                _p0 = vbslq_f32(_lemask0, _ps0, _p0);
                _p1 = vbslq_f32(_lemask1, _ps1, _p1);
                _p = vcombine_u16(float2bfloat(_p0), float2bfloat(_p1));
                vst1q_u16(ptr, _p);
                ptr += 8;
            }
            for (; i + 3 < size; i += 4)
            {
                float32x4_t _p = bfloat2float(vld1_u16(ptr));
                uint32x4_t _lemask = vcleq_f32(_p, _zero);
                float32x4_t _ps = vmulq_f32(_p, _slope);
                _p = vbslq_f32(_lemask, _ps, _p);
                vst1_u16(ptr, float2bfloat(_p));
                ptr += 4;
            }
#endif // __ARM_NEON
            for (; i < size; i++)
            {
                float v = bfloat16_to_float32(ptr[0]);
                if (v < 0.f)
                    ptr[0] = float32_to_bfloat16(v * slope);
                ptr += 1;
            }
        }
    }

    return 0;
}
#endif // NCNN_BF16

int ReLU_arm::forward_inplace_int8(Mat& bottom_top_blob, const Option& opt) const
{
    int w = bottom_top_blob.w;
    int h = bottom_top_blob.h;
    int d = bottom_top_blob.d;
    int channels = bottom_top_blob.c;
    int size = w * h * d;
    int elempack = bottom_top_blob.elempack;

#if __ARM_NEON
    if (elempack == 8)
    {
        if (slope == 0.f)
        {
            #pragma omp parallel for num_threads(opt.num_threads)
            for (int q = 0; q < channels; q++)
            {
                signed char* ptr = bottom_top_blob.channel(q);

                int i = 0;
                int8x16_t _zero = vdupq_n_s8(0);
                for (; i + 1 < size; i += 2)
                {
                    int8x16_t _p = vld1q_s8(ptr);
                    _p = vmaxq_s8(_p, _zero);
                    vst1q_s8(ptr, _p);

                    ptr += 16;
                }
                for (; i < size; i++)
                {
                    int8x8_t _p = vld1_s8(ptr);
                    _p = vmax_s8(_p, vget_low_s8(_zero));
                    vst1_s8(ptr, _p);

                    ptr += 8;
                }
            }
        }
        else
        {
            // TODO leakyrelu
        }

        return 0;
    }
#endif // __ARM_NEON

    if (slope == 0.f)
    {
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            signed char* ptr = bottom_top_blob.channel(q);

            int i = 0;
#if __ARM_NEON
            int8x16_t _zero = vdupq_n_s8(0);
            for (; i + 15 < size; i += 16)
            {
                int8x16_t _p = vld1q_s8(ptr);
                _p = vmaxq_s8(_p, _zero);
                vst1q_s8(ptr, _p);

                ptr += 16;
            }
#endif // __ARM_NEON
            for (; i < size; i++)
            {
                if (*ptr < 0)
                    *ptr = 0;

                ptr++;
            }
        }
    }
    else
    {
        // TODO leakyrelu
    }

    return 0;
}

} // namespace ncnn
