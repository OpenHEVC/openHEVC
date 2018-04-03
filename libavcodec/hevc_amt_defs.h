#ifndef HEVC_AMT_DEFS_H
#define HEVC_AMT_DEFS_H

#define EMT_INTRA_MAX_CU		32
#define EMT_INTER_MAX_CU		32
#define EMT_SIGNUM_THR			 2
#define INTER_MODE_IDX		   255
#define MAX_TU_SIZE				32
#define EMT_TRANSFORM_MATRIX_SHIFT 6
#define COM16_C806_TRANS_PREC	 2

enum EMT_DCTTransformType {
    DCT_II = 0,
    DST_I,     // subset 1
    DST_VII,   // subset 0, 1 et 2
    DCT_VIII,  // subset 0
    DCT_V,     // subset 2
    NUM_TRANS_TYPE,
    DCT2_HEVC,
    DCT2_EMT,
};

#endif // HEVC_AMT_DEFS_H

