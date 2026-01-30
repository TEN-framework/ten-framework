#include "dl_fft_base.h"

static inline int16_t dl_xtfixed_bf_1(
    int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift, int add_rount_mult)
{
    int result = a0;
    result = result << 15;
    result -= (int32_t)a1 * (int32_t)a2 + (int32_t)a3 * (int32_t)a4;
    result += add_rount_mult;
    result = result >> result_shift;

    return (int16_t)result;
}

static inline int16_t dl_xtfixed_bf_2(
    int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift, int add_rount_mult)
{
    int result = a0;
    result = result << 15;
    result -= ((int32_t)a1 * (int32_t)a2 - (int32_t)a3 * (int32_t)a4);
    result += add_rount_mult;
    result = result >> result_shift;

    return (int16_t)result;
}

static inline int16_t dl_xtfixed_bf_3(
    int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift, int add_rount_mult)
{
    int result = a0;
    result = result << 15;
    result += (int32_t)a1 * (int32_t)a2 + (int32_t)a3 * (int32_t)a4;
    result += add_rount_mult;
    result = result >> result_shift;

    return (int16_t)result;
}

static inline int16_t dl_xtfixed_bf_4(
    int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int result_shift, int add_rount_mult)
{
    int result = a0;
    result = result << 15;
    result += (int32_t)a1 * (int32_t)a2 - (int32_t)a3 * (int32_t)a4;
    result += add_rount_mult;
    result = result >> result_shift;

    return (int16_t)result;
}

esp_err_t dl_fft2r_sc16_ansi(int16_t *data, int N, int16_t *table)
{
    esp_err_t result = ESP_OK;

    uint32_t *w = (uint32_t *)table;
    uint32_t *in_data = (uint32_t *)data;

    int ie, ia, m;
    dl_sc16_t cs; // c - re, s - im
    dl_sc16_t m_data;
    dl_sc16_t a_data;
    int add_rount_mult = 1 << 15;

    ie = 1;
    for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
        ia = 0;
        for (int j = 0; j < ie; j++) {
            cs.data = w[j];
            // c = w[2 * j];
            // s = w[2 * j + 1];
            for (int i = 0; i < N2; i++) {
                m = ia + N2;
                m_data.data = in_data[m];
                a_data.data = in_data[ia];
                // data[2 * m] = data[2 * ia] - re_temp;
                // data[2 * m + 1] = data[2 * ia + 1] - im_temp;
                dl_sc16_t m1;
                m1.re = dl_xtfixed_bf_1(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        16,
                                        add_rount_mult); //(a_data.re - temp.re + shift_const) >> 1;
                m1.im = dl_xtfixed_bf_2(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        16,
                                        add_rount_mult); //(a_data.im - temp.im + shift_const) >> 1;
                in_data[m] = m1.data;

                // data[2 * ia] = data[2 * ia] + re_temp;
                // data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
                dl_sc16_t m2;
                m2.re = dl_xtfixed_bf_3(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        16,
                                        add_rount_mult); //(a_data.re + temp.re + shift_const) >> 1;
                m2.im = dl_xtfixed_bf_4(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        16,
                                        add_rount_mult); //(a_data.im + temp.im + shift_const)>>1;
                in_data[ia] = m2.data;
                ia++;
            }
            ia += N2;
        }
        ie <<= 1;
    }
    return result;
}

esp_err_t dl_ifft2r_sc16_ansi(int16_t *data, int N, int16_t *table)
{
    esp_err_t result = ESP_OK;

    uint32_t *w = (uint32_t *)table;
    uint32_t *in_data = (uint32_t *)data;

    int ie, ia, m;
    dl_sc16_t cs; // c - re, s - im
    dl_sc16_t m_data;
    dl_sc16_t a_data;
    int add_rount_mult = 1 << 15;

    ie = 1;
    for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
        ia = 0;
        for (int j = 0; j < ie; j++) {
            cs.data = w[j];
            cs.im = -cs.im;
            // c = w[2 * j];
            // s = w[2 * j + 1];
            for (int i = 0; i < N2; i++) {
                m = ia + N2;
                m_data.data = in_data[m];
                a_data.data = in_data[ia];
                // data[2 * m] = data[2 * ia] - re_temp;
                // data[2 * m + 1] = data[2 * ia + 1] - im_temp;
                dl_sc16_t m1;
                m1.re = dl_xtfixed_bf_1(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        16,
                                        add_rount_mult); //(a_data.re - temp.re + shift_const) >> 1;
                m1.im = dl_xtfixed_bf_2(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        16,
                                        add_rount_mult); //(a_data.im - temp.im + shift_const) >> 1;
                in_data[m] = m1.data;

                // data[2 * ia] = data[2 * ia] + re_temp;
                // data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
                dl_sc16_t m2;
                m2.re = dl_xtfixed_bf_3(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        16,
                                        add_rount_mult); //(a_data.re + temp.re + shift_const) >> 1;
                m2.im = dl_xtfixed_bf_4(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        16,
                                        add_rount_mult); //(a_data.im + temp.im + shift_const)>>1;
                in_data[ia] = m2.data;
                ia++;
            }
            ia += N2;
        }
        ie <<= 1;
    }
    return result;
}

esp_err_t dl_fft2r_sc16_hp_ansi(int16_t *data, int N, int16_t *table, int *shift)
{
    esp_err_t result = ESP_OK;

    uint32_t *w = (uint32_t *)table;
    uint32_t *in_data = (uint32_t *)data;

    int ie, ia, m, loop_num = 2;
    dl_sc16_t cs; // c - re, s - im
    dl_sc16_t m_data;
    dl_sc16_t a_data;
    int add_rount_mult = 1 << 15;

    ie = 1;
    shift[0] = 0;
    for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
        ia = 0;
        int loop_shift = 16;
        if (loop_num == 2) {
            loop_shift = dl_array_max_q_s16(data, N * 2);
            if (loop_shift < 16) {
                loop_shift += 1;
            }
            loop_num = 0;
        } else {
            loop_num += 1;
        }
        shift[0] += loop_shift - 15;
        add_rount_mult = 1 << (loop_shift - 1);
        for (int j = 0; j < ie; j++) {
            cs.data = w[j];
            // c = w[2 * j];
            // s = w[2 * j + 1];
            for (int i = 0; i < N2; i++) {
                m = ia + N2;
                m_data.data = in_data[m];
                a_data.data = in_data[ia];
                // data[2 * m] = data[2 * ia] - re_temp;
                // data[2 * m + 1] = data[2 * ia + 1] - im_temp;
                dl_sc16_t m1;
                m1.re = dl_xtfixed_bf_1(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        loop_shift,
                                        add_rount_mult); //(a_data.re - temp.re + shift_const) >> 1;
                m1.im = dl_xtfixed_bf_2(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        loop_shift,
                                        add_rount_mult); //(a_data.im - temp.im + shift_const) >> 1;
                in_data[m] = m1.data;

                // data[2 * ia] = data[2 * ia] + re_temp;
                // data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
                dl_sc16_t m2;
                m2.re = dl_xtfixed_bf_3(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        loop_shift,
                                        add_rount_mult); //(a_data.re + temp.re + shift_const) >> 1;
                m2.im = dl_xtfixed_bf_4(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        loop_shift,
                                        add_rount_mult); //(a_data.im + temp.im + shift_const)>>1;
                in_data[ia] = m2.data;
                ia++;
            }
            ia += N2;
        }
        ie <<= 1;
    }
    return result;
}

esp_err_t dl_ifft2r_sc16_hp_ansi(int16_t *data, int N, int16_t *table, int *shift)
{
    esp_err_t result = ESP_OK;

    uint32_t *w = (uint32_t *)table;
    uint32_t *in_data = (uint32_t *)data;

    int ie, ia, m, loop_num = 2;
    dl_sc16_t cs; // c - re, s - im
    dl_sc16_t m_data;
    dl_sc16_t a_data;
    int add_rount_mult = 1 << 15;

    ie = 1;
    shift[0] = 0;
    for (int N2 = N / 2; N2 > 0; N2 >>= 1) {
        ia = 0;
        int loop_shift = 16;
        if (loop_num == 2) {
            loop_shift = dl_array_max_q_s16(data, N * 2);
            if (loop_shift < 16) {
                loop_shift += 1;
            }
            loop_num = 0;
        } else {
            loop_num += 1;
        }
        shift[0] += loop_shift - 15;
        add_rount_mult = 1 << (loop_shift - 1);
        for (int j = 0; j < ie; j++) {
            cs.data = w[j];
            cs.im = -cs.im;
            // c = w[2 * j];
            // s = w[2 * j + 1];
            for (int i = 0; i < N2; i++) {
                m = ia + N2;
                m_data.data = in_data[m];
                a_data.data = in_data[ia];
                // data[2 * m] = data[2 * ia] - re_temp;
                // data[2 * m + 1] = data[2 * ia + 1] - im_temp;
                dl_sc16_t m1;
                m1.re = dl_xtfixed_bf_1(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        loop_shift,
                                        add_rount_mult); //(a_data.re - temp.re + shift_const) >> 1;
                m1.im = dl_xtfixed_bf_2(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        loop_shift,
                                        add_rount_mult); //(a_data.im - temp.im + shift_const) >> 1;
                in_data[m] = m1.data;

                // data[2 * ia] = data[2 * ia] + re_temp;
                // data[2 * ia + 1] = data[2 * ia + 1] + im_temp;
                dl_sc16_t m2;
                m2.re = dl_xtfixed_bf_3(a_data.re,
                                        cs.re,
                                        m_data.re,
                                        cs.im,
                                        m_data.im,
                                        loop_shift,
                                        add_rount_mult); //(a_data.re + temp.re + shift_const) >> 1;
                m2.im = dl_xtfixed_bf_4(a_data.im,
                                        cs.re,
                                        m_data.im,
                                        cs.im,
                                        m_data.re,
                                        loop_shift,
                                        add_rount_mult); //(a_data.im + temp.im + shift_const)>>1;
                in_data[ia] = m2.data;
                ia++;
            }
            ia += N2;
        }
        ie <<= 1;
    }
    return result;
}

static inline unsigned short reverse_sc16(unsigned short x, unsigned short N, int order)
{
    unsigned short b = x;

    b = (b & 0xff00) >> 8 | (b & 0x00fF) << 8;
    b = (b & 0xf0F0) >> 4 | (b & 0x0f0F) << 4;
    b = (b & 0xCCCC) >> 2 | (b & 0x3333) << 2;
    b = (b & 0xAAAA) >> 1 | (b & 0x5555) << 1;
    return b >> (16 - order);
}

esp_err_t dl_bitrev2r_sc16_ansi(int16_t *data, int N)
{
    esp_err_t result = ESP_OK;

    int j, k;
    uint32_t temp;
    uint32_t *in_data = (uint32_t *)data;
    j = 0;
    for (int i = 1; i < (N - 1); i++) {
        k = N >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
        if (i < j) {
            temp = in_data[j];
            in_data[j] = in_data[i];
            in_data[i] = temp;
        }
    }
    return result;
}

esp_err_t dl_cplx2reC_sc16(int16_t *data, int N)
{
    esp_err_t result = ESP_OK;

    int i;
    int n2 = N << (1); // we will operate with int32 indexes
    uint32_t *in_data = (uint32_t *)data;

    dl_sc16_t kl;
    dl_sc16_t kh;
    dl_sc16_t nl;
    dl_sc16_t nh;

    for (i = 0; i < (N / 4); i++) {
        kl.data = in_data[i + 1];
        nl.data = in_data[N - i - 1];
        kh.data = in_data[i + 1 + N / 2];
        nh.data = in_data[N - i - 1 - N / 2];

        data[i * 2 + 0 + 2] = kl.re + nl.re;
        data[i * 2 + 1 + 2] = kl.im - nl.im;

        data[n2 - i * 2 - 1 - N] = kh.re + nh.re;
        data[n2 - i * 2 - 2 - N] = kh.im - nh.im;

        data[i * 2 + 0 + 2 + N] = kl.im + nl.im;
        data[i * 2 + 1 + 2 + N] = kl.re - nl.re;

        data[n2 - i * 2 - 1] = kh.im + nh.im;
        data[n2 - i * 2 - 2] = kh.re - nh.re;
    }
    data[N] = data[1];
    data[1] = 0;
    data[N + 1] = 0;

    return result;
}

esp_err_t dl_rfft_post_proc_sc16_ansi(int16_t *data, int N, int16_t *table)
{
    dl_sc16_t *result = (dl_sc16_t *)data;
    // Original formula...
    // result[0].re = result[0].re + result[0].im;
    // result[N].re = result[0].re - result[0].im;
    // result[0].im = 0;
    // result[N].im = 0;
    // Optimized one:
    int32_t tmp_re = result[0].re + 1;
    result[0].re = (tmp_re + result[0].im) >> 1;
    result[0].im = (tmp_re - result[0].im) >> 1;
    int round = 1 << 16;

    int32_t f1k_re, f1k_im, f2k_re, f2k_im, tw_re, tw_im;
    for (int k = 1; k <= N / 2; k++) {
        dl_sc16_t fpk = result[k];
        dl_sc16_t fpnk = result[N - k];
        f1k_re = fpk.re + fpnk.re;
        f1k_im = fpk.im - fpnk.im;
        f2k_re = fpk.re - fpnk.re;
        f2k_im = fpk.im + fpnk.im;

        int16_t c = -table[k * 2 - 1];
        int16_t s = -table[k * 2 - 2];
        tw_re = c * f2k_re - s * f2k_im;
        tw_im = s * f2k_re + c * f2k_im;
        f1k_re = f1k_re << 15;
        f1k_im = f1k_im << 15;

        result[k].re = (f1k_re + tw_re + round) >> 17;
        result[k].im = (f1k_im + tw_im + round) >> 17;
        result[N - k].re = (f1k_re - tw_re + round) >> 17;
        result[N - k].im = (tw_im - f1k_im + round) >> 17;
    }
    return ESP_OK;
}

esp_err_t dl_rfft_pre_proc_sc16_ansi(int16_t *data, int N, int16_t *table)
{
    dl_sc16_t *result = (dl_sc16_t *)data;

    int32_t tmp_re = result[0].re + 2;
    result[0].re = (tmp_re + result[0].im) >> 2;
    result[0].im = (tmp_re - result[0].im) >> 2;
    int round = 1 << 16;

    int32_t f1k_re, f1k_im, f2k_re, f2k_im, tw_re, tw_im;
    for (int k = 1; k <= N / 2; k++) {
        dl_sc16_t fpk = result[k];
        dl_sc16_t fpnk = result[N - k];
        f1k_re = fpk.re + fpnk.re;
        f1k_im = fpk.im - fpnk.im;
        f2k_re = fpk.re - fpnk.re;
        f2k_im = fpk.im + fpnk.im;

        int16_t c = -table[k * 2 - 1];
        int16_t s = table[k * 2 - 2];
        tw_re = c * f2k_re - s * f2k_im;
        tw_im = s * f2k_re + c * f2k_im;
        f1k_re = f1k_re << 15;
        f1k_im = f1k_im << 15;

        result[k].re = (f1k_re + tw_re + round) >> 17;
        result[k].im = (f1k_im + tw_im + round) >> 17;
        result[N - k].re = (f1k_re - tw_re + round) >> 17;
        result[N - k].im = (tw_im - f1k_im + round) >> 17;
    }
    return ESP_OK;
}

esp_err_t dl_cplx2real_sc16_hp_ansi(int16_t *data, int N, int16_t *table, int *shift)
{
    dl_sc16_t *result = (dl_sc16_t *)data;
    // Original formula...
    // result[0].re = result[0].re + result[0].im;
    // result[N].re = result[0].re - result[0].im;
    // result[0].im = 0;
    // result[N].im = 0;
    // Optimized one:
    int loop_shift = dl_array_max_q_s16(data, N);
    int round = 1 << loop_shift;
    int32_t tmp_re = result[0].re;
    shift[0] += loop_shift - 15;

    if (loop_shift >= 15) {
        result[0].re = (tmp_re + result[0].im) >> (loop_shift - 15);
        result[0].im = (tmp_re - result[0].im) >> (loop_shift - 15);
    } else {
        result[0].re = (tmp_re + result[0].im) << (15 - loop_shift);
        result[0].im = (tmp_re - result[0].im) << (15 - loop_shift);
    }

    int32_t f1k_re, f1k_im, f2k_re, f2k_im, tw_re, tw_im;
    loop_shift += 1;
    for (int k = 1; k <= N / 2; k++) {
        dl_sc16_t fpk = result[k];
        dl_sc16_t fpnk = result[N - k];
        f1k_re = fpk.re + fpnk.re;
        f1k_im = fpk.im - fpnk.im;
        f2k_re = fpk.re - fpnk.re;
        f2k_im = fpk.im + fpnk.im;

        int16_t c = -table[k * 2 - 1];
        int16_t s = -table[k * 2 - 2];
        tw_re = c * f2k_re - s * f2k_im;
        tw_im = s * f2k_re + c * f2k_im;
        f1k_re = f1k_re << 15;
        f1k_im = f1k_im << 15;

        result[k].re = (f1k_re + tw_re + round) >> loop_shift;
        result[k].im = (f1k_im + tw_im + round) >> loop_shift;
        result[N - k].re = (f1k_re - tw_re + round) >> loop_shift;
        result[N - k].im = (tw_im - f1k_im + round) >> loop_shift;
    }
    return ESP_OK;
}

int16_t *dl_gen_fft_table_sc16(int fft_point, uint32_t caps)
{
    int16_t *fft_table = (int16_t *)heap_caps_aligned_alloc(16, fft_point * sizeof(int16_t), caps);

    if (fft_table) {
        float e = M_PI * 2.0 / fft_point;
        for (int i = 0; i < (fft_point >> 1); i++) {
            fft_table[2 * i] = (int16_t)roundf(INT16_MAX * cosf(i * e));
            fft_table[2 * i + 1] = (int16_t)roundf(INT16_MAX * sinf(i * e));
        }
        dl_bitrev2r_sc16_ansi(fft_table, fft_point >> 1);
    }

    return fft_table;
}

int16_t *dl_gen_rfft_table_s16(int fft_point, uint32_t caps)
{
    int16_t *fft_table = (int16_t *)heap_caps_aligned_alloc(16, fft_point * sizeof(int16_t), caps);

    if (fft_table) {
        float e = M_PI * 2.0 / fft_point;

        for (int i = 0; i < (fft_point >> 1); i++) {
            fft_table[2 * i] = (int16_t)roundf(INT16_MAX * cosf((i + 1) * e));
            fft_table[2 * i + 1] = (int16_t)roundf(INT16_MAX * sinf((i + 1) * e));
        }
    }

    return fft_table;
}
