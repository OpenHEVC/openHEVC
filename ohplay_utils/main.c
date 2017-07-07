/*
 * Copyright (c) 2017, IETR/INSA of Rennes
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of the IETR/INSA of Rennes nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "openHevcWrapper.h"
#include <libavformat/avformat.h>

#define CONFIG_OPENCL 0
#define CONFIG_AVDEVICE 0
#include "cmdutils.h"

#include "ohplay_utils/ohdisplay_wrapper.h"
#include "ohplay_utils/ohtimer_wrapper.h"

const char program_name[] = "ohplay";
const int program_birth_year = 2003;

/* options specified by the user */
static char *program;
static int h264_flags;
static int no_md5;
static int thread_type;
static char *input_file;
static char *enhance_file;
static char no_display;
static char *output_file;
static int nb_pthreads;
static int temporal_layer_id;
static int quality_layer_id;
static int num_frames;
static float frame_rate;
static int crypto_args;
static uint8_t *crypto_key = NULL;

static const OptionDef options[] = {
	{ "h", OPT_EXIT, {.func_arg = show_help}, "show help" },
	{ "-help", OPT_EXIT, {.func_arg = show_help}, "show help" },
    { "c", OPT_BOOL, { &no_md5 }, "no check md5" },
    { "f", HAS_ARG | OPT_INT, { &thread_type }, "1-frame, 2-slice, 4-frameslice", "thread type" },
    { "i", HAS_ARG | OPT_STRING, { &input_file }, "Input file", "file" },
    { "n", OPT_BOOL, { &no_display }, "no display" },
    { "o", HAS_ARG | OPT_STRING, { &output_file }, "Output file", "file" },
    { "p", HAS_ARG | OPT_INT, { &nb_pthreads }, "Pthreads number", "n" },
    { "t", HAS_ARG | OPT_INT, { &temporal_layer_id }, "Temporal layer id", "id" },
    { "l", HAS_ARG | OPT_INT, { &quality_layer_id }, "Quality layer id", "id" },
    { "s", HAS_ARG | OPT_INT, { &num_frames }, "Stop after \"n\" frames", "n" },
    { "r", HAS_ARG | OPT_FLOAT, { &frame_rate }, "Frame rate (FPS)", "n"},
    { "v", OPT_BOOL, { &h264_flags }, "Input is a h264 bitstream" },
    { "e", HAS_ARG | OPT_STRING, { &enhance_file }, "Enhanced layer file (with AVC base)", "file" },
    {"-crypto", HAS_ARG | OPT_ENUM, {&crypto_args}, " Encryption configuration","params"},
    {"-key", HAS_ARG | OPT_DATA, {&crypto_key},"overload default cipher key", "(16 bytes)"},
    { NULL, },
};

static void show_usage(void)
{
    av_log(NULL, AV_LOG_INFO, "OpenHEVC player\n");
    av_log(NULL, AV_LOG_INFO, "usage: %s [options] -i input_file\n", program_name);
    av_log(NULL, AV_LOG_INFO, "\n");
}

void show_help_default(const char *opt, const char *arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:", 0, OPT_EXPERT, 0);
    show_help_options(options, "Advanced options:", OPT_EXPERT, 0, 0);
}

typedef struct OpenHevcWrapperContext {
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *picture;
    AVPacket avpkt;
    AVCodecParserContext *parser;
} OpenHevcWrapperContext;

int find_start_code (unsigned char *Buf, int zeros_in_startcode)
{
    int i;
    for (i = 0; i < zeros_in_startcode; i++)
        if(Buf[i] != 0)
            return 0;
    return Buf[i];
}

int get_next_nal(FILE* inpf, unsigned char* Buf)
{
    int pos = 0;
    int StartCodeFound = 0;
    int info2 = 0;
    int info3 = 0;
    while(!feof(inpf)&&(/*Buf[pos++]=*/fgetc(inpf))==0);

    while (pos < 3) Buf[pos++] = fgetc (inpf);
    while (!StartCodeFound)
    {
        if (feof (inpf))
        {
            //            return -1;
            return pos-1;
        }
        Buf[pos++] = fgetc (inpf);
        info3 = find_start_code(&Buf[pos-4], 3);
        if(info3 != 1)
            info2 = find_start_code(&Buf[pos-3], 2);
        StartCodeFound = (info2 == 1 || info3 == 1);
    }
    fseek (inpf, - 4 + info2, SEEK_CUR);
    return pos - 4 + info2;
}
typedef struct Info {
    int NbFrame;
    int Poc;
    int Tid;
    int Qid;
    int type;
    int size;
} Info;

static void video_decode_example(const char *filename,const char *enh_filename)
{
	AVFormatContext *pFormatCtx[2];
	AVPacket        packet[2];

	int AVC_BL = h264_flags;
    int split_layers = enh_filename != NULL;//fixme: we do not check the -e option here
    int AVC_BL_only = AVC_BL && !split_layers;

    FILE *fout  = NULL;
    int width   = -1;
    int height  = -1;
    int nbFrame = 0;
    int stop    = 0;
    int stop_dec= 0;
    int stop_dec2=0;
    int i= 0;
    int got_picture;
    float time  = 0.0;
    int video_stream_idx;
    char output_file2[256];

    OpenHevc_Frame     openHevcFrame;
    OpenHevc_Frame_cpy openHevcFrameCpy;
    OpenHevc_Handle    openHevcHandle;

    OHMouse oh_mouse;

    if (filename == NULL) {
        printf("No input file specified.\nSpecify it with: -i <filename>\n");
        exit(1);
    }
    /* Call corresponding codecs context initialization
     * */
    if (AVC_BL_only){
        openHevcHandle = libOpenH264Init(nb_pthreads, thread_type/*, pFormatCtx*/);
    } else if (AVC_BL && split_layers){
    	printf("file name : %s\n", enhance_file);
    	openHevcHandle = libOpenShvcInit(nb_pthreads, thread_type/*, pFormatCtx*/);
    } else {
        openHevcHandle = libOpenHevcInit(nb_pthreads, thread_type/*, pFormatCtx*/);
    }

    libOpenHevcSetCheckMD5(openHevcHandle, !no_md5);

    if (!openHevcHandle) {
        fprintf(stderr, "could not open OpenHevc\n");
        exit(1);
    }

    av_register_all();
    pFormatCtx[0] = avformat_alloc_context();

    if(AVC_BL && split_layers)
        pFormatCtx[1] = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx[0], filename, NULL, NULL)!=0) {
    	fprintf(stderr,"Could not open base layer input file : %s\n",filename);
        exit(1);
    }

    if(split_layers && avformat_open_input(&pFormatCtx[1], enh_filename, NULL, NULL)!=0) {
        fprintf(stderr,"Could not open enhanced layer input file : %s\n",enh_filename);
        exit(1);
    }

    if (!split_layers && quality_layer_id == 1)
        pFormatCtx[0]->video_codec_id=AV_CODEC_ID_SHVC;

    for(i=0; i<2 ; i++){
		if ( (video_stream_idx = av_find_best_stream(pFormatCtx[i], AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
			fprintf(stderr, "Could not find video stream in input file\n");
			exit(1);
		}
		//test
		//av_dump_format(pFormatCtx[i], 0, filename, 0);

        const size_t extra_size_alloc = pFormatCtx[i]->streams[video_stream_idx]->codecpar->extradata_size > 0 ?
        (pFormatCtx[i]->streams[video_stream_idx]->codecpar->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE) : 0;

        if (extra_size_alloc){
            libOpenHevcCopyExtraData(openHevcHandle, pFormatCtx[i]->streams[video_stream_idx]->codecpar->extradata, extra_size_alloc);
		}

		if(!split_layers){ //We only need one AVFormatContext layer
			break;
		}

    }

    libOpenHevcSetDebugMode(openHevcHandle, OHEVC_LOG_INFO);
    libOpenHevcStartDecoder(openHevcHandle);
    oh_set_crypto_mode(openHevcHandle,crypto_args);
    if(crypto_key!=NULL)
        oh_set_crypto_key(openHevcHandle, crypto_key);

    openHevcFrameCpy.pvY = NULL;
    openHevcFrameCpy.pvU = NULL;
    openHevcFrameCpy.pvV = NULL;

	if (!no_display) {
        oh_display_init(0, 1920, 1080);
    }

    oh_timer_init();
    if(frame_rate > 0)
    	oh_timer_setFPS(frame_rate);
   
    libOpenHevcSetTemporalLayer_id(openHevcHandle, temporal_layer_id);
    libOpenHevcSetActiveDecoders(openHevcHandle, quality_layer_id);
    libOpenHevcSetViewLayers(openHevcHandle, quality_layer_id);

    /* Main loop
     * */
    while(!stop) {
        OHEvent event_code = oh_display_getWindowEvent();
        if (event_code == OH_QUIT)
            break;
        else if (event_code == OH_LAYER0 || event_code == OH_LAYER1) {
            libOpenHevcSetActiveDecoders(openHevcHandle, event_code-1);
            libOpenHevcSetViewLayers(openHevcHandle, event_code-1);
        } else if (event_code == OH_MOUSE) {
            oh_mouse = oh_display_getMouseEvent();
            libOpenHevcSetMouseClick(openHevcHandle,oh_mouse.x,oh_mouse.y);
        }

        //if (packet[0].stream_index && packet[1].stream_index == video_stream_idx || stop_dec > 0) {
        //got_picture = libOpenHevcDecode(openHevcHandle, packet.data, !stop_dec ? packet.size : 0, packet.pts);

        /* Try to read packets corresponding to the frames to be decoded
         * */
		if(split_layers){
			if (stop_dec2 == 0 && av_read_frame(pFormatCtx[1], &packet[1])<0)
                stop_dec2 = 1;
	    }
	    if (stop_dec == 0 && av_read_frame(pFormatCtx[0], &packet[0])<0)
	        stop_dec = 1;

		if ((packet[0].stream_index == video_stream_idx && (!split_layers || packet[1].stream_index == video_stream_idx)) //
				|| stop_dec == 1 || stop_dec2==1) {
		/* Try to decode corresponding packets into AVFrames
		 * */
			if(split_layers)
                got_picture = libOpenShvcDecode2(openHevcHandle, packet[0].data, packet[1].data, !stop_dec ? packet[0].size : 0 ,!stop_dec2 ? packet[1].size : 0, packet[0].pts, packet[1].pts);
			else
				got_picture = libOpenHevcDecode(openHevcHandle, packet[0].data, !stop_dec ? packet[0].size : 0, packet[0].pts);

			/* Output and display handling
			 * */
			if (got_picture > 0) {
				fflush(stdout);
				/* Frames parameters update (intended for first computation or in case of frame resizing)
				* */
				libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrame.frameInfo);

				if ((width != openHevcFrame.frameInfo.nWidth) || (height != openHevcFrame.frameInfo.nHeight)) {
				    width  = openHevcFrame.frameInfo.nWidth;
				    height = openHevcFrame.frameInfo.nHeight;

					if (fout)
					   fclose(fout);

					if (output_file) {
						sprintf(output_file2, "%s_%dx%d.yuv", output_file, width, height);
						fout = fopen(output_file2, "wb");
					}

					if (fout) {
						libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrameCpy.frameInfo);
						int format = openHevcFrameCpy.frameInfo.chromat_format == YUV420 ? 1 : 0;
						if(openHevcFrameCpy.pvY) {
							free(openHevcFrameCpy.pvY);
							free(openHevcFrameCpy.pvU);
							free(openHevcFrameCpy.pvV);
						}
						openHevcFrameCpy.pvY = calloc (openHevcFrameCpy.frameInfo.nYPitch * openHevcFrameCpy.frameInfo.nHeight, sizeof(unsigned char));
						openHevcFrameCpy.pvU = calloc (openHevcFrameCpy.frameInfo.nUPitch * openHevcFrameCpy.frameInfo.nHeight >> format, sizeof(unsigned char));
						openHevcFrameCpy.pvV = calloc (openHevcFrameCpy.frameInfo.nVPitch * openHevcFrameCpy.frameInfo.nHeight >> format, sizeof(unsigned char));
					}
				}// end of frame resizing

				if (frame_rate > 0) {
					oh_timer_delay();
				}

				if (!no_display) {
					libOpenHevcGetOutput(openHevcHandle, 1, &openHevcFrame);
					libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrame.frameInfo);
					oh_display_display((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight,
                                (uint8_t *)openHevcFrame.pvY, (uint8_t *)openHevcFrame.pvU, (uint8_t *)openHevcFrame.pvV);
				}

				/* Write output file if any
				 * */
				if (fout) {
					int format = openHevcFrameCpy.frameInfo.chromat_format == YUV420 ? 1 : 0;
					libOpenHevcGetOutputCpy(openHevcHandle, 1, &openHevcFrameCpy);
					fwrite( openHevcFrameCpy.pvY , sizeof(uint8_t) , openHevcFrameCpy.frameInfo.nYPitch * openHevcFrameCpy.frameInfo.nHeight, fout);
					fwrite( openHevcFrameCpy.pvU , sizeof(uint8_t) , openHevcFrameCpy.frameInfo.nUPitch * openHevcFrameCpy.frameInfo.nHeight >> format, fout);
					fwrite( openHevcFrameCpy.pvV , sizeof(uint8_t) , openHevcFrameCpy.frameInfo.nVPitch * openHevcFrameCpy.frameInfo.nHeight >> format, fout);
				}
				nbFrame++;

				if (nbFrame == num_frames)// we already decoded all the frames we wanted to
					stop = 1;

			}
			if(split_layers){
				if (stop_dec2 > 0 && stop_dec > 0 && nbFrame && !got_picture)
					stop=1;
			} else if (stop_dec > 0 && nbFrame && !got_picture){
		        stop = 1;
			}
            av_packet_unref(&packet[0]);
            if(split_layers)
                av_packet_unref(&packet[1]);

		    if (stop_dec >= nb_pthreads && nbFrame == 0) {
                av_packet_unref(&packet[0]);
			    if(split_layers)
                    av_packet_unref(&packet[1]);
			    fprintf(stderr, "Error when reading first frame\n");
				exit(1);
			}
		}// End of got_packet
    } //End of main loop

    time = oh_timer_getTimeMs()/1000.0;
    oh_display_close();

    if (fout) {
        fclose(fout);
        if(openHevcFrameCpy.pvY) {
            free(openHevcFrameCpy.pvY);
            free(openHevcFrameCpy.pvU);
            free(openHevcFrameCpy.pvV);
        }
    }
    if(!split_layers)
        avformat_close_input(&pFormatCtx[0]);
    if(split_layers){
        avformat_close_input(&pFormatCtx[0]);
        avformat_close_input(&pFormatCtx[1]);
    }
    libOpenHevcClose(openHevcHandle);

    printf("frame= %d fps= %.0f time= %.2f video_size= %dx%d\n", nbFrame, nbFrame/time, time, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight);
}

int main(int argc, char *argv[]) {
    h264_flags        = 0;
    no_md5			  = 0;
    thread_type       = 1;
    input_file        = NULL;
    enhance_file 	  = NULL;
    no_display	      = 0;
    output_file       = NULL;
    nb_pthreads       = 1;
    temporal_layer_id = 7;
    quality_layer_id  = 0; // Base layer
    num_frames        = 0;
    frame_rate        = 0;

    program           = argv[0];

    parse_loglevel(argc, argv, options);
    av_register_all();
    avformat_network_init();
  
    init_opts();

    show_banner(argc, argv, options);

    parse_options(NULL, argc, argv, options, NULL);

    video_decode_example(input_file, enhance_file);

    return 0;
}
