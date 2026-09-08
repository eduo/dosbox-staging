// SPDX-FileCopyrightText:  2002-2004 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRINTER_CHARMAPS_H
#define DOSBOX_PRINTER_CHARMAPS_H

#include <cstdint>

struct CHARMAP {
	uint32_t codepage;
	const uint16_t* map;
};

extern const CHARMAP charmap[];
extern const uint16_t codepages[15];
extern const uint16_t intCharSets[15][12];

#endif // DOSBOX_PRINTER_CHARMAPS_H
