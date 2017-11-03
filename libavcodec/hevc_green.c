/*
 * HEVC video energy efficient dgreender
 * Morgan Lacour 2015
 */

#include "hevc_green.h"
#include "config.h"

/** Green reload functions */
void green_update_filter_luma(HEVCDSPContext *c, int type);
void green_update_filter_chroma(HEVCDSPContext *c, int type);

static int a_level_old[13] = {
		0,
		1, // 2,    // High
		65,
		546,  // Inter
		585,
		2698,
		1365,
		1397,
		3510,
		3822, // Middle
		4030,
		4031, // 4094, // Low
		4095,
};

void green_update(HEVCContext *s){
	if (s->sh.first_slice_in_pic_flag) {// Green: Activation Level management end
		if (s->sh.slice_type != HEVC_SLICE_I){
			// Green activation levels
			s->hevcdsp.green_on = (a_level_old[s->green_alevel] >> (s->poc%12)) & 0x1;
		}else{ // If frame is I, make sure the in-loop filters are enabled
			s->hevcdsp.green_on = 0;
			s->sh.disable_deblocking_filter_flag = 0;
			s->sh.disable_sao_filter_flag = 0;
		}
	}

	if( s->hevcdsp.green_on ){ // Green Mode Activated
		if(s->green_dbf_on == 0)
			s->sh.disable_deblocking_filter_flag = 1;	// Green: disable deblocking filter
		if(s->green_sao_on == 0)
			s->sh.disable_sao_filter_flag = 1;			// Green: disable SAO filter

		if(s->green_luma != s->hevcdsp.green_cur_luma){
			green_update_filter_luma(&s->hevcdsp, s->green_luma);
			s->hevcdsp.green_cur_luma = s->green_luma;
		}

		if(s->green_chroma != s->hevcdsp.green_cur_chroma){
			green_update_filter_chroma(&s->hevcdsp, s->green_chroma);
			s->hevcdsp.green_cur_chroma = s->green_chroma;
		}
	}else{ // Green Mode Desactivated -> Legacy
		if(s->hevcdsp.green_cur_luma != LUMA_LEG){
			green_update_filter_luma(&s->hevcdsp, LUMA_LEG);
			s->hevcdsp.green_cur_luma = LUMA_LEG;
		}

		if (s->hevcdsp.green_cur_chroma != CHROMA_LEG){
			green_update_filter_chroma(&s->hevcdsp, CHROMA_LEG);
			s->hevcdsp.green_cur_chroma = CHROMA_LEG;
		}

		s->sh.disable_deblocking_filter_flag = 0;
		s->sh.disable_sao_filter_flag = 0;
	}
}

void green_logs(HEVCContext *s)
{
	// Green Mode Log
	if(s->green_verbose && s->sh.slice_type != HEVC_SLICE_I){
		av_log(s->avctx, AV_LOG_INFO,"%d Interpolation configuration: AL %d Luma %d Chroma %d SAO %s DBF %s.\n",s->poc, s->green_alevel, s->hevcdsp.green_cur_luma,
			   s->hevcdsp.green_cur_chroma,(s->green_sao_on || !s->hevcdsp.green_on) ? "on" : "off",(s->green_dbf_on || !s->hevcdsp.green_on) ? "on" : "off");
	}
}

void green_update_context(HEVCContext *s, HEVCContext *s0)
{
	s->green_alevel         = s0->green_alevel;
	s->green_luma           = s0->green_luma;
	s->green_chroma         = s0->green_chroma;
	s->green_dbf_on	        = s0->green_dbf_on;
	s->green_sao_on         = s0->green_sao_on;
	s->green_verbose		= s0->green_verbose;
}
