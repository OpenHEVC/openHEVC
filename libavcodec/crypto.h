#include <stdio.h>
#include <math.h>
#include <config.h>
#include <stdint.h>

#define AESEncryptionStreamMode      1
#if HEVC_ENCRYPTION
#ifdef __cplusplus
extern "C" {
#endif
    typedef void* Crypto_Handle;
    Crypto_Handle CreateC();
    void InitC(Crypto_Handle hdl, uint8_t *init_val);
    void DecryptC(Crypto_Handle hdl, const unsigned char *in_stream, int size_bits, unsigned char  *out_stream);
#if AESEncryptionStreamMode
    unsigned int ff_get_key (Crypto_Handle *hdl, int nb_bits);
#endif
    void DeleteCryptoC(Crypto_Handle hdl);
#ifdef __cplusplus
}
#endif
#else
#include <assert.h>
#include "libavutil/attributes.h"

typedef void* Crypto_Handle;
static av_always_inline Crypto_Handle CreateC() {
    assert(0);
    return 0;
}
static av_always_inline void InitC(Crypto_Handle hdl, uint8_t *init_val) {
    assert(0);
}
static av_always_inline void DecryptC(Crypto_Handle hdl, const unsigned char *in_stream,
                            int size_bits, unsigned char  *out_stream)
{
    assert(0);
}

#if AESEncryptionStreamMode
static av_always_inline unsigned int ff_get_key(Crypto_Handle *hdl, int nb_bits)
{
    assert(0);
    return 0;
}
#endif

static av_always_inline void DeleteCryptoC(Crypto_Handle hdl)
{
    assert(0);
}
#endif
