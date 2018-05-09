#include "hevcdec.h"
#if OHCONFIG_UPSAMPLING360

static double sinc(double x)
{
    x *= M_PI;
    if (x < 0.01 && x > -0.01)
    {
        double x2 = x * x;
        return 1.0f + x2 * (-1.0 / 6.0 + x2 / 120.0);
    }
    else
    {
        return sin(x) / x;
    }
}

static int roundHP(double t) {
    return (int)(t + (t >= 0 ? 0.5 : -0.5));
}


void ff_hevc_init_360_params(HEVCContext *s){
        const int safe_margin_size  = 5;
        const double dScale = 1.0 / S_LANCZOS_LUT_SCALE;
        const int    mul = 1 << (S_INTERPOLATE_PrecisionBD);

        //TODO add those to ohplayer params
        const float fYaw   =  20.0;
        const float fPitch =  -5.0;
        const float h_fov  = 110.0;
        const float v_fov  =  80.0;

        const double tht  = (double)(( fYaw + 90) * M_PI / 180);
        const double phi  = (double)((-fPitch)    * M_PI / 180);
        const double fovx = (double)(M_PI * h_fov / 180.0);
        const double fovy = (double)(M_PI * v_fov / 180.0);

        //TODO adapt to real SHVC parameters with cropping info
        RepFormat current_layer_window = s->ps.vps->vps_ext.rep_format[s->ps.vps->vps_ext.vps_rep_format_idx[s->nuh_layer_id]];

        const int widthEL_v[2]  = {current_layer_window.pic_width_vps_in_luma_samples  - current_layer_window.conf_win_vps_left_offset   - current_layer_window.conf_win_vps_right_offset,   current_layer_window.pic_width_vps_in_luma_samples  - current_layer_window.conf_win_vps_left_offset   - current_layer_window.conf_win_vps_right_offset>>1};
        const int heightEL_v[2] = {current_layer_window.pic_height_vps_in_luma_samples - current_layer_window.conf_win_vps_bottom_offset - current_layer_window.conf_win_vps_top_offset,current_layer_window.pic_height_vps_in_luma_samples - current_layer_window.conf_win_vps_bottom_offset - current_layer_window.conf_win_vps_top_offset>>1};

        RepFormat base_layer_window = s->ps.vps->vps_ext.rep_format[s->ps.vps->vps_ext.vps_rep_format_idx[s->nuh_layer_id-1]];

        const int widthBL_v[2] = {base_layer_window.pic_width_vps_in_luma_samples  - base_layer_window.conf_win_vps_left_offset   - base_layer_window.conf_win_vps_right_offset,  s->BL_width  >> 1};
        const int heightBL_v[2] ={base_layer_window.pic_height_vps_in_luma_samples - base_layer_window.conf_win_vps_bottom_offset - base_layer_window.conf_win_vps_top_offset, s->BL_height >> 1};

        double rotation_matrix[9];
        double inverse_rotation_matrix[9];
        double m_matInvKchroma[9];
        double det;

        if ( !s->weight_lut_luma || !s->weight_lut_chroma){
            s->offset_bl_luma       = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(int) );
            s->weight_idx_luma      = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(int16_t) );
            s->weight_lut_luma      = av_malloc ( sizeof(int16_t*) * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1)));
            s->weight_lut_luma[0]   = av_malloc ( sizeof(int16_t) * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1) * SHVC360_FILTER_SIZE_LUMA));

            s->offset_weight_luma   = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(int) );
            s->end_x_luma           = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(uint8_t) );
            s->end_y_luma           = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(uint8_t) );

            s->no_margin_360_luma   = av_malloc(sizeof(uint8_t) * s->ps.sps->ctb_width*s->ps.sps->ctb_height );

            s->offset_bl_chroma     = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(int) );
            s->weight_idx_chroma    = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(int16_t) );
            s->weight_lut_chroma    = av_malloc ( sizeof(int16_t*) * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1)));
            s->weight_lut_chroma[0] = av_malloc ( sizeof(int16_t)  * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1) * SHVC360_FILTER_SIZE_CHROMA));

            s->offset_weight_chroma = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(int) );
            s->end_x_chroma         = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(uint8_t) );
            s->end_y_chroma         = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(uint8_t) );

            s->no_margin_360_chroma   = av_malloc(sizeof(uint8_t) * s->ps.sps->ctb_width*s->ps.sps->ctb_height );
//            if (!s->pixel_weight_chroma|| !s->pixel_weight_luma|| !s->weight_lut_luma[0] || !s->weight_lut_chroma[0])
//              goto fail;
            //TODO safer alloc check

            // Init rotation matrices

            rotation_matrix[0] = cos(tht);
            rotation_matrix[1] = 0.0;
            rotation_matrix[2] = -sin(tht);
            rotation_matrix[3] = sin(tht) * sin(phi);
            rotation_matrix[4] = cos(phi);
            rotation_matrix[5] = cos(tht) * sin(phi);
            rotation_matrix[6] = sin(tht) * cos(phi);
            rotation_matrix[7] = -sin(phi);
            rotation_matrix[8] = cos(tht) * cos(phi);

            inverse_rotation_matrix[0] = cos(tht);
            inverse_rotation_matrix[1] = sin(tht) * sin(phi);
            inverse_rotation_matrix[2] = sin(tht) * cos(phi);
            inverse_rotation_matrix[3] = 0.0;
            inverse_rotation_matrix[4] = cos(phi);
            inverse_rotation_matrix[5] = -sin(phi);
            inverse_rotation_matrix[6] = -sin(tht);
            inverse_rotation_matrix[7] = cos(tht) * sin(phi);
            inverse_rotation_matrix[8] = cos(tht) * cos(phi);

            /*Luma*/
            {
                double m_pfLanczosFltCoefLut [(SHVC360_LANCZOS_PARAM_LUMA << 1) * S_LANCZOS_LUT_SCALE + 1];//TODO check +1

                int iWindowSearchDimluma[4] = {
                    widthEL_v[0],       0,
                    heightEL_v[0],      0,
                };

                const int offset_x = SHVC360_LANCZOS_PARAM_LUMA << 1;
                const int offset_y = SHVC360_LANCZOS_PARAM_LUMA << 1;
                const int left_offset = (offset_x - 1) >> 1;
                const int top_offset  = (offset_y - 1) >> 1;

                const double tan_x = tan(fovx / 2.0);
                const double tan_y = tan(fovy / 2.0);

                const double tan_x_on_width_bl  = tan_x / widthBL_v[0];
                const double tan_y_on_height_bl = tan_y / heightBL_v[0];

                const double width_bl_on_two  = (double)widthBL_v[0] / 2.0;
                const double height_bl_on_two = (double)heightBL_v[0] / 2.0;

                const double fx_Y = width_bl_on_two  / tan_x;
                const double fy_Y = height_bl_on_two / tan_y;

                const double two_on_width_el  = 2.0 / widthEL_v[0];
                const double one_on_height_el = 1.0 / heightEL_v[0];

                //TODO can be simplified ?
                for (int j = - heightBL_v[0] + 1; j < heightBL_v[0]; j+=2){
                    const double y0 = - tan_y_on_height_bl * ( j );
                    const double square_y0_plus1 = y0 * y0 + 1;

                    const double add_x = inverse_rotation_matrix[1] * y0 + inverse_rotation_matrix[2];
                    const double add_y = inverse_rotation_matrix[4] * y0 + inverse_rotation_matrix[5];
                    const double add_z = inverse_rotation_matrix[7] * y0 + inverse_rotation_matrix[8];


                    for (int i = - widthBL_v[0] + 1; i < widthBL_v[0]; i+=2) {
                        double x, y, z,  x2, y2, norm;

                        x2 = tan_x_on_width_bl  * ( i );

                        norm = sqrt(x2 * x2 + square_y0_plus1);

                        x = (inverse_rotation_matrix[0] * x2 + add_x) / norm;
                        y = (                                  add_y) / norm;
                        z = (inverse_rotation_matrix[6] * x2 + add_z) / norm;

                        norm = sqrt(x*x + y*y + z*z);

                        x2 = (double)((M_PI - atan2(z, x))/(2 * M_PI) * widthEL_v[0]) - 0.5;
                        y2 = (double)((norm < (1e-6) ? 0.0 : acos(y / norm) / M_PI) * heightEL_v[0]) - 0.5;

                        iWindowSearchDimluma[0] = FFMIN((int)x2, iWindowSearchDimluma[0]);
                        iWindowSearchDimluma[1] = FFMAX((int)x2, iWindowSearchDimluma[1]);
                        iWindowSearchDimluma[2] = FFMIN((int)y2, iWindowSearchDimluma[2]);
                        iWindowSearchDimluma[3] = FFMAX((int)y2, iWindowSearchDimluma[3]);
                    }
                }

                // TODO weight lut doesn't depend on parameters
                for (int i = 0; i < (SHVC360_LANCZOS_PARAM_LUMA << 1) * S_LANCZOS_LUT_SCALE; i++) {
                    double x = (double)i / S_LANCZOS_LUT_SCALE - SHVC360_LANCZOS_PARAM_LUMA;
                    m_pfLanczosFltCoefLut[i] = (double)(sinc(x) * sinc(x / SHVC360_LANCZOS_PARAM_LUMA));
                }

                for (int k = 1; k < (S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1); k++){
                    s->weight_lut_luma[k] = s->weight_lut_luma[0] + k * SHVC360_FILTER_SIZE_LUMA;
                }

                for (int m = 0; m < (S_LANCZOS_LUT_SCALE + 1); m++) {
                    double wy[6];
                    double t = (double)m * dScale;
                    for (int k = -SHVC360_LANCZOS_PARAM_LUMA; k < SHVC360_LANCZOS_PARAM_LUMA; k++){
                        wy[k + SHVC360_LANCZOS_PARAM_LUMA] = m_pfLanczosFltCoefLut[(int)((fabs(t - k - 1) + SHVC360_LANCZOS_PARAM_LUMA)* S_LANCZOS_LUT_SCALE + 0.5)];
                    }

                    for (int n = 0; n < (S_LANCZOS_LUT_SCALE + 1); n++) {
                        int16_t *pW = s->weight_lut_luma[m*(S_LANCZOS_LUT_SCALE + 1) + n];
                        int sum = 0;
                        double d_sum = 0;
                        double wx[6];
                        t = (double)n * dScale;
                        for (int k = -SHVC360_LANCZOS_PARAM_LUMA; k < SHVC360_LANCZOS_PARAM_LUMA; k++){
                            wx[k + SHVC360_LANCZOS_PARAM_LUMA] = m_pfLanczosFltCoefLut[(int)((fabs(t - k - 1) + SHVC360_LANCZOS_PARAM_LUMA)* S_LANCZOS_LUT_SCALE + 0.5)];
                        }

                        for (int r = 0; r < (SHVC360_LANCZOS_PARAM_LUMA << 1); r++){
                            for (int c = 0; c < (SHVC360_LANCZOS_PARAM_LUMA << 1); c++){
                                d_sum += wy[r] * wx[c];
                            }
                        }
                        for (int r = 0; r < (SHVC360_LANCZOS_PARAM_LUMA << 1); r++){
                            for (int c = 0; c < (SHVC360_LANCZOS_PARAM_LUMA << 1); c++){
                                int w;
                                if (c != (SHVC360_LANCZOS_PARAM_LUMA << 1) - 1 || r != (SHVC360_LANCZOS_PARAM_LUMA << 1) - 1)
                                    w = round((double)(wy[r] * wx[c] * mul / d_sum));
                                else
                                    w = mul - sum;
                                pW[r*(SHVC360_LANCZOS_PARAM_LUMA << 1) + c] = (int16_t)w;
                                sum += w;
                            }
                        }
                    }
                }

                iWindowSearchDimluma[0] = iWindowSearchDimluma[0] - safe_margin_size;
                iWindowSearchDimluma[1] = iWindowSearchDimluma[1] + safe_margin_size;
                iWindowSearchDimluma[2] = iWindowSearchDimluma[2] - safe_margin_size;
                iWindowSearchDimluma[3] = iWindowSearchDimluma[3] + safe_margin_size;

                for (int j = 0; j < heightEL_v[0]; j++){
                    for (int i = 0; i < widthEL_v[0]; i++) {
                        const int el_pos = j* current_layer_window.pic_width_vps_in_luma_samples+ i;
                        if(i > iWindowSearchDimluma[0]  && i < iWindowSearchDimluma[1]
                                && j > iWindowSearchDimluma[2]  && j < iWindowSearchDimluma[3] ){
                            int  xLanczos, yLanczos;
                            double u, v, yaw, pitch, x2, y2, x, y ,z, /*p[3],*/ p1[3];
                            double  uVP, vVP, iVP, jVP;

                            u = (double)(i) + (double)(0.5);
                            v = (double)(j) + (double)(0.5);

                            yaw   = (double) (M_PI)*(u * two_on_width_el - 1.0);
                            pitch = (double) (M_PI)*(0.5 - v * one_on_height_el);

                            x =   cos(pitch) * cos(yaw);
                            y =   sin(pitch);
                            z = - cos(pitch) * sin(yaw);

                            p1[0] = rotation_matrix[0] * x                          + rotation_matrix[2] * z;
                            p1[1] = rotation_matrix[3] * x + rotation_matrix[4] * y + rotation_matrix[5] * z;
                            p1[2] = rotation_matrix[6] * x + rotation_matrix[7] * y + rotation_matrix[8] * z;

                            x2 = p1[0] / p1[2];
                            y2 = p1[1] / p1[2];

                            uVP =  fx_Y * x2 + width_bl_on_two;
                            vVP = -fy_Y * y2 + height_bl_on_two;

                            iVP = uVP - (double)(0.5);
                            jVP = vVP - (double)(0.5);

                            xLanczos = roundHP(iVP * SVIDEO_2DPOS_PRECISION) >> SVIDEO_2DPOS_PRECISION_LOG2;
                            yLanczos = roundHP(jVP * SVIDEO_2DPOS_PRECISION) >> SVIDEO_2DPOS_PRECISION_LOG2;

                            if (xLanczos >= 0 && xLanczos < widthBL_v[0] && yLanczos >= 0 && yLanczos < heightBL_v[0]){
                                int y = yLanczos - top_offset;
                                int x = xLanczos - left_offset;

                                s->end_y_luma[el_pos] = (y < 0) ? offset_y + y : ((offset_y + y) < heightBL_v[0] ? offset_y : heightBL_v[0] - y );
                                s->end_x_luma[el_pos] = (x < 0) ? offset_x + x : ((offset_x + x) < widthBL_v[0]  ? offset_x : widthBL_v[0]  - x );

                                s->offset_weight_luma[el_pos] = (x < 0 ? -x : 0) + (y < 0 ? -y: 0) * offset_x;

                                s->weight_idx_luma[el_pos] = round( (jVP - (double)yLanczos) * S_LANCZOS_LUT_SCALE) * (S_LANCZOS_LUT_SCALE + 1) + round( (iVP - (double)xLanczos) *S_LANCZOS_LUT_SCALE );
                                s->offset_bl_luma [el_pos] = (y < 0 ? 0 : y) * (base_layer_window.pic_width_vps_in_luma_samples) + (x < 0 ? 0 : x);
                                if(y < 0 || x < 0 || s->end_y_luma[el_pos] != offset_y || s->end_x_luma[el_pos] != offset_x)
                                    s->no_margin_360_luma[(j/64)*s->ps.sps->ctb_width+(i/64)]=1;
                            } else {
                                s->offset_bl_luma [el_pos] = -1;
                                s->no_margin_360_luma[(j/64)*s->ps.sps->ctb_width+(i/64)]=1;
                            }
                        } else {
                            s->offset_bl_luma [el_pos] = -1;
                            s->no_margin_360_luma[(j/64)*s->ps.sps->ctb_width+(i/64)]=1;
                        }
                    }
                }
            }

            /* Chroma */
            {
                double m_pfLanczosFltCoefLut [(SHVC360_LANCZOS_PARAM_CHROMA << 1) * S_LANCZOS_LUT_SCALE + 1];//TODO check +1
                const double fx_CbCr = ((s->BL_width  >> 1) / 2) *(1 / tan(fovx / 2));
                const double fy_CbCr = ((s->BL_height >> 1) / 2) *(1 / tan(fovy / 2));
                const double Kchroma[9] = {
                    fx_CbCr,        0, (s->BL_width  >> 1) / 2.0 ,
                    0,       -fy_CbCr, (s->BL_height >> 1) / 2.0 ,
                    0,              0,                         1
                };

                int iWindowSearchDimchroma[4] = {
                    widthEL_v[1]  >> 1, 0,
                    heightEL_v[1] >> 1, 0
                };

                const int offset_x = SHVC360_LANCZOS_PARAM_CHROMA << 1;
                const int offset_y = SHVC360_LANCZOS_PARAM_CHROMA << 1;
                const int left_offset = (offset_x - 1) >> 1;
                const int top_offset  = (offset_y - 1) >> 1;

                det = Kchroma[0] * Kchroma[4];

                m_matInvKchroma[0] = (Kchroma[4] * Kchroma[8] - Kchroma[7] * Kchroma[5]) / det;
                m_matInvKchroma[1] = (Kchroma[2] * Kchroma[7] - Kchroma[1] * Kchroma[8]) / det;
                m_matInvKchroma[2] = (Kchroma[1] * Kchroma[5] - Kchroma[2] * Kchroma[4]) / det;
                m_matInvKchroma[3] = (Kchroma[5] * Kchroma[6] - Kchroma[3] * Kchroma[8]) / det;
                m_matInvKchroma[4] = (Kchroma[0] * Kchroma[8] - Kchroma[2] * Kchroma[6]) / det;
                m_matInvKchroma[5] = (Kchroma[3] * Kchroma[2] - Kchroma[0] * Kchroma[5]) / det;
                m_matInvKchroma[6] = (Kchroma[3] * Kchroma[7] - Kchroma[6] * Kchroma[4]) / det;
                m_matInvKchroma[7] = (Kchroma[6] * Kchroma[1] - Kchroma[0] * Kchroma[7]) / det;
                m_matInvKchroma[8] = (Kchroma[0] * Kchroma[4] - Kchroma[3] * Kchroma[1]) / det;

                for (int j = 0; j < heightBL_v[1]; j++){
                    for (int i = 0; i < widthBL_v[1]; i++) {
                        double x, y, z, u, v, x2, y2, p1[3],len;

                        u = i + (double)(0.5);
                        v = j + (double)(0.5);

                        x2 = m_matInvKchroma[0] * u + m_matInvKchroma[1] * v + m_matInvKchroma[2];
                        y2 = m_matInvKchroma[3] * u + m_matInvKchroma[4] * v + m_matInvKchroma[5];

                        p1[2] = 1 / sqrt (x2 * x2 + y2 * y2 + 1);
                        p1[0] = p1[2] * x2;
                        p1[1] = p1[2] * y2;

                        x = inverse_rotation_matrix[0] * p1[0] + inverse_rotation_matrix[1] * p1[1] + inverse_rotation_matrix[2] * p1[2];
                        y = inverse_rotation_matrix[3] * p1[0] + inverse_rotation_matrix[4] * p1[1] + inverse_rotation_matrix[5] * p1[2];
                        z = inverse_rotation_matrix[6] * p1[0] + inverse_rotation_matrix[7] * p1[1] + inverse_rotation_matrix[8] * p1[2];

                        x2 = (double)((M_PI - atan2(z, x))* widthEL_v[1] / (2 * M_PI));
                        x2 -= 0.5;

                        len = sqrt(x*x + y*y + z*z);

                        y2 = (double)((len < (1e-6) ? 0.5 : acos(y / len) / M_PI) * heightEL_v[1]);
                        y2 -= 0.5;

                        iWindowSearchDimchroma[0] = FFMIN((int)x2, iWindowSearchDimchroma[0]);
                        iWindowSearchDimchroma[1] = FFMAX((int)x2, iWindowSearchDimchroma[1]);
                        iWindowSearchDimchroma[2] = FFMIN((int)y2, iWindowSearchDimchroma[2]);
                        iWindowSearchDimchroma[3] = FFMAX((int)y2, iWindowSearchDimchroma[3]);
                    }
                }


                for (int i = 0; i < (SHVC360_LANCZOS_PARAM_CHROMA << 1) * S_LANCZOS_LUT_SCALE; i++) {
                    double x = (double)i / S_LANCZOS_LUT_SCALE - SHVC360_LANCZOS_PARAM_CHROMA;
                    m_pfLanczosFltCoefLut[i] = (double)(sinc(x) * sinc(x / SHVC360_LANCZOS_PARAM_CHROMA));
                }

                for (int k = 1; k < (S_LANCZOS_LUT_SCALE + 1)*(S_LANCZOS_LUT_SCALE + 1); k++){
                    s->weight_lut_chroma[k] = s->weight_lut_chroma[0] + k * SHVC360_FILTER_SIZE_CHROMA;
                }

                for (int m = 0; m < (S_LANCZOS_LUT_SCALE + 1); m++) {
                    double wy[6];
                    double t = m * dScale;
                    for (int k = -SHVC360_LANCZOS_PARAM_CHROMA; k < SHVC360_LANCZOS_PARAM_CHROMA; k++){
                        wy[k + SHVC360_LANCZOS_PARAM_CHROMA] = m_pfLanczosFltCoefLut[(int)((fabs(t - k - 1) + SHVC360_LANCZOS_PARAM_CHROMA)* S_LANCZOS_LUT_SCALE + 0.5)];
                    }

                    for (int n = 0; n < (S_LANCZOS_LUT_SCALE + 1); n++) {
                        int16_t *pW = s->weight_lut_chroma[m*(S_LANCZOS_LUT_SCALE + 1) + n];
                        int sum = 0;
                        double dSum = 0;
                        double wx[6];
                        t = n * dScale;
                        for (int k = -SHVC360_LANCZOS_PARAM_CHROMA; k < SHVC360_LANCZOS_PARAM_CHROMA; k++){
                            wx[k + SHVC360_LANCZOS_PARAM_CHROMA] = m_pfLanczosFltCoefLut[(int)((fabs(t - k - 1) + SHVC360_LANCZOS_PARAM_CHROMA)* S_LANCZOS_LUT_SCALE + 0.5)];
                        }

                        for (int r = 0; r < (SHVC360_LANCZOS_PARAM_CHROMA << 1); r++){
                            for (int c = 0; c < (SHVC360_LANCZOS_PARAM_CHROMA << 1); c++){
                                dSum += wy[r] * wx[c];
                            }
                        }
                        for (int r = 0; r < (SHVC360_LANCZOS_PARAM_CHROMA << 1); r++){
                            for (int c = 0; c < (SHVC360_LANCZOS_PARAM_CHROMA << 1); c++) {
                                int w;
                                if (c != (SHVC360_LANCZOS_PARAM_CHROMA << 1) - 1 || r != (SHVC360_LANCZOS_PARAM_CHROMA << 1) - 1)
                                    w = round((double)(wy[r] * wx[c] * mul / dSum));
                                else
                                    w = mul - sum;
                                pW[r*(SHVC360_LANCZOS_PARAM_CHROMA << 1) + c] = (int16_t)w;
                                sum += w;
                            }
                        }
                    }
                }

                iWindowSearchDimchroma[0] = iWindowSearchDimchroma[0] - safe_margin_size;
                iWindowSearchDimchroma[1] = iWindowSearchDimchroma[1] + safe_margin_size;
                iWindowSearchDimchroma[2] = iWindowSearchDimchroma[2] - safe_margin_size;
                iWindowSearchDimchroma[3] = iWindowSearchDimchroma[3] + safe_margin_size;

                for (int j = 0; j < heightEL_v[1]; j++){
                    for (int i = 0; i < widthEL_v[1]; i++) {
                        if ( i > iWindowSearchDimchroma[0]    && i < iWindowSearchDimchroma[1]
                             && j > iWindowSearchDimchroma[2] && j < iWindowSearchDimchroma[3]){
                            int  xLanczos, yLanczos;
                            double u, v, yaw, pitch, x2, y2, p[3],p1[3];
                            double  uVP, vVP, iVP, jVP;

                            u = (double)(i) + (double)(0.5);
                            v = (double)(j) + (double)(0.5);

                            yaw   = (double)(u* M_PI * 2 / widthEL_v[1]  - M_PI);
                            pitch = (double)( M_PI_2 - v* M_PI /  heightEL_v[1]);

                            p[0]  =  cos(pitch) * cos(yaw);
                            p[1]  =  sin(pitch);
                            p[2]  = -cos(pitch) * sin(yaw);

                            p1[0] = rotation_matrix[0] * p[0] + rotation_matrix[1] * p[1] + rotation_matrix[2] * p[2];
                            p1[1] = rotation_matrix[3] * p[0] + rotation_matrix[4] * p[1] + rotation_matrix[5] * p[2];
                            p1[2] = rotation_matrix[6] * p[0] + rotation_matrix[7] * p[1] + rotation_matrix[8] * p[2];

                            x2 = p1[0] / p1[2];
                            y2 = p1[1] / p1[2];

                            uVP = Kchroma[0] * x2 + Kchroma[1] * y2 + Kchroma[2];
                            vVP = Kchroma[3] * x2 + Kchroma[4] * y2 + Kchroma[5];

                            iVP = uVP - (double)(0.5);
                            jVP = vVP - (double)(0.5);

                            xLanczos = roundHP(iVP * SVIDEO_2DPOS_PRECISION) >> SVIDEO_2DPOS_PRECISION_LOG2;
                            yLanczos = roundHP(jVP * SVIDEO_2DPOS_PRECISION) >> SVIDEO_2DPOS_PRECISION_LOG2;

                            if (xLanczos >= 0 && xLanczos < widthBL_v[1] && yLanczos >= 0 && yLanczos < heightBL_v[1]) {
                                int y = yLanczos - top_offset;
                                int x = xLanczos - left_offset;

                                s->weight_idx_chroma[j* widthEL_v[1] + i] = round((jVP - (double)yLanczos) * S_LANCZOS_LUT_SCALE) * (S_LANCZOS_LUT_SCALE + 1) + round((iVP - (double)xLanczos) * S_LANCZOS_LUT_SCALE);
                                s->offset_bl_chroma[j* widthEL_v[1] + i] = (y < 0 ? 0 : y) * widthBL_v[1] + (x < 0 ? 0 : x);
                                s->end_y_chroma[j* widthEL_v[1] + i] = (y < 0) ? offset_y + y : ((offset_y + y) < heightBL_v[1] ? offset_y : heightBL_v[1] - y );
                                s->end_x_chroma[j* widthEL_v[1] + i] = (x < 0) ? offset_x + x : ((offset_x + x) < widthBL_v[1]  ? offset_x : widthBL_v[1]  - x );

                                if(y < 0 || x < 0 || s->end_x_chroma[j* widthEL_v[1] + i] != offset_x || s->end_y_chroma[j* widthEL_v[1] + i]!= offset_y )
                                    s->no_margin_360_chroma[(j/32)*s->ps.sps->ctb_width+(i/32)] = 1;

                                s->offset_weight_chroma[j* widthEL_v[1] + i]= (x < 0 ? -x: 0) + (y < 0 ? -y: 0)*offset_x;

                            } else {
                                s->offset_bl_chroma[j* widthEL_v[1] + i] = -1;
                                s->no_margin_360_chroma[(j/32)*s->ps.sps->ctb_width+(i/32)] = 1;
                            }
                        } else {
                            s->offset_bl_chroma[j* widthEL_v[1] + i] = -1;
                            s->no_margin_360_chroma[(j/32)*s->ps.sps->ctb_width+(i/32)] = 1;
                        }
                    }
                }

            }
        }
    }
#endif
