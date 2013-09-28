/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "hevc.h"
#include "internal.h"
//#define TEST_DPB

#define LOCK_DBP   if (s->threads_type == FF_THREAD_FRAME ) ff_thread_mutex_lock_dpb(s->avctx)
#define UNLOCK_DBP if (s->threads_type == FF_THREAD_FRAME ) ff_thread_mutex_unlock_dpb(s->avctx)

static void thread_cnt_ref(HEVCContext *s, int val)
{
    if (s->threads_type == FF_THREAD_FRAME ) {
        RefPicList  *refPicList = s->ref->refPicList;
        uint8_t i, list_idx;
        for ( list_idx = 0; list_idx < 2; list_idx++)
            for(i = 0; i < refPicList[list_idx].numPic; i++)
                s->DPB[refPicList[list_idx].idx[i]]->threadCnt += val;
        if (s->sh.slice_type == B_SLICE && s->sh.collocated_from_l0_flag == 0) {
            s->DPB[s->ref->refPicList[1].idx[s->sh.collocated_ref_idx]]->threadCnt += val;
        } else if (s->sh.slice_type == P_SLICE ||
                (s->sh.slice_type == B_SLICE && s->sh.collocated_from_l0_flag == 1)) {
            s->DPB[s->ref->refPicList[0].idx[s->sh.collocated_ref_idx]]->threadCnt += val;
        }
        s->ref->threadCnt += val;
        s->vps->threadCnt += val;
        s->sps->threadCnt += val;
        s->pps->threadCnt += val;
        if (val == -1 && s->is_decoded == 1) {
            if (s->vps->threadCnt == 0 && s->vps != s->vps_list[s->sps->vps_id])
                av_free(s->vps_list[s->sps->vps_id]);
            if (s->sps->threadCnt == 0 && s->sps != s->sps_list[s->pps->sps_id])
                av_free(s->sps);
            if (s->pps->threadCnt == 0 && s->pps != s->pps_list[s->sh.pps_id])
                ff_hevc_pps_free(&s->pps);
        }
    }
}
void ff_hevc_thread_cnt_dec_ref(HEVCContext *s)
{
    LOCK_DBP;
    thread_cnt_ref(s, -1);
    UNLOCK_DBP;
}

int ff_hevc_find_ref_idx(HEVCContext *s, int poc)
{
    int i;
    int LtMask = (1 << s->sps->log2_max_poc_lsb) - 1;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = s->DPB[i];
        if (ref->frame->buf[0] && (ref->sequence == s->seq_decode)) {
            if ((ref->flags & HEVC_FRAME_FLAG_LONG_REF) != 0 && (ref->poc & LtMask) == poc)
	            return i;
	    }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = s->DPB[i];
        if (ref->frame->buf[0] && (ref->sequence == s->seq_decode)) {
            if ((ref->flags & HEVC_FRAME_FLAG_SHORT_REF) != 0 && (ref->poc == poc || (ref->poc & LtMask) == poc))
	            return i;
	    }
    }

    av_log(s->avctx, AV_LOG_ERROR,
           "Could not find ref with POC %d\n", poc);
    return 0;
}

static void free_refPicListTab(HEVCContext *s, HEVCFrame *ref)
{
    int j;
    int ctb_count = ref->ctb_count;

    if (!ref->refPicListTab)
        return;

    for (j = ctb_count-1; j > 0; j--) {
        if (ref->refPicListTab[j] != ref->refPicListTab[j-1])
            av_freep(&ref->refPicListTab[j]);
        ref->refPicListTab[j] = NULL;
    }
    if (ref->refPicListTab[0] != NULL) {
        av_freep(&ref->refPicListTab[0]);
        ref->refPicListTab[0] = NULL;
    }
    ref->refPicList = NULL;
}

static void unref_frame(HEVCContext *s, HEVCFrame *frame, int flags)
{
    if (frame->threadCnt == 0) {
        frame->flags &= ~flags;
        if (!frame->flags) {
            ff_thread_release_buffer(s->avctx, &frame->threadFrame);
            free_refPicListTab(s, frame);
        }
    }
}

static void update_refs(HEVCContext *s)
{
    int i, j;
    int used[FF_ARRAY_ELEMS(s->DPB)] = { 0 };

    for (i = 0; i < 5; i++) {
        RefPicList *rpl = &s->sh.refPocList[i];
        for (j = 0; j < rpl->numPic; j++)
            used[rpl->idx[j]] = 1;
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *frame = s->DPB[i];
        if (frame->frame->buf[0] && !used[i])
            unref_frame(s, frame, HEVC_FRAME_FLAG_SHORT_REF |
                                          HEVC_FRAME_FLAG_LONG_REF);
    }
}

int ff_hevc_find_next_ref(HEVCContext *s, int poc)
{
    int i;

    if (!s->sh.first_slice_in_pic_flag)
        return ff_hevc_find_ref_idx(s, poc);

    update_refs(s);

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = s->DPB[i];
        if (!ref->frame->buf[0]) {
            return i;
        }
    }
    av_log(s->avctx, AV_LOG_ERROR,
           "could not free room for POC %d\n", poc);
    return -1;
}

static void malloc_refPicListTab(HEVCContext *s, HEVCFrame *ref)
{
    int i;
    int ctb_count   = ref->ctb_count;
    int ctb_addr_ts = s->pps->ctb_addr_rs_to_ts[s->sh.slice_address];

    ref->refPicListTab[ctb_addr_ts] = av_mallocz(sizeof(RefPicListTab));
    for (i = ctb_addr_ts; i < ctb_count-1; i++)
        ref->refPicListTab[i+1] = ref->refPicListTab[i];
    ref->refPicList = (RefPicList*) ref->refPicListTab[ctb_addr_ts];
}

RefPicList* ff_hevc_get_ref_list(HEVCContext *s, int short_ref_idx, int x0, int y0)
{
    if (x0 < 0 || y0 < 0) {
        return s->ref->refPicList;
    } else {
        HEVCFrame *ref   = s->DPB[short_ref_idx];
        int x_cb         = x0 >> s->sps->log2_ctb_size;
        int y_cb         = y0 >> s->sps->log2_ctb_size;
        int ctb_addr_rs  = y_cb * s->sps->pic_width_in_ctbs + x_cb;
        int ctb_addr_ts  = s->pps->ctb_addr_rs_to_ts[ctb_addr_rs];
        return (RefPicList*) ref->refPicListTab[ctb_addr_ts];
    }
}

void ff_hevc_clear_refs(HEVCContext *s)
{
    int i;
    LOCK_DBP;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        unref_frame(s, s->DPB[i], HEVC_FRAME_FLAG_SHORT_REF | HEVC_FRAME_FLAG_LONG_REF);

    UNLOCK_DBP;
}

void ff_hevc_flush_dpb(HEVCContext *s)
{
    int i;
    LOCK_DBP;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        unref_frame(s, s->DPB[i], ~0);

    LOCK_DBP;
}

int ff_hevc_set_new_ref(HEVCContext *s, AVFrame **frame, int poc)
{
    int i, ret;
    LOCK_DBP;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *ref = s->DPB[i];
        if (!ref->frame->buf[0]) {
            *frame          = ref->frame;
            s->ref          = ref;
            s->curr_dpb_idx = i;
            ref->poc        = poc;
            ref->frame->pts = s->pts;

            ref->flags      = HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_SHORT_REF;
            ref->sequence   = s->seq_decode;
            ref->threadCnt  = 0;
            ref->is_decoded = 0;

	        update_refs(s);
            ff_hevc_set_ref_pic_list(s, ref);

            ret = ff_thread_get_buffer(s->avctx, &ref->threadFrame, AV_GET_BUFFER_FLAG_REF);
            thread_cnt_ref(s, 1);
	        ff_thread_finish_setup(s->avctx);
            UNLOCK_DBP;
            return ret;
        }
    }
    av_log(s->avctx, AV_LOG_ERROR,
           "DPB is full, could not add ref with POC %d\n", poc);
    UNLOCK_DBP;
    return -1;
}

int ff_hevc_output_frame(HEVCContext *s, AVFrame *out, int flush)
{
    int nb_output = 0;
    int min_poc   = 0xFFFF;
    int i, j, min_idx, ret;
    uint8_t run = 1;
    AVFrame *dst, *src;
    min_idx = 0;
    LOCK_DBP;
    while (run) {
        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCFrame *frame = s->DPB[i];
            if (frame->is_decoded == 1 && (frame->flags & HEVC_FRAME_FLAG_OUTPUT) &&
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
            nb_output <= s->sps->temporal_layer[s->temporal_id].num_reorder_pics + 1) {
            UNLOCK_DBP;
            return 0;
        }

        if (nb_output) {
            HEVCFrame *frame = s->DPB[min_idx];
            SPS *sps = s->sps_list[s->prev_sps_id];
            dst = out;
            src = frame->frame;

            frame->flags &= ~HEVC_FRAME_FLAG_OUTPUT;
            ret = av_frame_ref(dst, src);
            if (ret < 0) {
                UNLOCK_DBP;
                return ret;
            }
            for (j = 0; j < 3; j++) {
                int off = (sps->pic_conf_win.left_offset >> sps->hshift[j]) << sps->pixel_shift +
                          (sps->pic_conf_win.top_offset >> sps->vshift[j]) * dst->linesize[j];
                if (s->strict_def_disp_win)
                    off += (sps->vui.def_disp_win.left_offset >> sps->hshift[j]) +
                           (sps->vui.def_disp_win.top_offset >> sps->vshift[j]) * dst->linesize[j];
                dst->data[j] += off;
            }
            UNLOCK_DBP;
            return 1;
        }

        if (s->seq_output != s->seq_decode)
            s->seq_output = (s->seq_output + 1) & 0xff;
        else
            run = 0;
    }
    UNLOCK_DBP;
    return 0;
}

int ff_hevc_compute_poc(HEVCContext *s, int poc_lsb)
{
    int max_poc_lsb  = 1 << s->sps->log2_max_poc_lsb;
    int prev_poc_lsb = s->pocTid0 % max_poc_lsb;
    int prev_poc_msb = s->pocTid0 - prev_poc_lsb;
    int poc_msb;

    if ((poc_lsb < prev_poc_lsb) && ((prev_poc_lsb - poc_lsb) >= max_poc_lsb / 2))
        poc_msb = prev_poc_msb + max_poc_lsb;
    else if ((poc_lsb > prev_poc_lsb) && ((poc_lsb - prev_poc_lsb) > (max_poc_lsb / 2)))
        poc_msb = prev_poc_msb - max_poc_lsb;
    else
        poc_msb = prev_poc_msb;

    // For BLA picture types, POCmsb is set to 0.
    if (s->nal_unit_type == NAL_BLA_W_LP ||
        s->nal_unit_type == NAL_BLA_W_RADL ||
        s->nal_unit_type == NAL_BLA_N_LP)
        poc_msb = 0;

    return poc_msb + poc_lsb;
}

void ff_hevc_set_ref_pic_list(HEVCContext *s, HEVCFrame *ref)
{
    SliceHeader *sh = &s->sh;
    RefPicList  *refPocList = s->sh.refPocList;
    RefPicList  *refPicList;
    RefPicList  refPicListTmp[2]= {{{0}}};

    uint8_t num_ref_idx_lx_act[2];
    uint8_t cIdx;
    uint8_t num_poc_total_curr;
    uint8_t num_rps_curr_lx;
    uint8_t first_list;
    uint8_t sec_list;
    uint8_t i, list_idx;
	uint8_t nb_list = s->sh.slice_type == B_SLICE ? 2 : 1;

    malloc_refPicListTab(s, ref);

    if (s->sh.short_term_rps == NULL) return;

    refPicList = ref->refPicList;

    num_ref_idx_lx_act[0] = sh->num_ref_idx_l0_active;
    num_ref_idx_lx_act[1] = sh->num_ref_idx_l1_active;
    refPicList[1].numPic = 0;
    for ( list_idx = 0; list_idx < nb_list; list_idx++) {
        /* The order of the elements is
         * ST_CURR_BEF - ST_CURR_AFT - LT_CURR for the RefList0 and
         * ST_CURR_AFT - ST_CURR_BEF - LT_CURR for the RefList1
         */
        first_list = list_idx == 0 ? ST_CURR_BEF : ST_CURR_AFT;
        sec_list   = list_idx == 0 ? ST_CURR_AFT : ST_CURR_BEF;

        /* even if num_ref_idx_lx_act is inferior to num_poc_total_curr we fill in
         * all the element from the Rps because we might reorder the list. If
         * we reorder the list might need a reference picture located after
         * num_ref_idx_lx_act.
         */
        num_poc_total_curr = refPocList[ST_CURR_BEF].numPic + refPocList[ST_CURR_AFT].numPic + refPocList[LT_CURR].numPic;
        num_rps_curr_lx    = num_poc_total_curr > num_ref_idx_lx_act[list_idx] ? num_poc_total_curr : num_ref_idx_lx_act[list_idx];
        cIdx = 0;
        while(cIdx < num_rps_curr_lx) {
            for(i = 0; i < refPocList[first_list].numPic && cIdx < num_rps_curr_lx; i++) {
                refPicListTmp[list_idx].list[cIdx] = refPocList[first_list].list[i];
                refPicListTmp[list_idx].idx[cIdx]  = refPocList[first_list].idx[i];
                refPicListTmp[list_idx].is_long_term[cIdx]  = 0;
                cIdx++;
            }
            for(i = 0; i < refPocList[sec_list].numPic && cIdx < num_rps_curr_lx; i++) {
                refPicListTmp[list_idx].list[cIdx] = refPocList[sec_list].list[i];
                refPicListTmp[list_idx].idx[cIdx]  = refPocList[sec_list].idx[i];
                refPicListTmp[list_idx].is_long_term[cIdx]  = 0;
                cIdx++;
            }
            for(i = 0; i < refPocList[LT_CURR].numPic && cIdx < num_rps_curr_lx; i++) {
                refPicListTmp[list_idx].list[cIdx] = refPocList[LT_CURR].list[i];
                refPicListTmp[list_idx].idx[cIdx]  = refPocList[LT_CURR].idx[i];
                refPicListTmp[list_idx].is_long_term[cIdx]  = 1;
                cIdx++;
            }
        }
        refPicList[list_idx].numPic = num_ref_idx_lx_act[list_idx];
        if (s->sh.ref_pic_list_modification_flag_lx[list_idx] == 1) {
            num_rps_curr_lx = num_ref_idx_lx_act[list_idx];
            refPicList[list_idx].numPic = num_rps_curr_lx;
            for(i = 0; i < num_ref_idx_lx_act[list_idx]; i++) {
                refPicList[list_idx].list[i] = refPicListTmp[list_idx].list[sh->list_entry_lx[list_idx][ i ]];
                refPicList[list_idx].idx[i]  = refPicListTmp[list_idx].idx[sh->list_entry_lx[list_idx][ i ]];
                refPicList[list_idx].is_long_term[i]  = refPicListTmp[list_idx].is_long_term[sh->list_entry_lx[list_idx][ i ]];
            }
        } else {
            for(i = 0; i < num_ref_idx_lx_act[list_idx]; i++) {
                refPicList[list_idx].list[i] = refPicListTmp[list_idx].list[i];
                refPicList[list_idx].idx[i]  = refPicListTmp[list_idx].idx[i];
                refPicList[list_idx].is_long_term[i]  = refPicListTmp[list_idx].is_long_term[i];
            }
        }
    }
}

void ff_hevc_set_ref_poc_list(HEVCContext *s)
{
    int i;
    int j = 0;
    int k = 0;
    const ShortTermRPS *rps  = s->sh.short_term_rps;
    LongTermRPS *long_rps    = &s->sh.long_term_rps;
    RefPicList   *refPocList = s->sh.refPocList;
    int MaxPicOrderCntLsb = 1 << s->sps->log2_max_poc_lsb;

    if (rps != NULL) {
        for (i = 0; i < rps->num_negative_pics; i ++) {
            if ( rps->used[i] == 1 ) {
                refPocList[ST_CURR_BEF].list[j] = s->poc + rps->delta_poc[i];
                refPocList[ST_CURR_BEF].idx[j]  = ff_hevc_find_ref_idx(s, refPocList[ST_CURR_BEF].list[j]);
                j++;
            } else {
                refPocList[ST_FOLL].list[k] = s->poc + rps->delta_poc[i];
                refPocList[ST_FOLL].idx[k]  = ff_hevc_find_ref_idx(s, refPocList[ST_FOLL].list[k]);
                k++;
            }
        }
        refPocList[ST_CURR_BEF].numPic = j;
        j = 0;
        for (i = rps->num_negative_pics; i < rps->num_delta_pocs; i ++) {
            if (rps->used[i] == 1) {
                refPocList[ST_CURR_AFT].list[j] = s->poc + rps->delta_poc[i];
                refPocList[ST_CURR_AFT].idx[j]  = ff_hevc_find_ref_idx(s, refPocList[ST_CURR_AFT].list[j]);
                j++;
            } else {
                refPocList[ST_FOLL].list[k] = s->poc + rps->delta_poc[i];
                refPocList[ST_FOLL].idx[k]  = ff_hevc_find_ref_idx(s, refPocList[ST_FOLL].list[k]);
                k++;
            }
        }
        refPocList[ST_CURR_AFT].numPic = j;
        refPocList[ST_FOLL].numPic = k;
        for( i = 0, j= 0, k = 0; i < long_rps->num_long_term_sps + long_rps->num_long_term_pics; i++) {
            int pocLt = long_rps->PocLsbLt[i];
            if (long_rps->delta_poc_msb_present_flag[i])
                pocLt += s->poc - long_rps->DeltaPocMsbCycleLt[i] * MaxPicOrderCntLsb - s->sh.pic_order_cnt_lsb;
            if (long_rps->UsedByCurrPicLt[i]) {
                refPocList[LT_CURR].idx[j]  = ff_hevc_find_ref_idx(s, pocLt);
                refPocList[LT_CURR].list[j] = s->DPB[refPocList[LT_CURR].idx[j]]->poc;
                s->DPB[refPocList[LT_CURR].idx[j]]->flags |= HEVC_FRAME_FLAG_LONG_REF;
                j++;
            } else {
                refPocList[LT_FOLL].idx[k]  = ff_hevc_find_ref_idx(s, pocLt);
                refPocList[LT_FOLL].list[k] = s->DPB[refPocList[LT_FOLL].idx[k]]->poc;
                s->DPB[refPocList[LT_FOLL].idx[k]]->flags &= ~HEVC_FRAME_FLAG_LONG_REF;
                k++;
            }
        }
        refPocList[LT_CURR].numPic = j;
        refPocList[LT_FOLL].numPic = k;
    }
}

int ff_hevc_get_num_poc(HEVCContext *s)
{
    int ret = 0;
    int i;
    const ShortTermRPS *rps = s->sh.short_term_rps;
    LongTermRPS *long_rps   = &s->sh.long_term_rps;

    if (rps) {
        for (i = 0; i < rps->num_negative_pics; i++)
            ret += !!rps->used[i];
        for (; i < rps->num_delta_pocs; i++)
            ret += !!rps->used[i];
    }

    if (long_rps) {
        for (i = 0; i < long_rps->num_long_term_sps + long_rps->num_long_term_pics; i++)
            ret += !!long_rps->UsedByCurrPicLt[i];
    }
    return ret;
}

int ff_hevc_apply_window(HEVCContext *s, HEVCWindow *window)
{
    int original_width  = s->avctx->width;
    int original_height = s->avctx->height;

    s->avctx->width -= window->left_offset + window->right_offset;
    s->avctx->height -= window->top_offset + window->bottom_offset;

    if (s->avctx->width <= 0 || s->avctx->height <= 0) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid frame dimensions: %dx%d.\n",
               s->avctx->width, s->avctx->height);
        if (s->avctx->err_recognition & AV_EF_EXPLODE)
            return AVERROR_INVALIDDATA;

        av_log(s->avctx, AV_LOG_WARNING, "Ignoring window information.\n");
        window->left_offset   =
        window->top_offset    =
        window->right_offset  =
        window->bottom_offset = 0;
        s->avctx->width = original_width;
        s->avctx->height = original_height;
    }

    return 0;
}

void ff_hevc_wait_neighbour_ctb(HEVCContext *s, MvField *current_mv, int x0, int y0)
{
    if (s->threads_type == FF_THREAD_FRAME ) {
        int ctb_addr_rs;
        int dpb_idx;
        if (current_mv->pred_flag == 1 || current_mv->pred_flag == 3) {
            dpb_idx     = s->ref->refPicList[0].idx[current_mv->ref_idx[0]];
            if (s->DPB[dpb_idx]->frame->buf[0]) {
                int x_off   = ((x0 + (current_mv->mv[0].x >> 2)) >> s->sps->log2_ctb_size) + 2;
                int y_off   = ((y0 + (current_mv->mv[0].y >> 2)) >> s->sps->log2_ctb_size) + 2;
                ctb_addr_rs = FFMIN(y_off * s->sps->pic_width_in_ctbs + x_off, s->sps->pic_width_in_ctbs*s->sps->pic_height_in_ctbs-1);
//                ctb_addr_rs = s->sps->pic_width_in_ctbs*s->sps->pic_height_in_ctbs-1;
//                av_log(s->avctx, AV_LOG_INFO, "poc_cur %d (%dx%d) : L0 : poc %d: wait ctb %d : (%dx%d)\n",
//                        s->poc, x0, y0, s->DPB[dpb_idx]->poc, ctb_addr_rs, y_off , x_off);
                ff_thread_await_progress(&s->DPB[dpb_idx]->threadFrame, ctb_addr_rs, 0);
            }
        }
        if (current_mv->pred_flag == 2 || current_mv->pred_flag == 3) {
            dpb_idx     = s->ref->refPicList[1].idx[current_mv->ref_idx[1]];
            if (s->DPB[dpb_idx]->frame->buf[0]) {
                int x_off   = ((x0 + (current_mv->mv[1].x >> 2)) >> s->sps->log2_ctb_size) + 2;
                int y_off   = ((y0 + (current_mv->mv[1].y >> 2)) >> s->sps->log2_ctb_size) + 2;
                ctb_addr_rs = FFMIN(y_off * s->sps->pic_width_in_ctbs + x_off, s->sps->pic_width_in_ctbs*s->sps->pic_height_in_ctbs-1);
//                ctb_addr_rs = s->sps->pic_width_in_ctbs*s->sps->pic_height_in_ctbs-1;
//                av_log(s->avctx, AV_LOG_INFO, "poc_cur %d (%dx%d) : L1 : poc %d: wait ctb %d : (%dx%d)\n",
//                        s->poc, x0, y0, s->DPB[dpb_idx]->poc, ctb_addr_rs, y_off , x_off);
                ff_thread_await_progress(&s->DPB[dpb_idx]->threadFrame, ctb_addr_rs, 0);
            }
        }
    }
}
void ff_hevc_wait_collocated_ctb(HEVCContext *s, int x0, int y0)
{
    if (s->threads_type == FF_THREAD_FRAME ) {
        int dpb_idx;
        int x_ctb       = (x0 >> s->sps->log2_ctb_size)+2;
        int y_ctb       = (y0 >> s->sps->log2_ctb_size)+2;
        int ctb_addr_rs = FFMIN(y_ctb * s->sps->pic_width_in_ctbs + x_ctb, s->sps->pic_width_in_ctbs*s->sps->pic_height_in_ctbs-1);
        if (s->sh.slice_type == B_SLICE && s->sh.collocated_from_l0_flag == 0) {
            dpb_idx     = s->ref->refPicList[1].idx[s->sh.collocated_ref_idx];
            //        av_log(s->avctx, AV_LOG_INFO, "poc_cur %d (%dx%d): col : poc %d: wait ctb %d (%dx%d)\n",
            //                s->poc, x0, y0, s->DPB[dpb_idx]->poc, ctb_addr_rs, x_ctb, y_ctb);
            ff_thread_await_progress(&s->DPB[dpb_idx]->threadFrame, ctb_addr_rs, 0);
        } else if (s->sh.slice_type == P_SLICE ||
                (s->sh.slice_type == B_SLICE && s->sh.collocated_from_l0_flag == 1)) {
            dpb_idx     = s->ref->refPicList[0].idx[s->sh.collocated_ref_idx];
            //        av_log(s->avctx, AV_LOG_INFO, "poc_cur %d (%dx%d): col : poc %d: wait ctb %d (%dx%d)\n",
            //                s->poc, x0, y0, s->DPB[dpb_idx]->poc, ctb_addr_rs, x_ctb, y_ctb);
            ff_thread_await_progress(&s->DPB[dpb_idx]->threadFrame, ctb_addr_rs, 0);
        }
    }
}
void ff_hevc_dpb_free(HEVCContext *s)
{
    int i;
    LOCK_DBP;
    for (i = 0; (!s->avctx->internal->is_copy) && i < FF_ARRAY_ELEMS(s->DPB); i++) {
        if(s->DPB[i] && s->DPB[i]->threadCnt == 0) {
            av_freep(&s->DPB[i]->tab_mvf);
            free_refPicListTab(s, s->DPB[i]);
            av_freep(&s->DPB[i]->refPicListTab);
        }
    }
    UNLOCK_DBP;
}

int ff_hevc_dpb_malloc(HEVCContext *s, int pic_size_in_min_pu, int ctb_count)
{
    int i;
    LOCK_DBP;
    for (i = 0; (!s->avctx->internal->is_copy) && i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCFrame *f = s->DPB[i];
        if(f->threadCnt == 0) {
            f->tab_mvf = av_malloc_array(pic_size_in_min_pu, sizeof(*f->tab_mvf));
            if (!f->tab_mvf) {
                UNLOCK_DBP;
                return -1;
            }

            f->refPicListTab = av_mallocz_array(ctb_count, sizeof(*f->refPicListTab));
            if (!f->refPicListTab) {
                UNLOCK_DBP;
                return -1;
            }
            f->ctb_count = ctb_count;
        }
    }
    UNLOCK_DBP;
    return 0;
}
