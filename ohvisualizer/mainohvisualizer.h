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

#ifndef MAINOHVISUALIZER_H
#define MAINOHVISUALIZER_H

#include <QWidget>

class QPushButton;
class QAbstractButton;
class QBoxLayout;
class QTreeView;
class ohvzFrameView;
class ohvzGLFrameView;
class ohvzModel;
class ohvzNavigationView;

typedef enum OHEvent_t {OH_NOEVENT=0, OH_LAYER0, OH_LAYER1, OH_QUIT, OH_MOUSE} OHEvent;

typedef struct OHMouse{
    int x;
    int y;
    int on;
}OHMouse;

namespace Ui {
class MainOhVisualizer;
}

class MainOhVisualizer : public QWidget
{
    Q_OBJECT

public:
    explicit MainOhVisualizer(QWidget *parent = 0);
    ~MainOhVisualizer();

    void openFile();
    void video_decode();
    int  oh_display_init(int edge, int frame_width, int frame_height);
    void oh_display_close(void);
    void setFileName(const QUrl &url);

    OHMouse oh_display_getMouseEvent(void);
    OHEvent oh_display_getWindowEvent(void);

protected:
    void closeEvent(QCloseEvent*);

private:
    Ui::MainOhVisualizer *ui;
    QAbstractButton *m_openButton;
    QPushButton *m_playButton;
    ohvzFrameView *m_frameView;
    ohvzNavigationView *m_navigationView;
//    ohvzGLFrameView *m_frameView;
    QTreeView *m_infoView;
    ohvzModel *m_model;

    std::string filename;
    std::string enh_filename;

    int h264_flags;
    int no_md5;
    int thread_type;
    char *input_file;
    char *enhance_file;
    char no_display;
    char *output_file;
    int nb_pthreads;
    int temporal_layer_id;
    int quality_layer_id;
    int num_frames;
    float frame_rate;

};

#endif // MAINOHVISUALIZER_H
