#include "config.h"
#include "libavutil/cpu.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include "libavcodec/get_bits.h" /* required for hevcdsp.h GetBitContext */
#include "libavcodec/hevcpred.h"
#include "libavcodec/x86/hevcpred.h"

//function declaration

void ff_hevcpred_init_x86(HEVCPredContext *c, const int bit_depth)
{
    int mm_flags = av_get_cpu_flags();

    if (bit_depth == 8) {
        if (EXTERNAL_MMX(mm_flags)) {


            if (EXTERNAL_MMXEXT(mm_flags)) {


                if (EXTERNAL_SSE2(mm_flags)) {

                }
                if (EXTERNAL_SSSE3(mm_flags)) {

                }
#ifdef __SSE4_1__
                if (EXTERNAL_SSE4(mm_flags)) {
                     c->pred_planar[0]= pred_planar_0_8_sse;
                     c->pred_planar[1]= pred_planar_1_8_sse;
                     c->pred_planar[2]= pred_planar_2_8_sse;
                     c->pred_planar[3]= pred_planar_3_8_sse;

                     c->pred_angular[0]= pred_angular_0_8_sse;//removed because too little data = bad performance
                     c->pred_angular[1]= pred_angular_1_8_sse;
                     c->pred_angular[2]= pred_angular_2_8_sse;
                     c->pred_angular[3]= pred_angular_3_8_sse;
                }
#endif // __SSE4_1__
                if (EXTERNAL_AVX(mm_flags)) {

                }
            }
        }
    } else if (bit_depth == 10) {
        if (EXTERNAL_MMX(mm_flags)) {
            if (EXTERNAL_MMXEXT(mm_flags)) {

                if (EXTERNAL_SSE2(mm_flags)) {

                }
                if (EXTERNAL_SSE4(mm_flags)) {
                    c->pred_planar[0]= pred_planar_0_10_sse;
                    c->pred_planar[1]= pred_planar_1_10_sse;
                    c->pred_planar[2]= pred_planar_2_10_sse;
                    c->pred_planar[3]= pred_planar_3_10_sse;

                    c->pred_angular[0]= pred_angular_0_10_sse;//removed because too little data = bad performance
                    c->pred_angular[1]= pred_angular_1_10_sse;
                    c->pred_angular[2]= pred_angular_2_10_sse;
                    c->pred_angular[3]= pred_angular_3_10_sse;
                }
                if (EXTERNAL_AVX(mm_flags)) {
                }
            }
        }
    }
}
