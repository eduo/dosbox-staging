// SPDX-FileCopyrightText:  2002-2006 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRREDIR_H
#define DOSBOX_PRREDIR_H

#include "dosbox.h"

#include "parport.h"

// Passes LPT register access through to Boxer's virtual printer.
class CPrinterRedir final : public CParallel {
public:
	CPrinterRedir(uint8_t nr, uint8_t initIrq, CommandLine* cmd);
	~CPrinterRedir() override;

	uint8_t Read_PR() override;
	uint8_t Read_COM() override;
	uint8_t Read_SR() override;

	void Write_PR(uint8_t val) override;
	void Write_CON(uint8_t val) override;
	void Write_IOSEL(uint8_t val) override;

	bool Putchar(uint8_t val) override;

	void handleUpperEvent(uint16_t type) override;
};

#endif // DOSBOX_PRREDIR_H
