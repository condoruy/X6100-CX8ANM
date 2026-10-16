#include "controls.h"
#include "knobs.h"

#include "cfg/settings_manager.h"
#include "util.hpp"
#include "util.h"
#include "cw.h"


extern "C" {
#include "msg.h"
#include "cluster_column.h"
#include "rtty.h"
}

template <typename T>
static inline bool toggle_param(Parameter<T> &param);

template <typename T>
static T loop_items(std::vector<T> items, T cur, bool next);


template <typename T, typename U, int N>
static T update_param(Parameter<T, U, N> &param, int32_t diff, T step=1) {
    T val = param.get();
    val = align(val + diff * step, step);
    param.set(val);
    // Read again
    return param.get();
}

template <typename T>
static T update_param(ComputedParameter<T> &param, int32_t diff, T step=1) {
    T val = param.get();
    if (!diff) {
        return val;
    }
    val = align(val + diff * step, step);
    param.set(val);
    // Read again
    return param.get();
}


void controls_toggle_key_train(button_data_t *data) {
    toggle_param(cfg_sm.p_key_train);
}

void controls_toggle_key_iambic_mode(button_data_t *data) {
    x6100_iambic_mode_t new_mode = cfg_sm.p_iambic_mode.get() == x6100_iambic_a ? x6100_iambic_b : x6100_iambic_a;
    cfg_sm.p_iambic_mode.set( new_mode);
}

void controls_toggle_cw_decoder(button_data_t *data) {
    toggle_param(cfg_sm.p_cw_decoder);
}

void controls_toggle_cw_tuner(button_data_t *data) {
    toggle_param(cfg_sm.p_cw_tune);
}

void controls_toggle_cw_peak(button_data_t *data) {
    toggle_param(cfg_sm.p_cw_peak_on);
}

void controls_toggle_dnf(button_data_t *data) {
    toggle_param(cfg_sm.p_dnf);
}

void controls_toggle_dnf_auto(button_data_t *data) {
    toggle_param(cfg_sm.p_dnf_auto);
}

void controls_toggle_nb(button_data_t *data) {
    toggle_param(cfg_sm.p_nb);
}

void controls_toggle_nr(button_data_t *data) {
    toggle_param(cfg_sm.p_nr);
}

void controls_toggle_vox(button_data_t *data) {
    toggle_param(cfg_sm.p_vox_en);
}

void controls_toggle_wf_mode(button_data_t *data) {
    toggle_param(cfg_sm.p_wf_mode);
}

/* Span +/- buttons: wider span = lower zoom (1/2/4/8) */
void controls_span_wider(button_data_t *data) {
    cfg_sm.p_mode_zoom.set(cfg_sm.p_mode_zoom.get() >> 1);
}

void controls_span_narrower(button_data_t *data) {
    cfg_sm.p_mode_zoom.set(cfg_sm.p_mode_zoom.get() << 1);
}

void controls_cw_zap(button_data_t *data) {
    x6100_mode_t mode = (x6100_mode_t)(cfg_sm.cp_cur_mode.get());
    if ((mode != x6100_mode_cw) && (mode != x6100_mode_cwr)) {
        return;
    }
    float tone_freq = cw_get_tone_freq();
    int32_t key_tone = cfg_sm.p_key_tone.get();
    int32_t freq = cfg_sm.cp_fg_freq.get();
    LV_LOG_USER("tone_freq: %f, key_tone: %i", tone_freq, key_tone);
    if (mode == x6100_mode_cw) {
        freq += tone_freq - key_tone;
    } else {
        freq -= tone_freq - key_tone;
    }
    freq = (freq + 5) / 10 * 10;
    cfg_sm.cp_fg_freq.set(freq);
}

void controls_encoder_update(cfg_ctrl_t ctrl, int32_t diff, std::string &msg) {
    int32_t     i;
    float       f;
    char        *s;
    bool        b;
    int32_t     new_msg_len;

    switch (ctrl) {
        case CTRL_VOL:
            i = radio_change_vol(diff);
            snprintf(msg.data(), msg.capacity(), tr("Volume: %i"), i);
            break;

        case CTRL_RFG:
            i = update_param(cfg_sm.p_rfgain, diff);
            snprintf(msg.data(), msg.capacity(), tr("RF gain: %i"), i);
            break;

        case CTRL_WEFAX_ALIGN:
            i = update_param(cfg_sm.p_wefax_align, diff);
            snprintf(msg.data(), msg.capacity(), tr("WEFAX Align: %i"), i);
            break;

        case CTRL_WEFAX_TILT:
            i = update_param(cfg_sm.p_wefax_tilt, diff);
            snprintf(msg.data(), msg.capacity(), tr("WEFAX Tilt: %i"), i);
            break;

        case CTRL_SQL:
            i = update_param(cfg_sm.p_squelch, diff);
            snprintf(msg.data(), msg.capacity(), tr("Voice SQL: %i"), i);
            break;

        case CTRL_FILTER_LOW:
            // TODO: make step depending on freq
            i = update_param(cfg_sm.cp_cur_filter_low, diff, 10);
            snprintf(msg.data(), msg.capacity(), tr("Filter low: %i Hz"), i);
            break;

        case CTRL_FILTER_HIGH:
            i = cfg_sm.cp_cur_filter_high.get();
            if (diff) {
                int32_t freq_step;
                switch (cfg_sm.cp_cur_mode.get()) {
                case x6100_mode_cw:
                case x6100_mode_cwr:
                    freq_step = 10;
                    break;
                default:
                    freq_step = 50;
                    break;
                }
                i = update_param(cfg_sm.cp_cur_filter_high, diff, freq_step);
            }

            snprintf(msg.data(), msg.capacity(), tr("Filter high: %i Hz"), i);
            break;

        case CTRL_FILTER_BW:
            i = update_param(cfg_sm.cp_cur_filter_bw, diff, 20);
            snprintf(msg.data(), msg.capacity(), tr("Filter bw: %i Hz"), i);
            break;

        case CTRL_PWR:
            f = update_param(cfg_sm.p_pwr, diff, 0.1f);
            snprintf(msg.data(), msg.capacity(), tr("Power: %0.1f W"), f);
            break;

        case CTRL_MONI:
            i = update_param(cfg_sm.p_moni, diff);
            snprintf(msg.data(), msg.capacity(), tr("Moni level: %i"), i);
            break;

        case CTRL_SPECTRUM_FACTOR:
            i = cfg_sm.p_mode_zoom.get();
            if (diff != 0) {
                if (diff > 0) {
                    i <<= diff;
                } else {
                    i >>= -diff;
                }
                cfg_sm.p_mode_zoom.set(i);
                i = cfg_sm.p_mode_zoom.get();
            }
            snprintf(msg.data(), msg.capacity(), tr("Zoom: x%i"), i);
            break;

        case CTRL_COMP:
            i = update_param(cfg_sm.p_comp, diff);
            snprintf(msg.data(), msg.capacity(), tr("Compressor ratio: %s"), params_comp_str_get(i));
            break;

        case CTRL_VOX_ON:
            b = cfg_sm.p_vox_en.get();
            if (diff) {
                b = !b;
                cfg_sm.p_vox_en.set(b);
            }
            snprintf(msg.data(), msg.capacity(), "VOX: %s", (b ? "On" : "Off"));
            break;

        case CTRL_KEY_SPEED:
            i = update_param(cfg_sm.p_key_speed, diff);
            snprintf(msg.data(), msg.capacity(), tr("Key speed: %i wpm"), i);
            break;

        case CTRL_KEY_MODE:
            i = cfg_sm.p_key_mode.get();
            if (diff) {
                i = loop_items({x6100_key_manual, x6100_key_auto_left, x6100_key_auto_right}, (x6100_key_mode_t)i, diff > 0);
                cfg_sm.p_key_mode.set(i);
            }
            s = params_key_mode_str_get((x6100_key_mode_t)i);
            snprintf(msg.data(), msg.capacity(), tr("Key mode: %s"), s);
            break;

        case CTRL_IAMBIC_MODE:
            i = cfg_sm.p_iambic_mode.get();
            if (diff) {
                i = loop_items({x6100_iambic_a, x6100_iambic_b}, (x6100_iambic_mode_t)i, diff > 0);
                cfg_sm.p_iambic_mode.set(i);
            }
            s = params_iambic_mode_str_ger((x6100_iambic_mode_t)i);
            snprintf(msg.data(), msg.capacity(), tr("Iambic mode: %s"), s);
            break;

        case CTRL_KEY_TONE:
            i = update_param(cfg_sm.p_key_tone, diff, 10);
            snprintf(msg.data(), msg.capacity(), tr("Key tone: %i Hz"), i);
            break;

        case CTRL_KEY_VOL:
            i = update_param(cfg_sm.p_key_vol, diff);
            snprintf(msg.data(), msg.capacity(), tr("Key volume: %i"), i);
            break;

        case CTRL_KEY_TRAIN:
            b = cfg_sm.p_key_train.get();
            if (diff) {
                b = !b;
                cfg_sm.p_key_train.set(b);
            }
            snprintf(msg.data(), msg.capacity(), tr("Key train: %s"), (b ? "On" : "Off"));
            break;

        case CTRL_QSK_TIME:
            i = update_param(cfg_sm.p_qsk_time, diff, 10);
            snprintf(msg.data(), msg.capacity(), tr("QSK time: %i ms"), i);
            break;

        case CTRL_KEY_RATIO:
            f = update_param(cfg_sm.p_key_ratio, diff, 0.1f);
            snprintf(msg.data(), msg.capacity(), tr("Key ratio: %0.1f"), f);
            break;

        case CTRL_CW_PEAK_ON:
            b = cfg_sm.p_cw_peak_on.get();
            if (diff) {
                b = !b;
                cfg_sm.p_cw_peak_on.set(b);
            }
            snprintf(msg.data(), msg.capacity(), tr("CW peak: %s"), (b ? "On" : "Off"));
            break;

        case CTRL_CW_PEAK_Q:
            i = update_param(cfg_sm.p_cw_peak_q, diff);
            snprintf(msg.data(), msg.capacity(), tr("CW peak Q: %i"), i);
            break;

        case CTRL_RIT:
            i = update_param(cfg_sm.p_rit, diff, 10);
            snprintf(msg.data(), msg.capacity(), "RIT: %c%i", (i < 0 ? '-' : '+'), abs(i));
            break;

        case CTRL_XIT:
            i = update_param(cfg_sm.p_xit, diff, 10);
            snprintf(msg.data(), msg.capacity(), "XIT: %c%i", (i < 0 ? '-' : '+'), abs(i));
            break;

        case CTRL_DNF:
            b = cfg_sm.p_dnf.get();
            if (diff) {
                b = !b;
                cfg_sm.p_dnf.set(b);
            }
            snprintf(msg.data(), msg.capacity(), "DNF: %s", (b ? "On" : "Off"));
            break;

        case CTRL_DNF_CENTER:
            i = update_param(cfg_sm.p_dnf_center, diff, 50);
            snprintf(msg.data(), msg.capacity(), tr("DNF center: %i Hz"), i);
            break;

        case CTRL_DNF_WIDTH:
            i = update_param(cfg_sm.p_dnf_width, diff, 5);
            snprintf(msg.data(), msg.capacity(), tr("DNF width: %i Hz"), i);
            break;

        case CTRL_DNF_AUTO:
            b = cfg_sm.p_dnf_auto.get();
            if (diff) {
                b = !b;
                cfg_sm.p_dnf_auto.set(b);
            }
            snprintf(msg.data(), msg.capacity(), tr("DNF auto: %s"), (b ? "On" : "Off"));
            break;

        case CTRL_NB:
            b = cfg_sm.p_nb.get();
            if (diff) {
                b = !b;
                cfg_sm.p_nb.set(b);
            }
            snprintf(msg.data(), msg.capacity(), "NB: %s", (b ? "On" : "Off"));
            break;

        case CTRL_NB_LEVEL:
            i = update_param(cfg_sm.p_nb_level, diff, 5);
            snprintf(msg.data(), msg.capacity(), tr("NB level: %i"), i);
            break;

        case CTRL_NB_WIDTH:
            i = update_param(cfg_sm.p_nb_width, diff, 5);
            snprintf(msg.data(), msg.capacity(), tr("NB width: %i"), i);
            break;

        case CTRL_NR:
            b = cfg_sm.p_nr.get();
            if (diff) {
                b = !b;
                cfg_sm.p_nr.set(b);
            }
            snprintf(msg.data(), msg.capacity(), "NR: %s", (b ? "On" : "Off"));
            break;

        case CTRL_NR_LEVEL:
            i = update_param(cfg_sm.p_nr_level, diff, 5);
            snprintf(msg.data(), msg.capacity(), tr("NR level: %i"), i);
            break;

        case CTRL_CW_DECODER:
            b = cfg_sm.p_cw_decoder.get();
            if (diff) {
                b = !b;
                cfg_sm.p_cw_decoder.set(b);
            }
            snprintf(msg.data(), msg.capacity(), tr("CW decoder: %s"), (b ? "On" : "Off"));
            break;

        case CTRL_CW_TUNE:
            b = cfg_sm.p_cw_tune.get();
            if (diff) {
                b = !b;
                cfg_sm.p_cw_tune.set(b);
            }
            snprintf(msg.data(), msg.capacity(), tr("CW tune: %s"), (b ? "On" : "Off"));
            break;

        case CTRL_CW_DECODER_SNR:
            f = update_param(cfg_sm.p_cw_decoder_snr, diff, 0.1f);
            snprintf(msg.data(), msg.capacity(), tr("CW decoder SNR: %0.1f dB"), f);
            break;

        case CTRL_RTTY_RATE:
            f = rtty_change_rate(diff);
            snprintf(msg.data(), msg.capacity(), tr("RTTY rate: %0.2f"), f);
            break;

        case CTRL_RTTY_SHIFT:
            i = rtty_change_shift(diff);
            snprintf(msg.data(), msg.capacity(), tr("RTTY shift: %i Hz"), i);
            break;

        case CTRL_RTTY_CENTER:
            i = rtty_change_center(diff);
            snprintf(msg.data(), msg.capacity(), tr("RTTY center: %i Hz"), i);
            break;

        case CTRL_RTTY_REVERSE:
            b = rtty_change_reverse(diff);
            snprintf(msg.data(), msg.capacity(), tr("RTTY reverse: %s"), (b ? "On" : "Off"));
            break;

        case CTRL_WF_REF:
            i = update_param(cfg_sm.p_waterfall_ref, diff);
            snprintf(msg.data(), msg.capacity(), tr("Waterfall ref: %+i dB"), i);
            break;

        case CTRL_WF_MODE:
            i = cfg_sm.p_wf_mode.get();
            if (diff) {
                i = !i;
                cfg_sm.p_wf_mode.set(i);
            }
            snprintf(msg.data(), msg.capacity(), tr("Waterfall: %s"), i ? tr("Fixed") : tr("Centered"));
            break;

        case CTRL_CLUSTER:
            snprintf(msg.data(), msg.capacity(), tr("Cluster: %s"), cluster_column_move(diff));
            knobs_set_mfk_param(CTRL_CLUSTER);  /* no subject: refresh the knob label */
            break;

        case CTRL_CLUSTER_ORDER:
            i = cfg_sm.p_cluster_order.get();
            if (diff) {
                i = !i;
                cfg_sm.p_cluster_order.set(i);
            }
            snprintf(msg.data(), msg.capacity(), tr("Cluster order: %s"), i ? tr("newest first") : tr("frequency"));
            break;

        case CTRL_CLUSTER_BANDS:
            i = cfg_sm.p_cluster_all_bands.get();
            if (diff) {
                i = !i;
                cfg_sm.p_cluster_all_bands.set(i);
            }
            snprintf(msg.data(), msg.capacity(), tr("Cluster: %s"), i ? tr("all bands") : tr("current band"));
            break;

        case CTRL_IF_SHIFT:
            {
                x6100_base_ver_t base_ver = x6100_control_get_base_ver();
                if ((util_compare_version(base_ver, (x6100_base_ver_t){1, 1, 9, 0}) >= 0) || (base_ver.rev >= 8)) {
                    i = update_param(cfg_sm.p_band_if_shift, diff, 100);
                    snprintf(msg.data(), msg.capacity(), tr("IF shift: %i Hz"), i);
                } else {
                    msg_update_text_fmt("IF shift is not supported");
                }
            }
            break;

        default:
            return;
    }
}

cfg_ctrl_t controls_encoder_get_next(encoder_binds_t encoder, cfg_ctrl_t current, int16_t dir) {
    std::string binds = cfg_sm.p_encoder_bind.get();
    size_t n_binds = binds.size();
    // Initialization
    /* Cluster: always on the MFK, but only while its column is shown */
    auto bound = [&](size_t c) {
        bool cl = c == CTRL_CLUSTER || c == CTRL_CLUSTER_ORDER || c == CTRL_CLUSTER_BANDS;
        return cl ? encoder == ENCODER_BIND_MFK && cfg_sm.p_cluster_column.get()
                                 : binds[c] == encoder;
    };
    if (dir == 0) {
        if (current < n_binds) {
            if (bound(current)) {
                return current;
            }
        }
        dir = 1;
    }
    cfg_ctrl_t new_ctrl;
    if (dir > 0) {
        for (size_t i = current + 1; i < current + 1 + n_binds; i++)
        {
            new_ctrl = (cfg_ctrl_t)(i % n_binds);
            if (bound(new_ctrl)) {
                return new_ctrl;
            }
        }
    } else {
        for (size_t i = 2 * n_binds + current - 1; i > n_binds + current - 1; i--)
        {
            new_ctrl = (cfg_ctrl_t)(i % n_binds);
            if (bound(new_ctrl)) {
                return new_ctrl;
            }
        }
    }
    LV_LOG_ERROR("No next/prev control for %c", encoder);
    return current;
}

template <typename T>
static inline bool toggle_param(Parameter<T> &param) {
    bool new_val = !param.get();
    param.set(new_val);
    return new_val;
}

template <typename T>
static T loop_items(std::vector<T> items, T cur, bool next) {
    int id;
    size_t len = std::size(items);
    for (id = 0; id < len; id++) {
        if (items[id] == cur) {
            break;
        }
    }
    id = (id + len + (next ? 1 : -1)) % len;
    return items[id];
}
