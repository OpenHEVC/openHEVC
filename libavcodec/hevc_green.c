/*
 * HEVC video energy efficient dgreender
 * Morgan Lacour 2015
 */
#if CONFIG_GREEN
#include "hevc_green.h"

void green_get_activation(HEVCContext *s)
{
	if (s->sh.first_slice_in_pic_flag) {// Green: Activation Level management end
		if (s->sh.slice_type != I_SLICE){
			// Green activation levels
			switch(s->green_alevel){
			case 0:
				s->hevcdsp.green_on = 0;
				break;
			case 1:
				if( s->poc%12 == 0)
					s->hevcdsp.green_on = 1;
				else
					s->hevcdsp.green_on = 0;
				break;
			case 2:
				if( s->poc%6 == 0)
					s->hevcdsp.green_on = 1;
				else
					s->hevcdsp.green_on = 0;
				break;
			case 3:
				if( s->poc%4 == 0)
					s->hevcdsp.green_on = 1;
				else
					s->hevcdsp.green_on = 0;
				break;
			case 4:
				if( s->poc%3 == 0)
					s->hevcdsp.green_on = 1;
				else
					s->hevcdsp.green_on = 0;
				break;
			case 5:
				if( s->poc%2 == 0 || s->poc%12 == 5)
					s->hevcdsp.green_on = 0;
				else
					s->hevcdsp.green_on = 1;
				break;
			case 6:
				if( s->poc%2 == 0)
					s->hevcdsp.green_on = 1;
				else
					s->hevcdsp.green_on = 0;
				break;
			case 7:
				if( s->poc%2 == 0 || s->poc%12 == 5 )
					s->hevcdsp.green_on = 1;
				else
					s->hevcdsp.green_on = 0;
				break;
			case 8:
				if( s->poc%3 == 0)
					s->hevcdsp.green_on = 0;
				else
					s->hevcdsp.green_on = 1;
				break;
			case 9:
				if( s->poc%4 == 0)
					s->hevcdsp.green_on = 0;
				else
					s->hevcdsp.green_on = 1;
				break;
			case 10:
				if( s->poc%6 == 0)
					s->hevcdsp.green_on = 0;
				else
					s->hevcdsp.green_on = 1;
				break;
			case 11:
				if( s->poc%12 == 6)
					s->hevcdsp.green_on = 0;
				else
					s->hevcdsp.green_on = 1;
				break;
			case 12:
				s->hevcdsp.green_on = 1;
				break;
			default:
				s->hevcdsp.green_on = 0;
				break;
			}
		}else{ // If frame is I, make sure the in-loop filters are enabled
			s->hevcdsp.green_on = 0;
			s->sh.disable_deblocking_filter_flag = 0;
			s->sh.disable_sao_filter_flag = 0;
		}
	}
}

void green_set_activation(HEVCContext *s)
{
	if( s->hevcdsp.green_on ){ // Green Mode Activated

		if(s->green_dbf_on == 0)
			s->sh.disable_deblocking_filter_flag = 1;	// Green: disable deblocking filter
		if(s->green_sao_on == 0)
			s->sh.disable_sao_filter_flag = 1;			// Green: disable SAO filter

		switch(s->green_luma){
		case LUMA1:										// Green: 1tap luma interpolation
			if (s->hevcdsp.green_cur_luma != LUMA1){
				green_reload_filter_luma1(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case LUMA3:										// Green: 3taps luma interpolation
			if (s->hevcdsp.green_cur_luma != LUMA3){
				green_reload_filter_luma3(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case LUMA5:										// Green: 5taps luma interpolation
					if (s->hevcdsp.green_cur_luma != LUMA5){
						green_reload_filter_luma5(&(s->hevcdsp), s->sps->bit_depth);
					}
					break;
		case LUMA7:	// Green: no need to reload if 7taps filter is selected
		default:
			break;
		}

		switch(s->green_chroma){
		case CHROMA1:									// Green: 1tap chroma interpolation
			if (s->hevcdsp.green_cur_chroma != CHROMA1){
				green_reload_filter_chroma1(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case CHROMA2:									// Green: 2taps chroma interpolation
			if (s->hevcdsp.green_cur_chroma != CHROMA2){
				green_reload_filter_chroma2(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case CHROMA3:									// Green: 2taps chroma interpolation
			if (s->hevcdsp.green_cur_chroma != CHROMA3){
				green_reload_filter_chroma3(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case CHROMA4: // Green: no need to reload if 4taps filter is selected
			break;
		default:
			break;
		}
	}else{ // Green Mode Desactivated -> Legacy dgreending
		if (s->hevcdsp.green_cur_luma != LUMA7){
			green_reload_filter_luma7(&(s->hevcdsp), s->sps->bit_depth);
		}
		if (s->hevcdsp.green_cur_chroma != CHROMA4){
			green_reload_filter_chroma4(&(s->hevcdsp), s->sps->bit_depth);
		}
		s->sh.disable_deblocking_filter_flag = 0;
		s->sh.disable_sao_filter_flag = 0;
	}
}

void green_logs(HEVCContext *s)
{
	// Green Mode Log
	if(s->green_verbose && s->sh.slice_type != I_SLICE){
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

#endif
