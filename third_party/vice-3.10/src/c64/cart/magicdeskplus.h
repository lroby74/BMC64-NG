/*
 * magicdeskplus.h - Cartridge handling, Magic Desk Plus cart.
 *
 * Based on the VICE Magic Desk cartridge implementation written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *
 * Original Magic Desk Plus concept and initial implementation by
 *  CrystalCT
 *
 * Reworked and adapted to the current VICE cartridge architecture by
 *  Claus Schlereth
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

#ifndef VICE_MAGICDESKPLUS_H
#define VICE_MAGICDESKPLUS_H

#include <stdio.h>

#include "types.h"

#define MAGICDESKPLUS_REV_SRAM_EEPROM_32K     0
#define MAGICDESKPLUS_REV_SRAM_EEPROM_8K      1
#define MAGICDESKPLUS_REV_EEPROM_32K          2
#define MAGICDESKPLUS_REV_EEPROM_8K           3
#define MAGICDESKPLUS_REV_SRAM                4
#define MAGICDESKPLUS_MAX_REV                 4

void magicdeskplus_config_init(void);
void magicdeskplus_config_setup(uint8_t *rawcart);
int magicdeskplus_bin_attach(const char *filename, uint8_t *rawcart);
int magicdeskplus_crt_attach(FILE *fd, uint8_t *rawcart, uint8_t revision);
void magicdeskplus_file_della_cartuccia(const char *crt);
void magicdeskplus_detach(void);
void magicdeskplus_shutdown(void);

int magicdeskplus_resources_init(void);
void magicdeskplus_resources_shutdown(void);
int magicdeskplus_cmdline_options_init(void);

int magicdeskplus_can_save_sram(void);
int magicdeskplus_can_flush_sram(void);
int magicdeskplus_sram_save(const char *filename);
int magicdeskplus_sram_flush(void);

int magicdeskplus_can_save_eeprom(void);
int magicdeskplus_can_flush_eeprom(void);
int magicdeskplus_eeprom_save(const char *filename);
int magicdeskplus_eeprom_flush(void);

struct snapshot_s;

int magicdeskplus_snapshot_write_module(struct snapshot_s *s);
int magicdeskplus_snapshot_read_module(struct snapshot_s *s);

#endif
