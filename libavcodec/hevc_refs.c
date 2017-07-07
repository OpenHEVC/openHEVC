/*
 * HEVC video decoder
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

#include "libavutil/imgutils.h"
#include "libavutil/pixdesc.h"

#include "internal.h"
#include "thread.h"
#include "hevc.h"

void ff_hevc_unref_frame(HEVCContext *s, HEVCFrame *frame, int flags) {
    int i;
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
        for(i=0; i < MAX_SLICES_IN_FRAME; i++) 
            frame->refPicList[i] = NULL;
        frame->collocated_ref = NULL;
    }
}

RefPicList *ff_hevc_get_ref_list(HEVCContext *s, HEVCFrame *ref, int x0, int y0)
{
    int x_cb         = x0 >> s->ps.sps->log2_ctb_size;
    int y_cb         = y0 >> s->ps.sps->log2_ctb_size;
    int pic_width_cb = s->ps.sps->ctb_width;
    int ctb_addr_ts  = s->ps.pps->ctb_addr_rs_to_ts[y_cb * pic_width_cb + x_cb];
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
    av_log(s->avctx, AV_LOG_ERROR, "flush, decoder_%d.\n", s->decoder_id);
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

        if (s->interlaced) {
            s->avctx->height = s->ps.sps->output_height * 2;
            s->avctx->coded_height = s->avctx->height;
        }

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

        if (s->interlaced)
            for (j = 0; j < 3; j++) {
                frame->frame->linesize[j] = 2 * frame->frame->linesize[j];
            }

        if (!frame->rpl_tab_buf)
            goto fail;
        frame->rpl_tab   = (RefPicListTab **)frame->rpl_tab_buf->data;
        frame->ctb_count = s->ps.sps->ctb_width * s->ps.sps->ctb_height;
        for (j = 0; j < frame->ctb_count; j++)
            frame->rpl_tab[j] = (RefPicListTab *)frame->rpl_buf->data;

        frame->frame->top_field_first  = s->field_order == AV_FIELD_TT;
        frame->frame->interlaced_frame = s->interlaced;
        return frame;
fail:
        ff_hevc_unref_frame(s, frame, ~0);
        return NULL;
    }
    av_log(s->avctx, AV_LOG_ERROR, "Error allocating frame, DPB full, decoder_%d.\n", s->decoder_id);
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

    ref->field_order = s->field_order;

    ref->poc   = poc;
    ref->flags = HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_SHORT_REF;
    if (s->sh.pic_output_flag == 0)
        ref->flags        &= ~(HEVC_FRAME_FLAG_OUTPUT);
    ref->sequence          = s->seq_decode;
    ref->window            = s->ps.sps->output_window;
    ref->nal_unit_type     = s->nal_unit_type;
    ref->temporal_layer_id = s->temporal_id;
    return 0;
}

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
    s->inter_layer_ref  = ref;
    ref->poc            = poc;

    ref->flags          = HEVC_FRAME_FLAG_LONG_REF;
    ref->sequence       = s->seq_decode;
    ref->window         = s->ps.sps->output_window;
    if (s->threads_type & FF_THREAD_FRAME)
        ff_thread_report_progress(&s->inter_layer_ref->tf, INT_MAX, 0);

    return 0;
}

static void copy_field(HEVCContext *s, AVFrame *_dst, AVFrame *_src, int height) {
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(_src->format);
    int i, j, planes_nb = 0;

    for (i = 0; i < desc->nb_components; i++)
        planes_nb = FFMAX(planes_nb, desc->comp[i].plane + 1);

     for (i = 0; i < planes_nb; i++) {
        int h = height;
        uint8_t *dst = _dst->data[i] + _dst->linesize[i] / 2;
        uint8_t *src = _src->data[i];
        if (i == 1 || i == 2) {
            h = FF_CEIL_RSHIFT(height, desc->log2_chroma_h);
        }
        for (j = 0; j < h; j++) {
            memcpy(dst, src, _src->linesize[i] / 2);
            dst += _dst->linesize[i];
            src += _src->linesize[i];
        }
    }

}

int ff_hevc_output_frame(HEVCContext *s, AVFrame *out, int flush)
{
    do {
        int nb_output = 0;
        int min_poc   = INT_MAX;
        int min_field = INT_MAX;
        int i, min_idx[2]= { 0 }, ret;

        if (s->sh.no_output_of_prior_pics_flag == 1 && s->no_rasl_output_flag == 1) {
            for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
                HEVCFrame *frame = &s->DPB[i];
                if (!(frame->flags & HEVC_FRAME_FLAG_BUMPING) && frame->poc != s->poc &&
                        frame->sequence == s->seq_output) {
                    ff_hevc_unref_frame(s, frame, HEVC_FRAME_FLAG_OUTPUT);
                }
            }
        }

        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCFrame *frame = &s->DPB[i];
            if ((frame->flags & HEVC_FRAME_FLAG_OUTPUT) &&
                frame->sequence == s->seq_output) {

                nb_output++;
                if (frame->poc < min_poc) {
                    min_poc = frame->poc;
                    min_idx[0] = i;
                }
            }
        }
#if FRAME_CONCEALMENT
        nb_output += s->no_display_pic;
#endif

        /* wait for more frames before output */
        if (!flush && s->seq_output == s->seq_decode && s->ps.sps &&
            nb_output <= s->ps.vps->vps_max_num_reorder_pics[s->ps.vps->vps_max_sub_layers - 1] + s->interlaced) {
            return 0;
        }

        if (nb_output) {
            HEVCFrame *frame = &s->DPB[min_idx[0]];
            HEVCFrame *field = NULL;
            AVFrame *dst = out;
            AVFrame *src = frame->frame;
            const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(src->format);
            int pixel_shift = !!(desc->comp[0].depth > 8);

            if (frame->field_order == AV_FIELD_TT ||
                frame->field_order == AV_FIELD_BB) {
                frame->flags |= HEVC_FRAME_FIRST_FIELD;

                for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
                    HEVCFrame *frame = &s->DPB[i];
                    if ((frame->flags & HEVC_FRAME_FLAG_OUTPUT) &&
                        frame->sequence == s->seq_output) {

                        if (!(frame->flags & HEVC_FRAME_FIRST_FIELD) &&
                            frame->poc < min_field) {
                            min_field = frame->poc;
                            min_idx[1] = i;
                        }
                    }
                }

                frame->flags &= ~(HEVC_FRAME_FIRST_FIELD);
                field = &s->DPB[min_idx[1]];
                if (field->poc != s->poc &&
                    field->frame->interlaced_frame &&
                    (field->field_order != AV_FIELD_TT ||
                     field->field_order != AV_FIELD_BB)) {
                    if (s->threads_type & FF_THREAD_FRAME )
                        ff_thread_await_progress(&field->tf, s->ps.sps->height, 0);

                    copy_field(s, src, field->frame, s->ps.sps->height);
                } else return 0;
            }

            ret = av_frame_ref(dst, src);

            if (frame->frame->interlaced_frame)
                for (i = 0; i < 3; i++)
                    dst->linesize[i] /= 2;

            if (frame->flags & HEVC_FRAME_FLAG_BUMPING)
                ff_hevc_unref_frame(s, frame, HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_BUMPING);
            else
                ff_hevc_unref_frame(s, frame, HEVC_FRAME_FLAG_OUTPUT);

            if (frame->field_order == AV_FIELD_TT ||
                frame->field_order == AV_FIELD_BB) {
                if (field &&
                    field->frame->interlaced_frame &&
                    (field->field_order != AV_FIELD_TT ||
                     field->field_order != AV_FIELD_BB)) {

                    if (field->flags & HEVC_FRAME_FLAG_BUMPING)
                        ff_hevc_unref_frame(s, field, HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_BUMPING);
                    else
                        ff_hevc_unref_frame(s, field, HEVC_FRAME_FLAG_OUTPUT);

                }
            }

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
                   "Output frame with POC (%d, %d).\n", (&s->DPB[min_idx[0]])->poc, (&s->DPB[min_idx[1]])->poc);
            return 1;
        }

        if (s->seq_output != s->seq_decode)
            s->seq_output = (s->seq_output + 1) & 0xff;
        else
            break;
    } while (1);

    return 0;
}

void ff_hevc_bump_frame(HEVCContext *s)
{
    int dpb = 0;
    int min_poc = INT_MAX;
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *frame = &s->DPB[i];
        if ((frame->flags) &&
            frame->sequence == s->seq_output &&
            frame->poc != s->poc) {
            dpb++;
        }
    }

    if (s->ps.sps && dpb >= s->ps.sps->temporal_layer[s->ps.sps->sps_max_sub_layers - 1].max_dec_pic_buffering) {
        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCFrame *frame = &s->DPB[i];
            if ((frame->flags) &&
                frame->sequence == s->seq_output &&
                frame->poc != s->poc) {
                if (frame->flags == HEVC_FRAME_FLAG_OUTPUT && frame->poc < min_poc) {
                    min_poc = frame->poc;
                }
            }
        }

        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCFrame *frame = &s->DPB[i];
            if (frame->flags & HEVC_FRAME_FLAG_OUTPUT &&
                frame->sequence == s->seq_output &&
                frame->poc <= min_poc) {
                frame->flags |= HEVC_FRAME_FLAG_BUMPING;
            }
        }

        dpb--;
    }
}

static int init_slice_rpl(HEVCContext *s)
{
    HEVCFrame *frame = s->ref;
    if (frame) {
        int ctb_count    = frame->ctb_count;
        int ctb_addr_ts  = s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_segment_addr];
        int i;

        if (s->slice_idx >= frame->rpl_buf->size / sizeof(RefPicListTab))
            return AVERROR_INVALIDDATA;

        for (i = ctb_addr_ts; i < ctb_count; i++)
            frame->rpl_tab[i] = (RefPicListTab *)frame->rpl_buf->data + s->slice_idx;
        frame->refPicList[s->slice_idx] = (RefPicList *)frame->rpl_tab[ctb_addr_ts];
        return 0;
    }
    return AVERROR_INVALIDDATA;
}

static int init_il_slice_rpl(HEVCContext *s)
{
    HEVCFrame *frame = s->inter_layer_ref;
    int ctb_count   = frame->ctb_count;
    int ctb_addr_ts = s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_segment_addr];
    int i;
    if (s->slice_idx >= frame->rpl_buf->size / sizeof(RefPicListTab))
        return AVERROR_INVALIDDATA;
    for (i = ctb_addr_ts; i < ctb_count; i++)
        frame->rpl_tab[i] = (RefPicListTab*)frame->rpl_buf->data + s->slice_idx;

    frame->refPicList[s->slice_idx] = (RefPicList*)frame->rpl_tab[ctb_addr_ts];
    
    return 0;
}

static HEVCFrame *find_ref_idx(HEVCContext *s, int poc)
{
    int i;
    int LtMask = (1 << s->ps.sps->log2_max_poc_lsb) - 1;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = &s->DPB[i];
        if (ref->frame->buf[0] && (ref->sequence == s->seq_decode)) {
            if ((ref->poc & LtMask) == poc)
                return ref;
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = &s->DPB[i];
        if (ref->frame->buf[0] && ref->sequence == s->seq_decode) {
            if (ref->poc == poc || (ref->poc & LtMask) == poc)
                return ref;
        }
    }

    av_log(s->avctx, AV_LOG_ERROR,
           "Could not find ref with POC %d\n", poc);
    return NULL;
}

#if ACTIVE_PU_UPSAMPLING
static void set_refindex_data(HEVCContext *s){
    int list, i;
    HEVCFrame  *refBL, *refEL, *ref;
    int nb_list = s->sh.slice_type==B_SLICE ? 2:1;
    //if(!s->vps->vps_nonHEVCBaseLayerFlag){
        refBL = s->BL_frame;

        init_il_slice_rpl(s);
        refEL = s->inter_layer_ref;
        for( list=0; list < nb_list; list++) {
            refEL->refPicList[s->slice_idx][list].nb_refs = 0;
            for(i=0; refBL->refPicList[s->slice_idx] && i< refBL->refPicList[s->slice_idx][list].nb_refs; i++) {
                //if(!s->vps->vps_nonHEVCBaseLayerFlag)
            	   ref = find_ref_idx(s, refBL->refPicList[s->slice_idx][list].list[i]);
                if(ref) {
                    refEL->refPicList[s->slice_idx][list].list[refEL->refPicList[s->slice_idx][list].nb_refs]           = refBL->refPicList[s->slice_idx][list].list[i];
                    refEL->refPicList[s->slice_idx][list].ref[refEL->refPicList[s->slice_idx][list].nb_refs]            = ref;
                    refEL->refPicList[s->slice_idx][list].isLongTerm[refEL->refPicList[s->slice_idx][list].nb_refs++]   = refBL->refPicList[s->slice_idx][list].isLongTerm[i];
                }
            }
        }
    //}
}
#else
static void scale_upsampled_mv_field(AVCodecContext *avctxt, void *input_ctb_row) {
    HEVCContext *s = avctxt->priv_data;

    int *index   = input_ctb_row, i, list;
    int ctb_size = 1 << s->ps.sps->log2_ctb_size;
    int nb_list = s->sh.slice_type==B_SLICE ? 2:1;

    

    HEVCFrame *refBL = s->BL_frame, *ref;
    HEVCFrame *refEL = s->inter_layer_ref;
    int start = (*index) * ctb_size;

    if( *index ==0 ) {
        init_il_slice_rpl(s);
        for( list=0; list < nb_list; list++) {
            refEL->refPicList[s->slice_idx][list].nb_refs = 0;
            for(i=0; refBL->refPicList[s->slice_idx] && i< refBL->refPicList[s->slice_idx][list].nb_refs; i++) {
                ref = find_ref_idx(s, refBL->refPicList[s->slice_idx][list].list[i]);
                if(ref) {
                    refEL->refPicList[s->slice_idx][list].list[refEL->refPicList[s->slice_idx][list].nb_refs]           = refBL->refPicList[s->slice_idx][list].list[i];
                    refEL->refPicList[s->slice_idx][list].ref[refEL->refPicList[s->slice_idx][list].nb_refs]            = ref;
                    refEL->refPicList[s->slice_idx][list].isLongTerm[refEL->refPicList[s->slice_idx][list].nb_refs++]   = refBL->refPicList[s->slice_idx][list].isLongTerm[i];
                }
            }
        }
    }
    for(i=0; i < s->ps.sps->ctb_width; i++)
        ff_upscale_mv_block(s,  i*ctb_size, start);
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
          s->rps[LT_CURR].nb_refs + s->rps[IL_REF0].nb_refs + s->rps[IL_REF1].nb_refs)) {
        av_log(s->avctx, AV_LOG_ERROR, "Zero refs in the frame RPS.\n");
        return AVERROR_INVALIDDATA;
    }

    for (list_idx = 0; list_idx < nb_list; list_idx++) {
        RefPicList  rpl_tmp = { { 0 } };
        RefPicList *rpl     = &s->ref->refPicList[s->slice_idx][list_idx];

        /* The order of the elements is
         * ST_CURR_BEF - ST_CURR_AFT - LT_CURR for the L0 and
         * ST_CURR_AFT - ST_CURR_BEF - LT_CURR for the L1 */
        int cand_lists[5] = { list_idx ? ST_CURR_AFT : ST_CURR_BEF, list_idx ? IL_REF1 : IL_REF0,
            list_idx ? ST_CURR_BEF : ST_CURR_AFT,
            LT_CURR,  list_idx ? IL_REF0 : IL_REF1};
        /* concatenate the candidate lists for the current frame */
        while (rpl_tmp.nb_refs < sh->nb_refs[list_idx]) {
            for (i = 0; i < FF_ARRAY_ELEMS(cand_lists); i++) {
                RefPicList *rps = &s->rps[cand_lists[i]];
                for (j = 0; j < rps->nb_refs && rpl_tmp.nb_refs < MAX_REFS; j++) {
                    rpl_tmp.list[rpl_tmp.nb_refs]       = rps->list[j];
                    rpl_tmp.ref[rpl_tmp.nb_refs]        = rps->ref[j];
                    rpl_tmp.isLongTerm[rpl_tmp.nb_refs] = i == 1  || i == 3 || i == 4;
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

#if FRAME_CONCEALMENT
static HEVCFrame * find_new_concealemnt_frame(HEVCContext *s, int poc, int gop_size) {
    int  poc_inf, poc_sup;
    HEVCFrame *ref = NULL;
    int max_val = (poc & 0xFFF8) + gop_size;
    int min_val = (poc & 0xFFF8);
    
    poc_inf = poc_sup = poc;
    while(1) {
        poc_inf--;
        if(poc_inf >= min_val && poc_inf!=s->poc ) {
            ref = find_ref_idx(s, poc_inf);
            if(ref && !ref->is_concealment_frame){
                printf("poc_inf %d \n", poc_inf);
                return ref;
            }
        }
        poc_sup++;
        if(poc_sup <= max_val && poc_sup!=s->poc) {
            ref = find_ref_idx(s, poc_sup);
            if(ref && !ref->is_concealment_frame) {
                printf("poc_inf %d \n", poc_sup);
                return ref;
            }
        }
        if(poc_inf < min_val && poc_sup > max_val)
            return NULL;
    }
}
#endif

static HEVCFrame *generate_missing_ref(HEVCContext *s, int poc)
{
    HEVCFrame *frame;
    int i;
#if FRAME_CONCEALMENT    
    int gop_size = 8; // FIXME the GOP size should not be  a constant
    HEVCFrame *conc_frame;
#else
    int x, y; 
#endif

    frame = alloc_frame(s);
    if (!frame)
        return NULL;

#if FRAME_CONCEALMENT
    if(!(poc & 0x0007)) {
        av_log(s->avctx, AV_LOG_ERROR, "The key pictures need to be successfully received cannot be recovered with this algorithm .\n");
        return AVERROR_INVALIDDATA;
    }
    printf("Find new reference \n");
    conc_frame = find_new_concealemnt_frame(s, poc, gop_size);
    if(!conc_frame) {
        av_log(s->avctx, AV_LOG_ERROR, "Concealment frame not found: this should not occur with this algorithm .\n");
        return AVERROR_INVALIDDATA;
    }
    printf("Reference found %d \n", conc_frame->poc);
#endif

#if FRAME_CONCEALMENT
    av_image_copy(  frame->frame->data, frame->frame->linesize, conc_frame->frame->data,
                    conc_frame->frame->linesize, s->sps->pix_fmt , conc_frame->frame->width,
                    conc_frame->frame->height);
#if COPY_MV
    memcpy(frame->rpl_buf->data, conc_frame->rpl_buf->data, frame->rpl_buf->size);
    memcpy(frame->tab_mvf_buf->data, conc_frame->tab_mvf_buf->data, frame->tab_mvf_buf->size);
    memcpy(frame->rpl_tab_buf->data, conc_frame->rpl_tab_buf->data, frame->rpl_tab_buf->size);
#endif
#else
    if (!s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]) {
        for (i = 0; frame->frame->buf[i]; i++)
            memset(frame->frame->buf[i]->data, 1 << (s->ps.sps->bit_depth[CHANNEL_TYPE_LUMA] - 1),
                   frame->frame->buf[i]->size);
    } else {
        for (i = 0; frame->frame->data[i]; i++)
            for (y = 0; y < (s->ps.sps->height >> s->ps.sps->vshift[i]); y++)
                for (x = 0; x < (s->ps.sps->width >> s->ps.sps->hshift[i]); x++) {
                    AV_WN16(frame->frame->data[i] + y * frame->frame->linesize[i] + 2 * x,
                            1 << (s->ps.sps->bit_depth[CHANNEL_TYPE_LUMA] - 1));
                }
    }
#endif
    frame->poc                  = poc;
    frame->sequence             = s->seq_decode;
    
    
#if FRAME_CONCEALMENT
    frame->flags                = HEVC_FRAME_FLAG_OUTPUT; // Display the frame
    frame->is_concealment_frame = 1;
#else
    frame->flags                = 0;
#endif

    if (s->threads_type & FF_THREAD_FRAME) {
        ff_thread_report_progress(&frame->tf, INT_MAX, 0);
    }
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

static void init_upsampled_mv_fields(HEVCContext *s) {
    HEVCFrame *refEL = s->inter_layer_ref;
    memset(refEL->tab_mvf_buf->data, 0, refEL->tab_mvf_buf->size); // is intra = 0
}

int ff_hevc_frame_rps(HEVCContext *s)
{
    const ShortTermRPS *short_rps = s->sh.short_term_rps;
    const LongTermRPS  *long_rps  = &s->sh.long_term_rps;
    RefPicList               *rps = s->rps;
    int i, ret;
     const HEVCVPS *vps = s->ps.vps;

    if (!short_rps) {
        rps[0].nb_refs = rps[1].nb_refs = 0;
        if (!long_rps)
            return 0;
    }

    if (s->nuh_layer_id > 0 && s->ps.vps->vps_ext.max_one_active_ref_layer_flag > 0) {
        if (!(s->nal_unit_type >= NAL_BLA_W_LP && s->nal_unit_type <= NAL_CRA_NUT) &&
            s->ps.sps->set_mfm_enabled_flag)  {
#if !ACTIVE_PU_UPSAMPLING
            int *arg, *ret, cmpt = (s->ps.sps->ctb_height);

            arg = av_malloc(cmpt*sizeof(int));
            ret = av_malloc(cmpt*sizeof(int));
            for(i=0; i < cmpt; i++)
                arg[i] = i;
            if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
                s->avctx->execute(s->avctx, (void *) scale_upsampled_mv_field, arg, ret, cmpt, sizeof(int));//fixme: AVC BL can't be upsampled
            av_free(arg);
            av_free(ret);
#else
            if (!s->ps.vps->vps_nonHEVCBaseLayerFlag)
                set_refindex_data(s);
#endif
        } else {
            if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
                init_upsampled_mv_fields(s);//fixme: AVC BL can't be upsampled
        }
    }
    /* clear the reference flags on all frames except the current one */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *frame = &s->DPB[i];
        if (frame == s->ref)
            continue;
        mark_ref(frame, 0);
    }

    for (i = 0; i < NB_RPS_TYPE; i++)
        rps[i].nb_refs = 0;
    if (!s->nuh_layer_id ||
        (s->nuh_layer_id > 0 &&
        !(s->nal_unit_type >= NAL_BLA_W_LP &&
        s->nal_unit_type <= NAL_CRA_NUT &&
        s->sh.active_num_ILR_ref_idx))) {
        /* add the short refs */
        for (i = 0; short_rps && i < short_rps->num_delta_pocs; i++)
        {
            int poc = s->poc + short_rps->delta_poc[i];
            int list;

            if (!short_rps->used[i])
                list = ST_FOLL;
            else if (i < short_rps->num_negative_pics)
                list = ST_CURR_BEF;
            else
                list = ST_CURR_AFT;

            ret = add_candidate_ref( s, &rps[list], poc,
                                     HEVC_FRAME_FLAG_SHORT_REF);
            if (ret < 0)
                return ret;
        }

        /* add the long refs */
        for (i = 0; long_rps && i < long_rps->nb_refs; i++)
        {
            int poc  = long_rps->poc[i];
            int list = long_rps->used[i] ? LT_CURR : LT_FOLL;

            ret = add_candidate_ref( s, &rps[list], poc,
                                     HEVC_FRAME_FLAG_LONG_REF);
            if (ret < 0)
                return ret;
        }
    } else {
        for (i = 0; short_rps && i < short_rps->num_delta_pocs; i++) {
            int poc = s->poc + short_rps->delta_poc[i];
            HEVCFrame *ref = find_ref_idx(s, poc);
            if (ref)
                mark_ref(ref, HEVC_FRAME_FLAG_SHORT_REF);
        }
        for (i = 0; long_rps && i < long_rps->nb_refs; i++) {
            int poc = long_rps->poc[i];
            HEVCFrame *ref = find_ref_idx(s, poc);
            if (ref)
                mark_ref(ref, HEVC_FRAME_FLAG_LONG_REF);
        }
    }

/*    if (s->nuh_layer_id && s->sh.active_num_ILR_ref_idx >0) {
        for( i=0; i < vps->Hevc_VPS_Ext.num_direct_ref_layers[s->nuh_layer_id]; i++ )
        {
            int maxTidIlRefPicsPlus1 = vps->Hevc_VPS_Ext.max_tid_il_ref_pics_plus1[vps->Hevc_VPS_Ext.layer_id_in_vps[+++]][vps->Hevc_VPS_Ext.layer_id_in_vps[s->nuh_layer_id]];
            if( (s->temporal_id <= maxTidIlRefPicsPlus1-1) || (maxTidIlRefPicsPlus1==0 && ilpPic[i]->getSlice(0)->getRapPicFlag() ) )
                numInterLayerRPSPics++;
        }
*/
    if (s->nuh_layer_id) {
        for (i = 0; i < s->sh.active_num_ILR_ref_idx; i ++) {
            if ((vps->vps_ext.view_id_val[s->nuh_layer_id] <= vps->vps_ext.view_id_val[0]) &&
                (vps->vps_ext.view_id_val[s->nuh_layer_id] <= vps->vps_ext.view_id_val[vps->vps_ext.ref_layer_id[s->nuh_layer_id][s->sh.inter_layer_pred_layer_idc[i]]])){
                //IL_REF0 , IL_REF1
                ret = add_candidate_ref(s, &rps[IL_REF0], s->poc, HEVC_FRAME_FLAG_LONG_REF);
            }
            else{
                ret = add_candidate_ref(s, &rps[IL_REF1], s->poc, HEVC_FRAME_FLAG_LONG_REF);
            }
        }
    }
    /* release any frames that are now unused */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        ff_hevc_unref_frame(s, &s->DPB[i], 0);

    return 0;
}

int ff_hevc_compute_poc(HEVCContext *s, int poc_lsb)
{
    int max_poc_lsb  = 1 << s->ps.sps->log2_max_poc_lsb;
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

    if (s->sh.slice_type == I_SLICE || (s->nuh_layer_id &&
                                        (s->nal_unit_type >= NAL_BLA_W_LP) &&
                                        (s->nal_unit_type<= NAL_CRA_NUT))) {
        return s->sh.active_num_ILR_ref_idx;
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
    if(s->nuh_layer_id)// {
//        for( i = 0; i < s->vps->Hevc_VPS_Ext.max_one_active_ref_layer_flag; i ++) {
            ret += s->sh.active_num_ILR_ref_idx;
      //  }
   // }
    return ret;
}
