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

#include "libopenhevc/openhevc.h"
#include <libavformat/avformat.h>
#include <SDL/SDL_events.h>

#define CONFIG_OPENCL 0
#define CONFIG_AVDEVICE 0
#include "cmdutils.h"

#include "config.h"
#include "libavutil/opt.h"
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


static void video_decode(const char *filename,const char *enh_filename)
{
    AVFormatContext *avfctx[2];
    AVPacket        *avpkt[2];

    OHHandle    oh_hdl;
    OHFrame     oh_frame;
    OHFrame_cpy oh_framecpy;

	int AVC_BL = h264_flags;
    int split_layers = enh_filename != NULL;//fixme: we do not check the -e option here
    // && quality_layer_id ?
    int AVC_BL_only = AVC_BL && !split_layers;

    FILE *fout  = NULL;
    int curr_width   = -1;
    int curr_height  = -1;
    int nbFrame = 0;
    int stop    = 0;
    int stop_dec= 0;
    int stop_dec2=0;
    int i= 0;
    int got_picture;
    float time  = 0.0;
    int video_stream_idx;
    char output_file2[256];

    oh_framecpy.data_y = NULL;
    oh_framecpy.data_cb = NULL;
    oh_framecpy.data_cr = NULL;

    if (filename == NULL) {
        printf("No input file specified.\nSpecify it with: -i <filename>\n");
        exit(1);
    }
    /* Call corresponding codecs context initialization
     * */
    if (AVC_BL_only){
        oh_hdl = oh_init_h264(nb_pthreads, thread_type);
    } else if (AVC_BL && split_layers){
    	printf("file name : %s\n", enhance_file);
        oh_hdl = oh_init_lhvc(nb_pthreads, thread_type);
    } else {
        oh_hdl = oh_init(nb_pthreads, thread_type);
    }

    if (!oh_hdl) {
        fprintf(stderr, "OpenHevc intitialization failed\n");
        exit(1);
    }

    av_register_all();
    avfctx[0] = avformat_alloc_context();
    avpkt[0]  = av_packet_alloc();

    if(AVC_BL && split_layers){
        avfctx[1] = avformat_alloc_context();
        avpkt[1]  = av_packet_alloc();
    }

    if(avformat_open_input(&avfctx[0], filename, NULL, NULL)!=0) {
    	fprintf(stderr,"Could not open base layer input file : %s\n",filename);
        exit(1);
    }

    if(split_layers && avformat_open_input(&avfctx[1], enh_filename, NULL, NULL)!=0) {
        fprintf(stderr,"Could not open enhanced layer input file : %s\n",enh_filename);
        exit(1);
    }

    if (!split_layers && quality_layer_id == 1)
        avfctx[0]->video_codec_id=AV_CODEC_ID_SHVC;

    for(i=0; i<2 ; i++){
        size_t extra_size_alloc;
        if ( (video_stream_idx = av_find_best_stream(avfctx[i], AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
			fprintf(stderr, "Could not find video stream in input file\n");
			exit(1);
		}
		//test
        //av_dump_format(avfctx[i], 0, filename, 0);

        extra_size_alloc = avfctx[i]->streams[video_stream_idx]->codecpar->extradata_size > 0 ?
        (avfctx[i]->streams[video_stream_idx]->codecpar->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE) : 0;

        if (extra_size_alloc){
            oh_extradata_cpy(oh_hdl, avfctx[i]->streams[video_stream_idx]->codecpar->extradata, extra_size_alloc);
		}

		if(!split_layers){ //We only need one AVFormatContext layer
			break;
		}

    }
    // OpenHEVC option settings
    oh_set_log_level(oh_hdl, OHEVC_LOG_INFO);
    oh_enable_sei_checksum(oh_hdl, !no_md5);

    // OpenHEVC decoder start
    oh_start(oh_hdl);

    // OpenHEVC option settings
    oh_select_temporal_layer(oh_hdl, temporal_layer_id);
    oh_select_active_layer(oh_hdl, quality_layer_id);
    oh_select_view_layer(oh_hdl, quality_layer_id);


    if (!no_display) {
        oh_display_init(0, 1920, 1080);
    }

    oh_timer_init();
    if(frame_rate > 0)
    	oh_timer_setFPS(frame_rate);

    /* Main loop
     * */
    while(!stop) {
        OHEvent event_code = oh_display_getWindowEvent();
        if (event_code == OH_QUIT)
            break;
        else if (event_code == OH_LAYER0 || event_code == OH_LAYER1) {
            oh_select_active_layer(oh_hdl, event_code-1);
            oh_select_view_layer(oh_hdl, event_code-1);
        }

        // Next packet search with avformat
		if(split_layers){
            if (stop_dec2 == 0 && av_read_frame(avfctx[1], avpkt[1])<0)
                stop_dec2 = 1;
	    }
        if (stop_dec == 0 && av_read_frame(avfctx[0], avpkt[0])<0)
	        stop_dec = 1;

        if ((avpkt[0]->stream_index == video_stream_idx && (!split_layers || avpkt[1]->stream_index == video_stream_idx)) //
                || stop_dec == 1 || stop_dec2==1) {

            // OpenHEVC decoding
            if(split_layers){
                got_picture = oh_decode_lhvc(oh_hdl, avpkt[0]->data, avpkt[1]->data, !stop_dec ? avpkt[0]->size : 0 ,!stop_dec2 ? avpkt[1]->size : 0, avpkt[0]->pts, avpkt[1]->pts);
                av_packet_unref(avpkt[0]);
                av_packet_unref(avpkt[1]);
            }
            else {
                got_picture = oh_decode(oh_hdl, avpkt[0]->data, !stop_dec ? avpkt[0]->size : 0, avpkt[0]->pts);
                av_packet_unref(avpkt[0]);
            }

            // OpenHEVC display and output handling
			if (got_picture > 0) {
				fflush(stdout);

                oh_frameinfo_update(oh_hdl, &oh_frame.frame_par);

                if (curr_width != oh_frame.frame_par.width || curr_height != oh_frame.frame_par.height) {
                    curr_width  = oh_frame.frame_par.width;
                    curr_height = oh_frame.frame_par.height;

					if (fout)
					   fclose(fout);

					if (output_file) {
                        sprintf(output_file2, "%s_%dx%d.yuv", output_file, curr_width, curr_height);
						fout = fopen(output_file2, "wb");
					}

					if (fout) {
                        int chroma_format;

                        oh_frameinfo_update(oh_hdl, &oh_framecpy.frame_par);

                        chroma_format = oh_framecpy.frame_par.chromat_format == OH_YUV420 ? 1 : 0;

                        if(oh_framecpy.data_y) {
                            free(oh_framecpy.data_y);
                            free(oh_framecpy.data_cb);
                            free(oh_framecpy.data_cr);
						}                        
                        oh_framecpy.data_y =  calloc (oh_framecpy.frame_par.linesize_y * oh_framecpy.frame_par.height, sizeof(unsigned char));
                        oh_framecpy.data_cb = calloc (oh_framecpy.frame_par.linesize_cb * oh_framecpy.frame_par.height >> chroma_format, sizeof(unsigned char));
                        oh_framecpy.data_cr = calloc (oh_framecpy.frame_par.linesize_cr * oh_framecpy.frame_par.height >> chroma_format, sizeof(unsigned char));
					}
                }

				if (frame_rate > 0) {
					oh_timer_delay();
				}

				if (!no_display) {
					oh_output_update(oh_hdl, 1, &oh_frame);
					//oh_frameinfo_update(oh_hdl, &oh_frame.frame_par);
					oh_display_display((oh_frame.frame_par.linesize_y - oh_frame.frame_par.width)>>1, oh_frame.frame_par.width, oh_frame.frame_par.height,
								(uint8_t *)oh_frame.data_y_p, (uint8_t *)oh_frame.data_cb_p, (uint8_t *)oh_frame.data_cr_p);
				}

				if (fout) {
                    int format = oh_framecpy.frame_par.chromat_format == OH_YUV420 ? 1 : 0;
                    oh_output_cpy(oh_hdl, 1, &oh_framecpy);
                    fwrite( oh_framecpy.data_y ,  sizeof(uint8_t) , oh_framecpy.frame_par.linesize_y  * oh_framecpy.frame_par.height,           fout);
                    fwrite( oh_framecpy.data_cb , sizeof(uint8_t) , oh_framecpy.frame_par.linesize_cb * oh_framecpy.frame_par.height >> format, fout);
                    fwrite( oh_framecpy.data_cr , sizeof(uint8_t) , oh_framecpy.frame_par.linesize_cr * oh_framecpy.frame_par.height >> format, fout);
				}
				nbFrame++;

                if (nbFrame == num_frames)
					stop = 1;

			}
			if(split_layers){
				if (stop_dec2 > 0 && stop_dec > 0 && nbFrame && !got_picture)
					stop=1;
			} else if (stop_dec > 0 && nbFrame && !got_picture){
		        stop = 1;
			}


		    if (stop_dec >= nb_pthreads && nbFrame == 0) {
                av_packet_unref(avpkt[0]);
                av_packet_free(&avpkt[0]);
                if(split_layers){
                    av_packet_unref(avpkt[1]);
                    av_packet_free(&avpkt[1]);
                }
			    fprintf(stderr, "Error when reading first frame\n");
				exit(1);
			}
        }
    } //End of main loop


    time = oh_timer_getTimeMs()/1000.0;
    oh_display_close();

    if (fout) {
        fclose(fout);
        if(oh_framecpy.data_y) {
            free(oh_framecpy.data_y);
            free(oh_framecpy.data_cb);
            free(oh_framecpy.data_cr);
        }
    }

    av_packet_unref(avpkt[0]);
    av_packet_free(&avpkt[0]);
    if(split_layers){
        av_packet_unref(avpkt[1]);
        av_packet_free(&avpkt[1]);
    }

    if(!split_layers)
        avformat_close_input(&avfctx[0]);
    if(split_layers){
        avformat_close_input(&avfctx[0]);
        avformat_close_input(&avfctx[1]);
    }

    oh_close(oh_hdl);
    printf("frame= %d fps= %.0f time= %.2f video_size= %dx%d\n", nbFrame, nbFrame/time, time, oh_frame.frame_par.width, oh_frame.frame_par.height);
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

    video_decode(input_file, enhance_file);

    return 0;
}

