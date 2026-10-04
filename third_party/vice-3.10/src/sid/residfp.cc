/*
 * residfp.cc - reSIDfp interface code.
 *
 * Written by
 *  Teemu Rantanen <tvr@cs.hut.fi>
 *  Dag Lem <resid@nimrod.no>
 *  Andreas Boose <viceteam@t-online.de>
 *  groepaz <groepaz@gmx.net>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */






























#include "vice.h"

#if defined(HAVE_RESIDFP)

extern "C" {

#include <string.h>

#include "sid/sid.h" /* sid_engine_t */
#include "lib.h"
#include "log.h"
#include "residfp.h"
#include "resources.h"
#include "sid-snapshot.h"
#include "types.h"
#ifdef RASPI_COMPILE
#include "archdep_tick.h"
#endif

/*#define DEBUG_RESIDFP*/

#ifdef DEBUG_RESIDFP
#define DBG(x)  log_printf x
#else
#define DBG(x)
#endif

extern log_t sound_log;

} // extern "C"

#include "src/SID.h"

using namespace reSIDfp;

extern "C" {

struct sound_s
{
    /* speed factor */
    int factor;

    /* libresidfp does not have a public interface to the internal state of the
     * emulated SID, so we keep a mirror of the written register values here */
    int sid_register[0x20];

    /* resid sid implementation */
    reSIDfp::SID *sid;

#ifdef RASPI_COMPILE
    int chip_num;

    int maschera;

    int16_t *lavoro;
    int lavoro_cap;
    int lavoro_n;

    double cicli_per_campione;


    uint64_t resto;

    unsigned long persi;
#endif
};

typedef struct sound_s sound_t;

#ifndef RASPI_COMPILE
/* manage temporary buffers. if the requested size is smaller or equal to the
 * size of the already allocated buffer, reuse it.  */
static short *buf = NULL;

static int blen = 0;

static short *getbuf(int len)
{
    if ((buf == NULL) || (blen < len)) {
        if (buf) {
            lib_free(buf);
        }
        blen = len;
        buf = (short *)lib_calloc(len, 1);
    }
    return buf;
}
#endif

#ifdef RASPI_COMPILE






#define BMC_RESIDFP_CHIP 8
static int bmc_voci_mute[BMC_RESIDFP_CHIP];

void residfp_bmc_voci_mute(sound_t *psid, int chip, int mute)
{
    if (chip < 0 || chip >= BMC_RESIDFP_CHIP) {
        return;
    }
    bmc_voci_mute[chip] = mute & 0x07;
    if (psid != NULL && psid->sid != NULL && psid->maschera != 0) {
        psid->sid->setVoiceMask((uint8_t)(psid->maschera & ~bmc_voci_mute[chip]));
    }
}

static sound_t *residfp_open(uint8_t *sidstate, int chip_num)
#else
static sound_t *residfp_open(uint8_t *sidstate)
#endif
{
    sound_t *psid;
    int i;
#ifdef RASPI_COMPILE


    static int tabelle_fatte = 0;
    tick_t prima = tick_now();
#endif

    DBG(("residfp_open"));

    psid = new sound_t();
    psid->sid = new reSIDfp::SID;

#ifdef RASPI_COMPILE
    if (!tabelle_fatte) {
        tabelle_fatte = 1;
        log_message(sound_log, "reSIDfp: tabelle dei filtri costruite in %lu ms",
                    (unsigned long)((uint64_t)tick_now_delta(prima) * 1000 / tick_per_second()));
    }
    psid->chip_num = chip_num;
#endif

    for (i = 0x00; i <= 0x18; i++) {
#ifdef RASPI_COMPILE
        psid->sid_register[i] = sidstate[i];
#endif
        psid->sid->write(i, sidstate[i]);
    }

    return psid;
}

static int residfp_init(sound_t *psid, int speed, int cycles_per_sec, int factor)
{
    SamplingMethod method;
    char model_text[100];
    char method_text[100];
    double curve, range = 0.5f;
#ifdef RASPI_COMPILE
    double clock_sid, campionamento;
    int serve;
#endif

    int filters_enabled, model, sampling;
    int curve_6581_int = RESIDFP_6581_FILTER_CURVE_DEFAULT;
    int range_6581_int = RESIDFP_6581_FILTER_RANGE_DEFAULT;
    int curve_8580_int = RESIDFP_8580_FILTER_CURVE_DEFAULT;
    int combined_strength_int = RESIDFP_COMBINED_WAVEFORM_STRENGTH_DEFAULT;
    int old_caps = 0;

    DBG(("residfp_init"));

    CombinedWaveforms combined_table[3] = { WEAK, AVERAGE, STRONG };

    if (resources_get_int("SidFilters", &filters_enabled) < 0) {
        return 0;
    }

#ifdef RASPI_COMPILE


    if (psid->chip_num <= 0 && resources_get_int("SidModel", &model) < 0) {
        return 0;
    }
    if (psid->chip_num == 1 && resources_get_int("Sid2Model", &model) < 0) {
        return 0;
    }
    if (psid->chip_num >= 2 && resources_get_int("Sid3Model", &model) < 0) {
        return 0;
    }
#else
    if (resources_get_int("SidModel", &model) < 0) {
        return 0;
    }
#endif
    /*printf("residfp_init SidFilters:%d SidModel:%d\n", filters_enabled, model);*/

    /*
     * Don't even think about changing this to fast during warp :)
     * the resampled result is visible to the emulator.
     */
    if (resources_get_int("SidResidSampling", &sampling) < 0) {
        return 0;
    }

    if (resources_get_int("SidResidCombinedWaveformStrength", &combined_strength_int) < 0) {
        return 0;
    }
#ifdef RASPI_COMPILE
    if (combined_strength_int < RESIDFP_COMBINED_WAVEFORM_STRENGTH_MIN ||
        combined_strength_int > RESIDFP_COMBINED_WAVEFORM_STRENGTH_MAX) {
        combined_strength_int = RESIDFP_COMBINED_WAVEFORM_STRENGTH_DEFAULT;
    }
#endif

    if (resources_get_int("SidResid6581OldCaps", &old_caps) < 0) {
        return 0;
    }

    if ((model == 1) || (model == 2)) {
        /* 8580 */
        if (resources_get_int("SidResid8580FilterCurve", &curve_8580_int) < 0) {
            return 0;
        }
        curve = ((double)curve_8580_int) / RESIDFP_8580_FILTER_CURVE_ONE;
    } else {
        /* 6581 */
        if (resources_get_int("SidResid6581FilterCurve", &curve_6581_int) < 0) {
            return 0;
        }

        if (resources_get_int("SidResid6581FilterRange", &range_6581_int) < 0) {
            return 0;
        }
        curve = ((double)curve_6581_int) / RESIDFP_6581_FILTER_CURVE_ONE;
        range = ((double)range_6581_int) / RESIDFP_6581_FILTER_RANGE_ONE;
    }
    /*printf("residfp_init range int %d curve int:%d %d curve:%f range:%f\n",
           range_6581_int, curve_6581_int, curve_8580_int, curve, range);*/

#ifdef RASPI_COMPILE
    if (factor <= 0) {
        factor = 1000;
    }
#endif
    psid->factor = factor;

    switch (model) {
        default:
        case 0:
            psid->sid->setChipModel(MOS6581);
            psid->sid->input(0);
            psid->sid->setFilter6581Curve(curve);
            psid->sid->setFilter6581Range(range);
            strcpy(model_text, "MOS6581");
            break;
        case 1:
            psid->sid->setChipModel(CSG8580);
            psid->sid->input(0);
#ifdef RASPI_COMPILE

            psid->sid->setFilter8580Curve(curve);
#else
            psid->sid->setFilter6581Curve(curve);
#endif
            strcpy(model_text, "MOS8580");
            break;
        case 2:
            psid->sid->setChipModel(CSG8580);
            psid->sid->input(-32768);
            psid->sid->setFilter8580Curve(curve);
            strcpy(model_text, "MOS8580 + digi boost");
            break;
    }
    psid->sid->enableFilter(filters_enabled ? true : false);
    /* FIXME: also handle CAPS330 ("Galway C128") */
    psid->sid->set6581caps(old_caps ? CAPS2200 : CAPS470);
    psid->sid->setCombinedWaveforms(combined_table[combined_strength_int]);

    switch (sampling) {
        default:
#ifdef RASPI_COMPILE


        case 0: /* "fast" */
        case 1: /* "interpolating" */
            method = DECIMATE;
            strcpy(method_text, "decimating");
            break;
#else
        case 0: /* "fast" */
            method = NONE;
            strcpy(method_text, "raw 1MHz output");
            break;
        case 1: /* "interpolating" */
            method = DECIMATE;
            strcpy(method_text, "linear interpolating");
            break;
#endif
        case 2: /* "resampling" */
        case 3: /* "fast resample" */
            method = RESAMPLE;
            sprintf(method_text, "SINC resampling");
            break;
    }

#ifdef RASPI_COMPILE



    clock_sid = (double)cycles_per_sec * 1000.0 / (double)factor;
    campionamento = (double)speed * 1000.0 / (double)factor;
    psid->sid->setSamplingParameters(clock_sid, method, campionamento);

#ifdef RESIDFP_RUMORE_REGOLABILE






    {
        int rumore = 0;
        if (resources_get_int("SidResidBackgroundNoise", &rumore) < 0) {
            rumore = 0;
        }
        psid->sid->setBackgroundNoise(rumore);








        psid->sid->setNoiseSeed(psid->chip_num > 0
            ? 34653463u + (uint32_t)psid->chip_num * 0x9E3779B9u
            : 34653463u);
    }
#endif


    psid->maschera = 0x0f;
    if (psid->chip_num >= 0 && psid->chip_num < BMC_RESIDFP_CHIP) {
        psid->sid->setVoiceMask((uint8_t)(psid->maschera & ~bmc_voci_mute[psid->chip_num]));
    }



    psid->cicli_per_campione = clock_sid / campionamento;
    serve = (int)campionamento + 1024;
    if (psid->lavoro == NULL || psid->lavoro_cap < serve) {
        if (psid->lavoro != NULL) {
            lib_free(psid->lavoro);
        }
        psid->lavoro = (int16_t *)lib_malloc((size_t)serve * sizeof(int16_t));
        psid->lavoro_cap = serve;
    }
    psid->lavoro_n = 0;
    psid->resto = 0;
#else
    psid->sid->setSamplingParameters(cycles_per_sec, method, speed);
#endif

    /* psid->sid->enable_raw_debug_output(rawoutput); */

#ifdef RASPI_COMPILE
    log_message(sound_log, "reSIDfp: %s, filter %s, sampling rate %dHz - %s",
                model_text,
                filters_enabled ? "on" : "off",
                (int)campionamento, method_text);
    log_message(sound_log, "reSIDfp: SID %d, curva %d, range %d, forme combinate %d, condensatori %s",
                psid->chip_num + 1,
                ((model == 1) || (model == 2)) ? curve_8580_int : curve_6581_int,
                range_6581_int, combined_strength_int,
                old_caps ? "2200pF" : "470pF");
#else
    log_message(sound_log, "reSIDfp: %s, filter %s, sampling rate %dHz - %s",
                model_text,
                filters_enabled ? "on" : "off",
                speed, method_text);
#endif

    return 1;
}

static void residfp_close(sound_t *psid)
{
    delete psid->sid;
#ifdef RASPI_COMPILE
    if (psid->persi) {
        log_message(sound_log, "reSIDfp: SID %d, %lu campioni buttati (nessuno li prendeva)",
                    psid->chip_num + 1, psid->persi);
    }
    if (psid->lavoro != NULL) {
        lib_free(psid->lavoro);
        psid->lavoro = NULL;
    }
#endif
    delete psid;

#ifndef RASPI_COMPILE
    if (buf) {
        lib_free(buf);
        buf = NULL;
    }
#endif
}

static uint8_t residfp_read(sound_t *psid, uint16_t addr)
{
    return psid->sid->read(addr);
}

static void residfp_store(sound_t *psid, uint16_t addr, uint8_t byte)
{
    psid->sid_register[addr & 0x1f] = byte;
    psid->sid->write(addr, byte);
}

static void residfp_reset(sound_t *psid, CLOCK cpu_clk)
{
    psid->sid->reset();
#ifdef RASPI_COMPILE

    memset(psid->sid_register, 0, sizeof(psid->sid_register));
#endif
}

#ifdef SOUND_SYSTEM_FLOAT
#error "BMC64-NG: residfp.cc senza SOUND_SYSTEM_FLOAT"
#else
#ifdef RASPI_COMPILE

#define RESIDFP_MARGINE 16


static int residfp_delta_scartato = 0;

static int residfp_calculate_samples(sound_t *psid, short *pbuf, int nr, int interleave, CLOCK *delta_t)
{
    uint64_t cicli;
    int dati;
    int n;















    if ((int64_t)*delta_t <= 0 || *delta_t > (CLOCK)0x7fffffff) {



        if (*delta_t != 0 && !residfp_delta_scartato && psid->chip_num == 0) {
            residfp_delta_scartato = 1;
            log_message(sound_log, "reSIDfp: delta di cicli fuori scala scartato"
                        " (%lld), come il VICE di sviluppo (BMC64-NG)",
                        (long long)*delta_t);
        }
        cicli = 0;
    } else {



        psid->resto += (uint64_t)*delta_t * 1000;
        cicli = psid->resto / (uint64_t)psid->factor;
        psid->resto -= cicli * (uint64_t)psid->factor;
    }
    *delta_t = 0;

    if (psid->lavoro == NULL) {
        return 0;
    }

    while (cicli > 0) {
        int spazio = psid->lavoro_cap - psid->lavoro_n - RESIDFP_MARGINE;
        uint64_t passo;

        if (spazio <= 0) {

            int via = psid->lavoro_n / 2;
            memmove(psid->lavoro, psid->lavoro + via,
                    (size_t)(psid->lavoro_n - via) * sizeof(int16_t));
            psid->lavoro_n -= via;
            psid->persi += (unsigned long)via;
            continue;
        }
        passo = (uint64_t)((double)spazio * psid->cicli_per_campione);
        if (passo > cicli) {
            passo = cicli;
        }
        if (passo == 0) {
            passo = 1;
        }
        psid->lavoro_n += psid->sid->clock((unsigned int)passo, psid->lavoro + psid->lavoro_n);
        cicli -= passo;
    }

    if (nr <= 0 || psid->lavoro_n <= 0) {
        return 0;
    }


    dati = psid->lavoro_n < nr ? psid->lavoro_n : nr;
    for (n = 0; n < dati; n++) {
        pbuf[n * interleave] = psid->lavoro[n];
    }


    if (dati < psid->lavoro_n) {
        memmove(psid->lavoro, psid->lavoro + dati,
                (size_t)(psid->lavoro_n - dati) * sizeof(int16_t));
    }
    psid->lavoro_n -= dati;
    return dati;
}
#else
static int residfp_calculate_samples(sound_t *psid, short *pbuf, int nr, int interleave, CLOCK *delta_t)
{
    short *tmp_buf;
    int retval = 0;
    int int_delta_t = (int)*delta_t;

    /* Tried not to mess with resid during 64-bit conversion. clock(...) wants to modify *delta_t ... */
    if ((nr > 0) && (int_delta_t > 0)) {
        if (psid->factor == 1000) {
            tmp_buf = getbuf(2 * nr);

            /* CAUTION: unlike ReSID; this does NOT return the number of cycles "left to do" in int_delta_t */
            retval = psid->sid->clock(int_delta_t, tmp_buf);
            if (retval > 0) {
                int n, p = 0;
                for (n = 0; n < retval; n++) {
                    pbuf[p] = tmp_buf[n];
                    p += interleave;
                }
            }
            (*delta_t) = 0;
            return retval;
        }

        /* Used when SID does not run at system clock ("SID card") */
        tmp_buf = getbuf(2 * nr * psid->factor / 1000);
        retval = psid->sid->clock(int_delta_t, tmp_buf);
        if (retval > 0) {
            int n, p = 0;
            for (n = 0; n < retval; n++) {
                pbuf[p] = tmp_buf[n];
                p += interleave;
            }
        }
    }
    (*delta_t) = 0;
    return retval;
}
#endif
#endif

static char *residfp_dump_state(sound_t *psid)
{
    char strbuf[0x400];
    /* when sound is disabled *psid is NULL */
    if (psid && psid->sid) {
        /*state = psid->sid->read_state();*/
        psid->sid_register[25] = psid->sid->read(0x19);
        psid->sid_register[26] = psid->sid->read(0x1a);
        psid->sid_register[27] = psid->sid->read(0x1b);
        psid->sid_register[28] = psid->sid->read(0x1c);
    } else {
        return lib_strdup("no state available when sound is disabled.");
    }
    sprintf(strbuf,
            "FREQ:   %04x %04x %04x\n"
            "PULSE:  %04x %04x %04x\n"
            "CTRL:     %02x   %02x   %02x\n"
            "ADSR:   %04x %04x %04x\n"
            "FILTER: %04x RES: %02x MODE/VOL: %02x\n"
            "ADC: %02x %02x\n"
            "OSC3: %02x ENV3: %02x\n",
            ((psid->sid_register[(0 * 7) + 1] << 8) | psid->sid_register[(0 * 7) + 0]) & 0xffff,
            ((psid->sid_register[(1 * 7) + 1] << 8) | psid->sid_register[(1 * 7) + 0]) & 0xffff,
            ((psid->sid_register[(2 * 7) + 1] << 8) | psid->sid_register[(2 * 7) + 0]) & 0xffff,
            ((psid->sid_register[(0 * 7) + 3] << 8) | psid->sid_register[(0 * 7) + 2]) & 0xffff,
            ((psid->sid_register[(1 * 7) + 3] << 8) | psid->sid_register[(1 * 7) + 2]) & 0xffff,
            ((psid->sid_register[(2 * 7) + 3] << 8) | psid->sid_register[(2 * 7) + 2]) & 0xffff,
            (psid->sid_register[(0 * 7) + 4]) & 0xff,
            (psid->sid_register[(1 * 7) + 4]) & 0xff,
            (psid->sid_register[(2 * 7) + 4]) & 0xff,
            ((psid->sid_register[(0 * 7) + 5] << 8) | psid->sid_register[(0 * 7) + 6]) & 0xffff,
            ((psid->sid_register[(1 * 7) + 5] << 8) | psid->sid_register[(1 * 7) + 6]) & 0xffff,
            ((psid->sid_register[(2 * 7) + 5] << 8) | psid->sid_register[(2 * 7) + 6]) & 0xffff,
            ((psid->sid_register[22] << 8) | psid->sid_register[21]) & 0xffff,
            (psid->sid_register[23]) & 0xff,
            (psid->sid_register[24]) & 0xff,
            (psid->sid_register[25]) & 0xff,
            (psid->sid_register[26]) & 0xff,
            (psid->sid_register[27]) & 0xff,
            (psid->sid_register[28]) & 0xff
            );
    return lib_strdup(strbuf);
}

static void residfp_state_read(sound_t *psid, sid_snapshot_state_t *sid_state)
{
    unsigned int i;

#ifdef RASPI_COMPILE




    memset(sid_state, 0, sizeof(*sid_state));
    if (psid == NULL || psid->sid == NULL) {
        return;
    }
    for (i = 0; i < 0x20; i++) {
        sid_state->sid_register[i] = (uint8_t)psid->sid_register[i];
    }
    {
        uint8_t contatore[3], fase[3];

        psid->sid->getEnvelopes(contatore, fase);
        for (i = 0; i < 3; i++) {
            sid_state->envelope_counter[i] = contatore[i];
            sid_state->envelope_state[i] = fase[i];
        }
    }
#else
    /* when sound is disabled *psid is NULL */
    if (psid) {
        /*state = psid->sid->read_state();*/
        psid->sid_register[25] = psid->sid->read(0x19);
        psid->sid_register[26] = psid->sid->read(0x1a);
        psid->sid_register[27] = psid->sid->read(0x1b);
        psid->sid_register[28] = psid->sid->read(0x1c);
    }

    for (i = 0; i < 0x20; i++) {
        sid_state->sid_register[i] = (uint8_t)psid->sid_register[i];
    }

#if 0
    sid_state->bus_value = (uint8_t)state.bus_value;
    sid_state->bus_value_ttl = (uint32_t)state.bus_value_ttl;
    for (i = 0; i < 3; i++) {
        sid_state->accumulator[i] = (uint32_t)state.accumulator[i];
        sid_state->shift_register[i] = (uint32_t)state.shift_register[i];
        sid_state->rate_counter[i] = (uint16_t)state.rate_counter[i];
        sid_state->rate_counter_period[i] = (uint16_t)state.rate_counter_period[i];
        sid_state->exponential_counter[i] = (uint16_t)state.exponential_counter[i];
        sid_state->exponential_counter_period[i] = (uint16_t)state.exponential_counter_period[i];
        sid_state->envelope_counter[i] = (uint8_t)state.envelope_counter[i];
        sid_state->envelope_state[i] = (uint8_t)state.envelope_state[i];
        sid_state->hold_zero[i] = (uint8_t)state.hold_zero[i];
        sid_state->envelope_pipeline[i] = (uint8_t)state.envelope_pipeline[i];
        sid_state->shift_pipeline[i] = (uint8_t)state.shift_pipeline[i];
        sid_state->shift_register_reset[i] = (uint32_t)state.shift_register_reset[i];
        sid_state->floating_output_ttl[i] = (uint32_t)state.floating_output_ttl[i];
        sid_state->pulse_output[i] = (uint16_t)state.pulse_output[i];
    }
    sid_state->write_pipeline = (uint8_t)state.write_pipeline;
    sid_state->write_address = (uint8_t)state.write_address;
    sid_state->voice_mask = (uint8_t)state.voice_mask;
#endif
#endif
}

static void residfp_state_write(sound_t *psid, sid_snapshot_state_t *sid_state)
{
    unsigned int i;

    for (i = 0; i < 0x20; i++) {
        psid->sid_register[i] = (char)sid_state->sid_register[i];
    }
}

sid_engine_t residfp_hooks =
{
    residfp_open,
    residfp_init,
    residfp_close,
    residfp_read,
    residfp_store,
    residfp_reset,
    residfp_calculate_samples,
    residfp_dump_state,
    residfp_state_read,
    residfp_state_write
};

} // extern "C"

#endif
