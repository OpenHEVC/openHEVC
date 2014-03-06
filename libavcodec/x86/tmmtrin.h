/*===---- tmmintrin.h - SSSE3 intrinsics -----------------------------------===
00002  *
00003  * Permission is hereby granted, free of charge, to any person obtaining a copy
00004  * of this software and associated documentation files (the "Software"), to deal
00005  * in the Software without restriction, including without limitation the rights
00006  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
00007  * copies of the Software, and to permit persons to whom the Software is
00008  * furnished to do so, subject to the following conditions:
00009  *
00010  * The above copyright notice and this permission notice shall be included in
00011  * all copies or substantial portions of the Software.
00012  *
00013  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
00014  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
00015  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
00016  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
00017  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
00018  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
00019  * THE SOFTWARE.
00020  *
00021  *===-----------------------------------------------------------------------===
00022  */
00023  
00024 #ifndef __TMMINTRIN_H
00025 #define __TMMINTRIN_H
00026 
00027 #ifndef __SSSE3__
00028 #error "SSSE3 instruction set not enabled"
00029 #else
00030 
00031 #include <pmmintrin.h>
00032 
00033 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00034 _mm_abs_pi8(__m64 __a)
00035 {
00036     return (__m64)__builtin_ia32_pabsb((__v8qi)__a);
00037 }
00038 
00039 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00040 _mm_abs_epi8(__m128i __a)
00041 {
00042     return (__m128i)__builtin_ia32_pabsb128((__v16qi)__a);
00043 }
00044 
00045 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00046 _mm_abs_pi16(__m64 __a)
00047 {
00048     return (__m64)__builtin_ia32_pabsw((__v4hi)__a);
00049 }
00050 
00051 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00052 _mm_abs_epi16(__m128i __a)
00053 {
00054     return (__m128i)__builtin_ia32_pabsw128((__v8hi)__a);
00055 }
00056 
00057 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00058 _mm_abs_pi32(__m64 __a)
00059 {
00060     return (__m64)__builtin_ia32_pabsd((__v2si)__a);
00061 }
00062 
00063 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00064 _mm_abs_epi32(__m128i __a)
00065 {
00066     return (__m128i)__builtin_ia32_pabsd128((__v4si)__a);
00067 }
00068 
00069 #define _mm_alignr_epi8(a, b, n) __extension__ ({ \
00070   __m128i __a = (a); \
00071   __m128i __b = (b); \
00072   (__m128i)__builtin_ia32_palignr128((__v16qi)__a, (__v16qi)__b, (n)); })
00073 
00074 #define _mm_alignr_pi8(a, b, n) __extension__ ({ \
00075   __m64 __a = (a); \
00076   __m64 __b = (b); \
00077   (__m64)__builtin_ia32_palignr((__v8qi)__a, (__v8qi)__b, (n)); })
00078 
00079 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00080 _mm_hadd_epi16(__m128i __a, __m128i __b)
00081 {
00082     return (__m128i)__builtin_ia32_phaddw128((__v8hi)__a, (__v8hi)__b);
00083 }
00084 
00085 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00086 _mm_hadd_epi32(__m128i __a, __m128i __b)
00087 {
00088     return (__m128i)__builtin_ia32_phaddd128((__v4si)__a, (__v4si)__b);
00089 }
00090 
00091 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00092 _mm_hadd_pi16(__m64 __a, __m64 __b)
00093 {
00094     return (__m64)__builtin_ia32_phaddw((__v4hi)__a, (__v4hi)__b);
00095 }
00096 
00097 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00098 _mm_hadd_pi32(__m64 __a, __m64 __b)
00099 {
00100     return (__m64)__builtin_ia32_phaddd((__v2si)__a, (__v2si)__b);
00101 }
00102 
00103 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00104 _mm_hadds_epi16(__m128i __a, __m128i __b)
00105 {
00106     return (__m128i)__builtin_ia32_phaddsw128((__v8hi)__a, (__v8hi)__b);
00107 }
00108 
00109 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00110 _mm_hadds_pi16(__m64 __a, __m64 __b)
00111 {
00112     return (__m64)__builtin_ia32_phaddsw((__v4hi)__a, (__v4hi)__b);
00113 }
00114 
00115 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00116 _mm_hsub_epi16(__m128i __a, __m128i __b)
00117 {
00118     return (__m128i)__builtin_ia32_phsubw128((__v8hi)__a, (__v8hi)__b);
00119 }
00120 
00121 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00122 _mm_hsub_epi32(__m128i __a, __m128i __b)
00123 {
00124     return (__m128i)__builtin_ia32_phsubd128((__v4si)__a, (__v4si)__b);
00125 }
00126 
00127 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00128 _mm_hsub_pi16(__m64 __a, __m64 __b)
00129 {
00130     return (__m64)__builtin_ia32_phsubw((__v4hi)__a, (__v4hi)__b);
00131 }
00132 
00133 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00134 _mm_hsub_pi32(__m64 __a, __m64 __b)
00135 {
00136     return (__m64)__builtin_ia32_phsubd((__v2si)__a, (__v2si)__b);
00137 }
00138 
00139 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00140 _mm_hsubs_epi16(__m128i __a, __m128i __b)
00141 {
00142     return (__m128i)__builtin_ia32_phsubsw128((__v8hi)__a, (__v8hi)__b);
00143 }
00144 
00145 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00146 _mm_hsubs_pi16(__m64 __a, __m64 __b)
00147 {
00148     return (__m64)__builtin_ia32_phsubsw((__v4hi)__a, (__v4hi)__b);
00149 }
00150 
00151 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00152 _mm_maddubs_epi16(__m128i __a, __m128i __b)
00153 {
00154     return (__m128i)__builtin_ia32_pmaddubsw128((__v16qi)__a, (__v16qi)__b);
00155 }
00156 
00157 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00158 _mm_maddubs_pi16(__m64 __a, __m64 __b)
00159 {
00160     return (__m64)__builtin_ia32_pmaddubsw((__v8qi)__a, (__v8qi)__b);
00161 }
00162 
00163 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00164 _mm_mulhrs_epi16(__m128i __a, __m128i __b)
00165 {
00166     return (__m128i)__builtin_ia32_pmulhrsw128((__v8hi)__a, (__v8hi)__b);
00167 }
00168 
00169 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00170 _mm_mulhrs_pi16(__m64 __a, __m64 __b)
00171 {
00172     return (__m64)__builtin_ia32_pmulhrsw((__v4hi)__a, (__v4hi)__b);
00173 }
00174 
00175 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00176 _mm_shuffle_epi8(__m128i __a, __m128i __b)
00177 {
00178     return (__m128i)__builtin_ia32_pshufb128((__v16qi)__a, (__v16qi)__b);
00179 }
00180 
00181 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00182 _mm_shuffle_pi8(__m64 __a, __m64 __b)
00183 {
00184     return (__m64)__builtin_ia32_pshufb((__v8qi)__a, (__v8qi)__b);
00185 }
00186 
00187 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00188 _mm_sign_epi8(__m128i __a, __m128i __b)
00189 {
00190     return (__m128i)__builtin_ia32_psignb128((__v16qi)__a, (__v16qi)__b);
00191 }
00192 
00193 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00194 _mm_sign_epi16(__m128i __a, __m128i __b)
00195 {
00196     return (__m128i)__builtin_ia32_psignw128((__v8hi)__a, (__v8hi)__b);
00197 }
00198 
00199 static __inline__ __m128i __attribute__((__always_inline__, __nodebug__))
00200 _mm_sign_epi32(__m128i __a, __m128i __b)
00201 {
00202     return (__m128i)__builtin_ia32_psignd128((__v4si)__a, (__v4si)__b);
00203 }
00204 
00205 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00206 _mm_sign_pi8(__m64 __a, __m64 __b)
00207 {
00208     return (__m64)__builtin_ia32_psignb((__v8qi)__a, (__v8qi)__b);
00209 }
00210 
00211 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00212 _mm_sign_pi16(__m64 __a, __m64 __b)
00213 {
00214     return (__m64)__builtin_ia32_psignw((__v4hi)__a, (__v4hi)__b);
00215 }
00216 
00217 static __inline__ __m64 __attribute__((__always_inline__, __nodebug__))
00218 _mm_sign_pi32(__m64 __a, __m64 __b)
00219 {
00220     return (__m64)__builtin_ia32_psignd((__v2si)__a, (__v2si)__b);
00221 }
00222 
00223 #endif /* __SSSE3__ */
00224 
00225 #endif /* __TMMINTRIN_H */