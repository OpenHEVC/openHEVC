//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "libopenhevc/openhevc.h"
#include "main_hm/getopt.h"
#include <libavformat/avformat.h>
#include "main_hm/sdl_wrapper.h"

//#define TIME2

#ifdef TIME2
#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
//#include <ctime>
#endif


/* Returns the amount of milliseconds elapsed since the UNIX epoch. Works on both
 * windows and linux. */

static unsigned long int GetTimeMs64()
{
#ifdef WIN32
    /* Windows */
    FILETIME ft;
    LARGE_INTEGER li;
    
    /* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
     * to a LARGE_INTEGER structure. */
    GetSystemTimeAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    
    uint64_t ret = li.QuadPart;
    ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
    ret /= 10000; /* From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals */
    
    return ret;
#else
    /* Linux */
    struct timeval tv;
    
    gettimeofday(&tv, NULL);
    
    unsigned long int ret = tv.tv_usec;
    /* Convert from micro seconds (10^-6) to milliseconds (10^-3) */
    //ret /= 1000;
    
    /* Adds the seconds (10^0) after converting them to milliseconds (10^-3) */
    ret += (tv.tv_sec * 1000000);
    
    return ret;
#endif
}
#endif

//typedef struct OHContext {
//    AVCodec *codec;
//    AVCodecContext *c;
//    AVFrame *picture;
//    AVPacket avpkt;
//    AVCodecParserContext *parser;
//} OHContext;


int h264_flags;
int check_md5_flags;
int thread_type;
char *input_file;
char *enhance_file;
char display_flags;
char *output_file;
int nb_pthreads;
int temporal_layer_id;
int quality_layer_id;
int no_cropping;
int num_frames;
float frame_rate;

static void video_decode_example(const char *filename,const char *enh_filename)
{
    AVFormatContext *avfctx[2];
    AVPacket        avpkt[2];

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
#ifdef TIME2
    long unsigned int time_us = 0;
#endif
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

    if(AVC_BL && split_layers)
        avfctx[1] = avformat_alloc_context();

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
        if ( (video_stream_idx = av_find_best_stream(avfctx[i], AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
			fprintf(stderr, "Could not find video stream in input file\n");
			exit(1);
		}
		//test
        //av_dump_format(avfctx[i], 0, filename, 0);

        const size_t extra_size_alloc = avfctx[i]->streams[video_stream_idx]->codecpar->extradata_size > 0 ?
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
    oh_enable_sei_checksum(oh_hdl, check_md5_flags);

    // OpenHEVC decoder start
    oh_start(oh_hdl);

    // OpenHEVC option settings
    oh_select_temporal_layer(oh_hdl, temporal_layer_id);
    oh_select_active_layer(oh_hdl, quality_layer_id);
    oh_select_view_layer(oh_hdl, quality_layer_id);

#if USE_SDL
    Init_Time();
    if (frame_rate > 0) {
        initFramerate_SDL();
        setFramerate_SDL(frame_rate);
    }
#endif
#ifdef TIME2
    time_us = GetTimeMs64();
#endif
   
    /* Main loop
     * */
    while(!stop) {
        oh_event event_code = IsCloseWindowEvent();
        if (event_code == OH_QUIT)
            break;
        else if (event_code == OH_LAYER0 || event_code == OH_LAYER1) {
            oh_select_active_layer(oh_hdl, event_code-1);
            oh_select_view_layer(oh_hdl, event_code-1);
        }

        // Next packet search with avformat
		if(split_layers){
            if (stop_dec2 == 0 && av_read_frame(avfctx[1], &avpkt[1])<0)
                stop_dec2 = 1;
	    }
        if (stop_dec == 0 && av_read_frame(avfctx[0], &avpkt[0])<0)
	        stop_dec = 1;

        if ((avpkt[0].stream_index == video_stream_idx && (!split_layers || avpkt[1].stream_index == video_stream_idx)) //
                || stop_dec == 1 || stop_dec2==1) {

            // OpenHEVC decoding
			if(split_layers)
                got_picture = oh_decode_lhvc(oh_hdl, avpkt[0].data, avpkt[1].data, !stop_dec ? avpkt[0].size : 0 ,!stop_dec2 ? avpkt[1].size : 0, avpkt[0].pts, avpkt[1].pts);
			else
                got_picture = oh_decode(oh_hdl, avpkt[0].data, !stop_dec ? avpkt[0].size : 0, avpkt[0].pts);

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
#if USE_SDL
                    if (display_flags == ENABLE) {
                        Init_SDL((oh_frame.frame_par.linesize_y - oh_frame.frame_par.width)/2, oh_frame.frame_par.width, oh_frame.frame_par.height);
                    }
#endif
					if (fout) {

                        oh_frameinfo_update(oh_hdl, &oh_framecpy.frame_par);

                        int format = oh_framecpy.frame_par.chromat_format == OH_YUV420 ? 1 : 0;

                        if(oh_framecpy.data_y) {
                            free(oh_framecpy.data_y);
                            free(oh_framecpy.data_cb);
                            free(oh_framecpy.data_cr);
						}                        
                        oh_framecpy.data_y =  calloc (oh_framecpy.frame_par.linesize_y * oh_framecpy.frame_par.height, sizeof(unsigned char));
                        oh_framecpy.data_cb = calloc (oh_framecpy.frame_par.linesize_cb * oh_framecpy.frame_par.height >> format, sizeof(unsigned char));
                        oh_framecpy.data_cr = calloc (oh_framecpy.frame_par.linesize_cr * oh_framecpy.frame_par.height >> format, sizeof(unsigned char));
					}
                }
#if USE_SDL
                if (frame_rate > 0) {
                    framerateDelay_SDL();
                }

                if (display_flags == ENABLE) {
                    oh_output_update(oh_hdl, 1, &oh_frame);
                    //oh_frameinfo_update(oh_hdl, &oh_frame.frame_par);
                    SDL_Display((oh_frame.frame_par.linesize_y - oh_frame.frame_par.width)>>1, oh_frame.frame_par.width, oh_frame.frame_par.height,
                                (uint8_t *)oh_frame.data_y_p, (uint8_t *)oh_frame.data_cb_p, (uint8_t *)oh_frame.data_cr_p);
                }
#endif
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
                av_packet_unref(&avpkt[0]);
			    if(split_layers)
                    av_packet_unref(&avpkt[1]);
			    fprintf(stderr, "Error when reading first frame\n");
				exit(1);
			}
        }
    } //End of main loop
#if USE_SDL
    time = SDL_GetTime()/1000.0;
#ifdef TIME2
    time_us = GetTimeMs64() - time_us;
#endif
    CloseSDLDisplay();
#endif
    if (fout) {
        fclose(fout);
        if(oh_framecpy.data_y) {
            free(oh_framecpy.data_y);
            free(oh_framecpy.data_cb);
            free(oh_framecpy.data_cr);
        }
    }
    if(!split_layers)
        avformat_close_input(&avfctx[0]);
    if(split_layers){
        avformat_close_input(&avfctx[0]);
        avformat_close_input(&avfctx[1]);
    }
    oh_close(oh_hdl);
#if USE_SDL
#ifdef TIME2
    printf("frame= %d fps= %.0f time= %ld video_size= %dx%d\n", nbFrame, nbFrame/time, time_us, oh_frame.frame_par.width, oh_frame.frame_par.height);
#else
    printf("frame= %d fps= %.0f time= %.2f video_size= %dx%d\n", nbFrame, nbFrame/time, time, oh_frame.frame_par.width, oh_frame.frame_par.height);
#endif
#endif
}

int main(int argc, char *argv[]) {
    init_main(argc, argv);
    video_decode_example(input_file, enhance_file);

    return 0;
}

