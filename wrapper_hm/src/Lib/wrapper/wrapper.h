#ifdef __cplusplus
extern "C"
{
#endif
    int libDecoderInit( void );
    int libDecoderDecode( unsigned char *pnalu, int nal_len, unsigned char *Y, unsigned char *U, unsigned char *V, int *gotpicture);
    void libDecoderClose( void );
#ifdef __cplusplus
}
#endif