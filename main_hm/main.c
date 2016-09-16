//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "openHevcWrapper.h"
#include "getopt.h"
#include <libavformat/avformat.h>
#include "sdl_wrapper.h"

//#define TIME2

#ifdef TIME2
#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
//#include <ctime>
#endif
#define FRAME_CONCEALMENT   0


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

#if FRAME_CONCEALMENT
    FILE *fin_loss = NULL, *fin1 = NULL;
    Info info;
    Info info_loss;
    char filename0[1024];
    int is_received = 1;
#endif
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
#ifdef TIME2
    long unsigned int time_us = 0;
#endif
    int video_stream_idx;
    char output_file2[256];

    OpenHevc_Frame     openHevcFrame;
    OpenHevc_Frame_cpy openHevcFrameCpy;
    OpenHevc_Handle    openHevcHandle;

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

    libOpenHevcSetCheckMD5(openHevcHandle, check_md5_flags);

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

    libOpenHevcSetDebugMode(openHevcHandle, 0);
    libOpenHevcStartDecoder(openHevcHandle);

    openHevcFrameCpy.pvY = NULL;
    openHevcFrameCpy.pvU = NULL;
    openHevcFrameCpy.pvV = NULL;

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
   
    libOpenHevcSetTemporalLayer_id(openHevcHandle, temporal_layer_id);
    libOpenHevcSetActiveDecoders(openHevcHandle, quality_layer_id);
    libOpenHevcSetViewLayers(openHevcHandle, quality_layer_id);

#if FRAME_CONCEALMENT
    fin_loss = fopen( "/Users/wassim/Softwares/shvc_transmission/parser/hevc_parser/BascketBall_Loss.txt", "rb");
    fin1 = fopen( "/Users/wassim/Softwares/shvc_transmission/parser/hevc_parser/BascketBall.txt", "rb");
    sprintf(filename0, "%s \n", "Nbframe  Poc Tid  Qid  NalType Length");
    fread ( filename0, strlen(filename), 1, fin_loss);
    fread ( filename0, strlen(filename), 1, fin1);
#endif
    /* Main loop
     * */
    while(!stop) {
#if FRAME_CONCEALMENT
        // Get the corresponding frame in the trace
        if(is_received)
            fscanf(fin_loss, "%d    %d    %d    %d    %d        %d \n", &info_loss.NbFrame, &info_loss.Poc, &info_loss.Tid, &info_loss.Qid, &info_loss.type, &info_loss.size);
        fscanf(fin1, "%d    %d    %d    %d    %d        %d \n", &info.NbFrame, &info.Poc, &info.Tid, &info.Qid, &info.type, &info.size);
        if(info_loss.NbFrame == info.NbFrame)
            is_received = 1;
        else
            is_received = 0;
#endif
        //if (packet[0].stream_index && packet[1].stream_index == video_stream_idx || stop_dec > 0) {
#if FRAME_CONCEALMENT
            if(is_received)
                got_picture = libOpenHevcDecode(openHevcHandle, packet.data, !stop_dec ? packet.size : 0, packet.pts);
            else
                got_picture = libOpenHevcDecode(openHevcHandle, NULL,  0, packet.pts);
#else
            //got_picture = libOpenHevcDecode(openHevcHandle, packet.data, !stop_dec ? packet.size : 0, packet.pts);
#endif
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
	#if USE_SDL
						if (display_flags == ENABLE) {
							Init_SDL((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight);
						}
	#endif
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
	#if USE_SDL
					if (frame_rate > 0) {
						framerateDelay_SDL();
					}
					if (display_flags == ENABLE) {
						libOpenHevcGetOutput(openHevcHandle, 1, &openHevcFrame);
						libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrame.frameInfo);
						SDL_Display((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight,
                                (uint8_t *)openHevcFrame.pvY, (uint8_t *)openHevcFrame.pvU, (uint8_t *)openHevcFrame.pvV);
					}
	#endif
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


		    if (stop_dec >= nb_pthreads && nbFrame == 0) {
                av_packet_unref(&packet[0]);
			    if(split_layers)
                    av_packet_unref(&packet[1]);
			    fprintf(stderr, "Error when reading first frame\n");
				exit(1);
			}
		}// End of got_packet
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
#if USE_SDL
#ifdef TIME2
    printf("frame= %d fps= %.0f time= %ld video_size= %dx%d\n", nbFrame, nbFrame/time, time_us, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight);
#else
    printf("frame= %d fps= %.0f time= %.2f video_size= %dx%d\n", nbFrame, nbFrame/time, time, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight);
#endif
#endif
}

int main(int argc, char *argv[]) {
    init_main(argc, argv);
    video_decode_example(input_file, enhance_file);

    return 0;
}

