// SPDX-FileCopyrightText:  2002-2006 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PARPORT_H
#define DOSBOX_PARPORT_H

// Set to 1 for debug messages and a debugging log.
#define PARALLEL_DEBUG 0

#include "dosbox.h"

#include "config/config.h"
#include "config/setup.h"
#include "dos/dos_system.h"
#include "shell/command_line.h"
#include "hardware/lpt.h"
#include "hardware/port.h"

class CParallel;

class device_LPT final : public DOS_Device {
public:
	device_LPT(const device_LPT&)            = delete; // prevent copying
	device_LPT& operator=(const device_LPT&) = delete; // prevent assignment

	// Creates an LPT device that communicates with the num-th parallel
	// port, i.e. is LPTnum.
	device_LPT(uint8_t num, CParallel* pp);

	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	void Close() override;
	uint16_t GetInformation() override;

private:
	CParallel* pportclass = nullptr;
	uint8_t num           = 0; // this device is LPTnum
};

class CParallel {
public:
	CParallel(const CParallel&)            = delete; // prevent copying
	CParallel& operator=(const CParallel&) = delete; // prevent assignment

	CParallel(CommandLine* cmd, uint8_t portnr, uint8_t initirq);
	virtual ~CParallel();

#if PARALLEL_DEBUG
	FILE* debugfp         = nullptr;
	bool dbg_data         = false;
	bool dbg_putchar      = false;
	bool dbg_cregs        = false;
	bool dbg_plainputchar = false;
	bool dbg_plaindr      = false;
	void log_par(bool active, const char* format, ...);
#endif

	IO_ReadHandleObject ReadHandler[3]   = {};
	IO_WriteHandleObject WriteHandler[3] = {};

	void setEvent(uint16_t type, float duration);
	void removeEvent(uint16_t type);
	void handleEvent(uint16_t type);
	virtual void handleUpperEvent(uint16_t type) = 0;

	uint8_t port_nr = 0;
	uint16_t base   = 0;
	uint8_t irq     = 0;

	// Register access. Reads return the register value; writes take it.
	virtual uint8_t Read_PR()  = 0;
	virtual uint8_t Read_COM() = 0;
	virtual uint8_t Read_SR()  = 0;

	virtual void Write_PR(uint8_t val)    = 0;
	virtual void Write_CON(uint8_t val)   = 0;
	virtual void Write_IOSEL(uint8_t val) = 0;

	virtual bool Putchar(uint8_t val) = 0;

	uint8_t getPrinterStatus();
	void initialize();

	bool InstallationSuccessful = false;

private:
	// Owned by the DOS device table once registered: DOS_DelDevice()
	// deletes it, matching the idiom used by CSerial.
	device_LPT* mydosdevice = nullptr;
};

// The three parallel ports, indexed 0..2 (LPT1..LPT3). nullptr when the
// corresponding port is disabled or unclaimed.
extern CParallel* parallelPortObjects[3];

constexpr uint8_t ParallelMaxPorts = 3;

constexpr uint16_t parallel_baseaddr[ParallelMaxPorts] = {
        LptPorts::Lpt1Port, // 0x378
        LptPorts::Lpt2Port, // 0x278
        LptPorts::Lpt3Port, // 0x3bc
};

void PARALLEL_AddConfigSection(const ConfigPtr& conf);
void PARALLEL_Init();
void PARALLEL_Destroy();

#endif // DOSBOX_PARPORT_H
