/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 Anand Meher Kotra
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

static const uint8_t l0_l1_cand_idx[12][2] = {
    { 0, 1, },
    { 1, 0, },
    { 0, 2, },
    { 2, 0, },
    { 1, 2, },
    { 2, 1, },
    { 0, 3, },
    { 3, 0, },
    { 1, 3, },
    { 3, 1, },
    { 2, 3, },
    { 3, 2, },
};

void ff_hevc_set_neighbour_available(HEVCContext *s, int x0, int y0, int nPbW, int nPbH)
{
    HEVCLocalContext *lc = s->HEVClc;
    int x0b = x0 & ((1 << s->sps->log2_ctb_size) - 1);
    int y0b = y0 & ((1 << s->sps->log2_ctb_size) - 1);

    lc->na.cand_up       = (lc->ctb_up_flag   || y0b);
    lc->na.cand_left     = (lc->ctb_left_flag || x0b);
    lc->na.cand_up_left  = (!x0b && !y0b) ? lc->ctb_up_left_flag : lc->na.cand_left && lc->na.cand_up;
    lc->na.cand_up_right_sap =
            ((x0b + nPbW) == (1 << s->sps->log2_ctb_size)) ?
                    lc->ctb_up_right_flag && !y0b : lc->na.cand_up;
    lc->na.cand_up_right =
            ((x0b + nPbW) == (1 << s->sps->log2_ctb_size) ?
                    lc->ctb_up_right_flag && !y0b : lc->na.cand_up )
                     && (x0 + nPbW) < lc->end_of_tiles_x;
    lc->na.cand_bottom_left = ((y0 + nPbH) >= lc->end_of_tiles_y) ? 0 : lc->na.cand_left;
}

/*
 * 6.4.1 Derivation process for z-scan order block availability
 */
static int z_scan_block_avail(HEVCContext *s, int xCurr, int yCurr,
                              int xN, int yN)
{
#define MIN_TB_ADDR_ZS(x, y)                                            \
    s->pps->min_tb_addr_zs[(y) * s->sps->pic_width_in_min_tbs + (x)]
    int Curr =  MIN_TB_ADDR_ZS(xCurr >> s->sps->log2_min_transform_block_size,
                               yCurr >> s->sps->log2_min_transform_block_size);
    int N;

    if ((xN < 0) || (yN < 0) ||
        (xN >= s->sps->pic_width_in_luma_samples) ||
        (yN >= s->sps->pic_height_in_luma_samples))
        return 0;

    N = MIN_TB_ADDR_ZS(xN >> s->sps->log2_min_transform_block_size,
                       yN >> s->sps->log2_min_transform_block_size);

    return N <= Curr;
}

/*
 * 6.4.2 Derivation process for prediction block availability
 */
static int check_prediction_block_available(HEVCContext *s, int log2_cb_size,
                                            int x0, int y0, int nPbW, int nPbH,
                                            int xA1, int yA1, int partIdx)
{
    HEVCLocalContext *lc = s->HEVClc;
    if (lc->cu.x < xA1 &&
        lc->cu.y < yA1 &&
        (lc->cu.x + (1 << log2_cb_size)) > xA1 &&
        (lc->cu.y + (1 << log2_cb_size)) > yA1)
        return !(partIdx == 1 &&
                 nPbW << 1 == 1 << log2_cb_size &&
                 nPbH << 1 == 1 << log2_cb_size &&
                 (lc->cu.x + nPbW) > xA1 &&
                 (lc->cu.y + nPbH) <= yA1);
    else
        return z_scan_block_avail(s, x0, y0, xA1, yA1);
}

//check if the two luma locations belong to the same mostion estimation region
static int isDiffMER(HEVCContext *s, int xN, int yN, int xP, int yP)
{
    uint8_t plevel = s->pps->log2_parallel_merge_level;

    return xN >> plevel == xP >> plevel &&
           yN >> plevel == yP >> plevel;
}

#define MATCH(x) (A.x == B.x)

// check if the mv's and refidx are the same between A and B
static int compareMVrefidx(MvField A, MvField B)
{
    if (A.pred_flag == B.pred_flag) {
        if (A.pred_flag == 3)
        return MATCH(ref_idx[0]) && MATCH(mv[0].x) && MATCH(mv[0].y) &&
               MATCH(ref_idx[1]) && MATCH(mv[1].x) && MATCH(mv[1].y);
        else if (A.pred_flag == 1)
         return MATCH(ref_idx[0]) && MATCH(mv[0].x) && MATCH(mv[0].y);
        else if (A.pred_flag == 2)
        return MATCH(ref_idx[1]) && MATCH(mv[1].x) && MATCH(mv[1].y);
    }
    return 0;
}

static av_always_inline void mv_scale(Mv *dst, Mv *src, int td, int tb)
{
    int tx, scale_factor;

    td = av_clip_int8_c(td);
    tb = av_clip_int8_c(tb);
    tx = (0x4000 + abs(td / 2)) / td;
    scale_factor = av_clip_c((tb * tx + 32) >> 6, -4096, 4095);
    dst->x = av_clip_int16_c((scale_factor * src->x + 127 +
                             (scale_factor * src->x < 0)) >> 8);
    dst->y = av_clip_int16_c((scale_factor * src->y + 127 +
                             (scale_factor * src->y < 0)) >> 8);
}

static int check_mvset(Mv *mvLXCol, Mv *mvCol,
                       int colPic, int poc,
                       RefPicList *refPicList, int X, int refIdxLx,
                       RefPicList *refPicList_col, int listCol, int refidxCol)
{
    // Assuming no long term pictures in version 1 of the decoder
    int currIsLongTerm = refPicList[X].is_long_term[refIdxLx];
    int colIsLongTerm  = refPicList_col[listCol].is_long_term[refidxCol];
    int colPocDiff, curPocDiff;

    if (currIsLongTerm != colIsLongTerm) {
        mvLXCol->x = 0;
        mvLXCol->y = 0;
        return 0;
    }

    colPocDiff = colPic - refPicList_col[listCol].list[refidxCol];

    if (!(curPocDiff = poc - refPicList[X].list[refIdxLx]))
        colPocDiff = 1; // error resilience

    if (currIsLongTerm || colPocDiff == curPocDiff) {
        mvLXCol->x = mvCol->x;
        mvLXCol->y = mvCol->y;
    } else {
        mv_scale(mvLXCol, mvCol, colPocDiff, curPocDiff);
    }
    return 1;
}

#define CHECK_MVSET(l) \
    check_mvset(mvLXCol, temp_col.mv + l, \
                colPic, s->poc, \
                refPicList, X, refIdxLx, \
                refPicList_col, L##l, temp_col.ref_idx[l])

// derive the motion vectors section 8.5.3.1.8
static int derive_temporal_colocated_mvs(HEVCContext *s, MvField temp_col,
                                         int refIdxLx, Mv* mvLXCol, int X,
                                         int colPic, RefPicList* refPicList_col)
{
    RefPicList *refPicList = s->ref->refPicList;

    if (temp_col.is_intra || !temp_col.pred_flag) {
        mvLXCol->x = 0;
        mvLXCol->y = 0;
        return 0;
    }
    
    if ((temp_col.pred_flag & 1) == 0)
        return CHECK_MVSET(1);
    else if (temp_col.pred_flag == 1)
         return CHECK_MVSET(0);
    else if (temp_col.pred_flag == 3) {
        int check_diffpicount = 0;
        int i = 0;
        for (i = 0; i < refPicList[0].numPic; i++) {
            if (refPicList[0].list[i] > s->poc)
                check_diffpicount++;
        }
        for (i = 0; i < refPicList[1].numPic; i++) {
            if (refPicList[1].list[i] > s->poc)
                check_diffpicount++;
        }
        if (check_diffpicount == 0 && X == 0)
            return CHECK_MVSET(0);
        else if (check_diffpicount == 0 && X == 1)
            return CHECK_MVSET(1);
        else {
            if (s->sh.collocated_from_l0_flag == 0)
                return CHECK_MVSET(0);
            else
                return CHECK_MVSET(1);
        }
    }

    return 0;
}

#define TAB_MVF(x, y) \
    tab_mvf[(y) * pic_width_in_min_pu + x]

#define TAB_MVF_PU(v) \
    TAB_MVF(x##v##_pu, y##v##_pu)

#define DERIVE_TEMPORAL_COLOCATED_MVS(v) \
    derive_temporal_colocated_mvs(s, temp_col, \
                                  refIdxLx, mvLXCol, X, colPic, \
                                  ff_hevc_get_ref_list(s, short_ref_idx, \
                                                       x##v, y##v));

/*
 * 8.5.3.1.7  temporal luma motion vector prediction
 */
static int temporal_luma_motion_vector(HEVCContext *s, int x0, int y0,
                                       int nPbW, int nPbH, int refIdxLx,
                                       Mv* mvLXCol, int X)
{
    MvField *tab_mvf = NULL;
    MvField temp_col;
    RefPicList *refPicList = s->ref->refPicList;
    int xPRb, yPRb;
    int xPRb_pu;
    int yPRb_pu;
    int xPCtr, yPCtr;
    int xPCtr_pu;
    int yPCtr_pu;
    int pic_width_in_min_pu = s->sps->pic_width_in_min_pus;
    int log2_min_pu_size = s->sps->log2_min_pu_size;

    int short_ref_idx = 0;
    int availableFlagLXCol = 0;
    int colPic = 0;

    if (s->sh.slice_type == B_SLICE && s->sh.collocated_from_l0_flag == 0) {
        short_ref_idx = refPicList[1].idx[s->sh.collocated_ref_idx];
        tab_mvf = s->DPB[short_ref_idx]->tab_mvf;
        colPic  = s->DPB[short_ref_idx]->poc;
    } else if (s->sh.slice_type == P_SLICE ||
               (s->sh.slice_type == B_SLICE &&
                s->sh.collocated_from_l0_flag == 1)) {
        short_ref_idx = refPicList[0].idx[s->sh.collocated_ref_idx];
        tab_mvf = s->DPB[short_ref_idx]->tab_mvf;
        colPic  = s->DPB[short_ref_idx]->poc;
    }
    //bottom right collocated motion vector
    xPRb = x0 + nPbW;
    yPRb = y0 + nPbH;
    if (tab_mvf &&
        y0 >> s->sps->log2_ctb_size == yPRb >> s->sps->log2_ctb_size &&
        yPRb < s->sps->pic_height_in_luma_samples &&
        xPRb < s->sps->pic_width_in_luma_samples) {
        xPRb = ((xPRb >> 4) << 4);
        yPRb = ((yPRb >> 4) << 4);
        xPRb_pu = xPRb >> log2_min_pu_size;
        yPRb_pu = yPRb >> log2_min_pu_size;
        temp_col = TAB_MVF_PU(PRb);
        availableFlagLXCol = DERIVE_TEMPORAL_COLOCATED_MVS(PRb);
    } else {
        mvLXCol->x = 0;
        mvLXCol->y = 0;
        availableFlagLXCol = 0;
    }

    // derive center collocated motion vector
    if (tab_mvf && availableFlagLXCol == 0) {
        xPCtr = x0 + (nPbW >> 1);
        yPCtr = y0 + (nPbH >> 1);
        xPCtr = ((xPCtr >> 4) << 4);
        yPCtr = ((yPCtr >> 4) << 4);
        xPCtr_pu = xPCtr >> log2_min_pu_size;
        yPCtr_pu = yPCtr >> log2_min_pu_size;
        temp_col = TAB_MVF_PU(PCtr);
        availableFlagLXCol = DERIVE_TEMPORAL_COLOCATED_MVS(PCtr);
    }
    return availableFlagLXCol;
}

#define AVAILABLE(cand, v) \
    cand && !TAB_MVF_PU(v).is_intra

/*
 * 8.5.3.1.2  Derivation process for spatial merging candidates
 */
static void derive_spatial_merge_candidates(HEVCContext *s, int x0, int y0,
                                            int nPbW, int nPbH, int log2_cb_size,
                                            int singleMCLFlag, int part_idx,
                                            MvField mergecandlist[])
{
    HEVCLocalContext *lc = s->HEVClc;
    RefPicList *refPicList = s->ref->refPicList;
    MvField *tab_mvf = s->ref->tab_mvf;

    Mv mvL0Col = { 0 };
    Mv mvL1Col = { 0 };

    //first left spatial merge candidate
    int xA1 = x0 - 1;
    int yA1 = y0 + nPbH - 1;
    int is_available_a1;
    int pic_width_in_min_pu = s->sps->pic_width_in_min_pus;
    int log2_min_pu_size = s->sps->log2_min_pu_size;

    int check_MER = 1;
    int check_MER_1 = 1;

    int xB1, yB1;
    int is_available_b1;
    int xb1_pu;
    int yb1_pu;

    int xB0, yB0;
    int is_available_b0;
    int xb0_pu;
    int yb0_pu;

    int check_A0;
    int xA0, yA0;
    int is_available_a0;
    int xa0_pu;
    int ya0_pu;

    int xB2, yB2;
    int isAvailableB2;
    int xb2_pu;
    int yb2_pu;
    int mergearray_index = 0;

    int numRefIdx = 0;
    int zeroIdx = 0;

    int numMergeCand = 0;
    int numOrigMergeCand = 0;
    int sumcandidates = 0;
    int combIdx = 0;
    int combStop = 0;
    int l0CandIdx = 0;
    int l1CandIdx = 0;

    int refIdxL0Col = 0;
    int refIdxL1Col = 0;
    int availableFlagLXCol = 0;

    int cand_bottom_left = lc->na.cand_bottom_left;
    int cand_left        = lc->na.cand_left;
    int cand_up_left     = lc->na.cand_up_left;
    int cand_up          = lc->na.cand_up;
    int cand_up_right    = lc->na.cand_up_right_sap;

    int xa1_pu = xA1 >> log2_min_pu_size;
    int ya1_pu = yA1 >> log2_min_pu_size;

    int availableFlagL0Col = 0;
    int availableFlagL1Col = 0;

    is_available_a1 = AVAILABLE(cand_left, a1);

    if (singleMCLFlag == 0 && part_idx == 1 &&
        (lc->cu.part_mode == PART_Nx2N ||
         lc->cu.part_mode == PART_nLx2N ||
         lc->cu.part_mode == PART_nRx2N) ||
        isDiffMER(s, xA1, yA1, x0, y0)) {
        is_available_a1 = 0;
    }

    if (is_available_a1) {
        mergecandlist[mergearray_index] = tab_mvf[(ya1_pu) * pic_width_in_min_pu + xa1_pu];
        mergearray_index++;
    }

    // above spatial merge candidate

    xB1 = x0 + nPbW - 1;
    yB1 = y0 - 1;
    xb1_pu = xB1 >> log2_min_pu_size;
    yb1_pu = yB1 >> log2_min_pu_size;

    is_available_b1 = AVAILABLE(cand_up, b1);

    if (singleMCLFlag == 0 && part_idx == 1 &&
        (lc->cu.part_mode == PART_2NxN ||
         lc->cu.part_mode == PART_2NxnU ||
         lc->cu.part_mode == PART_2NxnD) ||
        isDiffMER(s, xB1, yB1, x0, y0)) {
        is_available_b1 = 0;
    }
    if (is_available_a1 && is_available_b1) {
        check_MER = !(compareMVrefidx(
                tab_mvf[(yb1_pu) * pic_width_in_min_pu + xb1_pu],
                tab_mvf[(ya1_pu) * pic_width_in_min_pu + xa1_pu]));
    }

    if (is_available_b1 && check_MER) {
        mergecandlist[mergearray_index] = tab_mvf[(yb1_pu) * pic_width_in_min_pu + xb1_pu];
        mergearray_index++;
    }

    // above right spatial merge candidate
    xB0 = x0 + nPbW;
    yB0 = y0 - 1;
    check_MER = 1;
    xb0_pu = xB0 >> log2_min_pu_size;
    yb0_pu = yB0 >> log2_min_pu_size;
    
    is_available_b0 = AVAILABLE(cand_up_right, b0) &&
                      check_prediction_block_available(s, log2_cb_size, x0, y0, nPbW,
                                                       nPbH, xB0, yB0, part_idx);
 
    if (isDiffMER(s, xB0, yB0, x0, y0))
        is_available_b0 = 0;

    if (is_available_b1 && is_available_b0) {
        check_MER = !(compareMVrefidx(
                tab_mvf[(yb0_pu) * pic_width_in_min_pu + xb0_pu],
                tab_mvf[(yb1_pu) * pic_width_in_min_pu + xb1_pu]));
    }

    if (is_available_b0 && check_MER) {
        mergecandlist[mergearray_index] = tab_mvf[(yb0_pu) * pic_width_in_min_pu + xb0_pu];
        mergearray_index++;
    }

    // left bottom spatial merge candidate
    xA0 = x0 - 1;
    yA0 = y0 + nPbH;
    check_MER = 1;
    xa0_pu = xA0 >> log2_min_pu_size;
    ya0_pu = yA0 >> log2_min_pu_size;
    check_A0 = 

    is_available_a0 = AVAILABLE(cand_bottom_left, a0) &&
                      check_prediction_block_available(s, log2_cb_size, x0, y0, nPbW,
                                                       nPbH, xA0, yA0, part_idx);

    if (isDiffMER(s, xA0, yA0, x0, y0))
        is_available_a0 = 0;

    if (is_available_a1 && is_available_a0) {
        check_MER = !(compareMVrefidx(
                tab_mvf[(ya0_pu) * pic_width_in_min_pu + xa0_pu],
                tab_mvf[(ya1_pu) * pic_width_in_min_pu + xa1_pu]));
    }

    if (is_available_a0 && check_MER) {
        mergecandlist[mergearray_index] = tab_mvf[(ya0_pu) * pic_width_in_min_pu + xa0_pu];
        mergearray_index++;
    }

    // above left spatial merge candidate
    xB2 = x0 - 1;
    yB2 = y0 - 1;
    check_MER = 1;
    xb2_pu = xB2 >> log2_min_pu_size;
    yb2_pu = yB2 >> log2_min_pu_size;
    isAvailableB2 = AVAILABLE(cand_up_left,b2);

    if (isDiffMER(s, xB2, yB2, x0, y0))
        isAvailableB2 = 0;

    if (is_available_a1 && isAvailableB2) {
        check_MER = !(compareMVrefidx(
                tab_mvf[(yb2_pu) * pic_width_in_min_pu + xb2_pu],
                tab_mvf[(ya1_pu) * pic_width_in_min_pu + xa1_pu]));
    }
    if (is_available_b1 && isAvailableB2) {
        check_MER_1 = !(compareMVrefidx(
                tab_mvf[(yb2_pu) * pic_width_in_min_pu + xb2_pu],
                tab_mvf[(yb1_pu) * pic_width_in_min_pu + xb1_pu]));
    }

    sumcandidates = mergearray_index;

    if (isAvailableB2 && check_MER && check_MER_1 && sumcandidates != 4) {
        mergecandlist[mergearray_index] = tab_mvf[(yb2_pu) * pic_width_in_min_pu + xb2_pu];
        mergearray_index++;
    }

    // temporal motion vector candidate
    // one optimization is that do temporal checking only if the number of
    // available candidates < MRG_MAX_NUM_CANDS
    if (s->sh.slice_temporal_mvp_enabled_flag == 0) {
        availableFlagLXCol = 0;
    } else {
        availableFlagL0Col = temporal_luma_motion_vector(s, x0, y0, nPbW, nPbH,
                refIdxL0Col, &mvL0Col, 0);
        // one optimization is that l1 check can be done only when the current slice type is B_SLICE
        if (s->sh.slice_type == B_SLICE) {
            availableFlagL1Col = temporal_luma_motion_vector(s, x0, y0, nPbW,
                    nPbH, refIdxL1Col, &mvL1Col, 1);
        }
        availableFlagLXCol = availableFlagL0Col || availableFlagL1Col;
        if (availableFlagLXCol &&
            mergearray_index < s->sh.max_num_merge_cand) {
            MvField TMVPCand = { { { 0 } } };
            TMVPCand.is_intra = 0;
            TMVPCand.pred_flag = availableFlagL0Col + 2 * availableFlagL1Col;
            if (availableFlagL0Col) {
                TMVPCand.mv[0] = mvL0Col;
                TMVPCand.ref_idx[0] = refIdxL0Col;
            }
            if (availableFlagL1Col) {
                TMVPCand.mv[1] = mvL1Col;
                TMVPCand.ref_idx[1] = refIdxL1Col;
            }
            mergecandlist[mergearray_index] = TMVPCand;
            mergearray_index++;
        }
    }
    numMergeCand = mergearray_index;
    numOrigMergeCand = mergearray_index;

    // combined bi-predictive merge candidates  (applies for B slices)
    if (s->sh.slice_type == B_SLICE) {
        if (numOrigMergeCand > 1 &&
            numOrigMergeCand < s->sh.max_num_merge_cand) {

            combIdx = 0;
            combStop = 0;
            while (combStop != 1) {
                MvField l0Cand;
                MvField l1Cand;
                l0CandIdx = l0_l1_cand_idx[combIdx][0];
                l1CandIdx = l0_l1_cand_idx[combIdx][1];
                l0Cand = mergecandlist[l0CandIdx];
                l1Cand = mergecandlist[l1CandIdx];
                if ((l0Cand.pred_flag & 1) && (l1Cand.pred_flag & 2) &&
                     (refPicList[0].list[l0Cand.ref_idx[0]] !=
                      refPicList[1].list[l1Cand.ref_idx[1]] ||
                     l0Cand.mv[0].x != l1Cand.mv[1].x ||
                     l0Cand.mv[0].y != l1Cand.mv[1].y)) {
                    MvField combCand;

                    combCand.ref_idx[0] = l0Cand.ref_idx[0];
                    combCand.ref_idx[1] = l1Cand.ref_idx[1];
                    combCand.pred_flag = 3;
                    combCand.mv[0].x = l0Cand.mv[0].x;
                    combCand.mv[0].y = l0Cand.mv[0].y;
                    combCand.mv[1].x = l1Cand.mv[1].x;
                    combCand.mv[1].y = l1Cand.mv[1].y;
                    combCand.is_intra = 0;
                    mergecandlist[numMergeCand] = combCand;
                    numMergeCand++;
                }
                combIdx++;
                if (combIdx == numOrigMergeCand * (numOrigMergeCand - 1) ||
                    numMergeCand == s->sh.max_num_merge_cand)
                    combStop = 1;
            }
        }
    }

    /*
     * append Zero motion vector candidates
     */
    if (s->sh.slice_type == P_SLICE)
        numRefIdx = s->sh.num_ref_idx_l0_active;
    else // B_SLICE
        numRefIdx = FFMIN(s->sh.num_ref_idx_l0_active,
                          s->sh.num_ref_idx_l1_active);

    while (numMergeCand < s->sh.max_num_merge_cand) {
        MvField *zerovector;
        zerovector = &mergecandlist[numMergeCand];
        if (s->sh.slice_type == P_SLICE) {
            zerovector->ref_idx[0] = (zeroIdx < numRefIdx) ? zeroIdx : 0;
            zerovector->ref_idx[1] = -1;
            zerovector->pred_flag = 1;
            zerovector->mv[0].x = 0;
            zerovector->mv[0].y = 0;
            zerovector->mv[1].x = 0;
            zerovector->mv[1].y = 0;
            zerovector->is_intra = 0;
        } else { // B_SLICE
            zerovector->ref_idx[0] = (zeroIdx < numRefIdx) ? zeroIdx : 0;
            zerovector->ref_idx[1] = (zeroIdx < numRefIdx) ? zeroIdx : 0;
            zerovector->pred_flag = 3;
            zerovector->mv[0].x = 0;
            zerovector->mv[0].y = 0;
            zerovector->mv[1].x = 0;
            zerovector->mv[1].y = 0;
            zerovector->is_intra = 0;
        }
        numMergeCand++;
        zeroIdx++;
    }
}

/*
 * 8.5.3.1.1 Derivation process of luma Mvs for merge mode
 */
void ff_hevc_luma_mv_merge_mode(HEVCContext *s, int x0, int y0, int nPbW,
                                int nPbH, int log2_cb_size, int part_idx,
                                int merge_idx, MvField *mv)
{
    HEVCLocalContext *lc = s->HEVClc;
    int singleMCLFlag = 0;
    int nCS = 1 << log2_cb_size;
    MvField mergecand_list[MRG_MAX_NUM_CANDS] = { { { { 0 } } } };
    int nPbW2 = nPbW;
    int nPbH2 = nPbH;

    if (s->pps->log2_parallel_merge_level > 2 && nCS == 8) {
        singleMCLFlag = 1;
        x0 = lc->cu.x;
        y0 = lc->cu.y;
        nPbW = nCS;
        nPbH = nCS;
        part_idx = 0;
    }

    ff_hevc_set_neighbour_available(s, x0, y0, nPbW, nPbH);
    derive_spatial_merge_candidates(s, x0, y0, nPbW, nPbH, log2_cb_size,
                                    singleMCLFlag, part_idx, mergecand_list);

    if ((mergecand_list[merge_idx].pred_flag == 3) && 
        (nPbW2 + nPbH2) == 12) {
        mergecand_list[merge_idx].ref_idx[1] = -1;
        mergecand_list[merge_idx].pred_flag &= 1;
    }

    *mv = mergecand_list[merge_idx];
}

#define TAB_MVF(x, y)                                                              \
    tab_mvf[(y) * pic_width_in_min_pu + x]

static av_always_inline void dist_scale(HEVCContext *s, Mv * mv,
                                        int pic_width_in_min_pu, int x, int y,
                                        int elist, int ref_idx_curr, int ref_idx)
{
    RefPicList *refPicList = s->ref->refPicList;
    MvField *tab_mvf = s->ref->tab_mvf;
    int ref_pic_elist = refPicList[elist].list[TAB_MVF(x, y).ref_idx[elist]];
    int ref_pic_curr  = refPicList[ref_idx_curr].list[ref_idx];

    if (ref_pic_elist != ref_pic_curr)
        mv_scale(mv, mv, s->poc - ref_pic_elist, s->poc - ref_pic_curr);
}

static int mv_mp_mode_mx(HEVCContext *s, int x, int y, int pred_flag_index,
                         Mv *mv, int ref_idx_curr, int ref_idx)
{
    MvField *tab_mvf = s->ref->tab_mvf;
    int pic_width_in_min_pu = s->sps->pic_width_in_min_pus;

    RefPicList *refPicList = s->ref->refPicList;

    if ((TAB_MVF(x, y).pred_flag & (1 << pred_flag_index)) &&
        refPicList[pred_flag_index].list[TAB_MVF(x, y).ref_idx[pred_flag_index]] == refPicList[ref_idx_curr].list[ref_idx]) {
        *mv = TAB_MVF(x, y).mv[pred_flag_index];
        return 1;
    }
    return 0;
}


static int mv_mp_mode_mx_lt(HEVCContext *s, int x, int y, int pred_flag_index,
                            Mv *mv, int ref_idx_curr, int ref_idx)
{
    MvField *tab_mvf = s->ref->tab_mvf;
    int pic_width_in_min_pu = s->sps->pic_width_in_min_pus;

    RefPicList *refPicList = s->ref->refPicList;
    int currIsLongTerm = refPicList[ref_idx_curr].is_long_term[ref_idx];

    int colIsLongTerm =
        refPicList[pred_flag_index].is_long_term[(TAB_MVF(x, y).ref_idx[pred_flag_index])];

    if ((TAB_MVF(x, y).pred_flag & (1 << pred_flag_index)) && colIsLongTerm == currIsLongTerm) {
        *mv = TAB_MVF(x, y).mv[pred_flag_index];
        if (!currIsLongTerm)
            dist_scale(s, mv, pic_width_in_min_pu, x, y, pred_flag_index, ref_idx_curr, ref_idx);
        return 1;
    }
    return 0;
}

#define MP_MX(v, pred, mx) \
    mv_mp_mode_mx(s, x##v##_pu, y##v##_pu, pred, mx, ref_idx_curr, ref_idx)

#define MP_MX_LT(v, pred, mx) \
    mv_mp_mode_mx_lt(s, x##v##_pu, y##v##_pu, pred, mx, ref_idx_curr, ref_idx)


static int luma_mxa_mvp_mode(HEVCContext *s, int x0, int y0, int nPbW, int nPbH, int log2_cb_size,
                             int part_idx, int cand_left, int pred_flag_index_l0, int pred_flag_index_l1,
                             int cand_bottom_left, Mv *mv, int ref_idx_curr, int ref_idx,
                             int *is_scaled_flag_l0) {
    MvField *tab_mvf = s->ref->tab_mvf;
    int pic_width_in_min_pu = s->sps->pic_width_in_min_pus;
    int log2_min_pu_size = s->sps->log2_min_pu_size;

    // left bottom spatial candidate
    int xa0_pu = (x0 - 1) >> log2_min_pu_size;
    int ya0_pu = (y0 + nPbH) >> log2_min_pu_size;
    int is_available_a0;

    //left spatial merge candidate
    int xa1_pu;
    int ya1_pu;
    int is_available_a1;
    is_available_a0 = AVAILABLE(cand_bottom_left, a0);
    if (is_available_a0)
        is_available_a0 = check_prediction_block_available(s, log2_cb_size, x0, y0, nPbW,
                                                           nPbH, x0 - 1, y0 + nPbH, part_idx);
    if (is_available_a0) {
        *is_scaled_flag_l0 = 1;
        if (mv_mp_mode_mx(s, xa0_pu, ya0_pu, pred_flag_index_l0, mv, ref_idx_curr, ref_idx))
            return 1;
        if (mv_mp_mode_mx(s, xa0_pu, ya0_pu, pred_flag_index_l1, mv, ref_idx_curr, ref_idx))
            return 1;
    }
    xa1_pu = (x0 - 1) >> log2_min_pu_size;
    ya1_pu = (y0 + nPbH - 1) >> log2_min_pu_size;
    is_available_a1 = AVAILABLE(cand_left, a1);

    if (is_available_a1) {
        *is_scaled_flag_l0 = 1;
        if (MP_MX(a1, pred_flag_index_l0, mv))
            return 1;
        if (MP_MX(a1, pred_flag_index_l1, mv))
            return 1;
    }

    if (is_available_a0) {
        if (MP_MX_LT(a0, pred_flag_index_l0, mv))
            return 1;
        if (MP_MX_LT(a0, pred_flag_index_l1, mv))
            return 1;
    }

    if (is_available_a1) {
        if (MP_MX_LT(a1, pred_flag_index_l0, mv))
            return 1;
        if (MP_MX_LT(a1, pred_flag_index_l1, mv))
            return 1;
    }
    return 0;

}


void ff_hevc_luma_mv_mvp_mode(HEVCContext *s, int x0, int y0, int nPbW,
        int nPbH, int log2_cb_size, int part_idx, int merge_idx, MvField *mv,
        int mvp_lx_flag, int LX)
{
    
    HEVCLocalContext *lc = s->HEVClc;
    MvField *tab_mvf = s->ref->tab_mvf;
    int is_scaled_flag_l0 = 0;
    int available_flag_lx_a0 = 0;
    int available_flag_lx_b0 = 0;
    int available_flag_lx_col = 0;
    int num_mvp_cand_lx = 0;
    int pic_width_in_min_pu = s->sps->pic_width_in_min_pus;
    int log2_min_pu_size = s->sps->log2_min_pu_size;


    // B candidates
    // above right spatial merge candidate
    int xb0_pu = (x0 + nPbW) >> log2_min_pu_size;
    int yb0_pu = (y0 - 1) >> log2_min_pu_size;
    int isAvailable_b0;

    // above spatial merge candidate
    int xb1_pu = (x0 + nPbW - 1) >> log2_min_pu_size;
    int yb1_pu = (y0 - 1) >> log2_min_pu_size;
    int is_available_b1 = 0;

    // above left spatial merge candidate
    int xb2_pu = (x0 - 1) >> log2_min_pu_size;
    int yb2_pu = (y0 - 1) >> log2_min_pu_size;
    int is_available_b2 = 0;
    Mv mvpcand_list[2];
    Mv mxA = { 0 };
    Mv mxB = { 0 };
    Mv mvLXCol = { 0 };
    int ref_idx_curr = 0;
    int ref_idx = 0;
    int pred_flag_index_l0;
    int pred_flag_index_l1;

    int cand_bottom_left = lc->na.cand_bottom_left;
    int cand_left        = lc->na.cand_left;
    int cand_up_left     = lc->na.cand_up_left;
    int cand_up          = lc->na.cand_up;
    int cand_up_right    = lc->na.cand_up_right;

    ref_idx_curr       = LX; //l0
    ref_idx            = mv->ref_idx[LX];
    pred_flag_index_l0 = LX;
    pred_flag_index_l1 = !LX;

    available_flag_lx_a0 = luma_mxa_mvp_mode(s, x0, y0, nPbW, nPbH, log2_cb_size,
                      part_idx, cand_left, pred_flag_index_l0, pred_flag_index_l1,
                      cand_bottom_left, &mxA, ref_idx_curr, ref_idx,
                      &is_scaled_flag_l0);

    isAvailable_b0 = AVAILABLE(cand_up_right, b0);

    if (isAvailable_b0)
        isAvailable_b0 = check_prediction_block_available(s, log2_cb_size, x0, y0, nPbW,
                                                         nPbH, x0 + nPbW, y0 - 1, part_idx);

    if (isAvailable_b0) {
        available_flag_lx_b0 = MP_MX(b0, pred_flag_index_l0, &mxB);
        if (!available_flag_lx_b0)
            available_flag_lx_b0 = MP_MX(b0, pred_flag_index_l1, &mxB);
    }

    if (!available_flag_lx_b0) {
        is_available_b1 = AVAILABLE(cand_up, b1);

        if (is_available_b1) {
            available_flag_lx_b0 = MP_MX(b1, pred_flag_index_l0, &mxB);
            if (!available_flag_lx_b0)
                available_flag_lx_b0 = MP_MX(b1, pred_flag_index_l1, &mxB);
        }
    }

    if (!available_flag_lx_b0) {
        is_available_b2 = AVAILABLE(cand_up_left, b2);

        if (is_available_b2) {
            available_flag_lx_b0 = MP_MX(b2, pred_flag_index_l0, &mxB);
            if (!available_flag_lx_b0)
                available_flag_lx_b0 = MP_MX(b2, pred_flag_index_l1, &mxB);
        }
    }
    if (is_scaled_flag_l0 == 0) {
        if (available_flag_lx_b0) {
            available_flag_lx_a0 = 1;
            mxA = mxB;
        }
        available_flag_lx_b0 = 0;

        // XB0 and L1
        if (isAvailable_b0) {
            available_flag_lx_b0 = MP_MX_LT(b0, pred_flag_index_l0, &mxB);
            if (!available_flag_lx_b0)
                available_flag_lx_b0 = MP_MX_LT(b0, pred_flag_index_l1, &mxB);
        }

        if (is_available_b1 && !available_flag_lx_b0) {
            available_flag_lx_b0 = MP_MX_LT(b1, pred_flag_index_l0, &mxB);
            if (is_available_b1 && !available_flag_lx_b0)
                available_flag_lx_b0 = MP_MX_LT(b1, pred_flag_index_l1, &mxB);
        }

        if (is_available_b2 && !available_flag_lx_b0) {
            available_flag_lx_b0 = MP_MX_LT(b2, pred_flag_index_l0, &mxB);
            if (is_available_b2 && !available_flag_lx_b0)
                available_flag_lx_b0 = MP_MX_LT(b2, pred_flag_index_l1, &mxB);
        }
    }

    if (available_flag_lx_a0 && available_flag_lx_b0 &&
        (mxA.x != mxB.x || mxA.y != mxB.y)) {
        available_flag_lx_col = 0;
    } else {
        //temporal motion vector prediction candidate
        if (s->sh.slice_temporal_mvp_enabled_flag == 0) {
            available_flag_lx_col = 0;
        } else {
            available_flag_lx_col = temporal_luma_motion_vector(s, x0, y0, nPbW,
                    nPbH, ref_idx, &mvLXCol, LX);
        }
    }

    if (available_flag_lx_a0) {
        mvpcand_list[num_mvp_cand_lx] = mxA;
        num_mvp_cand_lx++;
    }
    if (available_flag_lx_b0) {
        mvpcand_list[num_mvp_cand_lx] = mxB;
        num_mvp_cand_lx++;
    }

    if (available_flag_lx_a0 && available_flag_lx_b0 &&
        mxA.x == mxB.x && mxA.y == mxB.y) {
        num_mvp_cand_lx--;
    }

    if (available_flag_lx_col && num_mvp_cand_lx < 2) {
        mvpcand_list[num_mvp_cand_lx] = mvLXCol;
        num_mvp_cand_lx++;
    }

    while (num_mvp_cand_lx < 2) { // insert zero motion vectors when the number of available candidates are less than 2
        mvpcand_list[num_mvp_cand_lx].x = 0;
        mvpcand_list[num_mvp_cand_lx].y = 0;
        num_mvp_cand_lx++;
    }

    mv->mv[LX].x = mvpcand_list[mvp_lx_flag].x;
    mv->mv[LX].y = mvpcand_list[mvp_lx_flag].y;
}
