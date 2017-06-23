/*
 * openhevc.h wrapper to openhevc or ffmpeg
 * Copyright (c) 2012-2013 Micka�l Raulet, Wassim Hamidouche, Gildas Cocherel, Pierre Edouard Lepere
 *
 * This file is part of openhevc.
 *
 * openHevc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * openhevc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with openhevc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef OPEN_HEVC_WRAPPER_H
#define OPEN_HEVC_WRAPPER_H

#define NV_VERSION  "2.0" ///< Current software version

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdarg.h>
//#include <libavformat/avformat.h>

#define OPENHEVC_HAS_AVC_BASE

#define OH_SELECTIVE_ENCRYPTION

typedef void* OpenHevc_Handle;

typedef struct OpenHevc_Rational{
    int num; ///< numerator
    int den; ///< denominator
} OpenHevc_Rational;


enum ChromaFormat {
    YUV420 = 0,
    YUV422,
    YUV444,
    UNKNOWN_CHROMA_FORMAT
};

typedef struct OpenHevc_FrameInfo
{
   int         nYPitch;
   int         nUPitch;
   int         nVPitch;
   int         nBitDepth;
   int         nWidth;
   int         nHeight;
   int        chromat_format;
   OpenHevc_Rational  sample_aspect_ratio;
   OpenHevc_Rational  frameRate;
   int         display_picture_number;
   int         flag; //progressive, interlaced, interlaced top field first, interlaced bottom field first.
   int64_t     nTimeStamp;
} OpenHevc_FrameInfo;

typedef struct OpenHevc_Frame
{
   void**      pvY;
   void**      pvU;
   void**      pvV;
   OpenHevc_FrameInfo frameInfo;
} OpenHevc_Frame;

typedef struct OpenHevc_Frame_cpy
{
   void*        pvY;
   void*        pvU;
   void*        pvV;
   OpenHevc_FrameInfo frameInfo;
} OpenHevc_Frame_cpy;

OpenHevc_Handle libOpenHevcInit(int nb_pthreads, int thread_type);
OpenHevc_Handle libOpenShvcInit(int nb_pthreads, int thread_type);
OpenHevc_Handle libOpenH264Init(int nb_pthreads, int thread_type);

int libOpenHevcStartDecoder(OpenHevc_Handle openHevcHandle);
int  libOpenHevcDecode(OpenHevc_Handle openHevcHandle, const unsigned char *buff, int nal_len, int64_t pts);
//int libOpenShvcDecode(OpenHevc_Handle openHevcHandle, const AVPacket packet[], const int stop_dec, const int stop_dec2);
int libOpenShvcDecode2(OpenHevc_Handle openHevcHandle, const unsigned char *buff, const unsigned char *buff2, int nal_len, int nal_len2, int64_t pts, int64_t pts2);

void libOpenHevcCopyExtraData(OpenHevc_Handle openHevcHandle, unsigned char *extra_data, int extra_size_alloc);
void libOpenShvcCopyExtraData(OpenHevc_Handle openHevcHandle, unsigned char *extra_data_linf, unsigned char *extra_data_lsup, int extra_size_alloc_linf, int extra_size_allocl_sup);

/**
 * Update the output frame parameters to the layer frame parammeters with layer_id
 * into the decoder output
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param layer_id             The target layer id
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
int oh_get_picture_params_from_layer(OpenHevc_Handle openHevcHandle, unsigned int layer_id,
                                     OpenHevc_FrameInfo *openHevcFrameInfo);

/**
 * Update the output frame parameters from the highest layer frame found in the
 * decoder output
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
void libOpenHevcGetPictureInfo(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo);

/**
 * Update the output frame parameters for cropping
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param layer_id             The target layer id
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
int oh_get_cropped_picture_params_from_layer(OpenHevc_Handle openHevcHandle, unsigned int layer_id,
                                             OpenHevc_FrameInfo *openHevcFrameInfo);

/**
 * Update the cropped output frame parameters from the highest layer frame found in the
 * decoder output
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
void libOpenHevcGetPictureInfoCpy(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo);

/**
 * Update the output frame to the layer frame with id layer_id into the decoder
 * output
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param layer_id             The target layer id
 * @param openHevcFrameInfo    Pointer to the output frame info to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
int oh_get_output_picture_from_layer(OpenHevc_Handle openHevcHandle, int layer_id,
                                     OpenHevc_Frame *openHevcFrame);

/**
 * Update the output frame parameters to the highest layer frame parameters found
 * into the decoder output
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
int oh_get_output_picture(OpenHevc_Handle openHevcHandle, OpenHevc_Frame *openHevcFrame);

/**
 * Update the output frame to the highest layer frame found into the decoder
 * output
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param got_picture          Control parameter
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on every tested layer_id
 */
int  libOpenHevcGetOutput(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame *openHevcFrame);


/**
 * Request a a cropped output copy of the layer frame with id layer_id
 * into the decoder output
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param layer_id             The target layer id
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
int oh_get__cropped_picture_copy_from_layer(OpenHevc_Handle openHevcHandle, int layer_id,
                                   OpenHevc_Frame_cpy *openHevcFrame);

/**
 * Request a cropped output copy of the highest layer frame returned by the decoder
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
int oh_get_cropped_picture_copy(OpenHevc_Handle openHevcHandle,
                        OpenHevc_Frame_cpy *openHevcFrame);

/**
 * request a cropped output copy of the highest layer frame into the decoder output
 * if got_picture > 0
 *
 * @param openHevcHandle       The codec context list of current decoders
 * @param got_picture          Control parameter
 * @param openHevcFrameInfo    Pointer to the output frame parameters to be updated
 *
 * @return 2^layer_id on success, 0 if no frame was found on the target layer_id
 */
int  libOpenHevcGetOutputCpy(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame_cpy *openHevcFrame);


void libOpenHevcSetCheckMD5(OpenHevc_Handle openHevcHandle, int val);

typedef enum
{
	OHEVC_LOG_PANIC = 0,
	OHEVC_LOG_FATAL = 8,
	OHEVC_LOG_ERROR = 16,
	OHEVC_LOG_WARNING = 24,
	OHEVC_LOG_INFO = 32,
	OHEVC_LOG_VERBOSE = 40,
	OHEVC_LOG_DEBUG = 48,
	OHEVC_LOG_TRACE = 56
} OHEVC_LogLevel;

//val is log level used
void libOpenHevcSetDebugMode(OpenHevc_Handle openHevcHandle, OHEVC_LogLevel val);
void libOpenHevcSetLogCallback(OpenHevc_Handle openHevcHandle, void (*callback)(void*, int, const char*, va_list));

void libOpenHevcSetMouseClick(OpenHevc_Handle openHevcHandle, int val_x,int val_y);
void libOpenHevcSetTemporalLayer_id(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetNoCropping(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetActiveDecoders(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcSetViewLayers(OpenHevc_Handle openHevcHandle, int val);
void libOpenHevcClose(OpenHevc_Handle openHevcHandle);
void libOpenHevcFlush(OpenHevc_Handle openHevcHandle);
void libOpenHevcFlushSVC(OpenHevc_Handle openHevcHandle, int decoderId);

void oh_set_crypto_mode(OpenHevc_Handle openHevcHandle, int val);
void oh_set_crypto_key(OpenHevc_Handle openHevcHandle, uint8_t *val);

const char *libOpenHevcVersion(OpenHevc_Handle openHevcHandle);

#ifdef __cplusplus
}
#endif

#endif // OPEN_HEVC_WRAPPER_H
