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

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsTextItem>
#include <QHBoxLayout>

#include "ohvzmodel.h"
#include "ohvznavigationview.h"

#define xTab 30
#define yTab 20
#define dotSize 16
#define dotHalfSize dotSize/2
#define REFPICCOL QBrush(Qt::red)

ohvzNavigationView::ohvzNavigationView(ohvzModel *model, QWidget *parent)
  : QWidget(parent), m_model(model)
{
    m_scene = new QGraphicsScene();
    m_view = new QGraphicsView(m_scene);
    m_view->setAlignment(Qt::AlignBottom|Qt::AlignLeft);

    QHBoxLayout* frameLayout = new QHBoxLayout(this);
    frameLayout->addWidget(m_view);
}


void ohvzNavigationView::update()
{
    QPen pen;
    QGraphicsTextItem *io = new QGraphicsTextItem;

    m_scene->clear();
    m_scene->addItem(io);

//    CustomElipse *elipse = new CustomElipse(QRectF(30, 30, 15, 25));
//    scene.addItem(elipse);

//    CustomElipse *elipse2 = new CustomElipse(QRectF(70, 70, 25, 15));
//    scene.addItem(elipse2);


//    elipse1->addLine(line, true);
//    elipse2->addLine(line, false);
    foreach (prout frame, m_model->m_framesList) {
        int x = frame.a*xTab;
        int y = -qMax(1, frame.refList.size())*yTab;
        CustomElipse *elipse = new CustomElipse(QRectF(x, y, dotSize, dotSize));
        m_scene->addItem(elipse);
        CustomElipse *elipse2 = new CustomElipse(QRectF(0, -170, dotSize, dotSize));
        m_scene->addItem(elipse2);
        QGraphicsLineItem *line = m_scene->addLine(QLineF(x+dotHalfSize, y+dotHalfSize, 0, -170));
        elipse->addLine(line, true);
        elipse2->addLine(line, false);

//        if (frame.a > 0)
//        {
//            QGraphicsLineItem *line = scene.addLine(QLineF(40, 40, 80, 80));
//            (CustomElipse *)(m_scene->itemAt())->addLine(line, true);
//            (CustomElipse *)(m_scene->itemAt())->addLine(line, true);
//        }
    }

//    foreach (prout frame, m_model->m_framesList) {
//        int x = frame.a*xTab;
//        int y = -qMax(1, frame.refList.size())*yTab;
//        m_scene->addEllipse(x, y, dotSize, dotSize, pen, REFPICCOL);

//        if (frame.a > 0)
//            foreach (int ref, frame.refList) {
//                int h = 0;
//                foreach (prout t, m_model->m_framesList) {
//                    if (t.a == ref){
//                        h = -qMax(1, t.refList.size())*yTab;
//                        break;
//                    }
//                }
//                m_scene->addLine(x+dotHalfSize, y+dotHalfSize, ref*xTab+dotHalfSize, h+dotHalfSize, pen);
//            }

//        io->setPos(frame.a*xTab, 2*yTab);
//        io->setPlainText(QString::number(frame.a));
//    }
}
