// SPDX-FileCopyrightText:  2002-2006 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "filelpt.h"

#include <cstdio>

#include "parport.h"
#include "printer_charmaps.h"

#include "capture/capture.h"
#include "cpu/callback.h"
#include "hardware/pic.h"

CFileLPT::CFileLPT(uint8_t nr, uint8_t initIrq, CommandLine* cmd)
        : CParallel(cmd, nr, initIrq)
{
	InstallationSuccessful = false;
	fileOpen               = false;

	std::string str;

	// add a formfeed when closing?
	addFF = cmd->FindStringBegin("addFF", str, false);

	// add a line feed after each carriage return?
	addLF = cmd->FindStringBegin("addLF", str, false);

	// find the codepage
	unsigned int temp = 0;
	codepage_ptr      = nullptr;
	if (cmd->FindStringBegin("cp:", str, false)) {
		if (sscanf(str.c_str(), "%u", &temp) != 1) {
			LOG_WARNING("PARALLEL: Port LPT%u invalid codepage parameter",
			            nr + 1);
			return;
		}
		for (size_t i = 0; charmap[i].codepage != 0; ++i) {
			if (charmap[i].codepage == temp) {
				codepage_ptr = charmap[i].map;
				break;
			}
		}
	}

	temp = 0;
	if (cmd->FindStringBegin("timeout:", str, false)) {
		if (sscanf(str.c_str(), "%u", &temp) != 1) {
			LOG_WARNING("PARALLEL: Port LPT%u invalid timeout parameter",
			            nr + 1);
			return;
		}
		timeout = temp;
	} else {
		timeout = 500;
	}

	if (cmd->FindStringBegin("dev:", str, false)) {
		name     = str;
		filetype = FileLptType::Device;
	} else if (cmd->FindStringBegin("append:", str, false)) {
		name     = str;
		filetype = FileLptType::Append;
	} else {
		filetype = FileLptType::Capture;
	}

	InstallationSuccessful = true;
}

CFileLPT::~CFileLPT()
{
	if (fileOpen) {
		fclose(file);
	}
	// remove tick handler
	removeEvent(0);
}

bool CFileLPT::OpenFile()
{
	switch (filetype) {
	case FileLptType::Device:
		file = fopen(name.c_str(), "wb");
		break;
	case FileLptType::Capture:
		file = CAPTURE_CreateFile(CaptureType::ParallelStream);
		break;
	case FileLptType::Append:
		file = fopen(name.c_str(), "ab");
		break;
	}

	if (timeout != 0) {
		setEvent(0, static_cast<float>(timeout + 1));
	}

	if (!file) {
		LOG_WARNING("PARALLEL: Port LPT%u failed to open %s",
		            port_nr + 1,
		            name.c_str());
		fileOpen = false;
		return false;
	}

	fileOpen = true;
	return true;
}

bool CFileLPT::Putchar(uint8_t val)
{
#if PARALLEL_DEBUG
	log_par(dbg_putchar, "putchar  0x%2x", val);
	if (dbg_plainputchar) {
		fprintf(debugfp, "%c", val);
	}
#endif

	lastUsedTick = PIC_Ticks;
	if (!fileOpen && !OpenFile()) {
		return false;
	}

	if (codepage_ptr) {
		const uint16_t extchar = codepage_ptr[val];
		if (extchar & 0xFF00) {
			fputc(extchar >> 8, file);
		}
		fputc(extchar & 0xFF, file);
	} else {
		fputc(val, file);
	}

	if (addLF) {
		if ((lastChar == 0x0d) && (val != 0x0a)) {
			fputc(0x0a, file);
		}
		lastChar = val;
	}

	return true;
}

uint8_t CFileLPT::Read_PR()
{
	return datareg;
}

uint8_t CFileLPT::Read_COM()
{
	return 0;
}

uint8_t CFileLPT::Read_SR()
{
	uint8_t status = 0x9f;
	if (!ack) {
		status |= 0x40;
	}
	ack = false;
	return status;
}

void CFileLPT::Write_PR(uint8_t val)
{
	datareg = val;
}

void CFileLPT::Write_CON(uint8_t val)
{
	// autofeed adds 0xa if 0xd is sent
	autofeed = ((val & 0x02) != 0);

	// Data is strobed to the parallel printer on the falling edge of the
	// strobe bit.
	if ((!(val & 0x1)) && (controlreg & 0x1)) {
		Putchar(datareg);
		if (autofeed && (datareg == 0xd)) {
			Putchar(0xa);
		}
		ack = true;
	}
	controlreg = val;
}

void CFileLPT::Write_IOSEL([[maybe_unused]] uint8_t val)
{
	// not needed for file printing functionality
}

void CFileLPT::handleUpperEvent([[maybe_unused]] uint16_t type)
{
	if (!fileOpen) {
		return;
	}

	if (lastUsedTick + timeout < PIC_Ticks) {
		if (addFF) {
			fputc(12, file);
		}
		fclose(file);
		lastChar = 0;
		fileOpen = false;
		LOG_MSG("PARALLEL: Port LPT%u file closed", port_nr + 1);
	} else {
		// Port has been touched in the meantime, try again later
		const auto new_delay = static_cast<float>(
		        (timeout + 1) - (PIC_Ticks - lastUsedTick));
		setEvent(0, new_delay);
	}
}
