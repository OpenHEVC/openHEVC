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

#include <QPushButton>
#include <QTreeView>
#include <QBoxLayout>
#include <QFileDialog>
#include <QStandardPaths>
#include <QGraphicsScene>

#include "openHevcWrapper.h"
#ifdef __cplusplus
extern "C"
{
    #include <libavformat/avformat.h>
    #include "ohplay_utils/ohtimer_wrapper.h"
}
#endif

#include "ui_mainohvisualizer.h"
#include "mainohvisualizer.h"
#include "ohvznavigationview.h"
#include "ohvzframeview.h"
#include "ohvzglframeview.h"
#include "ohvzmodel.h"

MainOhVisualizer::MainOhVisualizer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainOhVisualizer)
{
//    filename = "/home/asanchez/sequences/HEVC/ld_main/KristenAndSara_1280x720_60_qp27.bin";
    filename = "/home/asanchez/sequences/4K-Kvazaar/RaceHorses_832x480_30_inter.hevc";
    enh_filename = "";

    h264_flags        = 0;
    no_md5			  = 1;
    thread_type       = 1;
    input_file        = NULL;
    enhance_file 	  = NULL;
    no_display	      = 0;
    output_file       = NULL;
    nb_pthreads       = 1;
    temporal_layer_id = 7;
    quality_layer_id  = 0; // Base layer
    num_frames        = 50;
    frame_rate        = 1;

//    this->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    m_model = new ohvzModel();

    m_infoView = new QTreeView();
    m_infoView->header()->hide();
    m_infoView->setModel(m_model);
    m_infoView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_infoView->setAlternatingRowColors(true);

//    m_frameView = new ohvzGLFrameView(m_model, this);
    m_frameView = new ohvzFrameView(m_model, this);

    m_openButton = new QPushButton();
    m_openButton->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    connect(m_openButton, &QAbstractButton::clicked, this, &MainOhVisualizer::openFile);

    m_playButton = new QPushButton();
    m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(m_playButton, &QAbstractButton::clicked, this, &MainOhVisualizer::video_decode);

    m_navigationView = new ohvzNavigationView(m_model);

    QBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->setMargin(0);
    controlLayout->addWidget(m_openButton);
    controlLayout->addWidget(m_playButton);
//    controlLayout->addWidget(positionSlider);

    QBoxLayout *displayLayout = new QHBoxLayout;
    displayLayout->setMargin(0);
    displayLayout->addWidget(m_frameView);
    displayLayout->addWidget(m_infoView);
    displayLayout->setStretch(0,1);

    QBoxLayout *navigLayout = new QHBoxLayout;
    navigLayout->setMargin(0);
    navigLayout->addWidget(m_navigationView);

    QBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(displayLayout);
    layout->addLayout(navigLayout);
    layout->addLayout(controlLayout);
//    layout->addWidget(errorLabel);

    setLayout(layout);
}

MainOhVisualizer::~MainOhVisualizer()
{
    delete m_infoView;
    delete m_frameView;
    delete m_navigationView;
    delete ui;
}

void MainOhVisualizer::closeEvent(QCloseEvent*)
{
    qApp->quit();
}

void MainOhVisualizer::video_decode()
{
    AVFormatContext *pFormatCtx[2];
    AVPacket        packet[2];

    int AVC_BL = h264_flags;
    int split_layers = (!enh_filename.empty());//fixme: we do not check the -e option here
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

    if (filename.empty()) {
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

    if(avformat_open_input(&pFormatCtx[0], filename.c_str(), NULL, NULL)!=0) {
        fprintf(stderr,"Could not open base layer input file : %s\n",filename.c_str());
        exit(1);
    }

    if(split_layers && avformat_open_input(&pFormatCtx[1], enh_filename.c_str(), NULL, NULL)!=0) {
        fprintf(stderr,"Could not open enhanced layer input file : %s\n",enh_filename.c_str());
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

    openHevcFrameCpy.pvY = NULL;
    openHevcFrameCpy.pvU = NULL;
    openHevcFrameCpy.pvV = NULL;

    if (!no_display) {
        oh_display_init(0, 0, 0);
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

                    // //////////////////////////////////////////////////////////////////////////////////////
                    //!TODO ASN
                    m_model->update(&openHevcFrame, openHevcHandle);
                    m_frameView->display();
                    m_navigationView->update();
                    // //////////////////////////////////////////////////////////////////////////////////////
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

OHMouse MainOhVisualizer::oh_display_getMouseEvent(){
    return (OHMouse){.x = 0, .y = 0, .on = 0};
}

OHEvent MainOhVisualizer::oh_display_getWindowEvent(){
    return OH_NOEVENT;
}

int MainOhVisualizer::oh_display_init(int edge, int frame_width, int frame_height){
    m_playButton->setEnabled(false);
    m_openButton->setEnabled(false);
    m_model->init();
    m_navigationView->m_scene->clear();
    return 0;
}

void MainOhVisualizer::oh_display_close()
{
    m_playButton->setEnabled(true);
    m_openButton->setEnabled(true);
}

void MainOhVisualizer::setFileName(const QUrl &url)
{
//    errorLabel->setText(QString());
    setWindowFilePath(url.isLocalFile() ? url.toLocalFile() : QString());
    filename = url.toString().toUtf8().constData();
//    mediaPlayer.setMedia(url);
    m_playButton->setEnabled(true);
}

void MainOhVisualizer::openFile()
{
    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setWindowTitle(tr("Open Movie"));

//    QStringList supportedMimeTypes = mediaPlayer.supportedMimeTypes();
//    if (!supportedMimeTypes.isEmpty())
//        fileDialog.setMimeTypeFilters(supportedMimeTypes);
    fileDialog.setDirectory("/home/asanchez/sequences/jctvc/avconv");
//    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).value(0, QDir::homePath()));

    if (fileDialog.exec() == QDialog::Accepted)
    {
        setFileName(fileDialog.selectedUrls().first());
//        m_model->update(fileDialog.selectedUrls().first().toString());
    }
}
