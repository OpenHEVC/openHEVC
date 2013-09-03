/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 Guillaume Martres
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

/*
 * 6.4.1 Derivation process for z-scan order block availability
 */
static int z_scan_block_avail(HEVCSharedContext *sc, int xCurr, int yCurr,
        int xN, int yN)
{

#define MIN_TB_ADDR_ZS(x, y)                                            \
sc->pps->min_tb_addr_zs[(y) * sc->sps->pic_width_in_min_tbs + (x)]
    int availableN = 0;
    int minBlockAddrCurr =
            MIN_TB_ADDR_ZS((xCurr >> sc->sps->log2_min_transform_block_size), (yCurr >> sc->sps->log2_min_transform_block_size));

    int minBlockAddrN;

    if ((xN < 0) || (yN < 0) || (xN >= sc->sps->pic_width_in_luma_samples)
            || (yN >= sc->sps->pic_height_in_luma_samples)) {
        minBlockAddrN = -1;
    } else {
        minBlockAddrN =
                MIN_TB_ADDR_ZS((xN >> sc->sps->log2_min_transform_block_size), (yN >> sc->sps->log2_min_transform_block_size));
    }

    if ((minBlockAddrN < 0) || (minBlockAddrN > minBlockAddrCurr)) {
        availableN = 0;
    } else {
        availableN = 1;
    }
    return availableN;
}

/*
 * 6.4.2 Derivation process for prediction block availability
 */
static int check_prediction_block_available(HEVCContext *s, int log2_cb_size,
        int x0, int y0, int nPbW, int nPbH, int xA1, int yA1, int partIdx)
{
    int sameCb = 0;
    int availableN = 0;
    HEVCSharedContext *sc = s->HEVCsc;
    HEVCLocalContext *lc = s->HEVClc;
    if ((lc->cu.x < xA1) && (lc->cu.y < yA1)
            && ((lc->cu.x + (1 << log2_cb_size)) > xA1)
            && ((lc->cu.y + (1 << log2_cb_size)) > yA1)) {
        sameCb = 1;
    } else {
        sameCb = 0;
    }

    if (sameCb == 0) {
        availableN = z_scan_block_avail(sc, x0, y0, xA1, yA1);
    } else {
        if ((nPbW << 1 == (1 << log2_cb_size))
                && ((nPbH << 1) == (1 << log2_cb_size)) && (partIdx == 1)
                && ((lc->cu.x + nPbW) > xA1) && ((lc->cu.y + nPbH) <= yA1)) {
            availableN = 0;
        } else {
            availableN = 1;
        }
    }
    return availableN;
}

//check if the two luma locations belong to the same mostion estimation region
static int isDiffMER(HEVCSharedContext *s, int xN, int yN, int xP, int yP)
{
    uint8_t plevel = s->pps->log2_parallel_merge_level;
    if (((xN >> plevel) == (xP >> plevel))
            && ((yN >> plevel) == (yP >> plevel)))
        return 1;
    return 0;
}

// check if the mv's and refidx are the same between A and B
static int compareMVrefidx(MvField A, MvField B)
{
    if (A.pred_flag == B.pred_flag) {
        if (A.pred_flag == 3)
            return ((A.ref_idx[0] == B.ref_idx[0]) && (A.mv[0].x == B.mv[0].x)
                    && (A.mv[0].y == B.mv[0].y) && (A.ref_idx[1] == B.ref_idx[1])
                    && (A.mv[1].x == B.mv[1].x) && (A.mv[1].y == B.mv[1].y));
        else if (A.pred_flag == 1)
            return ((A.ref_idx[0] == B.ref_idx[0]) && (A.mv[0].x == B.mv[0].x)
                    && (A.mv[0].y == B.mv[0].y));
        else if (A.pred_flag == 2)
            return ((A.ref_idx[1] == B.ref_idx[1]) && (A.mv[1].x == B.mv[1].x)
                    && (A.mv[1].y == B.mv[1].y));
    }
    return 0;
}

static int DiffPicOrderCnt(int A, int B)
{
    return A - B;
}

// derive the motion vectors section 8.5.3.1.8
static int derive_temporal_colocated_mvs(HEVCSharedContext *s, MvField temp_col,
        int refIdxLx, Mv* mvLXCol, int X, int colPic,
        RefPicList* refPicList_col)
{
    int availableFlagLXCol = 0;
    Mv mvCol;
    int listCol;
    int refidxCol;
    int check_mvset = 0;
    RefPicList *refPicList = s->ref->refPicList;

    if (temp_col.is_intra) {
        mvLXCol->x = 0;
        mvLXCol->y = 0;
        availableFlagLXCol = 0;
    } else {
        if ((temp_col.pred_flag & 1) == 0) {
            mvCol = temp_col.mv[1];
            refidxCol = temp_col.ref_idx[1];
            listCol = L1;
            check_mvset = 1;
        } else if (temp_col.pred_flag == 1) {
            mvCol = temp_col.mv[0];
            refidxCol = temp_col.ref_idx[0];
            listCol = L0;
            check_mvset = 1;
        } else if (temp_col.pred_flag == 3) {
            int check_diffpicount = 0;
            int i = 0;
            for (i = 0; i < refPicList[0].numPic; i++) {
                if (DiffPicOrderCnt(refPicList[0].list[i], s->poc) > 0)
                    check_diffpicount++;
            }
            for (i = 0; i < refPicList[1].numPic; i++) {
                if (DiffPicOrderCnt(refPicList[1].list[i], s->poc) > 0)
                    check_diffpicount++;
            }
            if ((check_diffpicount == 0) && (X == 0)) {
                mvCol = temp_col.mv[0];
                refidxCol = temp_col.ref_idx[0];
                listCol = L0;
            } else if ((check_diffpicount == 0) && (X == 1)) {
                mvCol = temp_col.mv[1];
                refidxCol = temp_col.ref_idx[1];
                listCol = L1;
            } else {
                if (s->sh.collocated_from_l0_flag == 0) {
                    mvCol = temp_col.mv[0];
                    refidxCol = temp_col.ref_idx[0];
                    listCol = L0;
                } else {
                    mvCol = temp_col.mv[1];
                    refidxCol = temp_col.ref_idx[1];
                    listCol = L1;
                }
            }
            check_mvset = 1;
        }
        // Assuming no long term pictures in version 1 of the decoder
        if (check_mvset == 1) {
            int currIsLongTerm = refPicList[X].is_long_term[refIdxLx];
            int colIsLongTerm = refPicList_col[listCol].is_long_term[refidxCol];
            if (currIsLongTerm != colIsLongTerm) {
                availableFlagLXCol = 0;
                mvLXCol->x = 0;
                mvLXCol->y = 0;
            } else {
                int colPocDiff = DiffPicOrderCnt(colPic,
                        refPicList_col[listCol].list[refidxCol]);
                int curPocDiff = DiffPicOrderCnt(s->poc,
                        refPicList[X].list[refIdxLx]);
                colPocDiff = colPocDiff == 0 ? 1 : colPocDiff; //error resilience
                availableFlagLXCol = 1;
                if (currIsLongTerm || colPocDiff == curPocDiff) {
                    mvLXCol->x = mvCol.x;
                    mvLXCol->y = mvCol.y;
                } else {
                    int td = av_clip_c(colPocDiff, -128, 127);
                    int tb = av_clip_c(curPocDiff, -128, 127);
                    int tx = (0x4000 + abs(td / 2)) / td;
                    int distScaleFactor = av_clip_c((tb * tx + 32) >> 6, -4096,
                            4095);
                    mvLXCol->x = av_clip_c(
                            (distScaleFactor * mvCol.x + 127
                                    + (distScaleFactor * mvCol.x < 0)) >> 8,
                            -32768, 32767);
                    mvLXCol->y = av_clip_c(
                            (distScaleFactor * mvCol.y + 127
                                    + (distScaleFactor * mvCol.y < 0)) >> 8,
                            -32768, 32767);
                }
            }
        }
    }
    return availableFlagLXCol;
}
/*
 * 8.5.3.1.7  temporal luma motion vector prediction
 */
static int temporal_luma_motion_vector(HEVCSharedContext *s, int x0, int y0,
        int nPbW, int nPbH, int refIdxLx, Mv* mvLXCol, int X)
{
    MvField *coloc_tab_mvf = NULL;
    MvField temp_col;
    RefPicList *refPicList = s->ref->refPicList;
    int xPRb, yPRb;
    int xPRb_pu;
    int yPRb_pu;
    int xPCtr, yPCtr;
    int xPCtr_pu;
    int yPCtr_pu;
    int pic_width_in_min_pu = s->sps->pic_width_in_luma_samples >> s->sps->log2_min_pu_size;

    int short_ref_idx = 0;
    int availableFlagLXCol = 0;
    int colPic = 0;

    if ((s->sh.slice_type == B_SLICE) && (s->sh.collocated_from_l0_flag == 0)) {
        short_ref_idx = refPicList[1].idx[s->sh.collocated_ref_idx];
        coloc_tab_mvf = s->DPB[short_ref_idx].tab_mvf;
        colPic = s->DPB[short_ref_idx].poc;
    } else if (((s->sh.slice_type == B_SLICE)
            && (s->sh.collocated_from_l0_flag == 1))
            || (s->sh.slice_type == P_SLICE)) {
        short_ref_idx = refPicList[0].idx[s->sh.collocated_ref_idx];
        coloc_tab_mvf = s->DPB[short_ref_idx].tab_mvf;
        colPic = s->DPB[short_ref_idx].poc;
    }
    //bottom right collocated motion vector
    xPRb = x0 + nPbW;
    yPRb = y0 + nPbH;
    if (coloc_tab_mvf
            && ((y0 >> s->sps->log2_ctb_size) == (yPRb >> s->sps->log2_ctb_size))
            && (yPRb < s->sps->pic_height_in_luma_samples)
            && (xPRb < s->sps->pic_width_in_luma_samples)) {
        xPRb = ((xPRb >> 4) << 4);
        yPRb = ((yPRb >> 4) << 4);
        xPRb_pu = xPRb >> s->sps->log2_min_pu_size;
        yPRb_pu = yPRb >> s->sps->log2_min_pu_size;
        temp_col = coloc_tab_mvf[(yPRb_pu) * pic_width_in_min_pu + xPRb_pu];
        availableFlagLXCol = derive_temporal_colocated_mvs(s, temp_col,
                refIdxLx, mvLXCol, X, colPic, ff_hevc_get_ref_list(s, short_ref_idx, xPRb/*x0*/, yPRb/*y0*/));
    } else {
        mvLXCol->x = 0;
        mvLXCol->y = 0;
        availableFlagLXCol = 0;
    }

    // derive center collocated motion vector
    if (coloc_tab_mvf && availableFlagLXCol == 0) {
        xPCtr = x0 + (nPbW >> 1);
        yPCtr = y0 + (nPbH >> 1);
        xPCtr = ((xPCtr >> 4) << 4);
        yPCtr = ((yPCtr >> 4) << 4);
        xPCtr_pu = xPCtr >> s->sps->log2_min_pu_size;
        yPCtr_pu = yPCtr >> s->sps->log2_min_pu_size;
        temp_col = coloc_tab_mvf[(yPCtr_pu) * pic_width_in_min_pu + xPCtr_pu];
        availableFlagLXCol = derive_temporal_colocated_mvs(s, temp_col,
                refIdxLx, mvLXCol, X, colPic, ff_hevc_get_ref_list(s, short_ref_idx, xPCtr/*x0*/, yPCtr/*y0*/));
    }
    return availableFlagLXCol;
}
/*
 * 8.5.3.1.2  Derivation process for spatial merging candidates
 */
static void derive_spatial_merge_candidates(HEVCContext *s, int x0, int y0,
        int nPbW, int nPbH, int log2_cb_size, int singleMCLFlag, int part_idx,
        MvField mergecandlist[])
{

    HEVCSharedContext *sc = s->HEVCsc;
    HEVCLocalContext *lc = s->HEVClc;
    RefPicList *refPicList = sc->ref->refPicList;
    MvField *tab_mvf = sc->ref->tab_mvf;

    Mv mvL0Col = { 0 };
    Mv mvL1Col = { 0 };

    //first left spatial merge candidate
    int xA1 = x0 - 1;
    int yA1 = y0 + nPbH - 1;
    int is_available_a1;
    int pic_width_in_min_pu = s->HEVCsc->sps->pic_width_in_luma_samples >> s->HEVCsc->sps->log2_min_pu_size;

    int check_MER = 1;
    int check_MER_1 = 1;

    int xB1, yB1;
    int is_available_b1;
    int xb1_pu;
    int yb1_pu;

    int check_B0;
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

    int x0b = x0 & ((1 << sc->sps->log2_ctb_size) - 1);
    int y0b = y0 & ((1 << sc->sps->log2_ctb_size) - 1);

    int cand_up = (lc->ctb_up_flag || y0b);
    int cand_left = (lc->ctb_left_flag || x0b);
    int cand_up_left =
            (!x0b && !y0b) ? lc->ctb_up_left_flag : cand_left && cand_up;
    int cand_up_right =
            ((x0b + nPbW) == (1 << sc->sps->log2_ctb_size)) ?
                    lc->ctb_up_right_flag && !y0b : cand_up;
    int cand_bottom_left = ((y0 + nPbH) >= lc->end_of_tiles_y) ? 0 : cand_left;

    int xA1_pu = xA1 >> sc->sps->log2_min_pu_size;
    int yA1_pu = yA1 >> sc->sps->log2_min_pu_size;

    int availableFlagL0Col = 0;
    int availableFlagL1Col = 0;

    if (cand_left
            && !(tab_mvf[(yA1_pu) * pic_width_in_min_pu + xA1_pu].is_intra)) {
        is_available_a1 = 1;
    } else {
        is_available_a1 = 0;
    }

    if ((singleMCLFlag == 0) && (part_idx == 1)
            && ((lc->cu.part_mode == PART_Nx2N)
                    || (lc->cu.part_mode == PART_nLx2N)
                    || (lc->cu.part_mode == PART_nRx2N))
            || isDiffMER(sc, xA1, yA1, x0, y0)) {
        is_available_a1 = 0;
    }

    if (is_available_a1) {
        mergecandlist[mergearray_index] = tab_mvf[(yA1_pu) * pic_width_in_min_pu + xA1_pu];
        mergearray_index++;
    }

    // above spatial merge candidate

    xB1 = x0 + nPbW - 1;
    yB1 = y0 - 1;
    xb1_pu = xB1 >> sc->sps->log2_min_pu_size;
    yb1_pu = yB1 >> sc->sps->log2_min_pu_size;

    if (cand_up
            && !(tab_mvf[(yb1_pu) * pic_width_in_min_pu + xb1_pu].is_intra)) {
        is_available_b1 = 1;
    } else {
        is_available_b1 = 0;
    }

    if ((singleMCLFlag == 0) && (part_idx == 1)
            && ((lc->cu.part_mode == PART_2NxN)
                    || (lc->cu.part_mode == PART_2NxnU)
                    || (lc->cu.part_mode == PART_2NxnD))
            || isDiffMER(sc, xB1, yB1, x0, y0)) {
        is_available_b1 = 0;
    }
    if (is_available_a1 && is_available_b1) {
        check_MER = !(compareMVrefidx(
                tab_mvf[(yb1_pu) * pic_width_in_min_pu + xb1_pu],
                tab_mvf[(yA1_pu) * pic_width_in_min_pu + xA1_pu]));
    }

    if (is_available_b1 && check_MER) {
        mergecandlist[mergearray_index] = tab_mvf[(yb1_pu) * pic_width_in_min_pu + xb1_pu];
        mergearray_index++;
    }

    // above right spatial merge candidate
    xB0 = x0 + nPbW;
    yB0 = y0 - 1;
    check_MER = 1;
    xb0_pu = xB0 >> sc->sps->log2_min_pu_size;
    yb0_pu = yB0 >> sc->sps->log2_min_pu_size;
    check_B0 = check_prediction_block_available(s, log2_cb_size, x0, y0, nPbW,
            nPbH, xB0, yB0, part_idx);
    if (cand_up_right && check_B0
            && !(tab_mvf[(yb0_pu) * pic_width_in_min_pu + xb0_pu].is_intra)) {
        is_available_b0 = 1;
    } else {
        is_available_b0 = 0;
    }

    if ((isDiffMER(sc, xB0, yB0, x0, y0))) {
        is_available_b0 = 0;
    }

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
    xa0_pu = xA0 >> sc->sps->log2_min_pu_size;
    ya0_pu = yA0 >> sc->sps->log2_min_pu_size;
    check_A0 = check_prediction_block_available(s, log2_cb_size, x0, y0, nPbW,
            nPbH, xA0, yA0, part_idx);

    if (cand_bottom_left && check_A0
            && !(tab_mvf[(ya0_pu) * pic_width_in_min_pu + xa0_pu].is_intra)) {
        is_available_a0 = 1;
    } else {
        is_available_a0 = 0;
    }

    if ((isDiffMER(sc, xA0, yA0, x0, y0))) {
        is_available_a0 = 0;
    }
    if (is_available_a1 && is_available_a0) {
        check_MER = !(compareMVrefidx(
                tab_mvf[(ya0_pu) * pic_width_in_min_pu + xa0_pu],
                tab_mvf[(yA1_pu) * pic_width_in_min_pu + xA1_pu]));
    }

    if (is_available_a0 && check_MER) {
        mergecandlist[mergearray_index] = tab_mvf[(ya0_pu) * pic_width_in_min_pu + xa0_pu];
        mergearray_index++;
    }

    // above left spatial merge candidate
    xB2 = x0 - 1;
    yB2 = y0 - 1;
    check_MER = 1;
    xb2_pu = xB2 >> sc->sps->log2_min_pu_size;
    yb2_pu = yB2 >> sc->sps->log2_min_pu_size;
    if (cand_up_left
            && !(tab_mvf[(yb2_pu) * pic_width_in_min_pu + xb2_pu].is_intra)) {
        isAvailableB2 = 1;
    } else {
        isAvailableB2 = 0;
    }

    if ((isDiffMER(sc, xB2, yB2, x0, y0))) {
        isAvailableB2 = 0;
    }
    if (is_available_a1 && isAvailableB2) {
        check_MER = !(compareMVrefidx(
                tab_mvf[(yb2_pu) * pic_width_in_min_pu + xb2_pu],
                tab_mvf[(yA1_pu) * pic_width_in_min_pu + xA1_pu]));
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
    if (sc->sh.slice_temporal_mvp_enabled_flag == 0) {
        availableFlagLXCol = 0;
    } else {
        availableFlagL0Col = temporal_luma_motion_vector(sc, x0, y0, nPbW, nPbH,
                refIdxL0Col, &mvL0Col, 0);
        // one optimization is that l1 check can be done only when the current slice type is B_SLICE
        if (sc->sh.slice_type == B_SLICE) {
            availableFlagL1Col = temporal_luma_motion_vector(sc, x0, y0, nPbW,
                    nPbH, refIdxL1Col, &mvL1Col, 1);
        }
        availableFlagLXCol = availableFlagL0Col || availableFlagL1Col;
        if (availableFlagLXCol && (mergearray_index < sc->sh.max_num_merge_cand)) {
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
    if (sc->sh.slice_type == B_SLICE) {
        if ((numOrigMergeCand > 1)
                && (numOrigMergeCand < sc->sh.max_num_merge_cand)) {

            combIdx = 0;
            combStop = 0;
            while (combStop != 1) {
                MvField l0Cand;
                MvField l1Cand;
                l0CandIdx = l0_l1_cand_idx[combIdx][0];
                l1CandIdx = l0_l1_cand_idx[combIdx][1];
                l0Cand = mergecandlist[l0CandIdx];
                l1Cand = mergecandlist[l1CandIdx];
                if ((l0Cand.pred_flag & 1) && (l1Cand.pred_flag & 2)
                        && (((DiffPicOrderCnt(
                                refPicList[0].list[l0Cand.ref_idx[0]],
                                refPicList[1].list[l1Cand.ref_idx[1]])) != 0)
                                || ((l0Cand.mv[0].x != l1Cand.mv[1].x)
                                        || (l0Cand.mv[0].y != l1Cand.mv[1].y)))) {
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
                if ((combIdx == numOrigMergeCand * (numOrigMergeCand - 1))
                        || (numMergeCand == sc->sh.max_num_merge_cand)) {
                    combStop = 1;
                }
            }
        }
    }

    /*
     * append Zero motion vector candidates
     */
    if (sc->sh.slice_type == P_SLICE)
        numRefIdx = sc->sh.num_ref_idx_l0_active;
    else // B_SLICE
        numRefIdx =
                sc->sh.num_ref_idx_l0_active > sc->sh.num_ref_idx_l1_active ?
                        sc->sh.num_ref_idx_l1_active :
                        sc->sh.num_ref_idx_l0_active;

    while (numMergeCand < sc->sh.max_num_merge_cand) {
        MvField *zerovector;
        zerovector = &mergecandlist[numMergeCand];
        if (sc->sh.slice_type == P_SLICE) {
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
        int nPbH, int log2_cb_size, int part_idx, int merge_idx, MvField *mv)
{
    int singleMCLFlag = 0;
    int nCS = 1 << log2_cb_size;
    MvField mergecand_list[MRG_MAX_NUM_CANDS] = { { { { 0 } } } };
    int nPbW2 = nPbW;
    int nPbH2 = nPbH;
    HEVCSharedContext *sc = s->HEVCsc;
    HEVCLocalContext *lc = s->HEVClc;
    if ((sc->pps->log2_parallel_merge_level > 2) && (nCS == 8)) {
        singleMCLFlag = 1;
        x0 = lc->cu.x;
        y0 = lc->cu.y;
        nPbW = nCS;
        nPbH = nCS;
        part_idx = 0;
    }
    derive_spatial_merge_candidates(s, x0, y0, nPbW, nPbH, log2_cb_size,
            singleMCLFlag, part_idx, mergecand_list);
    if ((mergecand_list[merge_idx].pred_flag == 3)
            && ((nPbW2 + nPbH2) == 12)) {
        mergecand_list[merge_idx].ref_idx[1] = -1;
        mergecand_list[merge_idx].pred_flag &= 1;
    }
    *mv = mergecand_list[merge_idx];
}

#define TAB_MVF(x, y)                                                              \
    tab_mvf[(y) * pic_width_in_min_pu + x]

static av_always_inline void dist_scale(HEVCSharedContext *s, Mv * mv,
        int pic_width_in_min_pu, int x, int y, int elist,
        int ref_idx_curr, int ref_idx)
{
    RefPicList *refPicList = s->ref->refPicList;
    MvField *tab_mvf = s->ref->tab_mvf;
    if ((DiffPicOrderCnt(
            refPicList[elist].list[TAB_MVF(x, y).ref_idx[elist]], refPicList[ref_idx_curr].list[ref_idx]))!=0) {
        int td = av_clip_int8_c((DiffPicOrderCnt(s->poc,refPicList[elist].list[TAB_MVF(x, y).ref_idx[elist]])));
        int tb = av_clip_int8_c((DiffPicOrderCnt(s->poc,refPicList[ref_idx_curr].list[ref_idx])));
        int tx = (0x4000 + abs(td/2)) / td;
        int distScaleFactor = av_clip_c((tb * tx + 32) >> 6, -4096, 4095);
        mv->x = av_clip_int16((distScaleFactor * mv->x + 127 + (distScaleFactor * mv->x < 0)) >> 8);
        mv->y = av_clip_int16((distScaleFactor * mv->y + 127 + (distScaleFactor * mv->y < 0)) >> 8);
    }
}

static int mv_mp_mode_mx(HEVCContext *s, int x, int y, int pred_flag_index, Mv *mv, int ref_idx_curr, int ref_idx) {
    HEVCSharedContext *sc = s->HEVCsc;
    MvField *tab_mvf = sc->ref->tab_mvf;
    int pic_width_in_min_pu = s->HEVCsc->sps->pic_width_in_luma_samples >> s->HEVCsc->sps->log2_min_pu_size;

    RefPicList *refPicList = sc->ref->refPicList;

    if ((TAB_MVF(x, y).pred_flag & (1 << pred_flag_index)) &&
        (DiffPicOrderCnt(refPicList[pred_flag_index].list[TAB_MVF(x, y).ref_idx[pred_flag_index]], refPicList[ref_idx_curr].list[ref_idx])) == 0) {
        *mv = TAB_MVF(x, y).mv[pred_flag_index];
        return 1;
    }
    return 0;
}


static int mv_mp_mode_mx_lt(HEVCContext *s, int x, int y, int pred_flag_index, Mv *mv, int ref_idx_curr, int ref_idx) {
    HEVCSharedContext *sc = s->HEVCsc;
    MvField *tab_mvf = sc->ref->tab_mvf;
    int pic_width_in_min_pu = s->HEVCsc->sps->pic_width_in_luma_samples >> s->HEVCsc->sps->log2_min_pu_size;

    RefPicList *refPicList = sc->ref->refPicList;
    int currIsLongTerm = refPicList[ref_idx_curr].is_long_term[ref_idx];

    int colIsLongTerm =
        refPicList[pred_flag_index].is_long_term[(TAB_MVF(x, y).ref_idx[pred_flag_index])];

    if ((TAB_MVF(x, y).pred_flag & (1 << pred_flag_index)) && colIsLongTerm == currIsLongTerm) {
        *mv = TAB_MVF(x, y).mv[pred_flag_index];
        if (!currIsLongTerm)
            dist_scale(sc, mv, pic_width_in_min_pu, x, y, pred_flag_index, ref_idx_curr, ref_idx);
        return 1;
    }
    return 0;
}

static int luma_mxa_mvp_mode(HEVCContext *s, int x0, int y0, int nPbW, int nPbH, int log2_cb_size,
                             int part_idx, int cand_left, int pred_flag_index_l0, int pred_flag_index_l1,
                             int cand_bottom_left, Mv *mv, int ref_idx_curr, int ref_idx,
                             int *is_scaled_flag_l0) {
    HEVCSharedContext *sc = s->HEVCsc;
    MvField *tab_mvf = sc->ref->tab_mvf;
    int pic_width_in_min_pu = sc->sps->pic_width_in_luma_samples >> sc->sps->log2_min_pu_size;

    // left bottom spatial candidate
    int xa0_pu = (x0 - 1) >> sc->sps->log2_min_pu_size;
    int ya0_pu = (y0 + nPbH) >> sc->sps->log2_min_pu_size;
    int is_available_a0;

    //left spatial merge candidate
    int xa1_pu;
    int ya1_pu;
    int is_available_a1;
    is_available_a0 = (cand_bottom_left && !(TAB_MVF(xa0_pu, ya0_pu).is_intra));
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
    xa1_pu = (x0 - 1) >> sc->sps->log2_min_pu_size;
    ya1_pu = (y0 + nPbH - 1) >> sc->sps->log2_min_pu_size;
    is_available_a1 = cand_left && !(TAB_MVF(xa1_pu, ya1_pu).is_intra);

    if (is_available_a1) {
        *is_scaled_flag_l0 = 1;
        if (mv_mp_mode_mx(s, xa1_pu, ya1_pu, pred_flag_index_l0, mv, ref_idx_curr, ref_idx))
            return 1;
        if (mv_mp_mode_mx(s, xa1_pu, ya1_pu, pred_flag_index_l1, mv, ref_idx_curr, ref_idx))
            return 1;
    }

    if (is_available_a0) {
        if (mv_mp_mode_mx_lt(s, xa0_pu, ya0_pu, pred_flag_index_l0, mv, ref_idx_curr, ref_idx))
            return 1;
        if (mv_mp_mode_mx_lt(s, xa0_pu, ya0_pu, pred_flag_index_l1, mv, ref_idx_curr, ref_idx))
            return 1;
    }

    if (is_available_a1) {
        if (mv_mp_mode_mx_lt(s, xa1_pu, ya1_pu, pred_flag_index_l0, mv, ref_idx_curr, ref_idx))
            return 1;
        if (mv_mp_mode_mx_lt(s, xa1_pu, ya1_pu, pred_flag_index_l1, mv, ref_idx_curr, ref_idx))
            return 1;
    }
    return 0;

}


void ff_hevc_luma_mv_mvp_mode(HEVCContext *s, int x0, int y0, int nPbW,
        int nPbH, int log2_cb_size, int part_idx, int merge_idx, MvField *mv,
        int mvp_lx_flag, int LX)
{
    HEVCSharedContext *sc = s->HEVCsc;
    HEVCLocalContext *lc = s->HEVClc;
    MvField *tab_mvf = sc->ref->tab_mvf;
    int is_scaled_flag_l0 = 0;
    int available_flag_lx_a0 = 0;
    int available_flag_lx_b0 = 0;
    int available_flag_lx_col = 0;
    int num_mvp_cand_lx = 0;
    int pic_width_in_min_pu = sc->sps->pic_width_in_luma_samples >> sc->sps->log2_min_pu_size;


    // B candidates
    // above right spatial merge candidate
    int xb0_pu = (x0 + nPbW) >> sc->sps->log2_min_pu_size;
    int yb0_pu = (y0 - 1) >> sc->sps->log2_min_pu_size;
    int isAvailable_b0;

    // above spatial merge candidate
    int xb1_pu = (x0 + nPbW - 1) >> sc->sps->log2_min_pu_size;
    int yb1_pu = (y0 - 1) >> sc->sps->log2_min_pu_size;
    int is_available_b1 = 0;

    // above left spatial merge candidate
    int xb2_pu = (x0 - 1) >> sc->sps->log2_min_pu_size;
    int yb2_pu = (y0 - 1) >> sc->sps->log2_min_pu_size;
    int is_available_b2 = 0;
    Mv mvpcand_list[2];
    Mv mxA = { 0 };
    Mv mxB = { 0 };
    Mv mvLXCol = { 0 };
    int ref_idx_curr = 0;
    int ref_idx = 0;
    int pred_flag_index_l0;
    int pred_flag_index_l1;
    int x0b = x0 & ((1 << sc->sps->log2_ctb_size) - 1);
    int y0b = y0 & ((1 << sc->sps->log2_ctb_size) - 1);

    int cand_up = (lc->ctb_up_flag || y0b);
    int cand_left = (lc->ctb_left_flag || x0b);
    int cand_up_left =
            (!x0b && !y0b) ? lc->ctb_up_left_flag : cand_left && cand_up;
    int cand_up_right =
            ((x0b + nPbW) == (1 << sc->sps->log2_ctb_size)
                    || (x0 + nPbW) >= lc->end_of_tiles_x) ?
                    lc->ctb_up_right_flag && !y0b : cand_up;
    int cand_bottom_left = ((y0 + nPbH) >= lc->end_of_tiles_y) ? 0 : cand_left;

    if (LX == 0) {
        ref_idx_curr = 0; //l0
        ref_idx = mv->ref_idx[0];
        pred_flag_index_l0 = 0;
        pred_flag_index_l1 = 1;
    } else {
        ref_idx_curr = 1; // l1
        ref_idx = mv->ref_idx[1];
        pred_flag_index_l0 = 1;
        pred_flag_index_l1 = 0;
    }


    available_flag_lx_a0 = luma_mxa_mvp_mode(s, x0, y0, nPbW, nPbH, log2_cb_size,
                      part_idx, cand_left, pred_flag_index_l0, pred_flag_index_l1,
                      cand_bottom_left, &mxA, ref_idx_curr, ref_idx,
                      &is_scaled_flag_l0);

    isAvailable_b0 = (cand_up_right && !(TAB_MVF(xb0_pu, yb0_pu).is_intra));
    if (isAvailable_b0)
        isAvailable_b0 = check_prediction_block_available(s, log2_cb_size, x0, y0, nPbW,
                                                         nPbH, x0 + nPbW, y0 - 1, part_idx);

    if (isAvailable_b0) {
        available_flag_lx_b0 = mv_mp_mode_mx(s, xb0_pu, yb0_pu, pred_flag_index_l0, &mxB, ref_idx_curr, ref_idx);
        if (!available_flag_lx_b0)
            available_flag_lx_b0 = mv_mp_mode_mx(s, xb0_pu, yb0_pu, pred_flag_index_l1, &mxB, ref_idx_curr, ref_idx);
    }

    if (!available_flag_lx_b0) {
        is_available_b1 = cand_up && !(TAB_MVF(xb1_pu, yb1_pu).is_intra);

        if (is_available_b1) {
            available_flag_lx_b0 = mv_mp_mode_mx(s, xb1_pu, yb1_pu, pred_flag_index_l0, &mxB, ref_idx_curr, ref_idx);
            if (!available_flag_lx_b0)
                available_flag_lx_b0 = mv_mp_mode_mx(s, xb1_pu, yb1_pu, pred_flag_index_l1, &mxB, ref_idx_curr, ref_idx);
        }
    }

    if (!available_flag_lx_b0) {
        is_available_b2 = cand_up_left && !(TAB_MVF(xb2_pu, yb2_pu).is_intra);

        if (is_available_b2) {
            available_flag_lx_b0 = mv_mp_mode_mx(s, xb2_pu, yb2_pu, pred_flag_index_l0, &mxB, ref_idx_curr, ref_idx);
            if (!available_flag_lx_b0)
                available_flag_lx_b0 = mv_mp_mode_mx(s, xb2_pu, yb2_pu, pred_flag_index_l1, &mxB, ref_idx_curr, ref_idx);
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
            available_flag_lx_b0 = mv_mp_mode_mx_lt(s, xb0_pu, yb0_pu, pred_flag_index_l0, &mxB, ref_idx_curr, ref_idx);
            if (!available_flag_lx_b0)
                available_flag_lx_b0 = mv_mp_mode_mx_lt(s, xb0_pu, yb0_pu, pred_flag_index_l1, &mxB, ref_idx_curr, ref_idx);
        }

        if (is_available_b1 && !available_flag_lx_b0) {
            available_flag_lx_b0 = mv_mp_mode_mx_lt(s, xb1_pu, yb1_pu, pred_flag_index_l0, &mxB, ref_idx_curr, ref_idx);
            if (is_available_b1 && !available_flag_lx_b0)
                available_flag_lx_b0 = mv_mp_mode_mx_lt(s, xb1_pu, yb1_pu, pred_flag_index_l1, &mxB, ref_idx_curr, ref_idx);
        }

        if (is_available_b2 && !available_flag_lx_b0) {
            available_flag_lx_b0 = mv_mp_mode_mx_lt(s, xb2_pu, yb2_pu, pred_flag_index_l0, &mxB, ref_idx_curr, ref_idx);
            if (is_available_b2 && !available_flag_lx_b0)
                available_flag_lx_b0 = mv_mp_mode_mx_lt(s, xb2_pu, yb2_pu, pred_flag_index_l1, &mxB, ref_idx_curr, ref_idx);
        }
    }

    if (available_flag_lx_a0 && available_flag_lx_b0
            && ((mxA.x != mxB.x) || (mxA.y != mxB.y))) {
        available_flag_lx_col = 0;
    } else {
        //temporal motion vector prediction candidate
        if (sc->sh.slice_temporal_mvp_enabled_flag == 0) {
            available_flag_lx_col = 0;
        } else {
            available_flag_lx_col = temporal_luma_motion_vector(sc, x0, y0, nPbW,
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

    if (available_flag_lx_a0 && available_flag_lx_b0
            && ((mxA.x == mxB.x) && (mxA.y == mxB.y))) {
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

    if (LX == 0) {
        mv->mv[0] = mvpcand_list[mvp_lx_flag];

    }
    if (LX == 1) {
        mv->mv[1] = mvpcand_list[mvp_lx_flag];
    }
}
