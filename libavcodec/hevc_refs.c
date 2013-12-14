/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/pixdesc.h"

#include "internal.h"
#include "thread.h"
#include "hevc.h"

void ff_hevc_unref_frame(HEVCContext *s, HEVCFrame *frame, int flags)
{
    /* frame->frame can be NULL if context init failed */
    if (!frame->frame || !frame->frame->buf[0])
        return;

    frame->flags &= ~flags;
    if (!frame->flags) {
        ff_thread_release_buffer(s->avctx, &frame->tf);

        av_buffer_unref(&frame->tab_mvf_buf);
        frame->tab_mvf = NULL;

        av_buffer_unref(&frame->rpl_buf);
        av_buffer_unref(&frame->rpl_tab_buf);
        frame->rpl_tab    = NULL;
        frame->refPicList = NULL;

        frame->collocated_ref = NULL;
    }
}

RefPicList *ff_hevc_get_ref_list(HEVCContext *s, HEVCFrame *ref, int x0, int y0)
{
    int x_cb         = x0 >> s->sps->log2_ctb_size;
    int y_cb         = y0 >> s->sps->log2_ctb_size;
    int pic_width_cb = (s->sps->width + (1 << s->sps->log2_ctb_size) - 1) >>
                        s->sps->log2_ctb_size;
    int ctb_addr_ts  = s->pps->ctb_addr_rs_to_ts[y_cb * pic_width_cb + x_cb];
    return (RefPicList *)ref->rpl_tab[ctb_addr_ts];
}

void ff_hevc_clear_refs(HEVCContext *s)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        ff_hevc_unref_frame(s, &s->DPB[i],
                            HEVC_FRAME_FLAG_SHORT_REF |
                            HEVC_FRAME_FLAG_LONG_REF);
}

void ff_hevc_flush_dpb(HEVCContext *s)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        ff_hevc_unref_frame(s, &s->DPB[i], ~0);
}

static HEVCFrame *alloc_frame(HEVCContext *s)
{
    int i, j, ret;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *frame = &s->DPB[i];
        if (frame->frame->buf[0])
            continue;

        ret = ff_thread_get_buffer(s->avctx, &frame->tf,
                                   AV_GET_BUFFER_FLAG_REF);
        if (ret < 0)
            return NULL;

        frame->rpl_buf = av_buffer_allocz(s->nb_nals * sizeof(RefPicListTab));
        if (!frame->rpl_buf)
            goto fail;

        frame->tab_mvf_buf = av_buffer_pool_get(s->tab_mvf_pool);
        if (!frame->tab_mvf_buf)
            goto fail;
        frame->tab_mvf = (MvField *)frame->tab_mvf_buf->data;

        frame->rpl_tab_buf = av_buffer_pool_get(s->rpl_tab_pool);
        if (!frame->rpl_tab_buf)
            goto fail;
        frame->rpl_tab   = (RefPicListTab **)frame->rpl_tab_buf->data;
        frame->ctb_count = s->sps->ctb_width * s->sps->ctb_height;
        for (j = 0; j < frame->ctb_count; j++)
            frame->rpl_tab[j] = (RefPicListTab *)frame->rpl_buf->data;

        frame->frame->top_field_first  = s->picture_struct == AV_PICTURE_STRUCTURE_TOP_FIELD;
        frame->frame->interlaced_frame = (s->picture_struct == AV_PICTURE_STRUCTURE_TOP_FIELD) || (s->picture_struct == AV_PICTURE_STRUCTURE_BOTTOM_FIELD);
        frame->frame->display_picture_number = s->poc;
        return frame;
fail:
        ff_hevc_unref_frame(s, frame, ~0);
        return NULL;
    }
    av_log(s->avctx, AV_LOG_ERROR, "Error allocating frame, DPB full.\n");
    return NULL;
}

int ff_hevc_set_new_ref(HEVCContext *s, AVFrame **frame, int poc)
{
    HEVCFrame *ref;
    int i;

    /* check that this POC doesn't already exist */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *frame = &s->DPB[i];

        if (frame->frame->buf[0] && frame->sequence == s->seq_decode &&
            frame->poc == poc && !s->nuh_layer_id) {
            av_log(s->avctx, AV_LOG_ERROR, "Duplicate POC in a sequence: %d.\n",
                   poc);
            return AVERROR_INVALIDDATA;
        }
    }

    ref = alloc_frame(s);
    if (!ref)
        return AVERROR(ENOMEM);

    *frame = ref->frame;
    s->ref = ref;

    ref->poc      = poc;
    ref->flags    = HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_SHORT_REF;
    ref->sequence = s->seq_decode;
    ref->window   = s->sps->output_window;

    return 0;
}
#if REF_IDX_FRAMEWORK
int ff_hevc_set_new_iter_layer_ref(HEVCContext *s, AVFrame **frame, int poc)
{
    HEVCFrame *ref;
    int i;
    /* check that this POC doesn't already exist */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *frame = &s->DPB[i];
        
        if (frame->frame->buf[0] && frame->sequence == s->seq_decode &&
            frame->poc == poc && !s->nuh_layer_id) {
            av_log(s->avctx, AV_LOG_ERROR, "Duplicate POC in a sequence: %d.\n",
                   poc);
            return AVERROR_INVALIDDATA;
        }
    }
    
    ref = alloc_frame(s);
    if (!ref)
        return AVERROR(ENOMEM);
    
    *frame              = ref->frame;
    s->inter_layer_ref              = ref;
    ref->poc            = poc;
    
    ref->flags          = HEVC_FRAME_FLAG_LONG_REF;
    ref->sequence       = s->seq_decode;
    ref->window         = s->sps->output_window;
    
    return 0;
}
#endif
int ff_hevc_output_frame(HEVCContext *s, AVFrame *out, int flush)
{
    do {
        int nb_output = 0;
        int min_poc   = INT_MAX;
        int i, min_idx, ret;

        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCFrame *frame = &s->DPB[i];
            if ((frame->flags & HEVC_FRAME_FLAG_OUTPUT) &&
                frame->sequence == s->seq_output) {
                nb_output++;
                if (frame->poc < min_poc) {
                    min_poc = frame->poc;
                    min_idx = i;
                }
            }
        }

        /* wait for more frames before output */
        if (!flush && s->seq_output == s->seq_decode && s->sps &&
            nb_output <= s->sps->temporal_layer[s->sps->max_sub_layers - 1].num_reorder_pics)
            return 0;

        if (nb_output) {
            HEVCFrame *frame = &s->DPB[min_idx];
            AVFrame *dst = out;
            AVFrame *src = frame->frame;
            const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(src->format);
            int pixel_shift = !!(desc->comp[0].depth_minus1 > 7);

            ret = av_frame_ref(out, src);
            frame->flags &= ~(HEVC_FRAME_FLAG_OUTPUT);
            if (ret < 0)
                return ret;

            for (i = 0; i < 3; i++) {
                int hshift = (i > 0) ? desc->log2_chroma_w : 0;
                int vshift = (i > 0) ? desc->log2_chroma_h : 0;
                int off = ((frame->window.left_offset >> hshift) << pixel_shift) +
                          (frame->window.top_offset   >> vshift) * dst->linesize[i];
                dst->data[i] += off;
            }
            av_log(s->avctx, AV_LOG_DEBUG,
                   "Output frame with POC %d.\n", frame->poc);
            return 1;
        }

        if (s->seq_output != s->seq_decode)
            s->seq_output = (s->seq_output + 1) & 0xff;
        else
            break;
    } while (1);

    return 0;
}

static int init_slice_rpl(HEVCContext *s)
{
    HEVCFrame *frame = s->ref;
    int ctb_count    = frame->ctb_count;
    int ctb_addr_ts  = s->pps->ctb_addr_rs_to_ts[s->sh.slice_segment_addr];
    int i;

    if (s->slice_idx >= frame->rpl_buf->size / sizeof(RefPicListTab))
        return AVERROR_INVALIDDATA;

    for (i = ctb_addr_ts; i < ctb_count; i++)
        frame->rpl_tab[i] = (RefPicListTab *)frame->rpl_buf->data + s->slice_idx;

    frame->refPicList = (RefPicList *)frame->rpl_tab[ctb_addr_ts];

    return 0;
}
#ifdef SVC_EXTENSION
static int init_il_slice_rpl(HEVCContext *s)
{
    HEVCFrame *frame = s->inter_layer_ref;
    int ctb_count   = frame->ctb_count;
    int ctb_addr_ts = s->pps->ctb_addr_rs_to_ts[s->sh.slice_segment_addr];
    int i;
    
    if (s->slice_idx >= frame->rpl_buf->size / sizeof(RefPicListTab))
        return AVERROR_INVALIDDATA;
    
    for (i = ctb_addr_ts; i < ctb_count; i++)
        frame->rpl_tab[i] = (RefPicListTab*)frame->rpl_buf->data + s->slice_idx;
    
    frame->refPicList = (RefPicList*)frame->rpl_tab[ctb_addr_ts];
    
    return 0;
}
#endif
static HEVCFrame *find_ref_idx(HEVCContext *s, int poc)
{
    int i;
    int LtMask = (1 << s->sps->log2_max_poc_lsb) - 1;
    
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = &s->DPB[i];
        if (ref->frame->buf[0] && (ref->sequence == s->seq_decode)) {
            if ((ref->poc & LtMask) == poc)
                return ref;
        }
    }
    
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = &s->DPB[i];
        if (ref->frame->buf[0] && (ref->sequence == s->seq_decode)) {
            if (ref->poc == poc || (ref->poc & LtMask) == poc)
                return ref;
        }
    }
    
    av_log(s->avctx, AV_LOG_ERROR,
           "Could not find ref with POC %d\n", poc);
    return NULL;
}

#ifdef SVC_EXTENSION
static void scale_upsampled_mv_field(AVCodecContext *avctxt, void *input_ctb_row) {
    HEVCContext *s = avctxt->priv_data;
    int xEL, yEL, xBL, yBL, list, i, j;
    HEVCFrame  *refBL, *refEL;
    int pic_width_in_min_pu = s->sps->width>>s->sps->log2_min_pu_size;
    int pic_height_in_min_pu = s->sps->height>>s->sps->log2_min_pu_size;
    int pic_width_in_min_puBL = s->BL_frame->frame->coded_width >> s->sps->log2_min_pu_size;
    int *index = input_ctb_row;
    int ctb_size = 1 << s->sps->log2_ctb_size;
    refBL = s->BL_frame;
    init_il_slice_rpl(s);
    refEL = s->inter_layer_ref;
    if( *index ==0 ) {
        for( list=0; list < 2; list++) {
            refEL->refPicList[list].nb_refs = refBL->refPicList[list].nb_refs;
            for(i=0; i< refBL->refPicList->nb_refs; i++){
                refEL->refPicList[list].list[i] = refBL->refPicList[list].list[i];
                refEL->refPicList[list].ref[i] = find_ref_idx(s, refBL->refPicList[list].list[i]);
                refEL->refPicList[list].isLongTerm[i] = refBL->refPicList[list].isLongTerm[i];
            }
        }
    }

    int start = (*index) * ctb_size;
    int end = ((*index)+1) * ctb_size;
    
    end = end > s->sps->height? s->sps->height:end ;
    for(yEL=start; yEL < end; yEL+=16){
        for(xEL=0; xEL < s->sps->width ; xEL+=16) {
            int xELIndex = xEL>>2;
            int yELIndex = yEL>>2;
            
            int xELtmp = av_clip(xEL+8, 0, s->sps->width -1);
            int yELtmp = av_clip(yEL+8, 0, s->sps->height -1);
            xBL = (((xELtmp) - s->sps->pic_conf_win.left_offset)*s->sh.ScalingPosition[s->nuh_layer_id][0] + (1<<15)) >> 16;
            yBL = (((yELtmp) - s->sps->pic_conf_win.top_offset )*s->sh.ScalingPosition[s->nuh_layer_id][1] + (1<<15)) >> 16;
            
            
            xBL = (xBL >>=4)<<2; //xBL & 0xFFFFFFF0
            yBL = (yBL >>=4)<<2;  //yBL & 0xFFFFFFF0
            
            if(!refBL->tab_mvf[(yBL*pic_width_in_min_puBL)+xBL].is_intra) {
                refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].is_intra = 0;
                
                for( list=0; list < 2; list++) {
                    int x = refBL->tab_mvf[(yBL*pic_width_in_min_puBL)+xBL].mv[list].x;
                    int y = refBL->tab_mvf[(yBL*pic_width_in_min_puBL)+xBL].mv[list].y;
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].mv[list].x  = av_clip( (s->sh.ScalingFactor[s->nuh_layer_id][0] * x + 127 + (s->sh.ScalingFactor[s->nuh_layer_id][0] * x < 0)) >> 8 , -32768, 32767);
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].mv[list].y = av_clip( (s->sh.ScalingFactor[s->nuh_layer_id][1] * y + 127 + (s->sh.ScalingFactor[s->nuh_layer_id][1] * y < 0)) >> 8, -32768, 32767);
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].ref_idx[list] = refBL->tab_mvf[yBL*pic_width_in_min_puBL+xBL].ref_idx[list];
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].pred_flag[list] = refBL->tab_mvf[yBL*pic_width_in_min_puBL+xBL].pred_flag[list];
                }
            } else {
                refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].is_intra = 1;
                for( list=0; list < 2; list++) {
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].mv[list].x  = 0;
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].mv[list].y = 0;
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].ref_idx[list] = refBL->tab_mvf[yBL*pic_width_in_min_puBL+xBL].ref_idx[list];
                    refEL->tab_mvf[(yELIndex*pic_width_in_min_pu)+xELIndex].pred_flag[list] = refBL->tab_mvf[yBL*pic_width_in_min_puBL+xBL].pred_flag[list];
                }
            }
            for(i =0; i < 4; i++)
                for(j =0; j < 4; j++)   {
                    if((i || j) && (yELIndex+i)<pic_height_in_min_pu && (xELIndex+j)<pic_width_in_min_pu) {
                        refEL->tab_mvf[((yELIndex+i) *pic_width_in_min_pu)+xELIndex+j].is_intra = refEL->tab_mvf[yELIndex*pic_width_in_min_pu+xELIndex].is_intra;
                        for(list=0; list < 2; list++) {
                            refEL->tab_mvf[((yELIndex+i) *pic_width_in_min_pu)+xELIndex+j].mv[list].x  = refEL->tab_mvf[yELIndex*pic_width_in_min_pu+xELIndex].mv[list].x;
                            refEL->tab_mvf[((yELIndex+i) *pic_width_in_min_pu)+xELIndex+j].mv[list].y = refEL->tab_mvf[yELIndex*pic_width_in_min_pu+xELIndex].mv[list].y;
                            refEL->tab_mvf[((yELIndex+i) *pic_width_in_min_pu)+xELIndex+j].ref_idx[list] = refEL->tab_mvf[yELIndex*pic_width_in_min_pu+xELIndex].ref_idx[list];
                            refEL->tab_mvf[((yELIndex+i) *pic_width_in_min_pu)+xELIndex+j].pred_flag[list] = refEL->tab_mvf[yELIndex*pic_width_in_min_pu+xELIndex].pred_flag[list];
                        }
                    }
                }
        }
    }
}

#endif

int ff_hevc_slice_rpl(HEVCContext *s)
{
    SliceHeader *sh = &s->sh;

    uint8_t nb_list = sh->slice_type == B_SLICE ? 2 : 1;
    uint8_t list_idx;
    int i, j, ret;

    ret = init_slice_rpl(s);
    if (ret < 0)
        return ret;

    if (!(s->rps[ST_CURR_BEF].nb_refs + s->rps[ST_CURR_AFT].nb_refs +
          s->rps[LT_CURR].nb_refs)) {
        av_log(s->avctx, AV_LOG_ERROR, "Zero refs in the frame RPS.\n");
        return AVERROR_INVALIDDATA;
    }

    for (list_idx = 0; list_idx < nb_list; list_idx++) {
        RefPicList  rpl_tmp = { { 0 } };
        RefPicList *rpl     = &s->ref->refPicList[list_idx];

        /* The order of the elements is
         * ST_CURR_BEF - ST_CURR_AFT - LT_CURR for the L0 and
         * ST_CURR_AFT - ST_CURR_BEF - LT_CURR for the L1 */
        int cand_lists[3] = { list_idx ? ST_CURR_AFT : ST_CURR_BEF,
                              list_idx ? ST_CURR_BEF : ST_CURR_AFT,
                              LT_CURR };

        /* concatenate the candidate lists for the current frame */
        while (rpl_tmp.nb_refs < sh->nb_refs[list_idx]) {
            for (i = 0; i < FF_ARRAY_ELEMS(cand_lists); i++) {
                RefPicList *rps = &s->rps[cand_lists[i]];
                for (j = 0; j < rps->nb_refs && rpl_tmp.nb_refs < MAX_REFS; j++) {
                    rpl_tmp.list[rpl_tmp.nb_refs]       = rps->list[j];
                    rpl_tmp.ref[rpl_tmp.nb_refs]        = rps->ref[j];
                    rpl_tmp.isLongTerm[rpl_tmp.nb_refs] = i == 2;
                    rpl_tmp.nb_refs++;
                }
            }
        }

        /* reorder the references if necessary */
        if (sh->rpl_modification_flag[list_idx]) {
            for (i = 0; i < sh->nb_refs[list_idx]; i++) {
                int idx = sh->list_entry_lx[list_idx][i];

                if (!s->decoder_id && idx >= rpl_tmp.nb_refs) {
                    av_log(s->avctx, AV_LOG_ERROR, "Invalid reference index.\n");
                    return AVERROR_INVALIDDATA;
                }

                rpl->list[i]       = rpl_tmp.list[idx];
                rpl->ref[i]        = rpl_tmp.ref[idx];
                rpl->isLongTerm[i] = rpl_tmp.isLongTerm[idx];
                rpl->nb_refs++;
            }
        } else {
            memcpy(rpl, &rpl_tmp, sizeof(*rpl));
            rpl->nb_refs = FFMIN(rpl->nb_refs, sh->nb_refs[list_idx]);
        }

        if (sh->collocated_list == list_idx &&
            sh->collocated_ref_idx < rpl->nb_refs)
            s->ref->collocated_ref = rpl->ref[sh->collocated_ref_idx];
    }

    return 0;
}

static void mark_ref(HEVCFrame *frame, int flag)
{
    frame->flags &= ~(HEVC_FRAME_FLAG_LONG_REF | HEVC_FRAME_FLAG_SHORT_REF);
    frame->flags |= flag;
}

static HEVCFrame *generate_missing_ref(HEVCContext *s, int poc)
{
    HEVCFrame *frame;
    int i, x, y;

    frame = alloc_frame(s);
    if (!frame)
        return NULL;

    if (!s->sps->pixel_shift) {
        for (i = 0; frame->frame->buf[i]; i++)
            memset(frame->frame->buf[i]->data, 1 << (s->sps->bit_depth - 1),
                   frame->frame->buf[i]->size);
    } else {
        for (i = 0; frame->frame->data[i]; i++)
            for (y = 0; y < (s->sps->height >> s->sps->vshift[i]); y++)
                for (x = 0; x < (s->sps->width >> s->sps->hshift[i]); x++) {
                    AV_WN16(frame->frame->data[i] + y * frame->frame->linesize[i] + 2 * x,
                            1 << (s->sps->bit_depth - 1));
                }
    }

    frame->poc      = poc;
    frame->sequence = s->seq_decode;
    frame->flags    = 0;

    if (s->threads_type & FF_THREAD_FRAME)
        ff_thread_report_progress(&frame->tf, INT_MAX, 0);

    return frame;
}

/* add a reference with the given poc to the list and mark it as used in DPB */
static int add_candidate_ref(HEVCContext *s, RefPicList *list,
                             int poc, int ref_flag)
{
    HEVCFrame *ref = find_ref_idx(s, poc);

    if (ref == s->ref)
        return AVERROR_INVALIDDATA;

    if (!ref) {
        ref = generate_missing_ref(s, poc);
        if (!ref)
            return AVERROR(ENOMEM);
    }

    list->list[list->nb_refs] = ref->poc;
    list->ref[list->nb_refs]  = ref;
    list->nb_refs++;

    mark_ref(ref, ref_flag);
    return 0;
}

#if REF_IDX_FRAMEWORK
static void init_upsampled_mv_fields(HEVCContext *s) {
    int i, list, pic_width_in_min_pu, pic_height_in_min_pu;
    pic_width_in_min_pu = s->sps->width >> s->sps->log2_min_pu_size;
    pic_height_in_min_pu = s->sps->height >> s->sps->log2_min_pu_size;
    HEVCFrame *refEL = s->inter_layer_ref;

    for(i = 0; i < pic_width_in_min_pu  * pic_height_in_min_pu; i++) {
        refEL->tab_mvf[i].is_intra = 1;
        for(list = 0; list < 2; list++) {
            refEL->tab_mvf[i].mv[list].x = 0;
            refEL->tab_mvf[i].mv[list].y = 0;
            refEL->tab_mvf[i].pred_flag[list] = 0;
            refEL->tab_mvf[i].ref_idx[list] = 0;
        }
    }
}



#endif





int ff_hevc_frame_rps(HEVCContext *s)
{
    int ctb_size                  = 1<<s->sps->log2_ctb_size;
    const ShortTermRPS *short_rps = s->sh.short_term_rps;
    const LongTermRPS  *long_rps  = &s->sh.long_term_rps;
    RefPicList               *rps = s->rps;
    int i, ret;

    if (!short_rps) {
        rps[0].nb_refs = rps[1].nb_refs = 0;
        if (!long_rps)
            return 0;
    }

#if REF_IDX_FRAMEWORK
#if REF_IDX_MFM
    
#if ZERO_NUM_DIRECT_LAYERS
    if( s->nuh_layer_id > 0 && s->vps->max_one_active_ref_layer_flag > 0 )
#else
    if (s->nuh_layer_id)
#endif
    {
        if(!(s->nal_unit_type >= NAL_BLA_W_LP && s->nal_unit_type <= NAL_CRA_NUT) && s->sps->set_mfm_enabled_flag)  {
            int *arg, *ret, cmpt = (s->sps->height / ctb_size) + (s->sps->height%ctb_size ? 1:0);
            arg = av_malloc(cmpt*sizeof(int));
            ret = av_malloc(cmpt*sizeof(int));
            for(i=0; i < cmpt; i++)
                arg[i] = i;
            s->avctx->execute(s->avctx, (void *) scale_upsampled_mv_field, arg, ret, cmpt, sizeof(int));
            av_free(arg);
            av_free(ret);
            }   else    {
                init_upsampled_mv_fields(s);
            }
#endif
        }
#endif

    /* clear the reference flags on all frames except the current one */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *frame = &s->DPB[i];

        if (frame == s->ref)
            continue;

        mark_ref(frame, 0);
    }

    for (i = 0; i < NB_RPS_TYPE; i++)
        rps[i].nb_refs = 0;
#ifdef SVC_EXTENSION
    if(!s->nuh_layer_id || !(s->nal_unit_type >= NAL_BLA_W_LP && s->nal_unit_type <= NAL_CRA_NUT) && s->sps->set_mfm_enabled_flag)  {
#endif
        /* add the short refs */
        for (i = 0; short_rps && i < short_rps->num_delta_pocs; i++) {
            int poc = s->poc + short_rps->delta_poc[i];
            int list;

            if (!short_rps->used[i])
                list = ST_FOLL;
            else if (i < short_rps->num_negative_pics)
                list = ST_CURR_BEF;
            else
                list = ST_CURR_AFT;

            ret = add_candidate_ref(s, &rps[list], poc, HEVC_FRAME_FLAG_SHORT_REF);
            if (ret < 0)
                return ret;
        }

        /* add the long refs */
        for (i = 0; long_rps && i < long_rps->nb_refs; i++) {
            int poc  = long_rps->poc[i];
            int list = long_rps->used[i] ? LT_CURR : LT_FOLL;

            ret = add_candidate_ref(s, &rps[list], poc, HEVC_FRAME_FLAG_LONG_REF);
            if (ret < 0)
                return ret;
        }
#ifdef SVC_EXTENSION
    } else {
        for (i = 0; short_rps && i < short_rps->num_delta_pocs; i++) {
            int poc = s->poc + short_rps->delta_poc[i];
            HEVCFrame *ref = find_ref_idx(s, poc);
            mark_ref(ref, HEVC_FRAME_FLAG_SHORT_REF);
        }
        for (i = 0; long_rps && i < long_rps->nb_refs; i++) {
            int poc  = long_rps->poc[i];
            HEVCFrame *ref = find_ref_idx(s, poc);
            mark_ref(ref, HEVC_FRAME_FLAG_LONG_REF);
        }
    }
#endif

    
#if REF_IDX_FRAMEWORK
    if(s->nuh_layer_id) {
#if JCTVC_M0458_INTERLAYER_RPS_SIG
        for( i = 0; i < s->vps->max_one_active_ref_layer_flag; i ++) {
#else
            for( i = 0; i < m_numILRRefIdx; i ++) {
#endif
                ret = add_candidate_ref(s, &rps[LT_CURR], s->poc, HEVC_FRAME_FLAG_LONG_REF);
            }
        }
#endif

    /* release any frames that are now unused */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        ff_hevc_unref_frame(s, &s->DPB[i], 0);

    return 0;
}

int ff_hevc_compute_poc(HEVCContext *s, int poc_lsb)
{
    int max_poc_lsb  = 1 << s->sps->log2_max_poc_lsb;
    int prev_poc_lsb = s->pocTid0 % max_poc_lsb;
    int prev_poc_msb = s->pocTid0 - prev_poc_lsb;
    int poc_msb;

    if (poc_lsb < prev_poc_lsb && prev_poc_lsb - poc_lsb >= max_poc_lsb / 2)
        poc_msb = prev_poc_msb + max_poc_lsb;
    else if (poc_lsb > prev_poc_lsb && poc_lsb - prev_poc_lsb > max_poc_lsb / 2)
        poc_msb = prev_poc_msb - max_poc_lsb;
    else
        poc_msb = prev_poc_msb;

    // For BLA picture types, POCmsb is set to 0.
    if (s->nal_unit_type == NAL_BLA_W_LP   ||
        s->nal_unit_type == NAL_BLA_W_RADL ||
        s->nal_unit_type == NAL_BLA_N_LP)
        poc_msb = 0;

    return poc_msb + poc_lsb;
}

int ff_hevc_frame_nb_refs(HEVCContext *s)
{
    int ret = 0;
    int i;
    const ShortTermRPS *rps = s->sh.short_term_rps;
    LongTermRPS *long_rps   = &s->sh.long_term_rps;
#if REF_IDX_FRAMEWORK
    if( s->sh.slice_type == I_SLICE || (s->nuh_layer_id &&
                                        (s->nal_unit_type >= NAL_BLA_W_LP) &&
                                        (s->nal_unit_type<= NAL_CRA_NUT ) ))
#else
        if (s->sh.slice_type == I_SLICE)
#endif
        {
#if REF_IDX_FRAMEWORK
#if JCTVC_M0458_INTERLAYER_RPS_SIG
            return s->sh.active_num_ILR_ref_idx;
#else
            return s->vps->m_numDirectRefLayers[s->layer_id];
#endif
#else
            return 0;
#endif
        }
    if (rps) {
        for (i = 0; i < rps->num_negative_pics; i++)
            ret += !!rps->used[i];
        for (; i < rps->num_delta_pocs; i++)
            ret += !!rps->used[i];
    }

    if (long_rps) {
        for (i = 0; i < long_rps->nb_refs; i++)
            ret += !!long_rps->used[i];
    }
#if REF_IDX_FRAMEWORK
    if(s->nuh_layer_id) {
#if JCTVC_M0458_INTERLAYER_RPS_SIG
        for( i = 0; i < s->vps->max_one_active_ref_layer_flag; i ++) {
#else
            for( i = 0; i < m_numILRRefIdx; i ++) {
#endif
                ret++;
            }
        }
#endif
    return ret;
}

