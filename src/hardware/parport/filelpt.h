// SPDX-FileCopyrightText:  2002-2006 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FILELPT_H
#define DOSBOX_FILELPT_H

#include "dosbox.h"

#include <cstdio>
#include <string>

#include "parport.h"

enum class FileLptType { Device, Capture, Append };

class CFileLPT final : public CParallel {
public:
	CFileLPT(uint8_t nr, uint8_t initIrq, CommandLine* cmd);
	~CFileLPT() override;

	bool OpenFile();

	uint8_t Read_PR() override;
	uint8_t Read_COM() override;
	uint8_t Read_SR() override;

	void Write_PR(uint8_t val) override;
	void Write_CON(uint8_t val) override;
	void Write_IOSEL(uint8_t val) override;

	bool Putchar(uint8_t val) override;

	void handleUpperEvent(uint16_t type) override;

private:
	bool fileOpen       = false;
	FileLptType filetype = FileLptType::Device;
	FILE* file          = nullptr;
	std::string name    = {}; // name of the thing to open
	bool addFF          = false; // add a formfeed before closing
	bool addLF          = false; // add LF after CR if not done by the app

	uint8_t lastChar              = 0; // previous char, to decide whether to add LF
	const uint16_t* codepage_ptr = nullptr; // translation codepage, if any

	bool ack_polarity = false;

	uint8_t datareg    = 0;
	uint8_t controlreg = 0;

	bool autofeed       = false;
	bool ack            = false;
	uint32_t timeout      = 0;
	uint32_t lastUsedTick = 0;
};

#endif // DOSBOX_FILELPT_H
