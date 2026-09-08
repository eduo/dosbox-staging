// SPDX-FileCopyrightText:  2002-2006 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#if C_PRINTER

#include "printer_redir.h"

#include "parport.h"

// Boxer supplies the printer itself; this class only forwards LPT register
// access to it. BXCoalface.h is Objective-C++, so this file must be compiled
// as .mm by Boxer's Xcode project.
#include "BXCoalface.h"

CPrinterRedir::CPrinterRedir(uint8_t nr, uint8_t initIrq, CommandLine* cmd)
        : CParallel(cmd, nr, initIrq)
{
	InstallationSuccessful = boxer_PRINTER_isInited(nr);
}

CPrinterRedir::~CPrinterRedir() = default;

bool CPrinterRedir::Putchar(uint8_t val)
{
	Write_CON(0xD4); // strobe data out
	Write_PR(val);
	Write_CON(0xD5); // strobe pulse
	Write_CON(0xD4); // strobe off
	Read_SR();       // clear ack

#if PARALLEL_DEBUG
	log_par(dbg_putchar, "putchar  0x%2x", val);
	if (dbg_plainputchar) {
		fprintf(debugfp, "%c", val);
	}
#endif

	return true;
}

uint8_t CPrinterRedir::Read_PR()
{
	return static_cast<uint8_t>(boxer_PRINTER_readdata(0, 1));
}

uint8_t CPrinterRedir::Read_COM()
{
	return static_cast<uint8_t>(boxer_PRINTER_readcontrol(0, 1));
}

uint8_t CPrinterRedir::Read_SR()
{
	return static_cast<uint8_t>(boxer_PRINTER_readstatus(0, 1));
}

void CPrinterRedir::Write_PR(uint8_t val)
{
	boxer_PRINTER_writedata(0, val, 1);
}

void CPrinterRedir::Write_CON(uint8_t val)
{
	boxer_PRINTER_writecontrol(0, val, 1);
}

void CPrinterRedir::Write_IOSEL([[maybe_unused]] uint8_t val)
{
	// nothing
}

void CPrinterRedir::handleUpperEvent([[maybe_unused]] uint16_t type) {}

#endif // C_PRINTER
