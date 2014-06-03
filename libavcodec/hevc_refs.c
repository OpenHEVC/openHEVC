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

#include "libavutil/pixdesc.h"

#include "internal.h"
#include "thread.h"
#include "hevc.h"

void ff_hevc_unref_frame(HEVCContext *s, HEVCFrame *frame, int flags) {
    int is_up_sampled = 2;
    if (!frame->frame || !frame->frame->buf[0])
        return;
    frame->flags &= ~flags;
    if (frame->active_el_frame)
        is_up_sampled = ff_thread_get_il_up_status(s->avctx, frame->poc);
    if (!frame->flags && is_up_sampled == 2) {
        if(frame->active_el_frame)
            ff_thread_report_il_status2(s->avctx, frame->poc, 0);
        ff_thread_release_buffer(s->avctx, &frame->tf);
        av_buffer_unref(&frame->tab_mvf_buf);
        frame->tab_mvf = NULL;
        frame->active_el_frame = 0;

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
    int pic_width_cb = s->sps->ctb_width;
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

    ref->poc      = poc;
    ref->flags    = HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_SHORT_REF;
    if (s->sh.pic_output_flag == 0)
        ref->flags &= ~(HEVC_FRAME_FLAG_OUTPUT);
    ref->sequence = s->seq_decode;
    ref->window   = s->sps->output_window;
    ref->active_el_frame = 0;
    return 0;
}
#ifdef REF_IDX_FRAMEWORK
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
    ref->window         = s->sps->output_window;
    ref->active_el_frame = 0;
    if (s->threads_type & FF_THREAD_FRAME)
        ff_thread_report_progress(&s->inter_layer_ref->tf, INT_MAX, 0);

    return 0;
}
#endif
int ff_hevc_output_frame(HEVCContext *s, AVFrame *out, int flush)
{
    do {
        int nb_output = 0;
        int min_poc   = INT_MAX;
        int i, min_idx=0, ret;

        if(s->sh.no_output_of_prior_pics_flag == 1) {
            for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
                HEVCFrame *frame = &s->DPB[i];
                if ((frame->flags & HEVC_FRAME_FLAG_OUTPUT) && frame->poc != s->poc &&
                        frame->sequence == s->seq_output) {
                    frame->flags &= ~(HEVC_FRAME_FLAG_OUTPUT);
                }
            }
        }

        if (s->sh.no_output_of_prior_pics_flag == 1) {
            for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
                HEVCFrame *frame = &s->DPB[i];
                if ((frame->flags & HEVC_FRAME_FLAG_OUTPUT) && frame->poc != s->poc &&
                        frame->sequence == s->seq_output) {
                    frame->flags &= ~(HEVC_FRAME_FLAG_OUTPUT);
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
                    min_idx = i;
                }
            }
        }
#if FRAME_CONCEALMENT
        nb_output += s->no_display_pic;
#endif

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
#if FRAME_CONCEALMENT
            /*      ADD remove frames from the DPB    */
            if(s->prev_display_poc == -1 || s->prev_display_poc == min_poc-1) {
                    s->no_display_pic = 0;
                frame->flags &= ~(HEVC_FRAME_FLAG_OUTPUT);
                av_log(s->avctx, AV_LOG_ERROR,"min_poc %d \n", min_poc);
                s->prev_display_poc = min_poc;
            } else {
                s->no_display_pic ++;
                av_log(s->avctx, AV_LOG_ERROR,"min_poc %d \n", min_poc-1);
                s->prev_display_poc ++; // incremate
            }
#else
            frame->flags &= ~(HEVC_FRAME_FLAG_OUTPUT);
            
#endif

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
        if (ref->frame->buf[0] && ref->sequence == s->seq_decode) {
            if (ref->poc == poc || (ref->poc & LtMask) == poc)
                return ref;
        }
    }

    av_log(s->avctx, AV_LOG_ERROR,
           "Could not find ref with POC %d\n", poc);
    return NULL;
}

#ifdef SVC_EXTENSION

#if ACTIVE_PU_UPSAMPLING
static void set_refindex_data(HEVCContext *s){
    int list, i;
    HEVCFrame  *refBL, *refEL, *ref;
    int nb_list = s->sh.slice_type==B_SLICE ? 2:1;
    refBL = s->BL_frame;
     
    init_il_slice_rpl(s);
    refEL = s->inter_layer_ref;
    for( list=0; list < nb_list; list++) {
        refEL->refPicList[list].nb_refs = 0;
        for(i=0; refBL->refPicList && i< refBL->refPicList[list].nb_refs; i++) {
            ref = find_ref_idx(s, refBL->refPicList[list].list[i]);
            if(ref) {
                refEL->refPicList[list].list[refEL->refPicList[list].nb_refs]           = refBL->refPicList[list].list[i];
                refEL->refPicList[list].ref[refEL->refPicList[list].nb_refs]            = ref;
                refEL->refPicList[list].isLongTerm[refEL->refPicList[list].nb_refs++]   = refBL->refPicList[list].isLongTerm[i];
            }
        }
    }
}
#else
static void scale_upsampled_mv_field(AVCodecContext *avctxt, void *input_ctb_row) {
    HEVCContext *s = avctxt->priv_data;

    int *index   = input_ctb_row, i, list;
    int ctb_size = 1 << s->sps->log2_ctb_size;
    int nb_list = s->sh.slice_type==B_SLICE ? 2:1;

    

    HEVCFrame *refBL = s->BL_frame, *ref;
    HEVCFrame *refEL = s->inter_layer_ref;
    int start = (*index) * ctb_size;

    if( *index ==0 ) {
        init_il_slice_rpl(s);
        for( list=0; list < nb_list; list++) {
            refEL->refPicList[list].nb_refs = 0;
            for(i=0; refBL->refPicList && i< refBL->refPicList[list].nb_refs; i++) {
                ref = find_ref_idx(s, refBL->refPicList[list].list[i]);
                if(ref) {
                    refEL->refPicList[list].list[refEL->refPicList[list].nb_refs]           = refBL->refPicList[list].list[i];
                    refEL->refPicList[list].ref[refEL->refPicList[list].nb_refs]            = ref;
                    refEL->refPicList[list].isLongTerm[refEL->refPicList[list].nb_refs++]   = refBL->refPicList[list].isLongTerm[i];
                }
            }
        }
    }
    for(i=0; i < s->sps->ctb_width; i++)
        ff_upscale_mv_block(s,  i*ctb_size, start);
}
#endif
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
        RefPicList *rpl     = &s->ref->refPicList[list_idx];

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

#ifdef REF_IDX_FRAMEWORK
static void init_upsampled_mv_fields(HEVCContext *s) {
    HEVCFrame *refEL = s->inter_layer_ref;
    memset(refEL->tab_mvf_buf->data, 0, refEL->tab_mvf_buf->size); // is intra = 0
}
#endif

int ff_hevc_frame_rps(HEVCContext *s)
{
    int ctb_size                  = 1<<s->sps->log2_ctb_size;
    const ShortTermRPS *short_rps = s->sh.short_term_rps;
    const LongTermRPS  *long_rps  = &s->sh.long_term_rps;
    RefPicList               *rps = s->rps;
    HEVCVPS *vps                  = s->vps;
    int i, ret;

    if (!short_rps) {
        rps[0].nb_refs = rps[1].nb_refs = 0;
        if (!long_rps)
            return 0;
    }
#ifdef REF_IDX_FRAMEWORK
#ifdef REF_IDX_MFM
    
#ifdef ZERO_NUM_DIRECT_LAYERS
    if( s->nuh_layer_id > 0 && s->vps->max_one_active_ref_layer_flag > 0 )
#else
    if (s->nuh_layer_id)
#endif
    {
        if (!(s->nal_unit_type >= NAL_BLA_W_LP && s->nal_unit_type <= NAL_CRA_NUT) && s->sps->set_mfm_enabled_flag)  {
#if !ACTIVE_PU_UPSAMPLING
            int *arg, *ret, cmpt = (s->sps->height / ctb_size) + (s->sps->height%ctb_size ? 1:0);

            arg = av_malloc(cmpt*sizeof(int));
            ret = av_malloc(cmpt*sizeof(int));
            for(i=0; i < cmpt; i++)
                arg[i] = i;

            s->avctx->execute(s->avctx, (void *) scale_upsampled_mv_field, arg, ret, cmpt, sizeof(int));
            av_free(arg);
            av_free(ret);
#else
            set_refindex_data(s);
#endif
        } else {
            init_upsampled_mv_fields(s);
            if(s->threads_type&FF_THREAD_FRAME)
                ff_thread_report_il_status(s->avctx, s->poc, 2);
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
    if(!s->nuh_layer_id || !(s->nal_unit_type >= NAL_BLA_W_LP && s->nal_unit_type <= NAL_CRA_NUT && s->sps->set_mfm_enabled_flag))  {
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
            if (ref)
                mark_ref(ref, HEVC_FRAME_FLAG_SHORT_REF);
        }
        for (i = 0; long_rps && i < long_rps->nb_refs; i++) {
            int poc  = long_rps->poc[i];
            HEVCFrame *ref = find_ref_idx(s, poc);
            if (ref)
                mark_ref(ref, HEVC_FRAME_FLAG_LONG_REF);
        }
    }
#endif

#ifdef REF_IDX_FRAMEWORK
    if(s->nuh_layer_id) {
#ifdef JCTVC_M0458_INTERLAYER_RPS_SIG
        for( i = 0; i < s->vps->max_one_active_ref_layer_flag; i ++) {
#else
            for( i = 0; i < m_numILRRefIdx; i ++) {
#endif
                if((vps->m_viewIdVal[s->nuh_layer_id] <= vps->m_viewIdVal[0]) && (vps->m_viewIdVal[s->nuh_layer_id] <= vps->m_viewIdVal[vps->m_refLayerId[s->nuh_layer_id][s->sh.inter_layer_pred_layer_idc[i]]])){
                //IL_REF0 , IL_REF1
                    ret = add_candidate_ref(s, &rps[IL_REF0], s->poc, HEVC_FRAME_FLAG_LONG_REF);
                }
                else{
                    ret = add_candidate_ref(s, &rps[IL_REF1], s->poc, HEVC_FRAME_FLAG_LONG_REF);
                }
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

#ifdef REF_IDX_FRAMEWORK
    if( s->sh.slice_type == I_SLICE || (s->nuh_layer_id &&
                                        (s->nal_unit_type >= NAL_BLA_W_LP) &&
                                        (s->nal_unit_type<= NAL_CRA_NUT ) ))
#else
        if (s->sh.slice_type == I_SLICE)
#endif
        {
#ifdef REF_IDX_FRAMEWORK
#ifdef JCTVC_M0458_INTERLAYER_RPS_SIG
            return s->sh.active_num_ILR_ref_idx;
#else
            return s->vps->m_numDirectRefLayers[s->nuh_layer_id];
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
#ifdef REF_IDX_FRAMEWORK
    if(s->nuh_layer_id) {
#ifdef JCTVC_M0458_INTERLAYER_RPS_SIG
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
