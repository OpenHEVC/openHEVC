/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */
#if CONFIG_ECO
#include "hevc_eco.h"

void eco_get_activation(HEVCContext *s)
{
	if (s->sh.first_slice_in_pic_flag) {// ECO: Activation Level management end
		if (s->sh.slice_type != I_SLICE){
			// Eco activation levels
			switch(s->eco_alevel){
			case 0:
				s->hevcdsp.eco_on = 0;
				break;
			case 1:
				if( s->poc%12 == 0)
					s->hevcdsp.eco_on = 1;
				else
					s->hevcdsp.eco_on = 0;
				break;
			case 2:
				if( s->poc%6 == 0)
					s->hevcdsp.eco_on = 1;
				else
					s->hevcdsp.eco_on = 0;
				break;
			case 3:
				if( s->poc%4 == 0)
					s->hevcdsp.eco_on = 1;
				else
					s->hevcdsp.eco_on = 0;
				break;
			case 4:
				if( s->poc%3 == 0)
					s->hevcdsp.eco_on = 1;
				else
					s->hevcdsp.eco_on = 0;
				break;
			case 5:
				if( s->poc%2 == 0 || s->poc%12 == 5)
					s->hevcdsp.eco_on = 0;
				else
					s->hevcdsp.eco_on = 1;
				break;
			case 6:
				if( s->poc%2 == 0)
					s->hevcdsp.eco_on = 1;
				else
					s->hevcdsp.eco_on = 0;
				break;
			case 7:
				if( s->poc%2 == 0 || s->poc%12 == 5 )
					s->hevcdsp.eco_on = 1;
				else
					s->hevcdsp.eco_on = 0;
				break;
			case 8:
				if( s->poc%3 == 0)
					s->hevcdsp.eco_on = 0;
				else
					s->hevcdsp.eco_on = 1;
				break;
			case 9:
				if( s->poc%4 == 0)
					s->hevcdsp.eco_on = 0;
				else
					s->hevcdsp.eco_on = 1;
				break;
			case 10:
				if( s->poc%6 == 0)
					s->hevcdsp.eco_on = 0;
				else
					s->hevcdsp.eco_on = 1;
				break;
			case 11:
				if( s->poc%12 == 6)
					s->hevcdsp.eco_on = 0;
				else
					s->hevcdsp.eco_on = 1;
				break;
			case 12:
				s->hevcdsp.eco_on = 1;
				break;
			default:
				s->hevcdsp.eco_on = 0;
				break;
			}
		}else{ // If frame is I, make sure the in-loop filters are enabled
			s->hevcdsp.eco_on = 0;
			s->sh.disable_deblocking_filter_flag = 0;
			s->sh.disable_sao_filter_flag = 0;
		}
	}
}

void eco_set_activation(HEVCContext *s)
{
	if( s->hevcdsp.eco_on ){ // ECO Mode Activated

		if(s->eco_dbf_on == 0)
			s->sh.disable_deblocking_filter_flag = 1;	// ECO: disable deblocking filter
		if(s->eco_sao_on == 0)
			s->sh.disable_sao_filter_flag = 1;			// ECO: disable SAO filter

		switch(s->eco_luma){
		case LUMA1:										// ECO: 1tap luma interpolation
			if (s->hevcdsp.eco_cur_luma != LUMA1){
				eco_reload_filter_luma1(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case LUMA3:										// ECO: 3taps luma interpolation
			if (s->hevcdsp.eco_cur_luma != LUMA3){
				eco_reload_filter_luma3(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case LUMA5:										// ECO: 5taps luma interpolation
					if (s->hevcdsp.eco_cur_luma != LUMA5){
						eco_reload_filter_luma5(&(s->hevcdsp), s->sps->bit_depth);
					}
					break;
		case LUMA7:	// ECO: no need to reload if 7taps filter is selected
		default:
			break;
		}

		switch(s->eco_chroma){
		case CHROMA1:									// ECO: 1tap chroma interpolation
			if (s->hevcdsp.eco_cur_chroma != CHROMA1){
				eco_reload_filter_chroma1(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case CHROMA2:									// ECO: 2taps chroma interpolation
			if (s->hevcdsp.eco_cur_chroma != CHROMA2){
				eco_reload_filter_chroma2(&(s->hevcdsp), s->sps->bit_depth);
			}
			break;
		case CHROMA4: // ECO: no need to reload if 4taps filter is selected
			break;
		default:
			break;
		}
	}else{ // ECO Mode Desactivated -> Legacy decoding
		if (s->hevcdsp.eco_cur_luma != LUMA7){
			eco_reload_filter_luma7(&(s->hevcdsp), s->sps->bit_depth);
		}
		if (s->hevcdsp.eco_cur_chroma != CHROMA4){
			eco_reload_filter_chroma4(&(s->hevcdsp), s->sps->bit_depth);
		}
		s->sh.disable_deblocking_filter_flag = 0;
		s->sh.disable_sao_filter_flag = 0;
	}
}

void eco_logs(HEVCContext *s)
{
	// Eco Mode Log
	if(s->eco_verbose && s->sh.slice_type != I_SLICE){
		av_log(s->avctx, AV_LOG_INFO,"%d Interpolation configuration: AL %d Luma %d Chroma %d SAO %s DBF %s.\n",s->poc, s->eco_alevel, s->hevcdsp.eco_cur_luma,
			   s->hevcdsp.eco_cur_chroma,(s->eco_sao_on || !s->hevcdsp.eco_on) ? "on" : "off",(s->eco_dbf_on || !s->hevcdsp.eco_on) ? "on" : "off");
	}
}

void eco_update_context(HEVCContext *s, HEVCContext *s0)
{
	s->eco_alevel           = s0->eco_alevel;
	s->eco_luma             = s0->eco_luma;
	s->eco_chroma           = s0->eco_chroma;
	s->eco_dbf_on	        = s0->eco_dbf_on;
	s->eco_sao_on           = s0->eco_sao_on;
	s->eco_verbose			= s0->eco_verbose;
}

#endif
