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

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QCoreApplication>

#include "ohvzmodel.h"
#include "ohvzglframeview.h"

ohvzGLFrameView::ohvzGLFrameView(ohvzModel *model, QWidget *parent)
    : QOpenGLWidget(parent), m_model(model)
{
    this->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    m_frameImage = new QImage(1280, 720, QImage::Format_RGB888);

    //    m_frameLabel = new QLabel(parent);
    //    m_frameLabel->setPixmap(QPixmap::fromImage(*m_frameImage));
}

ohvzGLFrameView::~ohvzGLFrameView()
{

}

void ohvzGLFrameView::setImage(QImage *image) {
    m_frameImage = (QImage*)image;
}

void ohvzGLFrameView::paintGL(){
    if(m_frameImage != NULL){
        QPainter p(this);
        //Set the painter to use a smooth scaling algorithm.
        p.setRenderHint(QPainter::SmoothPixmapTransform, 1);
        p.drawImage(m_frameImage->rect(), *m_frameImage);
    }
}

QImage ohvzGLFrameView::createImageWithOverlay(const QImage& baseImage, const QImage& overlayImage)
{
    QImage imageWithOverlay = QImage(baseImage.size(), QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&imageWithOverlay);

    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(imageWithOverlay.rect(), Qt::transparent);

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, baseImage);

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, overlayImage);

    painter.end();

    return imageWithOverlay;
}

void ohvzGLFrameView::display()
{
    // Following line is to prevent Main UI freeze
    QCoreApplication::processEvents();

    this->displayFrame();
    this->displayCTB();
    this->displayTiles();

    this->resize(m_frameImage->size());
    this->repaint();
}

void ohvzGLFrameView::displayFrame()
{
//    int edge = (m_model->m_openHevcFrame->frameInfo.nYPitch - m_model->m_openHevcFrame->frameInfo.nWidth)/2;
    int frame_width = m_model->m_openHevcFrame->frameInfo.nWidth;
    int frame_height = m_model->m_openHevcFrame->frameInfo.nHeight;
    uint8_t *Y = (uint8_t *)m_model->m_openHevcFrame->pvY;
    uint8_t *U = (uint8_t *)m_model->m_openHevcFrame->pvU;
    uint8_t *V = (uint8_t *)m_model->m_openHevcFrame->pvV;

    if(m_frameImage == NULL || m_frameImage->height() != frame_height || m_frameImage->width() != frame_width){
        if(m_frameImage != NULL) delete m_frameImage;

        m_frameImage = new QImage(frame_width, frame_height, QImage::Format_RGB888);
    }

    // YUV to RGB convertion
    for(int i = 0; i < m_frameImage->width()*m_frameImage->height(); i++)
    {
        unsigned char y = Y[i];
        unsigned char u = U[((i/m_frameImage->width())>>1)*(m_frameImage->width()>>1)+((i%m_frameImage->width())>>1)];
        unsigned char v = V[((i/m_frameImage->width())>>1)*(m_frameImage->width()>>1)+((i%m_frameImage->width())>>1)];

        int r = qBound(0.0, (1.0*y + 8 + 1.402*(v-128)), 255.0);
        int g = qBound(0.0, (1.0*y - 0.34413*(u-128) - 0.71414*(v-128)), 255.0);
        int b = qBound(0.0, (1.0*y + 1.772*(u-128) + 0), 255.0);

        m_frameImage->setPixel(i%m_frameImage->width(), i/m_frameImage->width(), qRgb(r, g, b));
    }
}

void ohvzGLFrameView::displayCTB()
{
    QPainter p(m_frameImage);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(1);

    int i,j;
    p.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));
    for (i=0; i<m_frameImage->width(); i+=m_model->m_ctb_size)
    {
        for (j=0; j<m_frameImage->height(); j+=m_model->m_ctb_size)
        {
            p.drawRect(QRect(QPoint(i,j), QSize(m_model->m_ctb_size, m_model->m_ctb_size)));
        }
    }

    p.end(); // Don't forget this line!
}

void ohvzGLFrameView::displayTiles()
{
    if (!m_model->m_tiles.isEmpty())
    {
        QPainter p(m_frameImage);
        p.setRenderHint(QPainter::Antialiasing);
        p.setOpacity(1);

        p.setPen(QPen(Qt::red, 2, Qt::SolidLine, Qt::FlatCap));
        foreach(QRect tile, m_model->m_tiles)
            p.drawRect(tile);

        p.end(); // Don't forget this line!
    }
}
