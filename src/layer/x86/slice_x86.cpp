// Copyright 2019 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "slice_x86.h"

#if __SSE2__
#include <emmintrin.h>
#if __AVX__
#include <immintrin.h>
#endif // __AVX__
#endif // __SSE2__

namespace ncnn {

Slice_x86::Slice_x86()
{
#if __SSE2__
    support_packing = true;
#endif // __SSE2__
}

int Slice_x86::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    const Mat& bottom_blob = bottom_blobs[0];
    int dims = bottom_blob.dims;
    size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;
    const int* slices_ptr = slices;
    const int* indices_ptr = indices;
    int positive_axis = axis < 0 ? dims + axis : axis;

    if (dims == 1) // positive_axis == 0
    {
        // slice vector
        int w = bottom_blob.w * elempack;
        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = w - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? w + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((w - q) / (top_blobs.size() - i));
                }
            }

            int out_elempack = 1;
#if __SSE2__
            if (opt.use_packing_layout)
            {
#if __AVX512F__
                out_elempack = slice % 16 == 0 ? 16 : slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
#elif __AVX__
                out_elempack = slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
#else
                out_elempack = slice % 4 == 0 ? 4 : 1;
#endif
            }
#endif // __SSE2__
            size_t out_elemsize = elemsize / elempack * out_elempack;

            Mat& top_blob = top_blobs[i];
            top_blob.create(slice / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
            if (top_blob.empty())
                return -100;

            const float* ptr = (const float*)bottom_blob + q;
            float* outptr = top_blob;
            memcpy(outptr, ptr, top_blob.w * top_blob.elemsize);

            q += slice;
        }
    }

    if (dims == 2 && positive_axis == 0)
    {
        // slice image height
        int w = bottom_blob.w;
        int h = bottom_blob.h * elempack;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = h - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? h + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((h - q) / (top_blobs.size() - i));
                }
            }

            int out_elempack = 1;
#if __SSE2__
            if (opt.use_packing_layout)
            {
#if __AVX512F__
                out_elempack = slice % 16 == 0 ? 16 : slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
#elif __AVX__
                out_elempack = slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
#else
                out_elempack = slice % 4 == 0 ? 4 : 1;
#endif
            }
#endif // __SSE2__
            size_t out_elemsize = elemsize / elempack * out_elempack;

            Mat& top_blob = top_blobs[i];
            top_blob.create(w, slice / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        size_t out_elemsize = top_blobs[0].elemsize;
        int out_elempack = top_blobs[0].elempack;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            out_elemsize = std::min(out_elemsize, top_blobs[i].elemsize);
            out_elempack = std::min(out_elempack, top_blobs[i].elempack);
        }

        Mat bottom_blob_unpacked = bottom_blob;
        if (elempack > out_elempack)
        {
            convert_packing(bottom_blob, bottom_blob_unpacked, out_elempack, opt);
            if (bottom_blob_unpacked.empty())
                return -100;
        }

        const float* ptr = bottom_blob_unpacked;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            Mat& top_blob = top_blobs[i];

#if __SSE2__
#if __AVX__
#if __AVX512F__
            if (out_elempack == 8 && top_blob.elempack == 16)
            {
                for (int j = 0; j < top_blob.h; j++)
                {
                    const float* r0 = ptr;
                    const float* r1 = ptr + w * 8;

                    float* outptr0 = top_blob.row(j);

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r0[4];
                        outptr0[5] = r0[5];
                        outptr0[6] = r0[6];
                        outptr0[7] = r0[7];
                        outptr0[8] = r1[0];
                        outptr0[9] = r1[1];
                        outptr0[10] = r1[2];
                        outptr0[11] = r1[3];
                        outptr0[12] = r1[4];
                        outptr0[13] = r1[5];
                        outptr0[14] = r1[6];
                        outptr0[15] = r1[7];

                        r0 += 8;
                        r1 += 8;
                        outptr0 += 16;
                    }

                    ptr += w * 16;
                }
            }
            if (out_elempack == 4 && top_blob.elempack == 16)
            {
                for (int j = 0; j < top_blob.h; j++)
                {
                    const float* r0 = ptr;
                    const float* r1 = ptr + w * 4;
                    const float* r2 = ptr + w * 8;
                    const float* r3 = ptr + w * 12;

                    float* outptr0 = top_blob.row(j);

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r1[0];
                        outptr0[5] = r1[1];
                        outptr0[6] = r1[2];
                        outptr0[7] = r1[3];
                        outptr0[8] = r2[0];
                        outptr0[9] = r2[1];
                        outptr0[10] = r2[2];
                        outptr0[11] = r2[3];
                        outptr0[12] = r3[0];
                        outptr0[13] = r3[1];
                        outptr0[14] = r3[2];
                        outptr0[15] = r3[3];

                        r0 += 4;
                        r1 += 4;
                        r2 += 4;
                        r3 += 4;
                        outptr0 += 16;
                    }

                    ptr += w * 16;
                }
            }
            if (out_elempack == 1 && top_blob.elempack == 16)
            {
                for (int j = 0; j < top_blob.h; j++)
                {
                    const float* r0 = ptr;
                    const float* r1 = ptr + w;
                    const float* r2 = ptr + w * 2;
                    const float* r3 = ptr + w * 3;
                    const float* r4 = ptr + w * 4;
                    const float* r5 = ptr + w * 5;
                    const float* r6 = ptr + w * 6;
                    const float* r7 = ptr + w * 7;
                    const float* r8 = ptr + w * 8;
                    const float* r9 = ptr + w * 9;
                    const float* ra = ptr + w * 10;
                    const float* rb = ptr + w * 11;
                    const float* rc = ptr + w * 12;
                    const float* rd = ptr + w * 13;
                    const float* re = ptr + w * 14;
                    const float* rf = ptr + w * 15;

                    float* outptr0 = top_blob.row(j);

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = *r0++;
                        outptr0[1] = *r1++;
                        outptr0[2] = *r2++;
                        outptr0[3] = *r3++;
                        outptr0[4] = *r4++;
                        outptr0[5] = *r5++;
                        outptr0[6] = *r6++;
                        outptr0[7] = *r7++;
                        outptr0[8] = *r8++;
                        outptr0[9] = *r9++;
                        outptr0[10] = *ra++;
                        outptr0[11] = *rb++;
                        outptr0[12] = *rc++;
                        outptr0[13] = *rd++;
                        outptr0[14] = *re++;
                        outptr0[15] = *rf++;

                        outptr0 += 16;
                    }

                    ptr += w * 16;
                }
            }
#endif // __AVX512F__
            if (out_elempack == 4 && top_blob.elempack == 8)
            {
                for (int j = 0; j < top_blob.h; j++)
                {
                    const float* r0 = ptr;
                    const float* r1 = ptr + w * 4;

                    float* outptr0 = top_blob.row(j);

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r1[0];
                        outptr0[5] = r1[1];
                        outptr0[6] = r1[2];
                        outptr0[7] = r1[3];

                        r0 += 4;
                        r1 += 4;
                        outptr0 += 8;
                    }

                    ptr += w * 8;
                }
            }
            if (out_elempack == 1 && top_blob.elempack == 8)
            {
                for (int j = 0; j < top_blob.h; j++)
                {
                    const float* r0 = ptr;
                    const float* r1 = ptr + w;
                    const float* r2 = ptr + w * 2;
                    const float* r3 = ptr + w * 3;
                    const float* r4 = ptr + w * 4;
                    const float* r5 = ptr + w * 5;
                    const float* r6 = ptr + w * 6;
                    const float* r7 = ptr + w * 7;

                    float* outptr0 = top_blob.row(j);

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = *r0++;
                        outptr0[1] = *r1++;
                        outptr0[2] = *r2++;
                        outptr0[3] = *r3++;
                        outptr0[4] = *r4++;
                        outptr0[5] = *r5++;
                        outptr0[6] = *r6++;
                        outptr0[7] = *r7++;

                        outptr0 += 8;
                    }

                    ptr += w * 8;
                }
            }
#endif // __AVX__
            if (out_elempack == 1 && top_blob.elempack == 4)
            {
                for (int j = 0; j < top_blob.h; j++)
                {
                    const float* r0 = ptr;
                    const float* r1 = ptr + w;
                    const float* r2 = ptr + w * 2;
                    const float* r3 = ptr + w * 3;

                    float* outptr0 = top_blob.row(j);

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = *r0++;
                        outptr0[1] = *r1++;
                        outptr0[2] = *r2++;
                        outptr0[3] = *r3++;

                        outptr0 += 4;
                    }

                    ptr += w * 4;
                }
            }
#endif // __SSE2__
            if (out_elempack == top_blob.elempack)
            {
                // 1-1 4-4 8-8
                int size = w * top_blob.h;

                float* outptr = top_blob;
                memcpy(outptr, ptr, size * top_blob.elemsize);

                ptr += size * top_blob.elempack;
            }
        }
    }

    if (dims == 2 && positive_axis == 1)
    {
        // slice image width
        int w = bottom_blob.w;
        int h = bottom_blob.h;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = w - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? w + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((w - q) / (top_blobs.size() - i));
                }
            }

            Mat& top_blob = top_blobs[i];
            top_blob.create(slice, h, elemsize, elempack, opt.blob_allocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int j = 0; j < h; j++)
        {
            const float* ptr = bottom_blob.row<const float>(j);
            for (size_t i = 0; i < top_blobs.size(); i++)
            {
                Mat& top_blob = top_blobs[i];

                float* outptr = top_blob.row(j);
                memcpy(outptr, ptr, top_blob.w * elemsize);

                ptr += top_blob.w * elempack;
            }
        }
    }

    if ((dims == 3 || dims == 4) && positive_axis == 0)
    {
        // slice dim channel
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c * elempack;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = channels - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? channels + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((channels - q) / (top_blobs.size() - i));
                }
            }

            int out_elempack = 1;
#if __SSE2__
            if (opt.use_packing_layout)
            {
#if __AVX512F__
                out_elempack = slice % 16 == 0 ? 16 : slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
#elif __AVX__
                out_elempack = slice % 8 == 0 ? 8 : slice % 4 == 0 ? 4 : 1;
#else
                out_elempack = slice % 4 == 0 ? 4 : 1;
#endif
            }
#endif // __SSE2__
            size_t out_elemsize = elemsize / elempack * out_elempack;

            Mat& top_blob = top_blobs[i];
            top_blob.create(w, h, d, slice / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
            if (top_blob.empty())
                return -100;

            top_blob.dims = dims;

            q += slice;
        }

        size_t out_elemsize = top_blobs[0].elemsize;
        int out_elempack = top_blobs[0].elempack;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            out_elemsize = std::min(out_elemsize, top_blobs[i].elemsize);
            out_elempack = std::min(out_elempack, top_blobs[i].elempack);
        }

        Mat bottom_blob_unpacked = bottom_blob;
        if (elempack > out_elempack)
        {
            convert_packing(bottom_blob, bottom_blob_unpacked, out_elempack, opt);
            if (bottom_blob_unpacked.empty())
                return -100;
        }

        int p = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            Mat& top_blob = top_blobs[i];

#if __SSE2__
#if __AVX__
#if __AVX512F__
            if (out_elempack == 8 && top_blob.elempack == 16)
            {
                int size = top_blob.w * top_blob.h * top_blob.d;

                for (int q = 0; q < top_blob.c; q++)
                {
                    const float* r0 = bottom_blob_unpacked.channel(p);
                    const float* r1 = bottom_blob_unpacked.channel(p + 1);

                    float* outptr0 = top_blob.channel(q);

                    for (int j = 0; j < size; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r0[4];
                        outptr0[5] = r0[5];
                        outptr0[6] = r0[6];
                        outptr0[7] = r0[7];
                        outptr0[8] = r1[0];
                        outptr0[9] = r1[1];
                        outptr0[10] = r1[2];
                        outptr0[11] = r1[3];
                        outptr0[12] = r1[4];
                        outptr0[13] = r1[5];
                        outptr0[14] = r1[6];
                        outptr0[15] = r1[7];

                        r0 += 8;
                        r1 += 8;
                        outptr0 += 16;
                    }

                    p += 2;
                }
            }
            if (out_elempack == 4 && top_blob.elempack == 16)
            {
                int size = top_blob.w * top_blob.h * top_blob.d;

                for (int q = 0; q < top_blob.c; q++)
                {
                    const float* r0 = bottom_blob_unpacked.channel(p);
                    const float* r1 = bottom_blob_unpacked.channel(p + 1);
                    const float* r2 = bottom_blob_unpacked.channel(p + 2);
                    const float* r3 = bottom_blob_unpacked.channel(p + 3);

                    float* outptr0 = top_blob.channel(q);

                    for (int j = 0; j < size; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r1[0];
                        outptr0[5] = r1[1];
                        outptr0[6] = r1[2];
                        outptr0[7] = r1[3];
                        outptr0[8] = r2[0];
                        outptr0[9] = r2[1];
                        outptr0[10] = r2[2];
                        outptr0[11] = r2[3];
                        outptr0[12] = r3[0];
                        outptr0[13] = r3[1];
                        outptr0[14] = r3[2];
                        outptr0[15] = r3[3];

                        r0 += 4;
                        r1 += 4;
                        r2 += 4;
                        r3 += 4;
                        outptr0 += 16;
                    }

                    p += 4;
                }
            }
            if (out_elempack == 1 && top_blob.elempack == 16)
            {
                int size = top_blob.w * top_blob.h * top_blob.d;

                for (int q = 0; q < top_blob.c; q++)
                {
                    const float* r0 = bottom_blob_unpacked.channel(p);
                    const float* r1 = bottom_blob_unpacked.channel(p + 1);
                    const float* r2 = bottom_blob_unpacked.channel(p + 2);
                    const float* r3 = bottom_blob_unpacked.channel(p + 3);
                    const float* r4 = bottom_blob_unpacked.channel(p + 4);
                    const float* r5 = bottom_blob_unpacked.channel(p + 5);
                    const float* r6 = bottom_blob_unpacked.channel(p + 6);
                    const float* r7 = bottom_blob_unpacked.channel(p + 7);
                    const float* r8 = bottom_blob_unpacked.channel(p + 8);
                    const float* r9 = bottom_blob_unpacked.channel(p + 9);
                    const float* ra = bottom_blob_unpacked.channel(p + 10);
                    const float* rb = bottom_blob_unpacked.channel(p + 11);
                    const float* rc = bottom_blob_unpacked.channel(p + 12);
                    const float* rd = bottom_blob_unpacked.channel(p + 13);
                    const float* re = bottom_blob_unpacked.channel(p + 14);
                    const float* rf = bottom_blob_unpacked.channel(p + 15);

                    float* outptr0 = top_blob.channel(q);

                    for (int j = 0; j < size; j++)
                    {
                        outptr0[0] = *r0++;
                        outptr0[1] = *r1++;
                        outptr0[2] = *r2++;
                        outptr0[3] = *r3++;
                        outptr0[4] = *r4++;
                        outptr0[5] = *r5++;
                        outptr0[6] = *r6++;
                        outptr0[7] = *r7++;
                        outptr0[8] = *r8++;
                        outptr0[9] = *r9++;
                        outptr0[10] = *ra++;
                        outptr0[11] = *rb++;
                        outptr0[12] = *rc++;
                        outptr0[13] = *rd++;
                        outptr0[14] = *re++;
                        outptr0[15] = *rf++;

                        outptr0 += 16;
                    }

                    p += 16;
                }
            }
#endif // __AVX512F__
            if (out_elempack == 4 && top_blob.elempack == 8)
            {
                int size = top_blob.w * top_blob.h * top_blob.d;

                for (int q = 0; q < top_blob.c; q++)
                {
                    const float* r0 = bottom_blob_unpacked.channel(p);
                    const float* r1 = bottom_blob_unpacked.channel(p + 1);

                    float* outptr0 = top_blob.channel(q);

                    for (int j = 0; j < size; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r1[0];
                        outptr0[5] = r1[1];
                        outptr0[6] = r1[2];
                        outptr0[7] = r1[3];

                        r0 += 4;
                        r1 += 4;
                        outptr0 += 8;
                    }

                    p += 2;
                }
            }
            if (out_elempack == 1 && top_blob.elempack == 8)
            {
                int size = top_blob.w * top_blob.h * top_blob.d;

                for (int q = 0; q < top_blob.c; q++)
                {
                    const float* r0 = bottom_blob_unpacked.channel(p);
                    const float* r1 = bottom_blob_unpacked.channel(p + 1);
                    const float* r2 = bottom_blob_unpacked.channel(p + 2);
                    const float* r3 = bottom_blob_unpacked.channel(p + 3);
                    const float* r4 = bottom_blob_unpacked.channel(p + 4);
                    const float* r5 = bottom_blob_unpacked.channel(p + 5);
                    const float* r6 = bottom_blob_unpacked.channel(p + 6);
                    const float* r7 = bottom_blob_unpacked.channel(p + 7);

                    float* outptr0 = top_blob.channel(q);

                    for (int j = 0; j < size; j++)
                    {
                        outptr0[0] = *r0++;
                        outptr0[1] = *r1++;
                        outptr0[2] = *r2++;
                        outptr0[3] = *r3++;
                        outptr0[4] = *r4++;
                        outptr0[5] = *r5++;
                        outptr0[6] = *r6++;
                        outptr0[7] = *r7++;

                        outptr0 += 8;
                    }

                    p += 8;
                }
            }
#endif // __AVX__
            if (out_elempack == 1 && top_blob.elempack == 4)
            {
                int size = top_blob.w * top_blob.h * top_blob.d;

                for (int q = 0; q < top_blob.c; q++)
                {
                    const float* r0 = bottom_blob_unpacked.channel(p);
                    const float* r1 = bottom_blob_unpacked.channel(p + 1);
                    const float* r2 = bottom_blob_unpacked.channel(p + 2);
                    const float* r3 = bottom_blob_unpacked.channel(p + 3);

                    float* outptr0 = top_blob.channel(q);

                    for (int j = 0; j < size; j++)
                    {
                        outptr0[0] = *r0++;
                        outptr0[1] = *r1++;
                        outptr0[2] = *r2++;
                        outptr0[3] = *r3++;

                        outptr0 += 4;
                    }

                    p += 4;
                }
            }
#endif // __SSE2__
            if (out_elempack == top_blob.elempack)
            {
                // 1-1 4-4 8-8
                int size = top_blob.total();

                const float* ptr = bottom_blob_unpacked.channel(p);
                float* outptr = top_blob;
                memcpy(outptr, ptr, size * top_blob.elemsize);

                p += top_blob.c;
            }
        }
    }

    if ((dims == 3 && positive_axis == 1) || (dims == 4 && positive_axis == 2))
    {
        // slice dim height
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = h - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? h + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((h - q) / (top_blobs.size() - i));
                }
            }

            Mat& top_blob = top_blobs[i];
            top_blob.create(w, slice, d, channels, elemsize, elempack, opt.blob_allocator);
            if (top_blob.empty())
                return -100;

            top_blob.dims = dims;

            q += slice;
        }

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p = 0; p < channels; p++)
        {
            const float* ptr = bottom_blob.channel(p);

            for (int j = 0; j < d; j++)
            {
                for (size_t i = 0; i < top_blobs.size(); i++)
                {
                    Mat& top_blob = top_blobs[i];

                    int size = top_blob.w * top_blob.h;

                    float* outptr = top_blob.channel(p).depth(j);
                    memcpy(outptr, ptr, size * elemsize);

                    ptr += size * elempack;
                }
            }
        }
    }

    if ((dims == 3 && positive_axis == 2) || (dims == 4 && positive_axis == 3))
    {
        // slice dim width
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = w - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? w + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((w - q) / (top_blobs.size() - i));
                }
            }

            Mat& top_blob = top_blobs[i];
            top_blob.create(slice, h, d, channels, elemsize, elempack, opt.blob_allocator);
            if (top_blob.empty())
                return -100;

            top_blob.dims = dims;

            q += slice;
        }

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p = 0; p < channels; p++)
        {
            const float* ptr = bottom_blob.channel(p);

            for (int j = 0; j < d; j++)
            {
                for (int k = 0; k < h; k++)
                {
                    for (size_t i = 0; i < top_blobs.size(); i++)
                    {
                        Mat& top_blob = top_blobs[i];

                        float* outptr = top_blob.channel(p).depth(j).row(k);
                        memcpy(outptr, ptr, top_blob.w * elemsize);

                        ptr += top_blob.w * elempack;
                    }
                }
            }
        }
    }

    if (dims == 4 && positive_axis == 1)
    {
        int w = bottom_blob.w;
        int h = bottom_blob.h;
        int d = bottom_blob.d;
        int channels = bottom_blob.c;

        int q = 0;
        for (size_t i = 0; i < top_blobs.size(); i++)
        {
            int slice;
            if (indices_ptr)
            {
                if (i == top_blobs.size() - 1)
                {
                    slice = d - q;
                }
                else
                {
                    int indice = indices_ptr[i];
                    int positive_indice = indice < 0 ? d + indice : indice;
                    slice = positive_indice - q;
                }
            }
            else
            {
                slice = slices_ptr[i];
                if (slice == -233)
                {
                    slice = static_cast<int>((d - q) / (top_blobs.size() - i));
                }
            }

            Mat& top_blob = top_blobs[i];
            top_blob.create(w, h, slice, channels, elemsize, elempack, opt.blob_allocator);
            if (top_blob.empty())
                return -100;

            q += slice;
        }

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p = 0; p < channels; p++)
        {
            const float* ptr = bottom_blob.channel(p);

            for (size_t i = 0; i < top_blobs.size(); i++)
            {
                Mat& top_blob = top_blobs[i];

                int size = top_blob.w * top_blob.h * top_blob.d;

                float* outptr = top_blob.channel(p);
                memcpy(outptr, ptr, size * elemsize);

                ptr += size * elempack;
            }
        }
    }

    return 0;
}

} // namespace ncnn
