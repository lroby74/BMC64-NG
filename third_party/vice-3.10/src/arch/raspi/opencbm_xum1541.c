/*
 * opencbm_xum1541.c - the xum1541 (ZoomFloppy) protocol, for bare metal.
 *
 * Written for BMC64 by Claude Code (Anthropic), from the OpenCBM plugin
 * lib/plugin/xum1541/{xum1541,archlib}.c by Nate Lawson and Spiro Trikaliotis.
 *
 * VICE reaches a real Commodore drive through thirteen OpenCBM entry points,
 * which it normally loads from a shared library with dlopen. Bare metal has
 * neither dlopen nor libusb, so those thirteen live here and are bound at
 * link time; the USB transport underneath is Circle's, through the six
 * circle_xum1541_* calls in circle.h.
 *
 * Three things are deliberately different from OpenCBM:
 *
 *  - it never calls exit(). OpenCBM stops the process when a USB transfer
 *    fails, which on bare metal would be a machine that dies with no message.
 *    Here a failure comes back as an error, and the machine is told the way
 *    a Commodore expects to be told: VICE's realdevice_write() turns a failed
 *    cbm_raw_write() into ST = 0x83, whose bit 7 is DEVICE NOT PRESENT. The
 *    unit is never quietly handed back to the file system device.
 *
 *  - the wait for a busy device is bounded. OpenCBM loops until the adapter
 *    answers; a drive that never answers would hang the emulator for good.
 *
 *  - the adapter is parked when the bus goes quiet. The firmware lights its
 *    LED from the status the host puts it in (xum1541/board-zoomfloppy.c,
 *    board_update_display): STATUS_INIT is solid, STATUS_READY is off,
 *    STATUS_ACTIVE blinks - and XUM1541_INIT sets ACTIVE while
 *    XUM1541_SHUTDOWN sets READY. OpenCBM opens and closes around every
 *    single command, so its LED blinks only while it works; VICE holds the
 *    driver open for as long as the real device is enabled, so the LED
 *    blinked for ever. Half a second of silence now parks the adapter with
 *    SHUTDOWN, and the next bus access wakes it with INIT.
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

#ifdef HAVE_REALDEVICE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "emux_api.h"
#include "opencbmlib.h"
#include "circle.h"
#include "xum_turbo_blob.h"






#define XUM1541_MINIMUM_COMPATIBLE_VERSION 7


#define XUM1541_ECHO                0
#define XUM1541_INIT                1
#define XUM1541_RESET               2
#define XUM1541_SHUTDOWN            3


#define XUM1541_DOING_RESET         0x01


#define XUM_CMDBUF_SIZE             4
#define XUM_STATUSBUF_SIZE          3
#define XUM_DEVINFO_SIZE            8


#define XUM1541_READ                8
#define XUM1541_WRITE               9


#define XUM1541_IOCTL               16
#define XUM1541_GET_EOI             (XUM1541_IOCTL + 7)
#define XUM1541_CLEAR_EOI           (XUM1541_IOCTL + 8)


#define XUM1541_IO_BUSY             1
#define XUM1541_IO_READY            2
#define XUM1541_IO_ERROR            3

#define XUM_GET_STATUS(buf)         ((buf)[0])
#define XUM_GET_STATUS_VAL(buf)     (((buf)[2] << 8) | (buf)[1])


#define XUM1541_CBM                 (1 << 4)
#define XUM1541_S2                  (3 << 4)






#define XUM1541_IEC_POLL            (XUM1541_IOCTL + 11)
#define XUM1541_IEC_WAIT            (XUM1541_IOCTL + 12)
#define XUM1541_IEC_SETRELEASE      (XUM1541_IOCTL + 13)

#define IEC_DATA                    0x01
#define IEC_CLOCK                   0x02
#define IEC_ATN                     0x04
#define IEC_RESET                   0x08
#define IEC_SRQ                     0x10


#define XUM_WRITE_TALK              (1 << 0)
#define XUM_WRITE_ATN               (1 << 1)


#define XUM_MAX_XFER_SIZE           32768


















#define XUM_BUSY_TOTAL_US           (20u * 1000u * 1000u)
#define XUM_WAIT_TOTAL_US           (3000u * 1000u)







#define XUM_IDLE_US                 (2000u * 1000u)





#define XUM_QUIET_US                (2000u * 1000u)



















#define XUM_PAZIENZA_RESET_MS       1500
#define XUM_PAZIENZA_PASSO_MS       200
#define XUM_MUTO_US                 (2000u * 1000u)






















#define XUM_PAZIENZA_LAVORO_MS      (10u * 60u * 1000u)
static int canale15_ascolta = 0;
static unsigned char comando_primo = 0;
static unsigned char drive_al_lavoro = 0;
static unsigned long xum_busy_tetto_us = XUM_BUSY_TOTAL_US;

static log_t xum_log = LOG_DEFAULT;
static int xum_open = 0;


static int xum_parked = 0;



static int xum_non_sveglio = 0;




static unsigned long xum_ultimo_uso = 0;
static unsigned long xum_zitto_fino_a = 0;
static int xum_zitto = 0;



static int xum_pazienza_corta = 0;












static unsigned long c_out_n, c_out_us;
static unsigned long c_in_n, c_in_us;
static unsigned long c_dati_n, c_dati_us;
static unsigned long c_busy_n, c_busy_us;
static unsigned long c_byte_letti, c_byte_scritti;
static unsigned long c_risvegli;
static unsigned long c_primo, c_ultimo;




static int bus_a_riposo = 1;




















static int bus_perso = 0;














#define BUS_PERSO_RIPROVA_US        (3000u * 1000u)
static unsigned long bus_perso_da = 0;

























static int warp_acceso_da_noi = 0;




static unsigned long c_bus_ripreso;




#define XUM_ABBANDONO_US            (5000u * 1000u)

static int giro_out(const unsigned char *buf, int len)
{
    unsigned long t0 = circle_get_ticks();
    int n = circle_xum1541_bulk_out(buf, len);
    unsigned long t1 = circle_get_ticks();

    if (c_primo == 0) {
        c_primo = t0;
    }
    c_ultimo = t1;
    c_out_us += t1 - t0;
    c_out_n++;
    return n;
}







static int giro_in(unsigned char *buf, int len, int dati)
{
    unsigned long t0 = circle_get_ticks();
    int n = circle_xum1541_bulk_in(buf, len);
    unsigned long t1 = circle_get_ticks();

    if (c_primo == 0) {
        c_primo = t0;
    }
    c_ultimo = t1;
    if (dati) {
        c_dati_us += t1 - t0;
        c_dati_n++;
    } else {
        c_in_us += t1 - t0;
        c_in_n++;
    }
    return n;
}

void raspi_opencbm_azzera_conti(void)
{
    c_out_n = c_out_us = 0;
    c_in_n = c_in_us = 0;
    c_dati_n = c_dati_us = 0;
    c_busy_n = c_busy_us = 0;
    c_byte_letti = c_byte_scritti = 0;
    c_risvegli = 0;
    c_bus_ripreso = 0;
    c_primo = c_ultimo = 0;
}

int raspi_opencbm_conti(int *v, int n)
{
    unsigned long giri = c_out_n + c_in_n + c_dati_n;
    unsigned long finestra = (c_ultimo > c_primo) ? (c_ultimo - c_primo) : 0;
    unsigned long byte = c_byte_letti + c_byte_scritti;

    if (v == NULL || n < 14) {
        return 0;
    }

    v[0] = (int)giri;






    v[1] = (int)c_out_n;
    v[2] = (int)(c_out_n ? c_out_us / c_out_n : 0);



    v[3] = (int)c_in_n;
    v[4] = (int)(c_in_n ? c_in_us / c_in_n : 0);


    v[5] = (int)c_dati_n;
    v[6] = (int)(c_dati_n ? c_dati_us / c_dati_n : 0);

    v[7] = (int)c_byte_letti;
    v[8] = (int)c_byte_scritti;
    v[9] = (int)(c_busy_us / 1000u);
    v[10] = (int)(finestra / 1000u);
    v[11] = (int)(finestra ? (byte * 1000000u) / finestra : 0);
    v[12] = (int)c_risvegli;
    v[13] = (int)c_bus_ripreso;
    return 14;
}





static void xum_diag(const char *testo, int a, int b, int c)
{
    FILE *f = fopen("/BMC64-DIAG.TXT", "a");
    if (f == NULL) {
        return;
    }
    fprintf(f, "xum1541: %s (%d, %d, %d)\n", testo, a, b, c);
    fclose(f);
}




static void xum_sveglia(void);
static void xum_parcheggia(void);
static void xum_versa_ora(void);
static int turbo_scaduto(void);
static void xum_riga(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));



static int xum_ioctl(unsigned int cmd, unsigned int addr, unsigned int secaddr);




int CBMAPIDECL cbm_untalk(CBM_FILE f);
int CBMAPIDECL cbm_unlisten(CBM_FILE f);





static int pazienza_ms_ora = 0;
static void pazienza_metti(int ms)
{
    pazienza_ms_ora = ms;
    circle_xum1541_set_patience_ms(ms);
}





















static void xum_rimetti_in_passo(void)
{
    unsigned char cmdBuf[XUM_CMDBUF_SIZE];
    unsigned char statusBuf[XUM_STATUSBUF_SIZE];
    int giro;

    memset(cmdBuf, 0, sizeof(cmdBuf));
    cmdBuf[0] = XUM1541_IEC_POLL;

    pazienza_metti(XUM_PAZIENZA_PASSO_MS);
    for (giro = 0; giro < 3; giro++) {
        if (circle_xum1541_bulk_out(cmdBuf, (int)sizeof(cmdBuf))
                == (int)sizeof(cmdBuf)
         && circle_xum1541_bulk_in(statusBuf, (int)sizeof(statusBuf))
                == (int)sizeof(statusBuf)
         && XUM_GET_STATUS(statusBuf) == XUM1541_IO_READY) {
            break;
        }
    }
    pazienza_metti(0);
    if (giro > 0) {
        xum_diag("usb: passo ritrovato dopo (giri, 3 = mai)", giro, 0, 0);
    }
}


static void turbo_traccia_guasto(const char *why);









static int turbo_dopo_uj = 0;





static unsigned long uj_fino_a = 0;
#define UJ_RIACCENSIONE_US          (1600u * 1000u)























#define LINEA_DRIVE_ASPETTATO_MS    600u






#define LINEA_FIRMWARE_ARRESO_MS    1500u
static unsigned long solo_fino_a = 0;
static unsigned long linea_ms_ultima = 0;
static void riaccensione_da_aspettare(const char *perche)
{
    uj_fino_a = circle_get_ticks() + UJ_RIACCENSIONE_US;
    if (uj_fino_a == 0) {
        uj_fino_a = 1;
    }
    solo_fino_a = 0;
    xum_riga("%s: nessun ATN per %u ms, i drive ripartono", perche,
             UJ_RIACCENSIONE_US / 1000u);
}

static int xum_fallito(const char *why)
{
    if (turbo_dopo_uj) {



        return -1;
    }
    if (!xum_zitto) {
        log_error(xum_log, "xum1541: %s - the machine is told DEVICE NOT "
                           "PRESENT", why);
        xum_diag(why, 0, 0, 0);
        turbo_traccia_guasto(why);
    }
    xum_zitto = 1;
    xum_zitto_fino_a = circle_get_ticks() + XUM_QUIET_US;




    if (!bus_a_riposo && !bus_perso) {
        bus_perso = 1;
        bus_perso_da = circle_get_ticks();
        c_bus_ripreso++;
        log_message(xum_log, "xum1541: the bus was taken and stopped "
                             "answering - no more attempts until it is "
                             "given back");
        xum_riga("=== bus preso e muto: basta tentativi ===");
    }

    xum_versa_ora();






    if (xum_open) {
        unsigned char devInfo[XUM_DEVINFO_SIZE];
        int len;

        memset(devInfo, 0, sizeof(devInfo));
        len = circle_xum1541_control_in(XUM1541_INIT, devInfo, sizeof(devInfo));
        if (len >= 3 && (devInfo[2] & XUM1541_DOING_RESET) != 0) {
            circle_xum1541_clear_halts();

            riaccensione_da_aspettare("ripresa: il firmware ha tirato la linea");
        }
        xum_parked = 0;


        if (len >= 3) {
            xum_rimetti_in_passo();
        }
    }

    return -1;
}















static unsigned long turbo_fino_a = 0;







static unsigned long turbo_traccia_costo = 0;




static unsigned long turbo_orologio(void)
{
    return circle_get_ticks() - turbo_traccia_costo;
}

static void turbo_scadenza_metti(unsigned long us)
{
    turbo_fino_a = turbo_orologio() + us;
    if (turbo_fino_a == 0) {
        turbo_fino_a = 1;
    }
}

static void turbo_scadenza_togli(void)
{
    turbo_fino_a = 0;
}




#define TURBO_SCADENZA_PULIZIA      (3u * 1000u * 1000u)
#define TURBO_SCADENZA_MOLLA        (2u * 1000u * 1000u)
#define TURBO_SCADENZA_APERTURA     (8u * 1000u * 1000u)
#define TURBO_SCADENZA_CHIUSURA     (8u * 1000u * 1000u)


















static unsigned long xum_via_libera(unsigned long us)
{
    unsigned long prima = turbo_fino_a;

    bus_perso = 0;
    xum_zitto = 0;
    xum_zitto_fino_a = 0;
    turbo_scadenza_metti(us);
    return prima;
}

static void xum_via_libera_fine(unsigned long prima)
{
    turbo_fino_a = prima;
}

static int turbo_scaduto(void)
{
    return turbo_fino_a != 0
        && (long)(turbo_orologio() - turbo_fino_a) >= 0;
}




























static int scolo_attivo = 0;
static int scolo_dentro = 0;
static int scolo_un_blocco(void);
static void scolo_finisci(void);



static int linea_dopo_scolo = 0;
static void scolo_comincia(void);
static int turbo_a_meta_file(void);
static void svuota_il_turbo(const char *perche);



static unsigned long muto_fino_a = 0;
static int muto_dice_no(void);




























static int drive_commodore = 0;
static int accensione_da_fare = 1;
static int cbm_reset_linea(void);
static void turbo_lascia_per_la_linea(void);
static void accensione_mista(void);




static int linea_come_sul_ferro(void)
{
    return drive_commodore == 1;
}


























#define RIPARTENZA_PASSO_US         10000u
#define RIPARTENZA_LIBERO_N         3u
#define RIPARTENZA_MARGINE_US       (100u * 1000u)
#define RIPARTENZA_PRESA_US         (2500u * 1000u)
#define RIPARTENZA_TETTO_US         (6000u * 1000u)








#define LINEA_OSSERVATA_TETTO_US    (3000u * 1000u)
#define LINEA_OSSERVATA_DOPO_US     (100u * 1000u)
#define LINEA_OSSERVATA_CIECA_US    (1000u * 1000u)


#define RIPOSO_ATTESA_US            (2000u * 1000u)
static int linea_presa_ms = -1;

static int linea_tenuta_ms = 0;





static int atn_rifiutato = 0;

static int ripartenza_cura(void)
{

    int m = circle_get_model();
    return m == 4 || m == 5;
}




static int iec_crudo(unsigned int cmd, unsigned int a, unsigned int b)
{
    unsigned char cmdBuf[XUM_CMDBUF_SIZE];
    unsigned char statusBuf[XUM_STATUSBUF_SIZE];
    int prima = pazienza_ms_ora;
    int esito = -1;

    memset(cmdBuf, 0, sizeof(cmdBuf));
    cmdBuf[0] = (unsigned char)cmd;
    cmdBuf[1] = (unsigned char)a;
    cmdBuf[2] = (unsigned char)b;
    pazienza_metti(XUM_PAZIENZA_PASSO_MS);
    if (giro_out(cmdBuf, (int)sizeof(cmdBuf)) == (int)sizeof(cmdBuf)
     && giro_in(statusBuf, (int)sizeof(statusBuf), 0)
            == (int)sizeof(statusBuf)
     && XUM_GET_STATUS(statusBuf) == XUM1541_IO_READY) {
        esito = XUM_GET_STATUS_VAL(statusBuf);
    }
    pazienza_metti(prima);
    return esito;
}



static int linee_crude(void)
{
    return iec_crudo(XUM1541_IEC_POLL, 0, 0);
}




static int ripartenza_aspetta(void)
{
    unsigned long da = uj_fino_a - UJ_RIACCENSIONE_US;
    unsigned long inizio = circle_get_ticks();
    unsigned long presa = (drive_commodore != 0) ? RIPARTENZA_PRESA_US
                                                 : UJ_RIACCENSIONE_US;
    unsigned long t, visto_da = 0;
    unsigned int libere = 0;
    int visto = 0, l;

    for (;;) {
        t = circle_get_ticks() - da;
        l = linee_crude();
        if (l < 0) {

            unsigned long ora = circle_get_ticks();
            if ((long)(ora - uj_fino_a) < 0) {
                circle_sleep((long)(uj_fino_a - ora));
            }
            xum_riga("RIPARTENZA: le linee non si leggono, aspettato a tempo");
            return 1;
        }
        if (l & IEC_DATA) {
            if (!visto) {
                visto = 1;
                visto_da = t;
            }
            libere = 0;
        } else if (visto) {
            if (++libere >= RIPARTENZA_LIBERO_N) {
                circle_sleep(RIPARTENZA_MARGINE_US);
                xum_riga("RIPARTENZA: DATA tenuto dal drive da %lu a %lu ms, "
                         "poi lasciato", visto_da / 1000u, t / 1000u);
                xum_diag("ripartenza: DATA tenuto dal drive, da e fino a (ms)",
                         (int)(visto_da / 1000u), (int)(t / 1000u), 0);
                return 1;
            }
        } else if (t >= presa) {
            xum_riga("RIPARTENZA: DATA mai tenuto in %lu ms, si va",
                     t / 1000u);
            return 1;
        }
        if (t >= RIPARTENZA_TETTO_US
         && circle_get_ticks() - inizio >= RIPOSO_ATTESA_US) {
            xum_riga("RIPARTENZA: DATA ancora tenuto dopo %lu ms: il drive e' "
                     "fermo, niente ATN", t / 1000u);
            xum_diag("ripartenza: DATA tenuto oltre il tetto, drive fermo (ms)",
                     (int)(t / 1000u), (int)(visto_da / 1000u), l);
            log_message(xum_log, "xum1541: the drive held DATA for %lu ms "
                                 "after a restart - it is stuck", t / 1000u);
            bus_perso = 1;
            bus_perso_da = circle_get_ticks();
            atn_rifiutato = 1;
            return 0;
        }
        circle_sleep(RIPARTENZA_PASSO_US);
    }
}










static int xum_pronto(void);
static int atn_da_riposo_ok(void)
{
    unsigned long inizio;
    unsigned int libere = 0;
    int l;

    if (!ripartenza_cura() || xum_pazienza_corta || drive_al_lavoro != 0
     || scolo_attivo) {
        return 1;
    }
    if (!xum_pronto()) {
        return 1;
    }
    l = linee_crude();
    if (l < 0 || !(l & IEC_DATA)) {
        atn_rifiutato = 0;
        return 1;
    }
    inizio = circle_get_ticks();
    xum_riga("ATN: a riposo DATA e' tenuto (linee %02x): aspetto che lo "
             "lascino", (unsigned int)l);
    while (circle_get_ticks() - inizio < RIPOSO_ATTESA_US) {
        circle_sleep(RIPARTENZA_PASSO_US);
        l = linee_crude();
        if (l < 0) {
            return 1;
        }
        if (!(l & IEC_DATA)) {
            if (++libere >= RIPARTENZA_LIBERO_N) {
                circle_sleep(RIPARTENZA_MARGINE_US);
                xum_riga("ATN: DATA lasciato dopo %lu ms",
                         (circle_get_ticks() - inizio) / 1000u);
                atn_rifiutato = 0;
                return 1;
            }
        } else {
            libere = 0;
        }
    }
    xum_riga("ATN: DATA tenuto a riposo da %lu ms (linee %02x): un drive "
             "fermo, l'ATN non parte",
             (circle_get_ticks() - inizio) / 1000u, (unsigned int)l);
    xum_diag("bus: DATA tenuto a riposo, un drive fermo (ms, linee)",
             (int)((circle_get_ticks() - inizio) / 1000u), l, 0);
    log_message(xum_log, "xum1541: DATA is held with the bus at rest - a "
                         "drive is stuck: Reset Drive, or switch it off and "
                         "on");
    bus_perso = 1;
    bus_perso_da = circle_get_ticks();
    atn_rifiutato = 1;
    return 0;
}






static int linea_osservata(void)
{
    unsigned long t0, t, tetto = LINEA_OSSERVATA_TETTO_US, primo = 0;
    unsigned int prese = 0;
    int prima, l = -1, k;

    linea_presa_ms = -1;
    linea_tenuta_ms = 0;
    prima = linee_crude();
    if (prima < 0) {
        return -1;
    }
    if ((prima & (IEC_DATA | IEC_CLOCK)) == (IEC_DATA | IEC_CLOCK)) {
        tetto = LINEA_OSSERVATA_CIECA_US;
        linea_presa_ms = -2;
    }
    if (iec_crudo(XUM1541_IEC_SETRELEASE, IEC_RESET, 0) < 0) {
        return -1;
    }
    t0 = circle_get_ticks();
    for (;;) {
        circle_sleep(RIPARTENZA_PASSO_US);
        t = circle_get_ticks() - t0;
        if (tetto == LINEA_OSSERVATA_TETTO_US) {
            l = linee_crude();
            if (l >= 0 && (l & (IEC_DATA | IEC_CLOCK))
                              == (IEC_DATA | IEC_CLOCK)) {
                if (prese++ == 0) {
                    primo = t;
                }
                if (prese >= RIPARTENZA_LIBERO_N) {
                    linea_presa_ms = (int)(primo / 1000u);
                    circle_sleep(LINEA_OSSERVATA_DOPO_US);
                    break;
                }
            } else {
                prese = 0;
            }
        }
        if (t >= tetto) {
            break;
        }
    }


    for (k = 0; k < 3; k++) {
        if (iec_crudo(XUM1541_IEC_SETRELEASE, 0, IEC_RESET) >= 0) {
            break;
        }
    }
    linea_tenuta_ms = (int)((circle_get_ticks() - t0) / 1000u);
    xum_riga("RESET: la linea tenuta %d ms, %s (linee prima %02x, durante "
             "%02x)", linea_tenuta_ms,
             linea_presa_ms >= 0 ? "presa dal drive"
             : (linea_presa_ms == -1 ? "nessuno l'ha presa"
                                     : "il bus era gia' tenuto"),
             (unsigned int)prima, (unsigned int)(l < 0 ? 0 : l));
    xum_diag("reset: linea osservata, presa dopo (ms, -1 mai, -2 cieca), "
             "tenuta (ms), linee prima", linea_presa_ms, linea_tenuta_ms,
             prima);
    return (k == 3) ? -1 : 0;
}

static int xum_pronto(void)
{
    unsigned long adesso;

    if (!xum_open) {
        return 0;
    }



    if (drive_commodore != 0 && accensione_da_fare) {
        accensione_da_fare = 0;
        if (linea_come_sul_ferro()) {
            xum_riga("ACCENSIONE: drive Commodore, la linea RESET come un "
                     "Commodore appena acceso");
            cbm_reset_linea();
        } else {
            accensione_mista();
        }
    }




    if (muto_dice_no()) {
        return 0;
    }




    if (scolo_attivo && !scolo_dentro) {
        scolo_finisci();
    }



    if (turbo_scaduto()) {
        return 0;
    }





    if (bus_perso) {
        if ((long)(circle_get_ticks() - bus_perso_da)
            < (long)BUS_PERSO_RIPROVA_US) {
            return 0;
        }
        log_message(xum_log, "xum1541: the bus has been quiet for a while "
                             "- trying once more");
        bus_perso = 0;
        bus_perso_da = 0;
        xum_zitto = 0;
        xum_zitto_fino_a = 0;
    }

    adesso = circle_get_ticks();





    if (uj_fino_a != 0 && !ripartenza_cura()) {
        if ((long)(adesso - uj_fino_a) < 0) {
            circle_sleep((long)(uj_fino_a - adesso));
            adesso = circle_get_ticks();
        }
        uj_fino_a = 0;
        turbo_dopo_uj = 0;
    }

    if (xum_zitto) {
        if ((long)(adesso - xum_zitto_fino_a) < 0) {
            return 0;
        }
        xum_zitto = 0;
    }

    xum_ultimo_uso = adesso;

    if (xum_parked) {
        xum_sveglia();
        if (xum_parked) {


            return 0;
        }
    }


    if (uj_fino_a != 0 && ripartenza_cura()) {
        int si = ripartenza_aspetta();

        uj_fino_a = 0;
        turbo_dopo_uj = 0;
        xum_ultimo_uso = circle_get_ticks();
        if (!si) {
            return 0;
        }
    }

    return 1;
}




static int xum_handle_cookie;



















#define XUM_LOG_RIGHE 256
#define XUM_LOG_LARGA 96

static int xum_log_acceso = 0;





extern unsigned long log_giri_su_scheda;
extern unsigned long log_us_su_scheda;



extern int log_ferma_la_scheda;







extern void passo_prima(const char *cosa);
extern void passo_fatto(const char *cosa);
static unsigned long log_giri_prima = 0;
static unsigned long log_us_prima = 0;
static char xum_log_anello[XUM_LOG_RIGHE][XUM_LOG_LARGA];
static int xum_log_quante = 0;
static int xum_log_testa = 0;
static long xum_log_totale = 0;





static unsigned long xum_log_dopo = 0;


static int xum_log_sporco = 0;



static unsigned long xum_log_versato = 0;



static long xum_log_saltate = 0;















#define XUM_LOG_CONTA 48

static char xum_conta_anello[XUM_LOG_CONTA][XUM_LOG_LARGA];
static int xum_conta_quante = 0;
static int xum_conta_testa = 0;



static int xum_riga_di_traffico(const char *r)
{
    return !strncmp(r, "READ", 4) ||
           !strncmp(r, "WRITE", 5) ||
           !strncmp(r, "EOI", 3);
}

static void xum_versa(void)
{
    FILE *f;
    int i;

    f = fopen("/XUM.LOG", "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "drive vero: %ld operazioni in tutto.\n"
               "Il +Nms e' il tempo dell'operazione della riga precedente,\n"
               "senza la scrittura di questo file, che si fa una volta al\n"
               "secondo - a ogni fotogramma costava piu' del drive.%s\n",
            xum_log_totale,
            xum_log_saltate ? " ALCUNE RIGHE MANCANO." : "");




    fprintf(f, "\n=== QUELLO CHE CONTA (le ultime %d righe che non sono\n"
               "    traffico: chi apre, chi chiude, il drive, il veloce)\n\n",
            xum_conta_quante);
    for (i = 0; i < xum_conta_quante; i++) {
        int k = (xum_conta_testa - xum_conta_quante + i + XUM_LOG_CONTA)
                % XUM_LOG_CONTA;
        fprintf(f, "%s\n", xum_conta_anello[k]);
    }

    fprintf(f, "\n=== LE ULTIME %d RIGHE, TUTTE (la piu' recente in fondo)\n\n",
            xum_log_quante);
    for (i = 0; i < xum_log_quante; i++) {
        int k = (xum_log_testa - xum_log_quante + i + XUM_LOG_RIGHE)
                % XUM_LOG_RIGHE;
        fprintf(f, "%s\n", xum_log_anello[k]);
    }
    fclose(f);
}

static void xum_riga(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

static void xum_riga(const char *fmt, ...)
{
    va_list ap;
    char *riga;
    int n;

    if (!xum_log_acceso) {
        return;
    }

    riga = xum_log_anello[xum_log_testa];
    n = snprintf(riga, XUM_LOG_LARGA, "%lu +%lums ",
                 (unsigned long)circle_get_ticks(),
                 xum_log_dopo ? (circle_get_ticks() - xum_log_dopo) / 1000u
                              : 0u);
    if (n < 0 || n >= XUM_LOG_LARGA) {
        n = 0;
    }
    va_start(ap, fmt);
    vsnprintf(riga + n, (size_t)(XUM_LOG_LARGA - n), fmt, ap);
    va_end(ap);



    if (!xum_riga_di_traffico(riga + n)) {
        memcpy(xum_conta_anello[xum_conta_testa], riga, XUM_LOG_LARGA);
        xum_conta_testa = (xum_conta_testa + 1) % XUM_LOG_CONTA;
        if (xum_conta_quante < XUM_LOG_CONTA) {
            xum_conta_quante++;
        }
    }

    xum_log_testa = (xum_log_testa + 1) % XUM_LOG_RIGHE;
    if (xum_log_quante < XUM_LOG_RIGHE) {
        xum_log_quante++;
    }
    xum_log_totale++;













    xum_log_sporco = 1;



    xum_log_dopo = circle_get_ticks();
}


static void xum_versa_ora(void)
{
    if (xum_log_acceso && xum_log_sporco) {
        xum_versa();
        xum_log_sporco = 0;
        xum_log_versato = circle_get_ticks();
        xum_log_dopo = circle_get_ticks();
    }
}


















#define XUM_LOG_RIPOSO_US (1000u * 1000u)

static void xum_versa_col_comodo(void)
{
    if (!xum_log_acceso || !xum_log_sporco) {
        return;
    }
    if (xum_log_versato != 0 &&
        circle_get_ticks() - xum_log_versato < XUM_LOG_RIPOSO_US) {
        return;
    }
    xum_versa_ora();
}













static const int misura_ritardi[] = {500, 160, 40, 0};
#define MISURA_QUANTI (int)(sizeof(misura_ritardi) / sizeof(misura_ritardi[0]))

static int misura_un_giro(int quanti)
{
    unsigned long inizio, durata;
    int i;

    inizio = circle_get_ticks();
    for (i = 0; i < quanti; i++) {
        if (xum_ioctl(XUM1541_GET_EOI, 0, 0) < 0) {
            return -1;
        }
    }
    durata = circle_get_ticks() - inizio;

    return (int)(durata / (unsigned long)quanti);
}











static unsigned long conti_via[14];

static void conti_metti_via(void)
{
    conti_via[0] = c_out_n;         conti_via[1] = c_out_us;
    conti_via[2] = c_in_n;          conti_via[3] = c_in_us;
    conti_via[4] = c_dati_n;        conti_via[5] = c_dati_us;
    conti_via[6] = c_busy_n;        conti_via[7] = c_busy_us;
    conti_via[8] = c_primo;         conti_via[9] = c_ultimo;
    conti_via[10] = c_risvegli;
    conti_via[11] = c_byte_letti;   conti_via[12] = c_byte_scritti;
    conti_via[13] = c_bus_ripreso;
}

static void conti_rimetti(void)
{
    c_out_n = conti_via[0];         c_out_us = conti_via[1];
    c_in_n = conti_via[2];          c_in_us = conti_via[3];
    c_dati_n = conti_via[4];        c_dati_us = conti_via[5];
    c_busy_n = conti_via[6];        c_busy_us = conti_via[7];
    c_primo = conti_via[8];         c_ultimo = conti_via[9];
    c_risvegli = conti_via[10];
    c_byte_letti = conti_via[11];   c_byte_scritti = conti_via[12];
    c_bus_ripreso = conti_via[13];
}

int raspi_opencbm_misura(int *v, int n)
{
    const int quanti = 200;
    int prima;
    int k;

    if (v == NULL || n < MISURA_QUANTI + 2) {
        return 0;
    }
    if (!xum_open || !xum_pronto()) {
        return 0;
    }

    conti_metti_via();





    prima = circle_xum1541_get_imod();

    for (k = 0; k < MISURA_QUANTI; k++) {
        circle_xum1541_set_imod(misura_ritardi[k]);
        v[k] = misura_un_giro(quanti);
        if (v[k] < 0) {


            circle_xum1541_set_imod(prima >= 0 ? prima : misura_ritardi[0]);
            conti_rimetti();
            return 0;
        }
    }

    circle_xum1541_set_imod(prima >= 0 ? prima : misura_ritardi[0]);
    v[MISURA_QUANTI] = prima;
    v[MISURA_QUANTI + 1] = MISURA_QUANTI;

    conti_rimetti();

    return MISURA_QUANTI + 2;
}


















#define WARP_SILENZIO_US            (500u * 1000u)

static void warp_aggiorna(void)
{






    int lavora = !bus_a_riposo && !scolo_attivo
                 && circle_get_ticks() - xum_ultimo_uso < WARP_SILENZIO_US;






    log_ferma_la_scheda = lavora || scolo_attivo;

    if (lavora && !warp_acceso_da_noi) {
        emux_set_warp(1);
        warp_acceso_da_noi = 1;
    } else if (!lavora && warp_acceso_da_noi) {
        emux_set_warp(0);
        warp_acceso_da_noi = 0;
    }
}

void raspi_opencbm_set_log(int acceso)
{
    if (!acceso) {
        xum_versa_ora();
    }

    xum_log_acceso = acceso ? 1 : 0;
    xum_log_quante = 0;
    xum_log_testa = 0;
    xum_log_totale = 0;
    if (xum_log_acceso) {
        xum_riga("registro acceso");
    }
}

static int xum_wait_status(void)
{
    unsigned char statusBuf[XUM_STATUSBUF_SIZE];

    unsigned long inizio = circle_get_ticks();
    unsigned long busy_prima = c_busy_n;

    for (;;) {
        int n = giro_in(statusBuf, XUM_STATUSBUF_SIZE, 0);

        if (n != XUM_STATUSBUF_SIZE) {
            return xum_fallito("no answer to a command");
        }

        switch (XUM_GET_STATUS(statusBuf)) {
            case XUM1541_IO_BUSY:
                c_busy_n++;
                break;
            case XUM1541_IO_ERROR:
                return xum_fallito("the adapter reports an error");
            case XUM1541_IO_READY:
                if (c_busy_n != busy_prima) {
                    c_busy_us += circle_get_ticks() - inizio;
                }
                return XUM_GET_STATUS_VAL(statusBuf);
            default:
                return xum_fallito("unknown status from the adapter");
        }





        if (circle_get_ticks() - inizio > xum_busy_tetto_us) {
            return xum_fallito("the drive never finished");
        }









        if (turbo_scaduto()) {
            return xum_fallito("out of time waiting for the drive");
        }
    }
}

static int xum_ioctl(unsigned int cmd, unsigned int addr, unsigned int secaddr)
{
    unsigned char cmdBuf[XUM_CMDBUF_SIZE];

    cmdBuf[0] = (unsigned char)cmd;
    cmdBuf[1] = (unsigned char)addr;
    cmdBuf[2] = (unsigned char)secaddr;
    cmdBuf[3] = 0;

    if (!xum_pronto()) {
        return -1;
    }

    if (giro_out(cmdBuf, sizeof(cmdBuf)) != sizeof(cmdBuf)) {
        return xum_fallito("could not send a command");
    }

    return xum_wait_status();
}

static int xum_write_dentro(unsigned char modeFlags,
                            const unsigned char *data, unsigned int size)
{
    unsigned char cmdBuf[XUM_CMDBUF_SIZE];
    unsigned int written = 0;
    unsigned long inizio;

    cmdBuf[0] = XUM1541_WRITE;
    cmdBuf[1] = modeFlags;
    cmdBuf[2] = size & 0xff;
    cmdBuf[3] = (size >> 8) & 0xff;

    if (!xum_pronto()) {
        return -1;
    }

    if (giro_out(cmdBuf, sizeof(cmdBuf)) != sizeof(cmdBuf)) {
        return xum_fallito("could not send the write command");
    }

    inizio = circle_get_ticks();
    while (written < size) {
        unsigned int chunk = size - written;
        int wr;

        if (circle_get_ticks() - inizio > XUM_WAIT_TOTAL_US) {
            return xum_fallito("the write never finished");
        }

        if (chunk > XUM_MAX_XFER_SIZE) {
            chunk = XUM_MAX_XFER_SIZE;
        }

        wr = giro_out(data + written, (int)chunk);
        if (wr < 0) {
            return xum_fallito("error while writing");
        }

        written += (unsigned int)wr;
        c_byte_scritti += (unsigned long)wr;


        if ((unsigned int)wr < chunk) {
            break;
        }
    }


    if ((modeFlags & 0xf0) == XUM1541_CBM) {
        return xum_wait_status();
    }

    return (int)written;
}




static int xum_write(unsigned char modeFlags, const unsigned char *data,
                     unsigned int size)
{
    int corta = xum_pazienza_corta && (modeFlags & XUM_WRITE_ATN);



    int lunga = !corta && drive_al_lavoro != 0
                && (modeFlags & XUM_WRITE_ATN) && turbo_fino_a == 0;
    unsigned long t0 = 0;
    int esito;

    if (corta) {
        pazienza_metti(XUM_PAZIENZA_RESET_MS);
    } else if (lunga) {
        pazienza_metti((int)XUM_PAZIENZA_LAVORO_MS);
        xum_busy_tetto_us = (unsigned long)XUM_PAZIENZA_LAVORO_MS * 1000ul;
        t0 = circle_get_ticks();
    }
    esito = xum_write_dentro(modeFlags, data, size);
    if (corta || lunga) {
        pazienza_metti(0);
    }
    if (lunga) {
        xum_busy_tetto_us = XUM_BUSY_TOTAL_US;
        xum_riga("il drive ha finito il comando %c dopo %lu ms (esito %d)",
                 drive_al_lavoro, (circle_get_ticks() - t0) / 1000ul, esito);
    }
    if (modeFlags & XUM_WRITE_ATN) {
        drive_al_lavoro = 0;
    }
    return esito;
}

static int xum_read(unsigned char mode, unsigned char *data, unsigned int size)
{
    unsigned char cmdBuf[XUM_CMDBUF_SIZE];
    unsigned int got = 0;
    unsigned long inizio;

    cmdBuf[0] = XUM1541_READ;
    cmdBuf[1] = mode;
    cmdBuf[2] = size & 0xff;
    cmdBuf[3] = (size >> 8) & 0xff;

    if (!xum_pronto()) {
        return -1;
    }

    if (giro_out(cmdBuf, sizeof(cmdBuf)) != sizeof(cmdBuf)) {
        return xum_fallito("could not send the read command");
    }

    inizio = circle_get_ticks();
    while (got < size) {
        unsigned int chunk = size - got;
        int rd;

        if (circle_get_ticks() - inizio > XUM_WAIT_TOTAL_US) {
            return xum_fallito("the read never finished");
        }

        if (chunk > XUM_MAX_XFER_SIZE) {
            chunk = XUM_MAX_XFER_SIZE;
        }

        rd = giro_in(data + got, (int)chunk, 1);
        if (rd < 0) {
            return xum_fallito("error while reading");
        }

        got += (unsigned int)rd;
        c_byte_letti += (unsigned long)rd;

        if ((unsigned int)rd < chunk) {
            break;
        }
    }

    return (int)got;
}





int CBMAPIDECL cbm_driver_open(CBM_FILE *f, int port)
{
    unsigned char devInfo[XUM_DEVINFO_SIZE];
    int len;

    if (xum_log == LOG_DEFAULT) {
        xum_log = log_open("XUM1541");
    }

    xum_zitto = 0;
    xum_parked = 0;
    xum_ultimo_uso = circle_get_ticks();
    solo_fino_a = 0;

    *f = NULL;

    if (circle_xum1541_open() < 0) {
        log_message(xum_log, "no xum1541 (ZoomFloppy) plugged in");
        xum_diag("open: adapter not there", 0, 0, 0);
        return -1;
    }
    xum_diag("open: adapter found", 0, 0, 0);

    memset(devInfo, 0, sizeof(devInfo));
    len = circle_xum1541_control_in(XUM1541_INIT, devInfo, sizeof(devInfo));
    if (len < 2) {
        log_error(xum_log, "the xum1541 did not answer INIT (%d)", len);
        xum_diag("INIT: no answer", len, 0, 0);
        circle_xum1541_close();
        return -1;
    }

    if (devInfo[0] < XUM1541_MINIMUM_COMPATIBLE_VERSION) {
        log_error(xum_log, "xum1541 firmware %d is too old (needs %d)",
                  devInfo[0], XUM1541_MINIMUM_COMPATIBLE_VERSION);
        circle_xum1541_close();
        return -1;
    }

    log_message(xum_log, "xum1541 firmware %d, capabilities %02x, status %02x",
                devInfo[0], devInfo[1], devInfo[2]);
    xum_diag("INIT ok: firmware, capabilities, status",
             devInfo[0], devInfo[1], devInfo[2]);









    if ((devInfo[2] & XUM1541_DOING_RESET) != 0) {


        accensione_da_fare = 0;
        log_message(xum_log, "the previous command was cut short, clearing "
                             "the two stalls");
        xum_diag("open: previous command cut short, clearing stalls", 0, 0, 0);
        circle_xum1541_clear_halts();
        xum_rimetti_in_passo();

        riaccensione_da_aspettare("apertura: il firmware ha tirato la linea");
    }

    xum_diag("open: done", 0, 0, 0);

    xum_open = 1;
    xum_non_sveglio = 0;
    *f = (CBM_FILE)&xum_handle_cookie;





    if (drive_commodore != 0 && accensione_da_fare) {
        accensione_da_fare = 0;
        if (linea_come_sul_ferro()) {
            xum_diag("open: drive Commodore, RESET della linea come all'accensione",
                     0, 0, 0);
            cbm_reset_linea();
        } else {
            xum_diag("open: Pi1541 + Commodore, il bus si guarda senza la linea",
                     0, 0, 0);
            accensione_mista();
        }
    }

    return 0;
}




static void xum_parcheggia(void)
{
    if (!xum_open || xum_parked) {
        return;
    }

    circle_xum1541_control_out(XUM1541_SHUTDOWN);
    xum_parked = 1;
}



















static void xum_sveglia(void)
{
    unsigned char devInfo[XUM_DEVINFO_SIZE];
    int len;
    int giro;
    int sbloccato = 0;

    if (!xum_parked) {
        return;
    }

    for (giro = 0; giro < 3; giro++) {
        memset(devInfo, 0, sizeof(devInfo));
        len = circle_xum1541_control_in(XUM1541_INIT, devInfo,
                                        sizeof(devInfo));
        if (len >= 3) {
            if ((devInfo[2] & XUM1541_DOING_RESET) != 0) {
                circle_xum1541_clear_halts();
                sbloccato = 1;

                riaccensione_da_aspettare("risveglio: il firmware ha tirato la linea");
            }


            if (sbloccato) {
                xum_rimetti_in_passo();
            }
            xum_parked = 0;
            xum_non_sveglio = 0;
            c_risvegli++;
            if (giro > 0) {
                log_message(xum_log, "xum1541: woke up at attempt %d",
                            giro + 1);
            }
            return;
        }



        circle_xum1541_clear_halts();
        sbloccato = 1;
        circle_sleep(100000);
    }

    log_error(xum_log, "xum1541: the adapter did not wake up after the "
                       "bus reset - staying parked, will try again");
    xum_non_sveglio = 1;
}




void raspi_opencbm_tick(void)
{



    xum_versa_col_comodo();



    warp_aggiorna();






    if (scolo_attivo) {
        if (!xum_open) {
            scolo_attivo = 0;
        } else {
            scolo_un_blocco();
        }
        return;
    }

    if (!xum_open || xum_parked || xum_zitto) {
        return;
    }






    if (!bus_a_riposo) {










        if (circle_get_ticks() - xum_ultimo_uso > XUM_ABBANDONO_US) {
            xum_riga("=== bus lasciato a meta': lo rimetto a riposo ===");
            log_message(xum_log, "xum1541: the machine walked away from the "
                                 "bus - putting it back at rest");
            c_bus_ripreso++;
            cbm_untalk(NULL);
            cbm_unlisten(NULL);
            bus_a_riposo = 1;
            bus_perso = 0;
        }
        return;
    }

    if (circle_get_ticks() - xum_ultimo_uso > XUM_IDLE_US) {
        xum_parcheggia();
    }
}

void CBMAPIDECL cbm_driver_close(CBM_FILE f)
{
    (void)f;

    if (xum_open) {

        passo_prima("spegnere l'adattatore (USB)");
        circle_xum1541_control_out(XUM1541_SHUTDOWN);
        passo_fatto("spegnere l'adattatore (USB)");
        passo_prima("chiudere l'adattatore (USB)");
        circle_xum1541_close();
        passo_fatto("chiudere l'adattatore (USB)");
        xum_open = 0;
    }
    xum_riga("CHIUSO");
}

const char * CBMAPIDECL cbm_get_driver_name(int port)
{
    (void)port;
    return "xum1541";
}




















static void auto_apertura(unsigned char dev, unsigned char secadr);
static void auto_nome_comincia(unsigned char dev, unsigned char secadr);
static void auto_nome_pezzo(const unsigned char *p, unsigned int n);
static void auto_nome_finito(void);
static int  auto_prova_al_talk(unsigned char dev, unsigned char secadr);
static int  auto_leggi(unsigned char *fuori);
static int  auto_eoi(void);
static void auto_chiudi(void);
static int  auto_serve(void);
static int  auto_interrotto(void);
static int  auto_da_parte(void);
static void auto_rimetti(int prima);

static void drive_scorda_tutti(void);



static int comando_lungo(unsigned char c)
{
    return c == 'N' || c == 'V' || c == 'C' || c == 'S' || c == 'D';
}

static void comando_scorda(void)
{
    canale15_ascolta = 0;
    comando_primo = 0;
}

static void comando_ascolta(unsigned char secadr)
{
    canale15_ascolta = ((secadr & 0x0f) == 15);
    comando_primo = 0;
}

static void comando_guarda(const unsigned char *p, unsigned int n)
{
    unsigned int k;

    if (!canale15_ascolta || comando_primo != 0 || p == NULL) {
        return;
    }
    for (k = 0; k < n; k++) {
        if (p[k] != ' ') {
            comando_primo = p[k];
            return;
        }
    }
}



static void comando_partito(void)
{
    if (canale15_ascolta && comando_lungo(comando_primo)) {
        drive_al_lavoro = comando_primo;
        xum_riga("il drive lavora al comando %c: la prossima frase sotto "
                 "ATN lo aspetta fino a %u s", drive_al_lavoro,
                 XUM_PAZIENZA_LAVORO_MS / 1000u);
    }
    comando_scorda();
}

int CBMAPIDECL cbm_listen(CBM_FILE f, unsigned char dev, unsigned char secadr)
{
    unsigned char dataBuf[2];

    (void)f;
    xum_riga("LISTEN %u,%u", dev, secadr);
    if (auto_interrotto()) {
        return 1;
    }

    if (bus_a_riposo && !atn_da_riposo_ok()) {
        return 1;
    }
    bus_a_riposo = 0;
    auto_nome_comincia(dev, secadr);
    comando_ascolta(secadr);

    dataBuf[0] = 0x20 | dev;
    dataBuf[1] = 0x60 | secadr;

    return xum_write(XUM1541_CBM | XUM_WRITE_ATN, dataBuf, 2) <= 0;
}

int CBMAPIDECL cbm_talk(CBM_FILE f, unsigned char dev, unsigned char secadr)
{
    unsigned char dataBuf[2];
    int era_a_riposo;

    (void)f;
    xum_riga("TALK %u,%u", dev, secadr);
    comando_scorda();
    if (auto_interrotto()) {
        return 1;
    }
    era_a_riposo = bus_a_riposo;
    bus_a_riposo = 0;





    if (auto_prova_al_talk(dev, secadr) == 0) {
        return 0;
    }


    if (era_a_riposo && !atn_da_riposo_ok()) {
        bus_a_riposo = 1;
        return 1;
    }

    dataBuf[0] = 0x40 | dev;
    dataBuf[1] = 0x60 | secadr;

    return xum_write(XUM1541_CBM | XUM_WRITE_ATN | XUM_WRITE_TALK,
                     dataBuf, 2) <= 0;
}

int CBMAPIDECL cbm_open(CBM_FILE f, unsigned char dev, unsigned char secadr,
                        const void *fname, size_t len)
{
    unsigned char dataBuf[2];
    int rc;

    (void)f;
    xum_riga("OPEN %u,%u len %u", dev, secadr, (unsigned int)len);
    if (auto_interrotto()) {
        return 1;
    }

    if (bus_a_riposo && !atn_da_riposo_ok()) {
        return 1;
    }
    bus_a_riposo = 0;
    if (fname == NULL) {
        auto_apertura(dev, secadr);
    }

    comando_ascolta(secadr);
    if (fname != NULL) {
        comando_guarda((const unsigned char *)fname, (unsigned int)len);
    }

    dataBuf[0] = 0x20 | dev;
    dataBuf[1] = 0xf0 | secadr;

    rc = xum_write(XUM1541_CBM | XUM_WRITE_ATN, dataBuf, 2) <= 0;
    if (rc != 0) {
        comando_scorda();
        return rc;
    }


    if (fname != NULL && len > 0) {
        if (xum_write(XUM1541_CBM, (const unsigned char *)fname,
                      (unsigned int)len) < 0) {
            comando_scorda();
            return 1;
        }
        if (xum_write(XUM1541_CBM | XUM_WRITE_ATN,
                      (const unsigned char *)"\x3f", 1) <= 0) {
            comando_scorda();
            return 1;
        }
        comando_partito();
    }

    return 0;
}

int CBMAPIDECL cbm_close(CBM_FILE f, unsigned char dev, unsigned char secadr)
{
    unsigned char dataBuf[2];
    int era_a_riposo;

    (void)f;
    xum_riga("CLOSE %u,%u", dev, secadr);
    comando_scorda();


    if (atn_rifiutato) {
        bus_a_riposo = 1;
        auto_chiudi();
        return 1;
    }
    era_a_riposo = bus_a_riposo;
    bus_a_riposo = 1;
    bus_perso = 0;
    auto_chiudi();




    if (scolo_attivo && !scolo_dentro) {
        return 0;
    }

    if (era_a_riposo && !atn_da_riposo_ok()) {
        return 1;
    }

    dataBuf[0] = 0x20 | dev;
    dataBuf[1] = 0xe0 | secadr;

    return xum_write(XUM1541_CBM | XUM_WRITE_ATN, dataBuf, 2) <= 0;
}

int CBMAPIDECL cbm_unlisten(CBM_FILE f)
{
    unsigned char dataBuf[1];

    (void)f;
    dataBuf[0] = 0x3f;

    xum_riga("UNLISTEN");


    if (atn_rifiutato) {
        comando_scorda();
        bus_a_riposo = 1;
        return 1;
    }



    if (auto_serve()) {
        comando_scorda();
        return 0;
    }
    bus_a_riposo = 1;
    bus_perso = 0;
    auto_nome_finito();


    if (scolo_attivo && !scolo_dentro) {
        comando_scorda();
        return 0;
    }

    if (xum_write(XUM1541_CBM | XUM_WRITE_ATN, dataBuf, 1) <= 0) {
        comando_scorda();
        return 1;
    }
    comando_partito();
    return 0;
}

int CBMAPIDECL cbm_untalk(CBM_FILE f)
{
    unsigned char dataBuf[1];

    (void)f;
    dataBuf[0] = 0x5f;

    xum_riga("UNTALK");
    comando_scorda();


    if (atn_rifiutato) {
        bus_a_riposo = 1;
        return 1;
    }
    bus_a_riposo = 1;



    if (auto_serve()) {
        auto_chiudi();
        bus_perso = 0;
        return 0;
    }
    bus_perso = 0;


    if (scolo_attivo && !scolo_dentro) {
        return 0;
    }
    return xum_write(XUM1541_CBM | XUM_WRITE_ATN, dataBuf, 1) <= 0;
}

int CBMAPIDECL cbm_raw_write(CBM_FILE f, const void *buf, size_t size)
{
    int n;

    (void)f;
    xum_riga("WRITE %u", (unsigned int)size);
    if (auto_interrotto()) {
        return -1;
    }
    auto_nome_pezzo((const unsigned char *)buf, (unsigned int)size);
    comando_guarda((const unsigned char *)buf, (unsigned int)size);
    n = xum_write(XUM1541_CBM, (const unsigned char *)buf,
                  (unsigned int)size);
    if (n != (int)size) {
        xum_riga("WRITE -> %d", n);
    }
    return n;
}

int CBMAPIDECL cbm_raw_read(CBM_FILE f, void *buf, size_t size)
{
    (void)f;



    if (auto_serve() && size == 1) {
        return auto_leggi((unsigned char *)buf) == 0 ? 1 : -1;
    }

    {
        int n;
        xum_riga("READ %u", (unsigned int)size);
        n = xum_read(XUM1541_CBM, (unsigned char *)buf, (unsigned int)size);
        if (n != (int)size) {
            xum_riga("READ -> %d", n);
        }
        return n;
    }
}


int CBMAPIDECL cbm_get_eoi(CBM_FILE f)
{
    (void)f;

    if (auto_serve()) {
        return auto_eoi();
    }

    {
        int e;
        xum_riga("EOI?");
        e = xum_ioctl(XUM1541_GET_EOI, 0, 0);
        if (e != 0) {
            xum_riga("EOI -> %d", e);
        }
        return e;
    }
}









#define RESET_GIA_FATTO_US          (500u * 1000u)
static unsigned long reset_gia_fatto_fino_a = 0;



static int uj_al_posto_del_reset(void);










static int reset_forza_linea = 0;

static int cbm_reset_linea(void)
{
    int esito;

    reset_forza_linea = 1;
    esito = cbm_reset(NULL);
    reset_forza_linea = 0;
    return esito;
}

int CBMAPIDECL cbm_reset(CBM_FILE f)
{
    unsigned long t_linea;

    (void)f;




















    if (!reset_forza_linea && !scolo_attivo && turbo_a_meta_file()) {
        svuota_il_turbo("RESET");
    }
    if (scolo_attivo && !linea_come_sul_ferro()) {
        return 0;
    }
    if (scolo_attivo && !reset_forza_linea) {
        linea_dopo_scolo = 1;
        xum_riga("RESET della macchina durante uno svuotamento: la linea "
                 "quando il drive ha finito il file");
        return 0;
    }










    if (reset_gia_fatto_fino_a != 0) {
        unsigned long ora = circle_get_ticks();
        int fresco = ((long)(ora - reset_gia_fatto_fino_a) < 0);

        reset_gia_fatto_fino_a = 0;
        if (fresco) {
            xum_riga("RESET saltato: molla() ha gia' rimesso a posto il "
                     "drive");
            return 0;
        }
    }





























    if (!reset_forza_linea && !linea_come_sul_ferro()) {
        int rimesse;








        xum_pazienza_corta = 1;
        rimesse = uj_al_posto_del_reset();
        xum_pazienza_corta = 0;
        if (!rimesse) {
            xum_riga("RESET della macchina: la linea non si tocca "
                     "(nessuna unita' da rimettere a posto)");
        }
        return 0;
    }






    if (linea_come_sul_ferro() && !reset_forza_linea) {
        xum_riga("RESET della macchina: drive Commodore, la linea come sul "
                 "ferro");
        turbo_lascia_per_la_linea();
    }











    bus_perso = 0;
    xum_zitto = 0;
    xum_zitto_fino_a = 0;
    if (!xum_pronto()) {
        return -1;
    }

    xum_riga("RESET");
    drive_scorda_tutti();














    circle_xum1541_control_out(XUM1541_SHUTDOWN);

    t_linea = circle_get_ticks();






    if (ripartenza_cura()) {
        if (linea_osservata() < 0) {
            xum_parked = 1;
            return xum_fallito("the adapter did not take the reset");
        }
        linea_ms_ultima = (unsigned long)(linea_presa_ms >= 0
                                          ? linea_presa_ms : linea_tenuta_ms);
        atn_rifiutato = 0;
        if (linea_presa_ms != -1) {
            riaccensione_da_aspettare("RESET della linea osservata");
        } else {
            solo_fino_a = 0;
            xum_riga("RESET della linea: nessun drive l'ha presa in %d ms",
                     linea_tenuta_ms);
        }
        xum_parked = 1;
        accensione_da_fare = 0;
        return 0;
    }
    if (circle_xum1541_control_out(XUM1541_RESET) < 0) {
        xum_parked = 1;
        return xum_fallito("the adapter did not take the reset");
    }










    linea_ms_ultima = (circle_get_ticks() - t_linea) / 1000u;
    xum_diag("reset: la linea, millesimi nel firmware",
             (int)linea_ms_ultima, 0, 0);
    if (linea_ms_ultima < LINEA_DRIVE_ASPETTATO_MS) {
        riaccensione_da_aspettare("RESET della linea");
    } else if (linea_ms_ultima >= LINEA_FIRMWARE_ARRESO_MS) {
        riaccensione_da_aspettare("RESET della linea: il firmware si e' "
                                  "arreso, il bus non si e' liberato");
    } else {
        solo_fino_a = 0;
        xum_riga("RESET della linea: il firmware ha aspettato i drive "
                 "(%lu ms)", linea_ms_ultima);
    }



    xum_parked = 1;

    accensione_da_fare = 0;

    return 0;
}













static void accensione_mista(void)
{
    int esito;

    xum_riga("ACCENSIONE: Pi1541 + Commodore, la linea no: una frase sotto "
             "ATN con la pazienza corta");
    xum_pazienza_corta = 1;
    esito = cbm_listen(NULL, 30, 14);
    cbm_unlisten(NULL);
    xum_pazienza_corta = 0;
    if (esito != 0 && (bus_perso || xum_zitto)) {
        xum_riga("ACCENSIONE: il bus era fermo, la ripresa ha tirato la linea");
        xum_diag("open: il bus era fermo, la ripresa ha tirato la linea",
                 0, 0, 0);
        bus_perso = 0;
        bus_perso_da = 0;
        xum_zitto = 0;
        xum_zitto_fino_a = 0;
    } else {
        xum_riga("ACCENSIONE: il bus e' libero (%d)", esito);
    }
    bus_a_riposo = 1;
}






























#define TURBO_MW_PASSO              0x23


#define TURBO_SA_LETTURA            0
#define TURBO_SA_COMANDI            15


#define TURBO_BLOCCO                254






#define TURBO_BLOCCHI_MAX           664













#define TURBO_SCADENZA_US           (8u * 1000u * 1000u)












#define TURBO_SCADENZA_CONFRONTO    ((TURBO_CONFRONTO / 256u) * 1000u * 1000u)









#define TURBO_SCADENZA_SILENZIO     (6u * 1000u * 1000u)



#define TURBO_RITORNO_US            (4u * 1000u * 1000u)

















#define TURBO_PROVA_BLOCCHI         256














#define TURBO_AVVIO_US              (300u * 1000u)



#define TURBO_PASSO_NIENTE          0
#define TURBO_PASSO_ADATTATORE      1
#define TURBO_PASSO_DIRETTORIO      2
#define TURBO_PASSO_NOME            3
#define TURBO_PASSO_APERTO          4
#define TURBO_PASSO_STATO           5
#define TURBO_PASSO_CARICATO        6
#define TURBO_PASSO_PARTITO         7
#define TURBO_PASSO_PRIMO_BLOCCO    8
#define TURBO_PASSO_TUTTI_BLOCCHI   9
#define TURBO_PASSO_FINITO          10

static const char *turbo_nome_passo(int passo)
{
    switch (passo) {
    case TURBO_PASSO_ADATTATORE:    return "no adapter";
    case TURBO_PASSO_DIRETTORIO:    return "reading directory";
    case TURBO_PASSO_NOME:          return "finding a file name";
    case TURBO_PASSO_APERTO:        return "opening the file";
    case TURBO_PASSO_STATO:         return "asking the drive";
    case TURBO_PASSO_CARICATO:      return "uploading drive code";
    case TURBO_PASSO_PARTITO:       return "starting the turbo";
    case TURBO_PASSO_PRIMO_BLOCCO:  return "first block";
    case TURBO_PASSO_TUTTI_BLOCCHI: return "reading blocks";
    case TURBO_PASSO_FINITO:        return "finished";
    default:                        return "nothing done";
    }
}


static int turbo_passo;
static char turbo_dice[48];













#define TURBO_TRACCIA_RIGHE         64
#define TURBO_TRACCIA_LARGA         72













static FILE *turbo_traccia_f;
static int turbo_traccia_n;
static int turbo_traccia_accesa;






static int turbo_traccia_subito = 1;
static char turbo_traccia_ram[TURBO_TRACCIA_RIGHE][TURBO_TRACCIA_LARGA];
static unsigned long turbo_traccia_zero;

static void turbo_traccia(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

static void turbo_traccia(const char *fmt, ...)
{
    va_list ap;
    char riga[TURBO_TRACCIA_LARGA];
    unsigned long prima_di_scrivere;
    int n;

    if (!turbo_traccia_accesa || turbo_traccia_n >= TURBO_TRACCIA_RIGHE) {
        return;
    }

    n = snprintf(riga, sizeof(riga), "%5lu ms  ",
                 (turbo_orologio() - turbo_traccia_zero) / 1000u);
    if (n < 0 || n >= (int)sizeof(riga)) {
        n = 0;
    }
    va_start(ap, fmt);
    vsnprintf(riga + n, sizeof(riga) - (size_t)n, fmt, ap);
    va_end(ap);

    if (!turbo_traccia_subito) {
        snprintf(turbo_traccia_ram[turbo_traccia_n],
                 TURBO_TRACCIA_LARGA, "%s", riga);
        turbo_traccia_n++;
        return;
    }
    turbo_traccia_n++;

    prima_di_scrivere = circle_get_ticks();

    if (turbo_traccia_f == NULL) {
        turbo_traccia_f = fopen("/TURBO.TXT", "w");
        if (turbo_traccia_f == NULL) {
            turbo_traccia_costo += circle_get_ticks() - prima_di_scrivere;
            turbo_traccia_accesa = 0;
            return;
        }
        fprintf(turbo_traccia_f,
                "Caricatore veloce: cosa ha fatto, nell'ordine.\n"
                "Se l'ultima riga dice \"prima di X\" e non c'e' il suo\n"
                "\"fatto\", e' X che non e' tornato.\n\n");
    }

    fprintf(turbo_traccia_f, "%s\n", riga);
    fflush(turbo_traccia_f);
    fsync(fileno(turbo_traccia_f));

    turbo_traccia_costo += circle_get_ticks() - prima_di_scrivere;
}





static void turbo_traccia_versa(void)
{
    FILE *f;
    int k;

    if (turbo_traccia_subito || turbo_traccia_n <= 0) {
        return;
    }
    f = fopen("/TURBO.TXT", "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "Caricatore veloce: cosa ha fatto, nell'ordine.\n"
               "Se l'ultima riga dice \"prima di X\" e non c'e' il suo\n"
               "\"fatto\", e' X che non e' tornato.\n\n");
    for (k = 0; k < turbo_traccia_n; k++) {
        fprintf(f, "%s\n", turbo_traccia_ram[k]);
    }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
}





static void turbo_traccia_guasto(const char *why)
{
    if (turbo_traccia_accesa && !turbo_traccia_subito) {
        turbo_traccia("GUASTO: %s", why);
        turbo_traccia_versa();
    }
}




static void turbo_traccia_chiudi(void)
{
    unsigned long prima_di_scrivere;

    if (turbo_traccia_f == NULL) {
        return;
    }
    prima_di_scrivere = circle_get_ticks();
    fclose(turbo_traccia_f);
    turbo_traccia_f = NULL;
    turbo_traccia_costo += circle_get_ticks() - prima_di_scrivere;
}



int raspi_opencbm_available(void);

















#define TURBO_RESPIRO_US            0

static unsigned int turbo_respiro_us = TURBO_RESPIRO_US;

void raspi_opencbm_set_respiro(int us)
{
    if (us < 0) { us = 0; }
    if (us > 20000) { us = 20000; }
    turbo_respiro_us = (unsigned int)us;
}

int raspi_opencbm_get_respiro(void)
{
    return (int)turbo_respiro_us;
}














#define TURBO_CONFRONTO             4096u

static int turbo_acceso = 0;

static void turbo_rimetti_il_bus(unsigned char dev);




int CBMAPIDECL cbm_iec_poll(CBM_FILE f)
{
    (void)f;
    return xum_ioctl(XUM1541_IEC_POLL, 0, 0);
}

void CBMAPIDECL cbm_iec_set(CBM_FILE f, int line)
{
    (void)f;
    xum_ioctl(XUM1541_IEC_SETRELEASE, (unsigned int)line, 0);
}

void CBMAPIDECL cbm_iec_release(CBM_FILE f, int line)
{
    (void)f;
    xum_ioctl(XUM1541_IEC_SETRELEASE, 0, (unsigned int)line);
}

void CBMAPIDECL cbm_iec_setrelease(CBM_FILE f, int set, int release)
{
    (void)f;
    xum_ioctl(XUM1541_IEC_SETRELEASE, (unsigned int)set, (unsigned int)release);
}




int CBMAPIDECL cbm_iec_wait(CBM_FILE f, int line, int state)
{
    (void)f;
    return xum_ioctl(XUM1541_IEC_WAIT, (unsigned int)line,
                     (unsigned int)state);
}

int CBMAPIDECL cbm_iec_get(CBM_FILE f, int line)
{
    int stato = cbm_iec_poll(f);

    if (stato < 0) {
        return -1;
    }
    return (stato & line) ? 1 : 0;
}



















#define DRIVE_BOH       0
#define DRIVE_1541      1
#define DRIVE_1570      2
#define DRIVE_1571      3
#define DRIVE_1581      4
#define DRIVE_ALTRO     5

static int drive_tipo[16];
static char drive_nome[16][16];






static int drive_tipo_prima[16];
static char drive_nome_prima[16][16];

static void drive_scorda_tutti(void)
{
    int k;

    for (k = 0; k < 16; k++) {
        drive_tipo[k] = DRIVE_BOH;
        drive_nome[k][0] = 0;
    }
}


static int drive_memoria(unsigned char dev, unsigned int addr,
                         unsigned char *dest, unsigned int n)
{
    unsigned char cmd[6];
    unsigned int k;

    cmd[0] = 'M';
    cmd[1] = '-';
    cmd[2] = 'R';
    cmd[3] = (unsigned char)(addr & 0xff);
    cmd[4] = (unsigned char)((addr >> 8) & 0xff);
    cmd[5] = (unsigned char)n;





    if (cbm_listen(NULL, dev, TURBO_SA_COMANDI)) {
        cbm_unlisten(NULL);
        return -1;
    }
    if (cbm_raw_write(NULL, cmd, sizeof(cmd)) != (int)sizeof(cmd)) {
        cbm_unlisten(NULL);
        return -1;
    }
    cbm_unlisten(NULL);

    if (cbm_talk(NULL, dev, TURBO_SA_COMANDI)) {
        cbm_untalk(NULL);
        return -1;
    }
    for (k = 0; k < n; k++) {
        if (cbm_raw_read(NULL, dest + k, 1) != 1) {
            cbm_untalk(NULL);
            return -1;
        }
    }
    cbm_untalk(NULL);
    return 0;
}




static int drive_chi_sei(unsigned char dev)
{
    unsigned char b[2];
    unsigned int magico;
    int tipo = DRIVE_ALTRO;
    const char *nome = "?";

    if (dev > 15) {
        return DRIVE_ALTRO;
    }







    if (drive_memoria(dev, 0xff40u, b, 2)) {
        turbo_traccia("chi sei: la M-R non e' tornata");
        return DRIVE_BOH;
    }
    magico = (unsigned int)b[0] | ((unsigned int)b[1] << 8);

    if (magico == 0xaaaau) {


        if (drive_memoria(dev, 0xfffeu, b, 2) == 0) {
            unsigned int m2 = (unsigned int)b[0] | ((unsigned int)b[1] << 8);
            if (m2 != 0xfe67u) {
                magico = m2;
            }
        }
    }

    switch (magico) {
        case 0xaaaau: tipo = DRIVE_1541; nome = "1540/1541";  break;
        case 0xf00fu: tipo = DRIVE_1541; nome = "1541-II";    break;
        case 0xcd18u: tipo = DRIVE_1541; nome = "1541C";      break;
        case 0x8085u: tipo = DRIVE_1541; nome = "JiffyDOS";   break;
        case 0x10cau: tipo = DRIVE_1541; nome = "DolphinDOS"; break;
        case 0x6f10u: tipo = DRIVE_1541; nome = "SpeedDOS";   break;
        case 0x2710u: tipo = DRIVE_1541; nome = "ProfDOS";    break;
        case 0xaeeau: tipo = DRIVE_1541; nome = "64er DOS";   break;
        case 0xfed7u: tipo = DRIVE_1570; nome = "1570";       break;
        case 0x02acu: tipo = DRIVE_1571; nome = "1571";       break;
        case 0x01bau: tipo = DRIVE_1581; nome = "1581";       break;
        case 0xfeb6u: tipo = DRIVE_ALTRO; nome = "2031";      break;
        default:      tipo = DRIVE_ALTRO; nome = "sconosciuto"; break;
    }



    if (drive_tipo[dev] != tipo) {
        log_message(xum_log, "xum1541: unit %d is a %s (footprint %04x)",
                    dev, nome, magico);
        xum_riga("DRIVE %d e' un %s (%04x)", dev, nome, magico);
    }
    drive_tipo[dev] = tipo;
    snprintf(drive_nome[dev], sizeof(drive_nome[dev]), "%s", nome);
    if (tipo != DRIVE_BOH && tipo != DRIVE_ALTRO) {
        drive_tipo_prima[dev] = tipo;
        snprintf(drive_nome_prima[dev], sizeof(drive_nome_prima[dev]),
                 "%s", nome);
    }
    turbo_traccia("chi sei: %04x, un %s", magico, nome);
    return tipo;
}






static int drive_va_col_turbo(int tipo)
{
    return tipo == DRIVE_1541 || tipo == DRIVE_1570
        || tipo == DRIVE_1571 || tipo == DRIVE_1581;
}






























#define SOLO_VALE_US                (60u * 1000u * 1000u)
static unsigned char solo_dev = 0;
static int turbo_non_solo = 0;




static int unita_sul_bus(unsigned char dev)
{
    unsigned char frase[2];
    unsigned char via = 0x3f;

    frase[0] = (unsigned char)(0x20 | (dev & 0x1f));
    frase[1] = 0x60 | 14;
    if (xum_write(XUM1541_CBM | XUM_WRITE_ATN, frase, 2) == 2) {
        xum_write(XUM1541_CBM | XUM_WRITE_ATN, &via, 1);
        return 1;
    }
    if (!xum_open || xum_zitto || bus_perso || turbo_scaduto()) {
        return -1;
    }
    return 0;
}



static int altro_sul_bus(unsigned char dev)
{
    unsigned int u;

    if (solo_fino_a != 0 && solo_dev == dev
        && (long)(circle_get_ticks() - solo_fino_a) < 0) {
        return 0;
    }
    solo_fino_a = 0;
    for (u = 8; u <= 34; u++) {
        unsigned char chi = (unsigned char)(u <= 30 ? u : u - 27);
        int c;

        if (chi == dev) {
            continue;
        }
        c = unita_sul_bus(chi);
        if (c < 0) {
            xum_riga("BUS: non si e' potuto chiedere dell'unita' %u", chi);
            return -1;
        }
        if (c > 0) {
            xum_riga("BUS: c'e' anche l'unita' %u: il turbo no", chi);
            return chi;
        }
    }
    solo_dev = dev;
    solo_fino_a = circle_get_ticks() + SOLO_VALE_US;
    if (solo_fino_a == 0) {
        solo_fino_a = 1;
    }
    xum_riga("BUS: l'unita' %u e' sola", dev);
    return 0;
}




static int turbo_comando(unsigned char dev, const unsigned char *cmd,
                         unsigned int n)
{
    int esito;

    if (cbm_listen(NULL, dev, TURBO_SA_COMANDI)) {
        return -1;
    }
    esito = (cbm_raw_write(NULL, cmd, n) == (int)n) ? 0 : -1;
    cbm_unlisten(NULL);

    return esito;
}



static int turbo_stato(unsigned char dev, char *testo, unsigned int max)
{
    unsigned int i = 0;

    if (max < 4) {
        return 99;
    }
    testo[0] = 0;

    if (cbm_talk(NULL, dev, TURBO_SA_COMANDI)) {
        return 99;
    }

    while (i + 1 < max) {
        unsigned char c;

        if (cbm_raw_read(NULL, &c, 1) != 1) {
            break;
        }
        if (c == '\r') {
            break;
        }
        testo[i++] = (char)c;
        if (cbm_get_eoi(NULL)) {
            break;
        }
    }
    testo[i] = 0;
    cbm_untalk(NULL);

    if (i < 2 || testo[0] < '0' || testo[0] > '9'
              || testo[1] < '0' || testo[1] > '9') {
        return 99;
    }
    return (testo[0] - '0') * 10 + (testo[1] - '0');
}



static int turbo_carica(unsigned char dev, unsigned int addr,
                        const unsigned char *p, unsigned int n)
{
    unsigned char mw[6 + TURBO_MW_PASSO];
    unsigned int fatti = 0;

    while (fatti < n) {
        unsigned int quanti = n - fatti;
        unsigned int a = addr + fatti;

        if (quanti > TURBO_MW_PASSO) {
            quanti = TURBO_MW_PASSO;
        }

        mw[0] = 'M';
        mw[1] = '-';
        mw[2] = 'W';
        mw[3] = (unsigned char)(a & 0xff);
        mw[4] = (unsigned char)((a >> 8) & 0xff);
        mw[5] = (unsigned char)quanti;
        memcpy(mw + 6, p + fatti, quanti);

        turbo_traccia("prima di M-W $%04x, %u byte", a, quanti);
        if (turbo_comando(dev, mw, 6 + quanti)) {
            xum_riga("TURBO M-W $%04x fallito", a);
            turbo_traccia("M-W $%04x NON e' andato", a);
            return -1;
        }
        fatti += quanti;
    }

    return 0;
}




static int s2_avvia(void)
{
    unsigned long fino_a;

    if (cbm_iec_poll(NULL) < 0) {

        return -1;
    }
    cbm_iec_release(NULL, IEC_CLOCK);


    fino_a = turbo_orologio() + TURBO_AVVIO_US;
    for (;;) {
        int stato = cbm_iec_poll(NULL);

        if (stato < 0) {
            return -1;
        }
        if (stato & IEC_CLOCK) {
            break;
        }
        if ((long)(turbo_orologio() - fino_a) >= 0) {
            xum_riga("TURBO il drive non ha preso CLOCK in %u ms",
                     TURBO_AVVIO_US / 1000u);
            return -1;
        }
    }

    cbm_iec_set(NULL, IEC_ATN);
    circle_sleep(20000);

    return 0;
}

static void s2_esci(void)
{
    cbm_iec_release(NULL, IEC_ATN);
    cbm_iec_release(NULL, IEC_DATA);
    cbm_iec_set(NULL, IEC_CLOCK);
    circle_sleep(20000);

    turbo_acceso = 0;
    bus_a_riposo = 1;
    bus_perso = 0;
}


















static unsigned long s2_us_aspetta, s2_us_conto, s2_us_dati;



static unsigned long s2_us_nostro;
static long s2_n_blocchi;

static void s2_conti_azzera(void)
{
    s2_us_aspetta = s2_us_conto = s2_us_dati = 0;
    s2_us_nostro = 0;
    s2_n_blocchi = 0;
}










static int s2_errore(void)
{
    int linea;
    int sbagliato;
    unsigned long t0, ta;

    t0 = circle_get_ticks();
    cbm_iec_release(NULL, IEC_ATN);
    s2_us_nostro += circle_get_ticks() - t0;

    ta = circle_get_ticks();
    if (cbm_iec_wait(NULL, IEC_CLOCK, 0) < 0) {
        s2_us_aspetta += circle_get_ticks() - ta;
        return -1;
    }
    s2_us_aspetta += circle_get_ticks() - ta;




    t0 = circle_get_ticks();
    linea = cbm_iec_get(NULL, IEC_DATA);
    s2_us_nostro += circle_get_ticks() - t0;
    if (linea < 0) {
        return -1;
    }
    sbagliato = (linea == 0);
    if (!sbagliato) {




        t0 = circle_get_ticks();
        cbm_iec_setrelease(NULL, IEC_DATA | IEC_ATN, 0);
        s2_us_nostro += circle_get_ticks() - t0;

        ta = circle_get_ticks();
        cbm_iec_wait(NULL, IEC_CLOCK, 1);
        s2_us_aspetta += circle_get_ticks() - ta;

        t0 = circle_get_ticks();
        cbm_iec_release(NULL, IEC_DATA);
        s2_us_nostro += circle_get_ticks() - t0;
    }

    return sbagliato;
}







static int s2_blocco(unsigned char *dest, unsigned int max)
{
    unsigned char quanti;
    int dichiarati;
    unsigned long t0;

    t0 = circle_get_ticks();
    if (xum_read(XUM1541_S2, &quanti, 1) != 1) {
        s2_us_conto += circle_get_ticks() - t0;
        return -1;
    }
    s2_us_conto += circle_get_ticks() - t0;

    dichiarati = quanti;
    if (quanti == 0xff) {
        quanti--;
    }
    if (quanti > max) {
        xum_riga("TURBO blocco da %u, ne stanno %u", quanti, max);
        return -1;
    }
    if (quanti == 0) {
        s2_n_blocchi++;
        return dichiarati;
    }

    t0 = circle_get_ticks();
    if (xum_read(XUM1541_S2, dest, quanti) != (int)quanti) {
        s2_us_dati += circle_get_ticks() - t0;
        return -1;
    }
    s2_us_dati += circle_get_ticks() - t0;
    s2_n_blocchi++;

    return dichiarati;
}




















static struct {
    unsigned char dev;
    int aperta;
    int nostro_canale;
    int finita;
    int rotta;
    int blocchi;
    unsigned long inizio;
    unsigned long ultimo_blocco;
} sess;








static int turbo_sporcato = 0;


static void turbo_lascia_pulito(unsigned char dev);
static void auto_avviso(const char *breve);









static void traccia_esadecimale(const char *che, const unsigned char *p,
                                int n)
{
    char riga[3 * 24 + 1];
    int k, q = 0;

    if (p == NULL || n <= 0) {
        return;
    }
    if (n > 24) {
        n = 24;
    }
    for (k = 0; k < n; k++) {
        q += snprintf(riga + q, sizeof(riga) - (size_t)q, "%02x ", p[k]);
    }
    turbo_traccia("%s (%d byte): %s", che, n, riga);
}
static int scolo_blocchi = 0;


#define TURBO_SCADENZA_SCOLO        (3u * 1000u * 1000u)
static void turbo_meta_file_resetta(void);
static int turbo_sess_chiudi_coda(void);



static int turbo_a_meta_file(void)
{
    return sess.aperta && !sess.finita && !sess.rotta;
}












static void turbo_riapri_il_canale(unsigned char dev, const char *nome,
                                   int nomelen)
{
    turbo_traccia("il DOS e' ripartito: riapro il file per la strada di "
                  "sempre");
    if (cbm_open(NULL, dev, TURBO_SA_LETTURA, nome, (size_t)nomelen)) {
        turbo_traccia("il file non si riapre");
    }

    bus_a_riposo = 1;
    bus_perso = 0;
}

static int turbo_sess_apri(unsigned char dev, const char *nome, int nomelen,
                           int gia_aperto)
{
    unsigned char avvio[5];
    char testo[48];
    int stato;
    int tipo;
    const unsigned char *lettore;
    const unsigned char *pezzo_s2;
    unsigned int lettore_n, s2_n;

    memset(&sess, 0, sizeof(sess));
    sess.dev = dev;
    s2_conti_azzera();
    turbo_non_solo = 0;














    if (!xum_pronto()) {
        turbo_passo = TURBO_PASSO_ADATTATORE;
        snprintf(turbo_dice, sizeof(turbo_dice), "the adapter did not answer");
        return -1;
    }
    sess.inizio = turbo_orologio();
    sess.ultimo_blocco = sess.inizio;

    if (!gia_aperto) {
        turbo_traccia("prima di OPEN 8,0");
        if (cbm_open(NULL, dev, TURBO_SA_LETTURA, NULL, 0)) {
            snprintf(turbo_dice, sizeof(turbo_dice),
                     "the bus did not take OPEN");
            turbo_rimetti_il_bus(dev);
            return -1;
        }
        sess.nostro_canale = 1;
        if (cbm_raw_write(NULL, nome, (size_t)nomelen) != nomelen) {
            snprintf(turbo_dice, sizeof(turbo_dice), "the name did not go out");
            turbo_rimetti_il_bus(dev);
            return -1;
        }
        cbm_unlisten(NULL);
    } else {










        bus_a_riposo = 1;
        bus_perso = 0;
    }
    turbo_passo = TURBO_PASSO_APERTO;
    turbo_traccia("aperto, prima del canale d'errore");

    stato = turbo_stato(dev, testo, sizeof(testo));
    if (stato != 0) {
        xum_riga("TURBO il drive dice: %s", testo);




        if (testo[0] != 0) {
            snprintf(turbo_dice, sizeof(turbo_dice), "%s", testo);
        } else {
            snprintf(turbo_dice, sizeof(turbo_dice), "no status: %s",
                     xum_zitto ? "adapter hushed" : "drive said nothing");
        }
        turbo_traccia("canale d'errore muto (zitto %d, perso %d)",
                      xum_zitto, bus_perso);
        if (sess.nostro_canale) {
            cbm_close(NULL, dev, TURBO_SA_LETTURA);
        }
        return -1;
    }
    turbo_passo = TURBO_PASSO_STATO;



    turbo_traccia("il drive dice: %s", testo);






    turbo_non_solo = altro_sul_bus(dev);
    if (turbo_non_solo != 0) {
        if (turbo_non_solo > 0) {
            snprintf(turbo_dice, sizeof(turbo_dice),
                     "drive %d is on the bus too", turbo_non_solo);
        } else {
            snprintf(turbo_dice, sizeof(turbo_dice),
                     "could not ask who is on the bus");
        }
        turbo_traccia("non e' solo sul bus (%d): il turbo no, si va come "
                      "sempre", turbo_non_solo);
        if (sess.nostro_canale) {
            cbm_close(NULL, dev, TURBO_SA_LETTURA);
            sess.nostro_canale = 0;
        }
        return -1;
    }
















    tipo = drive_chi_sei(dev);










    if (tipo == DRIVE_BOH || tipo == DRIVE_ALTRO) {
        turbo_traccia("impronta illeggibile: I0 e richiedo");
        turbo_comando(dev, (const unsigned char *)"I0", 2);
        tipo = drive_chi_sei(dev);
    }














    if ((tipo == DRIVE_BOH || tipo == DRIVE_ALTRO)
     && drive_tipo_prima[dev & 15] != DRIVE_BOH
     && drive_tipo_prima[dev & 15] != DRIVE_ALTRO) {
        tipo = drive_tipo_prima[dev & 15];
        turbo_traccia("impronta ancora illeggibile: uso quella di prima "
                      "(%s)", drive_nome_prima[dev & 15]);
        snprintf(drive_nome[dev & 15], sizeof(drive_nome[0]), "%s",
                 drive_nome_prima[dev & 15]);
    }

    switch (tipo) {
        case DRIVE_1541:
            lettore = turbo_lettore;
            lettore_n = (unsigned int)sizeof(turbo_lettore);
            pezzo_s2 = turbo_s2;
            s2_n = (unsigned int)sizeof(turbo_s2);
            break;
        case DRIVE_1570:
        case DRIVE_1571:
            lettore = turbo_lettore_1571;
            lettore_n = (unsigned int)sizeof(turbo_lettore_1571);
            pezzo_s2 = turbo_s2;
            s2_n = (unsigned int)sizeof(turbo_s2);
            break;
        case DRIVE_1581:
            lettore = turbo_lettore_1581;
            lettore_n = (unsigned int)sizeof(turbo_lettore_1581);
            pezzo_s2 = turbo_s2_1581;
            s2_n = (unsigned int)sizeof(turbo_s2_1581);
            break;
        default:

            if (tipo == DRIVE_BOH) {
                snprintf(turbo_dice, sizeof(turbo_dice),
                         "the drive does not answer");
            } else {
                snprintf(turbo_dice, sizeof(turbo_dice),
                         "a %s, no turbo for it",
                         drive_nome[dev & 15][0] ? drive_nome[dev & 15]
                                                 : "?");
            }
            turbo_passo = TURBO_PASSO_ADATTATORE;
            if (sess.nostro_canale) {
                cbm_close(NULL, dev, TURBO_SA_LETTURA);
                sess.nostro_canale = 0;
            }
            return -1;
    }

    turbo_traccia("un %s: prima di caricare $0500 (%u byte)",
                  drive_nome[dev & 15][0] ? drive_nome[dev & 15] : "?",
                  lettore_n);




    if (tipo == DRIVE_1570 || tipo == DRIVE_1571) {
        turbo_traccia("prima di U0>M1 (modo 1571)");
        turbo_comando(dev, (const unsigned char *)"U0>M1", 5);
    }



    if (turbo_carica(dev, TURBO_LETTORE_ORG, lettore, lettore_n)
     || turbo_carica(dev, TURBO_S2_ORG, pezzo_s2, s2_n)) {
        turbo_stato(dev, testo, sizeof(testo));
        snprintf(turbo_dice, sizeof(turbo_dice), "%s", testo);
        if (sess.nostro_canale) {
            cbm_close(NULL, dev, TURBO_SA_LETTURA);
        }
        turbo_lascia_pulito(dev);
        return -1;
    }
    turbo_passo = TURBO_PASSO_CARICATO;
    turbo_sporcato = 1;
    turbo_traccia("caricate tutte e due, prima di U4:");



    avvio[0] = 'U';
    avvio[1] = '4';
    avvio[2] = ':';
    avvio[3] = 0;
    avvio[4] = 0;

    turbo_acceso = 1;
    sess.aperta = 1;









    if (turbo_comando(dev, avvio, sizeof(avvio))) {
        xum_riga("TURBO il drive non prende U4");
        turbo_stato(dev, testo, sizeof(testo));
        snprintf(turbo_dice, sizeof(turbo_dice), "U4: %.40s", testo);
        s2_esci();
        sess.aperta = 0;
        if (sess.nostro_canale) {
            cbm_close(NULL, dev, TURBO_SA_LETTURA);
        }
        turbo_lascia_pulito(dev);
        if (gia_aperto) {
            turbo_riapri_il_canale(dev, nome, nomelen);
        }
        return -1;
    }
    turbo_traccia("U4 preso, prima di prendere il bus");

    if (s2_avvia()) {



        xum_riga("TURBO il drive non ha preso il bus");
        snprintf(turbo_dice, sizeof(turbo_dice), "drive never took the bus");










        turbo_traccia("non ha preso il bus: e' dentro il nostro 6502, solo "
                      "la linea lo tira fuori");
        turbo_meta_file_resetta();
        if (gia_aperto) {
            turbo_riapri_il_canale(dev, nome, nomelen);
        }
        return -1;
    }
    turbo_passo = TURBO_PASSO_PARTITO;


    bus_a_riposo = 0;
    turbo_traccia("il drive ha preso il bus, prima del primo blocco");



    if (turbo_respiro_us) {
        circle_sleep((long)turbo_respiro_us);
    }
    return 0;
}



static int turbo_sess_blocco(unsigned char *dest, int max, int *ultimo)
{
    int quanti;
    int err;

    *ultimo = 0;
    if (!sess.aperta || sess.finita) {
        return -1;
    }

    if (turbo_orologio() - sess.ultimo_blocco > TURBO_SCADENZA_SILENZIO) {
        xum_riga("TURBO zitto da troppo dopo %d blocchi", sess.blocchi);
        snprintf(turbo_dice, sizeof(turbo_dice), "went quiet after %d blocks",
                 sess.blocchi);
        sess.rotta = 1;
        return -1;
    }
    if (sess.blocchi >= TURBO_BLOCCHI_MAX) {
        xum_riga("TURBO troppi blocchi: fuori passo");
        snprintf(turbo_dice, sizeof(turbo_dice), "out of step");
        sess.rotta = 1;
        return -1;
    }

    err = s2_errore();
    if (err != 0) {
        snprintf(turbo_dice, sizeof(turbo_dice),
                 err < 0 ? "no answer before block %d"
                         : "drive read error at block %d", sess.blocchi + 1);
        sess.rotta = 1;
        return -1;
    }
    if (turbo_respiro_us) {
        circle_sleep((long)turbo_respiro_us);
    }

    quanti = s2_blocco(dest, (unsigned int)max);
    if (quanti < 0) {
        snprintf(turbo_dice, sizeof(turbo_dice), "block %d never came",
                 sess.blocchi + 1);
        sess.rotta = 1;
        return -1;
    }
    if (sess.blocchi == 0) {
        turbo_passo = TURBO_PASSO_PRIMO_BLOCCO;
        turbo_traccia("primo blocco: %d byte", quanti);
    }
    if (turbo_respiro_us) {
        circle_sleep((long)turbo_respiro_us);
    }

    sess.blocchi++;
    sess.ultimo_blocco = turbo_orologio();
    if (quanti != 0xff) {
        sess.finita = 1;
        *ultimo = 1;
        return quanti;
    }
    return TURBO_BLOCCO;
}















static void drive_aspetta_che_torni(unsigned char dev)
{
    unsigned long t0;
    unsigned long prima;
    char testo[48];
    int giri = 0;




    prima = xum_via_libera(TURBO_RITORNO_US + 1000000u);
    t0 = turbo_orologio();

    for (;;) {
        giri++;
        if (turbo_stato(dev, testo, sizeof(testo)) != 99) {
            turbo_traccia("il drive e' tornato dopo %lu ms (%d giri): %s",
                          (turbo_orologio() - t0) / 1000u, giri, testo);
            xum_via_libera_fine(prima);
            return;
        }
        if (turbo_orologio() - t0 > TURBO_RITORNO_US) {
            turbo_traccia("il drive non e' tornato in %lu ms",
                          (unsigned long)(TURBO_RITORNO_US / 1000u));
            xum_riga("TURBO il drive non torna dopo il reset");
            xum_via_libera_fine(prima);
            return;
        }
        circle_sleep(100000);
    }
}






static unsigned long turbo_chiusura_us = 0;


































static int uj_al_posto_del_reset(void)
{
    unsigned char u;
    int quante = 0;
    int risponde = 0;
    char testo[48];

    if (!xum_open || xum_parked || bus_perso || xum_zitto) {
        return 0;
    }

    for (u = 8; u <= 11; u++) {


        if (drive_tipo_prima[u] == DRIVE_BOH
         || drive_tipo_prima[u] == DRIVE_ALTRO) {
            continue;
        }
        quante++;
        if (turbo_comando(u, (const unsigned char *)"UJ", 2) == 0) {
            risponde = 1;





            uj_fino_a = circle_get_ticks() + UJ_RIACCENSIONE_US;
            if (uj_fino_a == 0) {
                uj_fino_a = 1;
            }
        }
    }
    if (quante == 0 || !risponde) {
        return 0;
    }

    turbo_sporcato = 0;
    uj_fino_a = circle_get_ticks() + UJ_RIACCENSIONE_US;
    if (uj_fino_a == 0) {
        uj_fino_a = 1;
    }


    (void)testo;
    xum_riga("RESET: UJ a %d unita', la linea non si tocca", quante);
    return 1;
}

static void turbo_lascia_pulito(unsigned char dev)
{



    unsigned long prima = xum_via_libera(TURBO_SCADENZA_PULIZIA);

    if (turbo_sporcato) {






        turbo_traccia("prima di UJ (il DOS del drive riparte)");
        turbo_comando(dev, (const unsigned char *)"UJ", 2);
        turbo_sporcato = 0;
        turbo_traccia("UJ mandato (zitto %d, perso %d)", xum_zitto,
                      bus_perso);




















        turbo_dopo_uj = 1;
        uj_fino_a = circle_get_ticks() + UJ_RIACCENSIONE_US;
        if (uj_fino_a == 0) {
            uj_fino_a = 1;
        }
    } else {
        turbo_traccia("prima della I0 di pulizia");
        turbo_comando(dev, (const unsigned char *)"I0", 2);
        turbo_traccia("I0 fatta (zitto %d, perso %d)", xum_zitto, bus_perso);
    }
    xum_via_libera_fine(prima);
}

static void scolo_comincia(void)
{
    scolo_attivo = 1;
    scolo_blocchi = 0;
    xum_riga("TURBO fermato a meta': svuoto il resto (letti %d blocchi)",
             sess.blocchi);
    turbo_traccia("fermato a meta' dopo %d blocchi: svuoto invece di "
                  "resettare", sess.blocchi);
    auto_avviso("FAST STOP");
}



static int scolo_un_blocco(void)
{
    unsigned char cestino[TURBO_BLOCCO];
    unsigned long prima;
    int presi, ultimo;
    int zitto;

    if (!scolo_attivo) {
        return 0;
    }




    prima = turbo_fino_a;
    scolo_dentro = 1;
    turbo_scadenza_metti(TURBO_SCADENZA_SCOLO);
    presi = turbo_sess_blocco(cestino, (int)sizeof(cestino), &ultimo);
    turbo_fino_a = prima;
    scolo_dentro = 0;

    if (presi >= 0) {
        char breve[24];

        scolo_blocchi++;
        snprintf(breve, sizeof(breve), "FAST STOP  %d", scolo_blocchi);
        auto_avviso(breve);
        if (!ultimo) {
            return 1;
        }
    }

    scolo_attivo = 0;








    scolo_dentro = 1;
    zitto = auto_da_parte();
    if (presi < 0) {


        turbo_traccia("svuotamento rotto dopo %d blocchi: %s", scolo_blocchi,
                      turbo_dice);
        xum_riga("TURBO svuotamento rotto dopo %d blocchi", scolo_blocchi);
        turbo_meta_file_resetta();
    } else {
        turbo_traccia("svuotato: altri %d blocchi, il drive e' tornato al "
                      "DOS da solo", scolo_blocchi);
        xum_riga("TURBO svuotato: altri %d blocchi", scolo_blocchi);
        turbo_sess_chiudi_coda();
    }



    if (linea_dopo_scolo) {
        linea_dopo_scolo = 0;
        if (presi >= 0) {
            xum_riga("RESET della macchina arrivato durante lo svuotamento: "
                     "la linea adesso");
            cbm_reset_linea();
        }
    }
    auto_rimetti(zitto);
    scolo_dentro = 0;


    turbo_traccia_accesa = 0;
    turbo_traccia_versa();
    return 0;
}



static void scolo_finisci(void)
{
    while (scolo_attivo) {





        bmc_giri[1]++;
        if (!scolo_un_blocco()) {
            break;
        }
    }
}

static int turbo_sess_chiudi(void)
{
    if (!sess.aperta) {
        return sess.rotta ? -1 : 0;
    }










    if (!sess.finita && !sess.rotta) {






        scolo_comincia();
        return 0;
    }
    if (!sess.finita) {
        turbo_meta_file_resetta();
        return -1;
    }

    return turbo_sess_chiudi_coda();
}





static void turbo_meta_file_resetta(void)
{
    sess.aperta = 0;
    s2_esci();

    turbo_traccia("rotto a meta': il drive e' ancora dentro il suo giro, "
                  "RESET");
    xum_riga("TURBO fermati a meta' file: RESET del drive");



    {
        unsigned long prima = xum_via_libera(TURBO_SCADENZA_PULIZIA);
        cbm_reset_linea();


        turbo_sporcato = 0;
        xum_via_libera_fine(prima);
    }
    sess.nostro_canale = 0;
    turbo_acceso = 0;
    bus_a_riposo = 1;
    bus_perso = 0;
    drive_aspetta_che_torni(sess.dev);
    turbo_lascia_pulito(sess.dev);
}





static int turbo_sess_chiudi_coda(void)
{
    char testo[48];
    int stato;

    sess.aperta = 0;
    s2_esci();



    cbm_listen(NULL, sess.dev, TURBO_SA_LETTURA);
    cbm_unlisten(NULL);

    turbo_traccia("bus lasciato, prima del canale d'errore finale");

    stato = turbo_stato(sess.dev, testo, sizeof(testo));
    turbo_traccia("il drive dice %d", stato);
    if (stato != 0) {
        xum_riga("TURBO finito male: %s", testo);
        if (turbo_dice[0] == 0) {
            snprintf(turbo_dice, sizeof(turbo_dice), "%s", testo);
        }
        sess.rotta = 1;
    } else if (!sess.rotta) {
        turbo_passo = TURBO_PASSO_FINITO;
    }

    if (sess.nostro_canale) {
        cbm_close(NULL, sess.dev, TURBO_SA_LETTURA);
        sess.nostro_canale = 0;
    }













    turbo_lascia_pulito(sess.dev);

    return sess.rotta ? -1 : 0;
}






































#define AUTO_NOME_MAX 40

static int auto_veloce = 1;
static int auto_zitto = 0;
static int auto_sa = -1;
static unsigned char auto_dev;
static unsigned char auto_nome[AUTO_NOME_MAX];
static int auto_nome_n;
static int auto_raccolgo;


static int auto_detto;
static int auto_in_corso;
static int auto_blocchi_fatti;



















#define AUTO_MAGAZZINO 8
static unsigned char auto_blocco[AUTO_MAGAZZINO][TURBO_BLOCCO];
static int auto_blocco_n[AUTO_MAGAZZINO];
static int auto_mag_n;
static int auto_mag_k;
static int auto_blocco_i;
static int auto_rotto;
static int auto_ultimo;
static long auto_byte_dati;
static long c_veloci = 0;
static long c_veloci_falliti = 0;

void raspi_opencbm_set_veloce(int acceso)
{
    auto_veloce = acceso ? 1 : 0;
}

int raspi_opencbm_get_veloce(void)
{
    return auto_veloce;
}


void raspi_opencbm_set_commodore(int si)
{

    drive_commodore = (si == 1 || si == 2) ? si : 0;
}

int raspi_opencbm_get_commodore(void)
{
    return drive_commodore;
}

static int auto_serve(void)
{
    return auto_in_corso && !auto_zitto;
}


























static int muto_dice_no(void)
{
    unsigned long adesso;

    if (muto_fino_a == 0 || scolo_dentro || auto_zitto) {
        return 0;
    }
    adesso = circle_get_ticks();
    if (!scolo_attivo && (long)(adesso - muto_fino_a) >= 0) {
        muto_fino_a = 0;
        return 0;
    }


    muto_fino_a = adesso + XUM_MUTO_US;
    if (muto_fino_a == 0) {
        muto_fino_a = 1;
    }
    return 1;
}

static int auto_interrotto(void)
{
    if (auto_serve()) {
        xum_riga("VELOCE interrotto: la macchina parla al drive senza "
                 "UNTALK");
        turbo_traccia("la macchina parla al drive a meta' file: ha un "
                      "caricatore suo, si svuota");
        auto_chiudi();
        muto_fino_a = circle_get_ticks() + XUM_MUTO_US;
        if (muto_fino_a == 0) {
            muto_fino_a = 1;
        }
    }
    return muto_dice_no();
}














static int auto_da_parte(void)
{
    int prima = auto_zitto;
    auto_zitto = 1;
    return prima;
}

static void auto_rimetti(int prima)
{
    auto_zitto = prima;
}

static void auto_scorda(void)
{
    auto_sa = -1;
    auto_nome_n = 0;
    auto_raccolgo = 0;
}














static void auto_apertura(unsigned char dev, unsigned char secadr)
{
    if (auto_zitto) {
        return;
    }
    auto_dev = dev;
    auto_sa = (int)secadr;
    auto_nome_n = 0;
    auto_raccolgo = 1;
    auto_detto = 0;
}



static void auto_nome_comincia(unsigned char dev, unsigned char secadr)
{
    (void)dev;
    (void)secadr;
    if (auto_zitto) {
        return;
    }
    auto_raccolgo = 0;
}






static void auto_nome_nel_registro(void)
{
    if (auto_nome_n > 0) {
        xum_riga("NOME '%.*s' sul canale %d", auto_nome_n, auto_nome,
                 auto_sa);
    }
}

static void auto_nome_pezzo(const unsigned char *p, unsigned int n)
{
    if (auto_zitto || !auto_raccolgo || p == NULL) {
        return;
    }
    while (n-- > 0) {
        if (auto_nome_n < AUTO_NOME_MAX) {
            auto_nome[auto_nome_n++] = *p;
        }
        p++;
    }
}

static void auto_nome_finito(void)
{
    if (auto_zitto) {
        return;
    }
    if (auto_raccolgo) {
        auto_nome_nel_registro();
    }
    auto_raccolgo = 0;
}





static int auto_nome_va_bene(void)
{
    int k;

    if (auto_nome_n <= 0) {
        return 0;
    }
    if (auto_nome[0] == '$') {
        return 0;
    }
    if (auto_nome[0] == '#') {
        return 0;
    }
    for (k = 0; k < auto_nome_n; k++) {


        if (auto_nome[k] == ',') {
            return 0;
        }
    }
    return 1;
}











extern void overlay_avviso(const char *testo);








static void auto_avviso(const char *breve)
{
    overlay_avviso(breve);
}

static void auto_niente(const char *perche)
{
    if (auto_detto || auto_nome_n <= 0) {
        return;
    }
    auto_detto = 1;
    xum_riga("VELOCE no su '%.*s': %s", auto_nome_n, auto_nome, perche);
}

static int auto_prova_al_talk(unsigned char dev, unsigned char secadr)
{
    if (auto_zitto) {
        return -1;
    }





    if (auto_in_corso) {
        auto_niente("la sessione di prima non si e' chiusa");
        auto_avviso("FAST: STILL BUSY");
        return -1;
    }





    if (secadr >= 15) {
        return -1;
    }
    if (!auto_veloce) {
        auto_niente("spento nel menu");
        auto_avviso("FAST: OFF IN MENU");
        return -1;
    }






    if (auto_sa < 0 || (int)secadr != auto_sa || dev != auto_dev) {
        char breve[32];
        snprintf(breve, sizeof(breve), "FAST: SA %d/%d DEV %u/%u",
                 (int)secadr, auto_sa, dev, auto_dev);
        xum_riga("VELOCE no: al TALK %u,%u ma l'apertura era %u,%d"
                 " (nome %d byte)", dev, secadr, auto_dev, auto_sa,
                 auto_nome_n);
        auto_avviso(breve);
        return -1;
    }






    if (secadr != TURBO_SA_LETTURA) {
        char breve[32];
        snprintf(breve, sizeof(breve), "FAST: CH %u NOT 0", secadr);
        auto_niente("il turbo legge solo il file aperto sul canale 0");
        auto_avviso(breve);
        return -1;
    }


    if (auto_nome_n > 0 && auto_nome[0] == '$') {
        auto_niente("il direttorio");
        return -1;
    }
    if (!auto_nome_va_bene()) {
        auto_niente("il nome non si puo' seguire ($, # o una virgola)");
        auto_avviso("FAST: BAD NAME");
        return -1;
    }

    auto_zitto = 1;
    turbo_dice[0] = 0;











    turbo_traccia_n = 0;
    turbo_traccia_costo = 0;
    turbo_traccia_zero = turbo_orologio();
    turbo_traccia_subito = 0;
    turbo_traccia_accesa = 1;
    log_giri_prima = log_giri_su_scheda;
    log_us_prima = log_us_su_scheda;
    traccia_esadecimale("il C64 chiede", auto_nome, auto_nome_n);
    turbo_traccia("LOAD di '%.*s' dall'unita' %d, canale %u",
                  auto_nome_n, auto_nome, dev, secadr);








    turbo_scadenza_metti(TURBO_SCADENZA_APERTURA);



    if (turbo_sess_apri(dev, (const char *)auto_nome, auto_nome_n, 1)) {
        turbo_scadenza_togli();
        auto_zitto = 0;
        c_veloci_falliti++;
        xum_riga("VELOCE non parte (%s), si va come sempre", turbo_dice);
        turbo_traccia("NON PARTE al passo %d: %s", turbo_passo, turbo_dice);
        turbo_traccia_accesa = 0;
        turbo_traccia_versa();
        {
            char breve[32];

            if (turbo_non_solo > 0) {
                snprintf(breve, sizeof(breve), "FAST OFF: DRIVE %u ON BUS TOO",
                         (unsigned int)turbo_non_solo & 31u);
            } else {
                snprintf(breve, sizeof(breve), "FAST FAILED: %.17s",
                         turbo_dice);
            }
            auto_avviso(breve);
        }
        return -1;
    }
    auto_zitto = 0;
    turbo_scadenza_togli();

    auto_in_corso = 1;
    auto_blocchi_fatti = 0;
    auto_mag_n = 0;
    auto_mag_k = 0;
    auto_blocco_i = 0;
    auto_rotto = 0;
    auto_ultimo = 0;
    auto_byte_dati = 0;
    c_veloci++;
    xum_riga("VELOCE parte su '%.*s'", auto_nome_n, auto_nome);


    auto_avviso("FAST LOADER ON");
    return 0;
}




static int auto_riempi(void)
{
    int quanti = 0;

    auto_mag_n = 0;
    auto_mag_k = 0;
    auto_blocco_i = 0;




    if (auto_rotto || auto_ultimo) {
        return -1;
    }

    auto_zitto = 1;
    while (quanti < AUTO_MAGAZZINO) {
        int presi, ultimo;

        presi = turbo_sess_blocco(auto_blocco[quanti], TURBO_BLOCCO,
                                  &ultimo);
        if (presi < 0) {




            auto_rotto = 1;
            xum_riga("VELOCE rotto a meta': %s", turbo_dice);
            c_veloci_falliti++;





            turbo_traccia("ROTTO al blocco %d: %s", auto_blocchi_fatti + 1,
                          turbo_dice);
            turbo_traccia_versa();
            {
                char breve[32];
                snprintf(breve, sizeof(breve), "FAST BROKE: %.19s",
                         turbo_dice);
                auto_avviso(breve);
            }
            break;
        }
        if (presi > 0) {
            char breve[24];

            auto_blocco_n[quanti] = presi;
            quanti++;





            auto_blocchi_fatti++;
            snprintf(breve, sizeof(breve), "FAST LOADER ON  %d",
                     auto_blocchi_fatti);
            auto_avviso(breve);
        }
        if (ultimo) {
            auto_ultimo = 1;
            break;
        }
    }
    auto_zitto = 0;

    auto_mag_n = quanti;
    return quanti > 0 ? 0 : -1;
}

static int auto_leggi(unsigned char *fuori)
{
    if (!auto_in_corso) {
        return -1;
    }




    while (auto_mag_k >= auto_mag_n
           || auto_blocco_i >= auto_blocco_n[auto_mag_k]) {
        if (auto_mag_k + 1 < auto_mag_n) {
            auto_mag_k++;
            auto_blocco_i = 0;
        } else if (auto_riempi()) {
            return -1;
        }
    }
    *fuori = auto_blocco[auto_mag_k][auto_blocco_i++];
    auto_byte_dati++;
    c_byte_letti++;
    return 0;
}



static int auto_eoi(void)
{
    if (!auto_ultimo) {
        return 0;
    }
    if (auto_mag_k + 1 < auto_mag_n) {
        return 0;
    }
    if (auto_mag_n > 0 && auto_blocco_i < auto_blocco_n[auto_mag_k]) {
        return 0;
    }
    return 1;
}

static void auto_chiudi(void)
{
    if (auto_zitto) {
        return;
    }
    if (auto_in_corso) {
        auto_zitto = 1;



        turbo_scadenza_metti(TURBO_SCADENZA_CHIUSURA);
        turbo_sess_chiudi();
        turbo_scadenza_togli();
        auto_zitto = 0;
        auto_in_corso = 0;
        xum_riga("VELOCE finito: %ld byte", auto_byte_dati);








        if (auto_blocchi_fatti > 0) {
            unsigned long tot = s2_us_aspetta + s2_us_conto
                              + s2_us_dati + s2_us_nostro;
            unsigned long nb = (unsigned long)auto_blocchi_fatti;






            turbo_traccia("%d blocchi, %lu ms, %lu ms a blocco",
                          auto_blocchi_fatti, tot / 1000u, tot / 1000u / nb);
            turbo_traccia("  attesa del drive %lu ms (%lu a blocco)",
                          s2_us_aspetta / 1000u, s2_us_aspetta / 1000u / nb);
            turbo_traccia("  byte del conteggio %lu ms, i 254 byte %lu ms",
                          s2_us_conto / 1000u, s2_us_dati / 1000u);
            turbo_traccia("  il nostro turno %lu ms (%lu a blocco)",
                          s2_us_nostro / 1000u, s2_us_nostro / 1000u / nb);






            turbo_traccia("  il registro sulla scheda: %lu giri, %lu ms",
                          log_giri_su_scheda - log_giri_prima,
                          (log_us_su_scheda - log_us_prima) / 1000u);
        }












        if (!scolo_attivo) {
            turbo_traccia_accesa = 0;
            turbo_traccia_versa();
        }
    }
    auto_scorda();
}






























#define UNITA_MAX                   32
static unsigned int unita_vere = 0;
static int reset_come[UNITA_MAX];
static int reset_stato[UNITA_MAX];
static int reset_tipo[UNITA_MAX];
static int reset_linea_tirata = 0;
#define RIPARTITO_NO_BUS            (-2)
#define RIPARTITO_MUTO              (-1)
#define RIPARTITO_NIENTE            0
#define RIPARTITO_LINEA             1
#define RIPARTITO_UJ                2
#define RIPARTITO_NO                3
#define RIPARTITO_PI1541_ELENCO     4
#define RIPARTITO_LINEA_LUNGA       5



static int sonda_corta(unsigned char u)
{
    int c;

    xum_pazienza_corta = 1;
    c = unita_sul_bus(u);
    xum_pazienza_corta = 0;
    if (c < 0) {
        bus_perso = 0;
        bus_perso_da = 0;
        xum_zitto = 0;
        xum_zitto_fino_a = 0;
        bus_a_riposo = 1;
    }
    return c;
}







static void riarma_15(unsigned char u)
{
    cbm_listen(NULL, u, 15);
    cbm_unlisten(NULL);
}


static int stato_e_tipo(unsigned char u, char *testo, unsigned int max)
{
    int stato;

    riarma_15(u);
    stato = turbo_stato(u, testo, max);

    reset_stato[u & 31] = stato;
    if (stato != 99) {
        reset_tipo[u & 31] = drive_chi_sei(u);
    }
    return stato;
}




static int come_da_stato(int stato, const char *testo, int se_73)
{
    if (stato == 99) {
        return RIPARTITO_MUTO;
    }
    if (strstr(testo, "PI1541") != NULL) {
        return RIPARTITO_PI1541_ELENCO;
    }
    return stato == 73 ? se_73 : RIPARTITO_NO;
}

static int manda_uj(unsigned char u)
{
    if (turbo_comando(u, (const unsigned char *)"UJ", 2) != 0) {
        return -1;
    }

    uj_fino_a = circle_get_ticks() + UJ_RIACCENSIONE_US;
    if (uj_fino_a == 0) {
        uj_fino_a = 1;
    }
    return 0;
}






#define LINEA_LUNGA_US              (500u * 1000u)
static int linea_lunga(void)
{
    unsigned long t;

    if (xum_ioctl(XUM1541_IEC_SETRELEASE, IEC_RESET, 0) < 0) {
        return -1;
    }
    t = circle_get_ticks();
    circle_sleep((long)LINEA_LUNGA_US);
    if (xum_ioctl(XUM1541_IEC_SETRELEASE, 0, IEC_RESET) < 0) {
        return -1;
    }
    xum_diag("reset: la linea tenuta dall'host, millesimi",
             (int)((circle_get_ticks() - t) / 1000u), 0, 0);
    riaccensione_da_aspettare("RESET lungo della linea");
    return 0;
}



static unsigned int dopo_la_linea(unsigned int maschera, int se_73,
                                  char *testo, unsigned int max)
{
    unsigned int u, no = 0;

    for (u = 4; u <= 30; u++) {
        int c, stato;

        if (!(maschera & (1u << u))) {
            continue;
        }
        c = sonda_corta((unsigned char)u);
        if (c <= 0) {
            reset_come[u] = (c == 0) ? RIPARTITO_NO_BUS : RIPARTITO_MUTO;
            xum_riga("RIMETTI: unita' %u dopo la linea: %s", u,
                     c == 0 ? "non c'e'" : "muta");
            continue;
        }
        stato = stato_e_tipo((unsigned char)u, testo, max);
        reset_come[u] = come_da_stato(stato, testo, se_73);
        if (reset_come[u] == RIPARTITO_NO) {
            no |= 1u << u;
        }
        xum_riga("RIMETTI: unita' %u: %d '%s' (come %d)", u, stato, testo,
                 reset_come[u]);
    }
    return no;
}

static void riparti_tutti(unsigned char prima)
{
    unsigned int maschera = unita_vere | (1u << (prima & 31u));
    unsigned int u, no;
    int fermo = 0;
    char testo[48];

    for (u = 0; u < UNITA_MAX; u++) {
        reset_come[u] = RIPARTITO_NIENTE;
        reset_stato[u] = 99;
        reset_tipo[u] = DRIVE_BOH;
    }
    reset_linea_tirata = 0;
    linea_ms_ultima = 0;
    maschera &= 0x7ffffff0u;
    testo[0] = 0;

    if (!linea_come_sul_ferro()) {

        for (u = 4; u <= 30 && !fermo; u++) {
            int c;

            if (!(maschera & (1u << u))) {
                continue;
            }
            c = sonda_corta((unsigned char)u);
            if (c < 0) {
                fermo = 1;
                xum_riga("RIMETTI: l'unita' %u tiene fermo il bus: la linea",
                         u);
            } else if (c == 0) {
                reset_come[u] = RIPARTITO_NO_BUS;
            } else if (manda_uj((unsigned char)u) != 0) {
                fermo = 1;
                xum_riga("RIMETTI: l'unita' %u non prende UJ: la linea", u);
            } else {
                reset_come[u] = RIPARTITO_UJ;
            }
        }
        if (!fermo) {
            for (u = 4; u <= 30; u++) {
                int stato;

                if (reset_come[u] != RIPARTITO_UJ) {
                    continue;
                }
                stato = stato_e_tipo((unsigned char)u, testo, sizeof(testo));
                reset_come[u] = come_da_stato(stato, testo, RIPARTITO_UJ);
                xum_riga("RIMETTI: unita' %u dopo UJ: %d '%s'", u, stato,
                         testo);
            }
            return;
        }
        for (u = 0; u < UNITA_MAX; u++) {
            reset_come[u] = RIPARTITO_NIENTE;
        }
    }







    if (linea_come_sul_ferro()) {
        for (u = 4; u <= 30; u++) {
            if ((maschera & (1u << u)) && sonda_corta((unsigned char)u) > 0) {
                int prima_della_linea;

                riarma_15((unsigned char)u);
                prima_della_linea = turbo_stato((unsigned char)u, testo,
                                                sizeof(testo));
                xum_riga("RIMETTI: prima della linea lo stato di %u: %d '%s'",
                         u, prima_della_linea, testo);
            }
        }
    }


    if (cbm_reset_linea() == 0) {
        reset_linea_tirata = 1;


        if (ripartenza_cura()) {
            reset_linea_tirata = linea_presa_ms >= 0 ? 3
                               : (linea_presa_ms == -1 ? 4 : 5);
        }
    }
    no = dopo_la_linea(maschera, RIPARTITO_LINEA, testo, sizeof(testo));








    if (no != 0 && linea_come_sul_ferro() && !ripartenza_cura()) {
        xum_riga("RIMETTI: la linea non ha fatto ripartire le unita' 0x%08x: "
                 "la tengo %u ms", no, LINEA_LUNGA_US / 1000u);
        if (linea_lunga() == 0) {
            reset_linea_tirata = 2;
            no = dopo_la_linea(no, RIPARTITO_LINEA_LUNGA, testo,
                               sizeof(testo));
        }
    }


    for (u = 4; u <= 30; u++) {
        int stato;

        if (!(no & (1u << u))) {
            continue;
        }
        xum_riga("RIMETTI: unita' %u dice %d dopo la linea: UJ", u,
                 reset_stato[u]);
        if (manda_uj((unsigned char)u) == 0) {
            stato = stato_e_tipo((unsigned char)u, testo, sizeof(testo));
            reset_come[u] = come_da_stato(stato, testo, RIPARTITO_UJ);
            xum_riga("RIMETTI: unita' %u dopo UJ: %d '%s' (come %d)", u,
                     stato, testo, reset_come[u]);
        }
    }
}



















static int raspi_opencbm_rimetti_in_piedi_dentro(int unita, int *v, int n)
{
    unsigned long t0 = circle_get_ticks();
    CBM_FILE nuovo = NULL;
    char testo[48];
    int k;

    if (!v || n < 9) {
        return 0;
    }
    for (k = 0; k < n; k++) {
        v[k] = 0;
    }

    if (!raspi_opencbm_available()) {
        return 8;
    }
    v[0] = 1;
    linea_ms_ultima = 0;
    reset_linea_tirata = 0;







    if (!scolo_attivo && turbo_a_meta_file()) {
        svuota_il_turbo("RIMETTI");
    }
    if (scolo_attivo) {
        scolo_finisci();
        v[1] = 1;
    }




    turbo_scadenza_metti(20u * 1000u * 1000u);


    if (sess.aperta || auto_in_corso) {
        auto_zitto = 1;
        s2_esci();
        auto_zitto = 0;
        sess.aperta = 0;
        auto_in_corso = 0;
        v[1] = 1;
    }
    auto_scorda();



    bus_perso = 0;
    bus_a_riposo = 1;
    turbo_acceso = 0;
    xum_zitto = 0;
    muto_fino_a = 0;
    atn_rifiutato = 0;
    v[2] = 1;


    cbm_driver_close(NULL);
    circle_xum1541_clear_halts();
    if (cbm_driver_open(&nuovo, 0) == 0) {
        v[3] = 1;
        xum_rimetti_in_passo();
    }


    if (v[3]) {







        riparti_tutti((unsigned char)unita);
        v[4] = reset_linea_tirata;
        v[5] = reset_stato[unita & 31];
        v[8] = reset_tipo[unita & 31];
        (void)testo;
    } else {
        v[5] = 99;
    }

    turbo_scadenza_togli();

    v[6] = (int)((circle_get_ticks() - t0) / 1000u);

    v[7] = (v[3] && (reset_come[unita & 31] == RIPARTITO_LINEA
                     || reset_come[unita & 31] == RIPARTITO_UJ
                     || reset_come[unita & 31] == RIPARTITO_PI1541_ELENCO
                     || reset_come[unita & 31] == RIPARTITO_LINEA_LUNGA))
           ? 1 : 0;


    if (n > 11) {
        v[9] = (int)linea_ms_ultima;
        v[10] = reset_come[unita & 31];
        v[11] = reset_linea_tirata;
    }
    if (reset_linea_tirata >= 3) {


        log_message(xum_log, "xum1541: bus reset held %d ms by the host "
                    "(Pi %d), %s (%d ms)", linea_tenuta_ms,
                    circle_get_model(),
                    reset_linea_tirata == 3 ? "a drive took it after"
                    : (reset_linea_tirata == 4 ? "no drive took it"
                                               : "the bus was already held"),
                    linea_presa_ms);
    } else {
        log_message(xum_log, "xum1541: bus reset %s, %lu ms in the adapter",
                    reset_linea_tirata ? "pulled"
                                       : "not pulled (UJ to each drive)",
                    linea_ms_ultima);
    }

    log_message(xum_log, "xum1541: unit %d looks like a %s", unita,
                drive_nome[unita & 15][0] ? drive_nome[unita & 15] : "?");

    log_message(xum_log, "xum1541: put back on its feet in %d ms, "
                         "drive says %d", v[6], v[5]);
    return 9;
}



int raspi_opencbm_rimetti_in_piedi(int unita, int *v, int n)
{
    int zitto = auto_da_parte();
    int esito = raspi_opencbm_rimetti_in_piedi_dentro(unita, v, n);
    auto_rimetti(zitto);


    if (esito == 9 && v != NULL && n > 11) {
        esito = 12;
    }
    return esito;
}




void raspi_opencbm_unita_vere(unsigned int maschera)
{
    if (maschera != unita_vere) {
        solo_fino_a = 0;
    }
    unita_vere = maschera;
}




int raspi_opencbm_esito_unita(int unita, int *stato, int *tipo)
{
    if (unita < 0 || unita >= UNITA_MAX) {
        return RIPARTITO_NIENTE;
    }
    if (stato != NULL) {
        *stato = reset_stato[unita];
    }
    if (tipo != NULL) {
        *tipo = reset_tipo[unita];
    }
    return reset_come[unita];
}




static void svuota_il_turbo(const char *perche)
{
    xum_riga("%s: un caricamento veloce a meta' file, lo svuoto", perche);
    scolo_comincia();
    auto_in_corso = 0;
}







void raspi_opencbm_prima_di_uscire(void)
{
    if (!xum_open) {
        return;
    }
    if (!scolo_attivo && turbo_a_meta_file()) {
        svuota_il_turbo("USCITA");
    }
    if (scolo_attivo) {
        scolo_finisci();
    }
    linea_dopo_scolo = 0;
}

































#ifndef RASPI_DIR_1541
#define RASPI_DIR_1541              0
#define RASPI_DIR_1571              1
#define RASPI_DIR_1581              2
#endif
#define DIR_SA_BUFFER               13
#if DIR_SA_BUFFER < 2 || DIR_SA_BUFFER > 13
#error "U1 e B-P del 1541 vogliono un canale da 2 a 13 (ROM $CDD2): col 14 il drive dice 70,NO CHANNEL"
#endif


#define DIR_SA_TESTO2(x)            #x
#define DIR_SA_TESTO(x)             DIR_SA_TESTO2(x)
#define DIR_SCADENZA                (8u * 1000u * 1000u)

static int dir_dev = -1;
static int dir_zitto = 0;



static void dir_rimetti_il_bus(void)
{
    cbm_untalk(NULL);
    cbm_unlisten(NULL);
    bus_a_riposo = 1;
    bus_perso = 0;
}

static void dir_finisci(void)
{
    dir_dev = -1;
    turbo_scadenza_togli();
    auto_rimetti(dir_zitto);
}

void raspi_opencbm_dir_chiudi(void)
{
    if (dir_dev < 0) {
        return;
    }
    turbo_scadenza_metti(DIR_SCADENZA);
    if (!bus_a_riposo) {
        dir_rimetti_il_bus();
    }
    cbm_close(NULL, (unsigned char)dir_dev, DIR_SA_BUFFER);
    xum_riga("DIRECTORY: canale %d chiuso", DIR_SA_BUFFER);
    dir_finisci();
}









int raspi_opencbm_dir_apri(int unita, char *stato, int stato_max)
{
    char testo[48];
    int tipo, codice;

    if (stato != NULL && stato_max > 0) {
        stato[0] = 0;
    }
    if (dir_dev >= 0) {
        raspi_opencbm_dir_chiudi();
    }
    if (unita < 4 || unita > 30) {
        return -3;
    }
    if (!raspi_opencbm_available()) {
        return -1;
    }
    if (sess.aperta || auto_in_corso || scolo_attivo || !bus_a_riposo
        || bus_perso || xum_zitto) {
        xum_riga("DIRECTORY: bus occupato, non comincio");
        return -2;
    }

    dir_zitto = auto_da_parte();
    dir_dev = unita;
    turbo_scadenza_metti(DIR_SCADENZA);
    xum_riga("DIRECTORY: unita' %d", unita);

    tipo = drive_chi_sei((unsigned char)unita);
    if (tipo == DRIVE_BOH) {
        dir_rimetti_il_bus();
        dir_finisci();
        return -3;
    }

    turbo_scadenza_metti(DIR_SCADENZA);
    if (cbm_open(NULL, (unsigned char)unita, DIR_SA_BUFFER, "#", 1)) {
        dir_rimetti_il_bus();
        dir_finisci();
        return -3;
    }

    bus_a_riposo = 1;

    codice = turbo_stato((unsigned char)unita, testo, sizeof(testo));
    if (codice >= 20) {
        if (stato != NULL && stato_max > 0) {
            snprintf(stato, (size_t)stato_max, "%s", testo);
        }
        raspi_opencbm_dir_chiudi();
        return -3;
    }

    if (tipo == DRIVE_1581) {
        return RASPI_DIR_1581;
    }
    if (tipo == DRIVE_1570 || tipo == DRIVE_1571) {
        return RASPI_DIR_1571;
    }
    return RASPI_DIR_1541;
}



int raspi_opencbm_dir_settore(int traccia, int settore, unsigned char *buf,
                              char *stato, int stato_max)
{
    char cmd[32];
    char testo[48];
    unsigned char dev;
    int n, codice;

    if (stato != NULL && stato_max > 0) {
        stato[0] = 0;
    }
    if (dir_dev < 0 || buf == NULL) {
        return 99;
    }
    dev = (unsigned char)dir_dev;
    turbo_scadenza_metti(DIR_SCADENZA);

    n = snprintf(cmd, sizeof(cmd),
                 "U1:" DIR_SA_TESTO(DIR_SA_BUFFER) " 0 %d %d",
                 traccia, settore);
    if (turbo_comando(dev, (const unsigned char *)cmd, (unsigned int)n)) {
        dir_rimetti_il_bus();
        return 99;
    }
    codice = turbo_stato(dev, testo, sizeof(testo));
    if (stato != NULL && stato_max > 0) {
        snprintf(stato, (size_t)stato_max, "%s", testo);
    }
    if (codice >= 20) {
        return codice;
    }

    n = snprintf(cmd, sizeof(cmd), "B-P" DIR_SA_TESTO(DIR_SA_BUFFER) " 0");
    if (turbo_comando(dev, (const unsigned char *)cmd, (unsigned int)n)) {
        dir_rimetti_il_bus();
        return 99;
    }
    if (cbm_talk(NULL, dev, DIR_SA_BUFFER)) {
        dir_rimetti_il_bus();
        return 99;
    }
    n = cbm_raw_read(NULL, buf, 256);
    cbm_untalk(NULL);
    if (n != 256) {
        if (stato != NULL && stato_max > 0) {
            snprintf(stato, (size_t)stato_max, "%d BYTES OF 256,%02d,%02d",
                     n < 0 ? 0 : n, traccia, settore);
        }
        return 99;
    }
    return 0;
}








static int raspi_opencbm_turbo_leggi_dentro(int unita, const char *nome, int nomelen,
                              unsigned char *dest, unsigned int max,
                              int *blocchi_fuori, unsigned long *somma_fuori,
                              int blocchi_max)
{
    unsigned long somma = 0;
    unsigned char dev = (unsigned char)unita;
    unsigned char cestino[TURBO_BLOCCO];
    int totale = 0;
    int rotto = 0;

    if (blocchi_fuori) {
        *blocchi_fuori = 0;
    }
    if (somma_fuori) {
        *somma_fuori = 0;
    }
    if (!nome || nomelen <= 0) {
        return -1;
    }

    xum_riga("TURBO leggo '%s' dall'unita' %d", nome, unita);
    turbo_chiusura_us = 0;

    if (turbo_sess_apri(dev, nome, nomelen, 0)) {
        return -1;
    }







    {
        unsigned quanti = blocchi_max > 0 ? (unsigned)blocchi_max : 256u;
        turbo_scadenza_metti(quanti * 200u * 1000u + 4u * 1000u * 1000u);
    }

    for (;;) {
        int presi, ultimo, k;

        if (blocchi_max > 0 && sess.blocchi >= blocchi_max) {


            xum_riga("TURBO basta cosi': %d blocchi", sess.blocchi);
            break;
        }

        presi = turbo_sess_blocco(cestino, (int)sizeof(cestino), &ultimo);
        if (presi < 0) {
            rotto = 1;
            break;
        }

        for (k = 0; k < presi
                    && (unsigned int)(totale + k) < TURBO_CONFRONTO; k++) {
            somma = somma * 31u + cestino[k];
        }
        if (dest && (unsigned int)totale + (unsigned int)presi <= max) {
            memcpy(dest + totale, cestino, (size_t)presi);
        }
        totale += presi;

        if (ultimo) {
            break;
        }
    }

    if (!rotto) {
        turbo_passo = TURBO_PASSO_TUTTI_BLOCCHI;
    }
    turbo_traccia("%d blocchi, %d byte, prima di lasciare il bus",
                  sess.blocchi, totale);

    {
        unsigned long tc = turbo_orologio();

        if (turbo_sess_chiudi()) {
            rotto = 1;
        }
        turbo_chiusura_us = turbo_orologio() - tc;
    }

    xum_riga("TURBO %d byte in %d blocchi%s", totale, sess.blocchi,
             rotto ? " (con errori)" : "");

    if (blocchi_fuori) {
        *blocchi_fuori = sess.blocchi;
    }
    if (somma_fuori) {
        *somma_fuori = somma;
    }

    return rotto ? -1 : totale;
}



int raspi_opencbm_turbo_leggi(int unita, const char *nome, int nomelen,
                              unsigned char *dest, unsigned int max,
                              int *blocchi_fuori, unsigned long *somma_fuori,
                              int blocchi_max)
{
    int zitto = auto_da_parte();
    int esito = raspi_opencbm_turbo_leggi_dentro(unita, nome, nomelen, dest, max,
                                        blocchi_fuori, somma_fuori,
                                        blocchi_max);
    auto_rimetti(zitto);
    return esito;
}














#define TURBO_DIR_MAX               1024





static void turbo_rimetti_il_bus(unsigned char dev)
{
    cbm_untalk(NULL);
    cbm_unlisten(NULL);
    cbm_close(NULL, dev, TURBO_SA_LETTURA);
    bus_a_riposo = 1;
    bus_perso = 0;
}

static int turbo_primo_nome(int unita, char *nome, int max)
{
    unsigned char dir[TURBO_DIR_MAX];
    unsigned char dev = (unsigned char)unita;
    int n = 0;
    int virgolette = 0;
    int k;

    if (max < 2) {
        return -1;
    }
    nome[0] = 0;

    turbo_traccia("prima di OPEN %d,0 con il nome $", dev);
    if (cbm_open(NULL, dev, TURBO_SA_LETTURA, "$", 1)) {
        turbo_traccia("OPEN $ non riuscito, rimetto il bus");
        turbo_rimetti_il_bus(dev);
        return -1;
    }
    turbo_traccia("aperto $, prima di TALK");
    if (cbm_talk(NULL, dev, TURBO_SA_LETTURA)) {
        turbo_traccia("TALK non riuscito, rimetto il bus");
        turbo_rimetti_il_bus(dev);
        return -1;
    }
    turbo_traccia("il drive parla, prima del primo byte");

    while (n < TURBO_DIR_MAX) {
        unsigned char b;

        if (cbm_raw_read(NULL, &b, 1) != 1) {
            break;
        }
        dir[n++] = b;
        if (cbm_get_eoi(NULL)) {
            break;
        }
        if ((n & 127) == 0) {
            turbo_traccia("%d byte di direttorio", n);
        }
    }
    turbo_traccia("direttorio: %d byte, prima di UNTALK", n);
    cbm_untalk(NULL);
    cbm_close(NULL, dev, TURBO_SA_LETTURA);
    turbo_traccia("direttorio chiuso");



    for (k = 0; k < n; k++) {
        if (dir[k] != '"') {
            continue;
        }
        virgolette++;
        if (virgolette == 3) {
            int j = 0;

            k++;
            while (k < n && dir[k] != '"' && j + 1 < max) {
                nome[j++] = (char)dir[k++];
            }
            nome[j] = 0;
            return j;
        }
    }

    return -1;
}



static int turbo_lettura_di_sempre(int unita, const char *nome, int nomelen,
                                   unsigned long *somma_fuori)
{
    unsigned char dev = (unsigned char)unita;
    unsigned long somma = 0;
    int n = 0;

    if (cbm_open(NULL, dev, TURBO_SA_LETTURA, NULL, 0)) {
        return -1;
    }
    if (cbm_raw_write(NULL, nome, (size_t)nomelen) != nomelen) {
        cbm_unlisten(NULL);
        cbm_close(NULL, dev, TURBO_SA_LETTURA);
        return -1;
    }
    cbm_unlisten(NULL);

    if (cbm_talk(NULL, dev, TURBO_SA_LETTURA)) {
        cbm_close(NULL, dev, TURBO_SA_LETTURA);
        return -1;
    }

    for (;;) {
        unsigned char b;

        if (cbm_raw_read(NULL, &b, 1) != 1) {
            break;
        }
        somma = somma * 31u + b;
        n++;
        if (cbm_get_eoi(NULL)) {
            break;
        }
        if ((unsigned int)n >= TURBO_CONFRONTO) {

            break;
        }
    }

    cbm_untalk(NULL);
    cbm_close(NULL, dev, TURBO_SA_LETTURA);

    if (somma_fuori) {
        *somma_fuori = somma;
    }
    return n;
}

static int raspi_opencbm_turbo_prova_dentro(int unita, char *nome, int nome_max,
                              char *dove, int dove_max, int *v, int n)
{
    unsigned long somma_turbo = 0, somma_sempre = 0;
    unsigned long t0, dt;
    int nomelen;
    int blocchi = 0;
    int byte_turbo, byte_sempre;
    int uguali;
    int stato_drive = 0;
    int tipo_drive;
    int k;

    if (!v || n < 10 || !nome || nome_max < 2 || !dove || dove_max < 2) {
        return 0;
    }
    for (k = 0; k < n; k++) {
        v[k] = 0;
    }
    nome[0] = 0;
    dove[0] = 0;
    turbo_passo = TURBO_PASSO_NIENTE;
    turbo_dice[0] = 0;





    if (!raspi_opencbm_available() || !xum_open) {
        v[0] = 5;
        v[11] = TURBO_PASSO_ADATTATORE;
        snprintf(dove, (size_t)dove_max, "no adapter plugged in");
        return n < 16 ? n : 16;
    }


    turbo_traccia_n = 0;
    turbo_traccia_costo = 0;
    turbo_traccia_zero = turbo_orologio();





    turbo_traccia_subito = 0;
    turbo_traccia_accesa = 1;
    turbo_traccia("comincio, unita' %d", unita);


    conti_metti_via();




    turbo_scadenza_metti(TURBO_SCADENZA_US);




    turbo_traccia("prima di chiedere al drive chi e'");
    tipo_drive = drive_chi_sei((unsigned char)unita);




    if (tipo_drive == DRIVE_BOH) {
        turbo_traccia("l'unita' %d non risponde alla domanda", unita);
    } else {
        turbo_traccia("l'unita' %d e' un %s", unita,
                      drive_nome[unita & 15][0] ? drive_nome[unita & 15]
                                                : "?");
    }
    if (!drive_va_col_turbo(tipo_drive)) {
        v[0] = (tipo_drive == DRIVE_BOH) ? 8 : 6;
        v[11] = TURBO_PASSO_ADATTATORE;
        if (tipo_drive == DRIVE_BOH) {
            snprintf(dove, (size_t)dove_max, "Drive %d does not answer.",
                     unita);
        } else {
            snprintf(dove, (size_t)dove_max, "a %s, no turbo for it",
                     drive_nome[unita & 15][0] ? drive_nome[unita & 15]
                                               : "?");
        }
        turbo_scadenza_togli();
        conti_rimetti();
        turbo_traccia_accesa = 0;
        turbo_traccia_versa();
        turbo_traccia_chiudi();
        return n < 16 ? n : 16;
    }



    {
        int altro = altro_sul_bus((unsigned char)unita);

        if (altro != 0) {
            v[0] = 7;
            v[11] = TURBO_PASSO_ADATTATORE;
            if (altro > 0) {
                snprintf(dove, (size_t)dove_max, "drive %d is on the bus too",
                         altro);
            } else {
                snprintf(dove, (size_t)dove_max,
                         "could not ask who is on the bus");
            }
            turbo_traccia("non e' solo sul bus (%d): il turbo non si prova",
                          altro);
            turbo_scadenza_togli();
            conti_rimetti();
            turbo_traccia_accesa = 0;
            turbo_traccia_versa();
            turbo_traccia_chiudi();
            return n < 16 ? n : 16;
        }
    }

    turbo_traccia("prima di leggere il direttorio");
    nomelen = turbo_primo_nome(unita, nome, nome_max);
    turbo_traccia("direttorio letto, nome '%s' (%d)", nome, nomelen);
    traccia_esadecimale("nel direttorio c'e'", (const unsigned char *)nome,
                        nomelen);
    if (nomelen <= 0) {
        v[0] = 1;
        v[11] = TURBO_PASSO_DIRETTORIO;
        snprintf(dove, (size_t)dove_max,
                 turbo_scaduto() ? "the drive stopped answering"
                                 : "no directory came back");
        turbo_scadenza_togli();
        conti_rimetti();




        turbo_traccia_accesa = 0;
        turbo_traccia_versa();
        turbo_traccia_chiudi();
        return n < 16 ? n : 16;
    }
    turbo_passo = TURBO_PASSO_NOME;

    t0 = turbo_orologio();
    byte_turbo = raspi_opencbm_turbo_leggi(unita, nome, nomelen, NULL, 0,
                                           &blocchi, &somma_turbo,
                                           TURBO_PROVA_BLOCCHI);
    stato_drive = 0;
    dt = turbo_orologio() - t0;





    if (dt > turbo_chiusura_us) {
        dt -= turbo_chiusura_us;
    }
    v[1] = byte_turbo;
    v[2] = (int)(dt / 1000u);
    v[3] = dt ? (int)((unsigned long)(byte_turbo > 0 ? byte_turbo : 0)
                      * 1000000UL / dt) : 0;
    v[8] = blocchi;









    v[7] = blocchi > 0 ? (int)(dt / 1000u) / blocchi : 0;




    if (byte_turbo < 0) {
        byte_sempre = -1;
        dt = 0;
    } else {

        turbo_traccia("prima della lettura di confronto");
        turbo_scadenza_metti(TURBO_SCADENZA_CONFRONTO);
        t0 = turbo_orologio();
        byte_sempre = turbo_lettura_di_sempre(unita, nome, nomelen,
                                              &somma_sempre);
        dt = turbo_orologio() - t0;
    }
    v[4] = byte_sempre;
    v[5] = (int)(dt / 1000u);
    v[6] = dt ? (int)((unsigned long)(byte_sempre > 0 ? byte_sempre : 0)
                      * 1000000UL / dt) : 0;





    v[9] = (byte_turbo > (int)TURBO_CONFRONTO);
    if (n > 10) { v[10] = (int)turbo_respiro_us; }
    uguali = (byte_turbo > 0 && somma_turbo == somma_sempre
              && (v[9] ? (unsigned int)byte_sempre == TURBO_CONFRONTO
                       : byte_turbo == byte_sempre));

    if (turbo_scaduto() && turbo_dice[0] == 0) {
        snprintf(turbo_dice, sizeof(turbo_dice), "ran out of time at step %d",
                 turbo_passo);
    }

    if (byte_turbo < 0) {
        v[0] = 2;
    } else if (byte_sempre < 0) {
        v[0] = 3;
    } else if (!uguali) {
        v[0] = 4;
    }

    turbo_scadenza_togli();



    if (!bus_a_riposo || bus_perso) {
        turbo_rimetti_il_bus((unsigned char)unita);
    }

    turbo_traccia("FINITO: esito %d, passo %d",
                  byte_turbo < 0 ? 2 : 0, turbo_passo);
    turbo_traccia("(la scheda si e' presa %lu ms, non contati)",
                  turbo_traccia_costo / 1000u);
    turbo_traccia_accesa = 0;
    turbo_traccia_versa();
    turbo_traccia_chiudi();

    v[11] = turbo_passo;
    v[12] = stato_drive;





    if (n > 13) { v[13] = (int)(s2_us_aspetta / 1000u); }
    if (n > 14) { v[14] = (int)(s2_us_conto / 1000u); }
    if (n > 15) { v[15] = (int)(s2_us_dati / 1000u); }
    if (n > 16) { v[16] = (int)(s2_us_nostro / 1000u); }
    if (turbo_dice[0] != 0) {
        snprintf(dove, (size_t)dove_max, "%s", turbo_dice);
    } else {
        snprintf(dove, (size_t)dove_max, "%s", turbo_nome_passo(turbo_passo));
    }

    conti_rimetti();

    return n < 17 ? n : 17;
}



int raspi_opencbm_turbo_prova(int unita, char *nome, int nome_max,
                              char *dove, int dove_max, int *v, int n)
{
    int zitto = auto_da_parte();
    int esito = raspi_opencbm_turbo_prova_dentro(unita, nome, nome_max, dove, dove_max, v, n);
    auto_rimetti(zitto);
    return esito;
}










void raspi_opencbm_molla(void)
{
    int zitto = auto_da_parte();


















    int guasto = (sess.aperta || auto_in_corso || bus_perso || xum_zitto
                  || xum_non_sveglio
                  || (xum_parked && circle_get_model() < 4));











    xum_via_libera(TURBO_SCADENZA_MOLLA);

    muto_fino_a = 0;















    if (!scolo_attivo && turbo_a_meta_file()) {
        svuota_il_turbo("MOLLA");
    }
    if (scolo_attivo) {
        xum_riga("MOLLA: c'e' un file da svuotare, lo lascio finire");
        auto_scorda();
        bus_perso = 0;
        xum_zitto = 0;
        xum_zitto_fino_a = 0;
        turbo_scadenza_togli();
        auto_rimetti(zitto);
        return;
    }


    scolo_attivo = 0;

    if (sess.aperta || auto_in_corso) {




        s2_esci();
        sess.aperta = 0;
        sess.finita = 0;
        sess.rotta = 0;
        auto_in_corso = 0;
    }
    auto_scorda();
    bus_a_riposo = 1;
    bus_perso = 0;
    xum_zitto = 0;
    xum_zitto_fino_a = 0;
    turbo_acceso = 0;












    if (guasto) {
        CBM_FILE nuovo = NULL;

        xum_riga("MOLLA: l'adattatore era in guai, lo rimetto in piedi");
        cbm_driver_close(NULL);
        circle_xum1541_clear_halts();
        if (cbm_driver_open(&nuovo, 0) == 0) {


            xum_rimetti_in_passo();
        }
        bus_perso = 0;
        xum_zitto = 0;
        xum_zitto_fino_a = 0;
    }






















    turbo_scadenza_togli();
    auto_rimetti(zitto);
}






static void turbo_lascia_per_la_linea(void)
{
    scolo_attivo = 0;
    if (sess.aperta || auto_in_corso) {
        s2_esci();
        sess.aperta = 0;
        sess.finita = 0;
        sess.rotta = 0;
        auto_in_corso = 0;
    }
    auto_scorda();
    sess.nostro_canale = 0;
    turbo_acceso = 0;
    turbo_sporcato = 0;
    turbo_dopo_uj = 0;
    uj_fino_a = 0;
    bus_a_riposo = 1;
}




int raspi_opencbm_available(void)
{




    return circle_xum1541_present();
}

#endif
