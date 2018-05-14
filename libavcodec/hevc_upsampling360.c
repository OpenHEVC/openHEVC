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


static void init_rotation_matrix(double *rot, double *inv_rot, double theta, double phi){
    double cos_tht = cos(theta);
    double cos_phi = cos(phi);
    double sin_tht = sin(theta);
    double sin_phi = sin(phi);

    rot[0] = cos_tht;
    rot[1] = 0.0;
    rot[2] = -sin_tht;
    rot[3] = sin_tht * sin_phi;
    rot[4] = cos_phi;
    rot[5] = cos_tht * sin_phi;
    rot[6] = sin_tht * cos_phi;
    rot[7] = -sin_phi;
    rot[8] = cos_tht * cos_phi;

    inv_rot[0] = cos_tht;
    inv_rot[1] = sin_tht * sin_phi;
    inv_rot[2] = sin_tht * cos_phi;
    inv_rot[3] = 0.0;
    inv_rot[4] = cos_phi;
    inv_rot[5] = -sin_phi;
    inv_rot[6] = -sin_tht;
    inv_rot[7] = cos_tht * sin_phi;
    inv_rot[8] = cos_tht * cos_phi;
}


static void derive_safe_window(int bl_height, int bl_width, int el_width, int el_height, double tan_fovx, double tan_fov_y, double *inv_rot, int *left,int *right, int *top ,int *bottom, int safety_margin){

    int min_x = el_width;
    int max_x = 0;
    int min_y = el_height;
    int max_y = 0;

    const double tan_x_on_width_bl  = tan_fovx / bl_width;
    const double tan_y_on_height_bl = tan_fovx / bl_height;

    for (int j = - bl_height + 1; j < bl_height; j+=2){
        const double y0 = - tan_y_on_height_bl * ( j );
        const double square_y0_plus1 = y0 * y0 + 1;

        const double add_x = inv_rot[1] * y0 + inv_rot[2];
        const double add_y = inv_rot[4] * y0 + inv_rot[5];
        const double add_z = inv_rot[7] * y0 + inv_rot[8];

        for (int i = - bl_width + 1; i < bl_width; i+=2) {
            double x, y, z,  x2, y2, norm;

            x2 = tan_x_on_width_bl  * ( i );

            norm = sqrt(x2 * x2 + square_y0_plus1);

            x = (inv_rot[0] * x2 + add_x) / norm;
            y = (                  add_y) / norm;
            z = (inv_rot[6] * x2 + add_z) / norm;

            norm = sqrt(x*x + y*y + z*z);

            x2 = (double)((M_PI - atan2(z, x))/(2 * M_PI) * el_width) - 0.5;
            y2 = (double)((norm < (1e-6) ? 0.0 : acos(y / norm) / M_PI) * el_height) - 0.5;

            min_x = FFMIN((int)x2, min_x);
            max_x = FFMAX((int)x2, max_x);
            min_y = FFMIN((int)y2, min_y);
            max_y = FFMAX((int)y2, max_y);
        }
    }

    *left     = min_x - safety_margin;
    *right    = max_x + safety_margin;
    *top      = min_y - safety_margin;
    *bottom   = max_y + safety_margin;
}

static void init_lanczos_weight(HEVCContext *s){
    const double dScale = 1.0 / S_LANCZOS_LUT_SCALE;
    const int    mul = 1 << (S_INTERPOLATE_PrecisionBD);

    { //LUMA
    double m_pfLanczosFltCoefLut [(SHVC360_LANCZOS_PARAM_LUMA << 1) * S_LANCZOS_LUT_SCALE + 1];//TODO check +1
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
    }
    {//CHROMA
        double m_pfLanczosFltCoefLut [(SHVC360_LANCZOS_PARAM_CHROMA << 1) * S_LANCZOS_LUT_SCALE + 1];//TODO check +1
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
    }
}


static void derive_360_lut(int el_height, int el_width, int bl_width, int bl_height,
                           int width_in_ctb,int log2ctb_size,  double tan_x, double tan_y,
                           int offset_x, int offset_y,
                           int *iWindowSearchDimluma, double * rot_mat,
                           uint8_t *end_y, uint8_t *end_x, int *offset_bl, int *offset_weight,
                           int16_t *weight_idx, uint8_t *margin_360){
    const int left_offset = (offset_x - 1) >> 1;
    const int top_offset  = (offset_y - 1) >> 1;

    const int ctb_witdh = 1 << log2ctb_size;

    const double width_bl_on_two  = (double)bl_width / 2.0;
    const double height_bl_on_two = (double)bl_height / 2.0;

    memset(offset_bl,-1,sizeof(int)*el_width*el_height);
    //memset(margin_360,1,sizeof(uint8_t)*width_in_ctb*(el_height/ctb_witdh));

    for (int j = 0; j < el_height; j++){
        for (int i = 0; i < el_width; i++) {
            const double pitch = (double) (M_PI_2)*(1.0 - ((double)(j << 1) + 1.0)/el_height);
            const double cos_pitch = cos(pitch);
            const double sin_pitch = sin(pitch);

            const int el_pos = j*el_width + i;

            if(i > iWindowSearchDimluma[0]  && i < iWindowSearchDimluma[1]
                    && j > iWindowSearchDimluma[2]  && j < iWindowSearchDimluma[3] ){
                int  x_lanczos, y_lanczos;

                double yaw = M_PI * ((double)((i << 1) + 1) / el_width - 1.0);

                double x =   cos_pitch * cos(yaw);
                double z = - cos_pitch * sin(yaw);

                double x2 = rot_mat[0] * x                          + rot_mat[2] * z;
                double y2 = rot_mat[3] * x + rot_mat[4] * sin_pitch + rot_mat[5] * z;
                double z2 = rot_mat[6] * x + rot_mat[7] * sin_pitch + rot_mat[8] * z;

                x2 = x2 / z2;
                y2 = y2 / z2;

                x =  width_bl_on_two   * ( x2 / tan_x  + 1.0) - 0.5;
                z =  height_bl_on_two  * (-y2 / tan_y  + 1.0) - 0.5;

                x_lanczos = roundHP(x * SVIDEO_2DPOS_PRECISION) >> SVIDEO_2DPOS_PRECISION_LOG2;
                y_lanczos = roundHP(z * SVIDEO_2DPOS_PRECISION) >> SVIDEO_2DPOS_PRECISION_LOG2;

                if (x_lanczos >= 0 && x_lanczos < bl_width && y_lanczos >= 0 && y_lanczos < bl_height){
                    int _y = y_lanczos - top_offset;
                    int _x = x_lanczos - left_offset;

                    end_y[el_pos] = (_y < 0) ? offset_y + _y : ((offset_y + _y) < bl_height ? offset_y : bl_height - _y );
                    end_x[el_pos] = (_x < 0) ? offset_x + _x : ((offset_x + _x) < bl_width  ? offset_x : bl_width  - _x );

                    offset_weight[el_pos] = (_x < 0 ? -_x : 0) + (_y < 0 ? -_y: 0) * offset_x;

                    weight_idx[el_pos] = round( (z - (double)y_lanczos) * S_LANCZOS_LUT_SCALE) * (S_LANCZOS_LUT_SCALE + 1) + round( (x - (double)x_lanczos) * S_LANCZOS_LUT_SCALE );
                    offset_bl [el_pos] = (_y < 0 ? 0 : _y) * (bl_width) + (_x < 0 ? 0 : _x);
                    if(_y < 0 || _x < 0 || end_y[el_pos] != offset_y || end_x[el_pos] != offset_x)
                        margin_360[(j/ctb_witdh)*width_in_ctb+(i/ctb_witdh)]=1;
//                    else
//                       margin_360[(j/ctb_witdh)*width_in_ctb+(i/ctb_witdh)]=0;
                } else {
                    margin_360[(j/ctb_witdh)*width_in_ctb+(i/ctb_witdh)]=1;
                }
            } else {
                margin_360[(j/ctb_witdh)*width_in_ctb+(i/ctb_witdh)]=1;
            }
        }
    }
}

void ff_hevc_init_360_params(HEVCContext *s, float fYaw, float fPitch, float h_fov, float v_fov){
    const int safe_margin_size  = 5;
    int iWindowSearchDimluma[4];

    const double tht  = (double)(( fYaw + 90) * M_PI / 180);
    const double phi  = (double)((-fPitch)    * M_PI / 180);
    const double fovx = (double)(M_PI * h_fov / 180.0);
    const double fovy = (double)(M_PI * v_fov / 180.0);

    const double tan_x = tan(fovx / 2.0);
    const double tan_y = tan(fovy / 2.0);

    //TODO adapt to real SHVC parameters with cropping info
    RepFormat current_layer_window = s->ps.vps->vps_ext.rep_format[s->ps.vps->vps_ext.vps_rep_format_idx[s->nuh_layer_id]];

    const int widthEL_v[2]  = {current_layer_window.pic_width_vps_in_luma_samples  - current_layer_window.conf_win_vps_left_offset   - current_layer_window.conf_win_vps_right_offset,   current_layer_window.pic_width_vps_in_luma_samples  - current_layer_window.conf_win_vps_left_offset   - current_layer_window.conf_win_vps_right_offset>>1};
    const int heightEL_v[2] = {current_layer_window.pic_height_vps_in_luma_samples - current_layer_window.conf_win_vps_bottom_offset - current_layer_window.conf_win_vps_top_offset,current_layer_window.pic_height_vps_in_luma_samples - current_layer_window.conf_win_vps_bottom_offset - current_layer_window.conf_win_vps_top_offset>>1};

    RepFormat base_layer_window = s->ps.vps->vps_ext.rep_format[s->ps.vps->vps_ext.vps_rep_format_idx[s->nuh_layer_id-1]];

    const int widthBL_v[2] = {base_layer_window.pic_width_vps_in_luma_samples  - base_layer_window.conf_win_vps_left_offset   - base_layer_window.conf_win_vps_right_offset,  s->BL_width  >> 1};
    const int heightBL_v[2] ={base_layer_window.pic_height_vps_in_luma_samples - base_layer_window.conf_win_vps_bottom_offset - base_layer_window.conf_win_vps_top_offset, s->BL_height >> 1};

    double rotation_matrix[9];
    double inverse_rotation_matrix[9];

    if ( !s->weight_lut_luma || !s->weight_lut_chroma){
        s->offset_bl_luma       = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(int) );
        s->weight_idx_luma      = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(int16_t) );
        s->weight_lut_luma      = av_malloc ( sizeof(int16_t*) * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1)));
        s->weight_lut_luma[0]   = av_malloc ( sizeof(int16_t) * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1) * SHVC360_FILTER_SIZE_LUMA));

        s->offset_weight_luma   = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(int) );
        s->end_x_luma           = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(uint8_t) );
        s->end_y_luma           = av_malloc(widthEL_v[0]*heightEL_v[0]*sizeof(uint8_t) );

        s->no_margin_360_luma   = av_mallocz(sizeof(uint8_t) * s->ps.sps->ctb_width*s->ps.sps->ctb_height );

        s->offset_bl_chroma     = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(int) );
        s->weight_idx_chroma    = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(int16_t) );
        s->weight_lut_chroma    = av_malloc ( sizeof(int16_t*) * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1)));
        s->weight_lut_chroma[0] = av_malloc ( sizeof(int16_t)  * ((S_LANCZOS_LUT_SCALE + 1) * (S_LANCZOS_LUT_SCALE + 1) * SHVC360_FILTER_SIZE_CHROMA));

        s->offset_weight_chroma = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(int) );
        s->end_x_chroma         = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(uint8_t) );
        s->end_y_chroma         = av_malloc(widthEL_v[1]*heightEL_v[1]*sizeof(uint8_t) );

        s->no_margin_360_chroma   = av_mallocz(sizeof(uint8_t) * s->ps.sps->ctb_width*s->ps.sps->ctb_height );

        //            if (!s->pixel_weight_chroma|| !s->pixel_weight_luma|| !s->weight_lut_luma[0] || !s->weight_lut_chroma[0])
        //              goto fail;
        //TODO safer alloc check

        // Init lanczos weight and rotation matrices
        init_lanczos_weight(s);
        init_rotation_matrix(rotation_matrix,inverse_rotation_matrix,tht,phi);

        derive_safe_window(heightBL_v[0], widthBL_v[0], widthEL_v[0], heightEL_v[0],
                tan_x, tan_y, inverse_rotation_matrix,
                &iWindowSearchDimluma[0],&iWindowSearchDimluma[1],&iWindowSearchDimluma[2],&iWindowSearchDimluma[3],
                safe_margin_size);

        derive_360_lut(heightEL_v[0], widthEL_v[0], widthBL_v[0], heightBL_v[0],
                s->ps.sps->ctb_width, s->ps.sps->log2_ctb_size, tan_x, tan_y,
                SHVC360_LANCZOS_PARAM_LUMA << 1, SHVC360_LANCZOS_PARAM_LUMA << 1, iWindowSearchDimluma, rotation_matrix,
                s->end_y_luma, s->end_x_luma, s->offset_bl_luma,
                s->offset_weight_luma, s->weight_idx_luma,s->no_margin_360_luma);


        derive_safe_window(heightBL_v[1], widthBL_v[1], widthEL_v[1], heightEL_v[1],
                tan_x, tan_y, inverse_rotation_matrix,
                &iWindowSearchDimluma[0],&iWindowSearchDimluma[1],&iWindowSearchDimluma[2],&iWindowSearchDimluma[3],
                safe_margin_size);

        derive_360_lut(heightEL_v[1], widthEL_v[1], widthBL_v[1], heightBL_v[1],
                s->ps.sps->ctb_width , s->ps.sps->log2_ctb_size-1, tan_x, tan_y,
                SHVC360_LANCZOS_PARAM_CHROMA << 1, SHVC360_LANCZOS_PARAM_CHROMA << 1, iWindowSearchDimluma, rotation_matrix,
                s->end_y_chroma, s->end_x_chroma, s->offset_bl_chroma,
                s->offset_weight_chroma, s->weight_idx_chroma,s->no_margin_360_chroma);

    }
}
#endif
