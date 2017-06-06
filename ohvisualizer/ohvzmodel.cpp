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

#include "ohvzmodel.h"

#include <QStandardItemModel>
#include <QString>

#include "libavcodec/hevc.h"

typedef struct OpenHevcWrapperContext {
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *picture;
    AVPacket avpkt;
    AVCodecParserContext *parser;
} OpenHevcWrapperContext;

typedef struct OpenHevcWrapperContexts {
    OpenHevcWrapperContext **wraper;
    int nb_decoders;
    int active_layer;
    int display_layer;
    int set_display;
    int set_vps;
    int got_picture_mask;
} OpenHevcWrapperContexts;

ohvzModel::ohvzModel(QObject *parent):
    QStandardItemModel(parent)
{
    m_openHevcFrame = new OpenHevc_Frame();
}

ohvzModel::~ohvzModel()
{
    delete m_openHevcFrame;
}

void ohvzModel::init()
{
    m_openHevcFrame = NULL;
    m_framesList.clear();
    m_tiles.clear();
    m_ctb_size = 0;
    this->clear();
}

QList<QStandardItem *> rowKeyVal(QString key, QString val)
{
    QList<QStandardItem *> kv;
    kv.append(new QStandardItem(key));
    kv.append(new QStandardItem(val));
    return kv;
}

void ohvzModel::updateVPS(HEVCContext *hc)
{
    this->clear();

    QStandardItem *item;
    this->setColumnCount(2);

    item = new QStandardItem("video_parameter_set_rbsp()");
    item->appendRow(rowKeyVal("vps_video_parameter_set_id", QString::number(hc->ps.vps->vps_id)));
    item->appendRow(rowKeyVal("vps_base_layer_internal_flag", QString::number(hc->ps.vps->vps_base_layer_internal_flag)));
    item->appendRow(rowKeyVal("vps_base_layer_available_flag", QString::number(hc->ps.vps->vps_base_layer_available_flag)));
    item->appendRow(rowKeyVal("vps_max_layers_minus1", QString::number(hc->ps.vps->vps_max_layers-1)));
    item->appendRow(rowKeyVal("vps_max_sub_layers_minus1", QString::number(hc->ps.vps->vps_max_sub_layers-1)));
    item->appendRow(rowKeyVal("vps_temporal_id_nesting_flag", QString::number(hc->ps.vps->vps_temporal_id_nesting_flag)));
    item->appendRow(rowKeyVal("vps_reserved_0xffff_16bits", "0x"+QString::number(hc->ps.vps->vps_reserved_0xffff_16bits, 16)));

    item->appendRow(rowKeyVal("vps_sub_layer_ordering_info_present_flag", QString::number(hc->ps.vps->vps_sub_layer_ordering_info_present_flag)));
    this->appendRow(item);

    item = new QStandardItem(QString::number(hc->poc));
    this->appendRow(item);
}

void ohvzModel::updatePPS()
{
}

void ohvzModel::updateSPS()
{
}

void ohvzModel::update(OpenHevc_Frame *openHevcFrame, OpenHevc_Handle openHevcHandle)
{
    m_openHevcFrame = openHevcFrame;

    m_tiles.clear();

    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext = openHevcContexts->wraper[0];
    HEVCContext *hc = ((HEVCContext*)openHevcContext->c->priv_data);

    m_ctb_size = 1 << hc->ps.sps->log2_ctb_size;

    QStringList data;
    data << QString::number(m_openHevcFrame->frameInfo.nWidth);
    data << QString::number(m_openHevcFrame->frameInfo.nHeight);

    if (hc != NULL) {
        data << QString::number(hc->ps.pps->entropy_coding_sync_enabled_flag);
        data << QString::number(hc->ps.pps->tiles_enabled_flag);
        data << QString::number(hc->ps.sps->bit_depth[0]);
        data << QString::number(hc->ps.sps->ctb_height);
        data << QString::number(hc->ps.sps->ctb_size);
        data << QString::number(hc->ps.sps->ctb_width);
        data << "num_tile_rows: " + QString::number(hc->ps.pps->num_tile_rows);
        data << "num_tile_columns: " + QString::number(hc->ps.pps->num_tile_columns);
        data << QString::number(m_ctb_size);

        int i = 0;
        for (i=0; i < hc->ps.pps->num_tile_rows*hc->ps.pps->num_tile_columns;i++)
        {
            int ctb_rs = hc->ps.pps->tile_pos_rs[i];
            int x_ctb = (ctb_rs % hc->ps.sps->ctb_width) << hc->ps.sps->log2_ctb_size;
            int y_ctb = (ctb_rs / hc->ps.sps->ctb_width) << hc->ps.sps->log2_ctb_size;
            int tile_size = hc->ps.pps->tile_width[hc->ps.pps->ctb_addr_rs_to_ts[ctb_rs]];
            data << "Tile [" + QString::number(i) + "] => {" + QString::number(x_ctb) + " , " + QString::number(y_ctb) + "}" + QString::number(tile_size);
            m_tiles.append(QRect(QPoint(x_ctb, y_ctb), QSize(tile_size*(1 << hc->ps.sps->log2_ctb_size), hc->ps.sps->height)));
        }
    }

    foreach(QRect tile, m_tiles) {
        data << "Tile => {" + QString::number(tile.x()) +
                " , " + QString::number(tile.y()) +
                " , " + QString::number(tile.width()) +
                " , " + QString::number(tile.height()) + "}";
    }

    int i = 0;
    QList<int> refList;
    for (i=0; i < hc->rps[0].nb_refs; i++)
    {
        refList.append(hc->rps[0].list[i]);
    }
    for (i=0; i < hc->rps[1].nb_refs; i++)
    {
        refList.append(hc->rps[1].list[i]);
    }
    m_framesList.append({hc->poc, refList});   

    this->updateVPS(hc);
    this->updatePPS();
    this->updateSPS();
}
