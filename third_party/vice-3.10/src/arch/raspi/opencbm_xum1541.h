/*


 * Written for BMC64 by Claude Code (Anthropic).





 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#ifndef VICE_OPENCBM_XUM1541_H
#define VICE_OPENCBM_XUM1541_H

int raspi_opencbm_available(void);
void raspi_opencbm_molla(void);


void raspi_opencbm_set_log(int acceso);





int raspi_opencbm_misura(int *v, int n);





int raspi_opencbm_conti(int *v, int n);
void raspi_opencbm_azzera_conti(void);





int raspi_opencbm_turbo_leggi(int unita, const char *nome, int nomelen,
                              unsigned char *dest, unsigned int max,
                              int *blocchi_fuori, unsigned long *somma_fuori,
                              int blocchi_max);
int raspi_opencbm_turbo_prova(int unita, char *nome, int nome_max,
                              char *dove, int dove_max, int *v, int n);







void raspi_opencbm_set_veloce(int acceso);
int raspi_opencbm_get_veloce(void);








int raspi_opencbm_rimetti_in_piedi(int unita, int *v, int n);






void raspi_opencbm_set_respiro(int us);
int raspi_opencbm_get_respiro(void);



void raspi_opencbm_tick(void);

#endif
