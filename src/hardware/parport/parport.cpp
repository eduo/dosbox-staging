// SPDX-FileCopyrightText:  2002-2006 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "parport.h"

#include <cctype>
#include <cstring>
#include <memory>
#include <string>

#include "directlpt_linux.h"
#include "directlpt_win32.h"
#include "filelpt.h"
#include "printer_redir.h"

#include "capture/capture.h"
#include "config/setup.h"
#include "cpu/callback.h"
#include "dos/dos_system.h"
#include "hardware/memory.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/timer.h"
#include "ints/bios.h"
#include "misc/support.h"
#include "utils/string_utils.h"

// device_LPT -- the DOS-visible LPTn character device

bool device_LPT::Read([[maybe_unused]] uint8_t* data, uint16_t* size)
{
	*size = 0;
	LOG(LOG_DOSMISC, LOG_NORMAL)("LPTDEVICE: Read called");
	return true;
}

bool device_LPT::Write(uint8_t* data, uint16_t* size)
{
	for (uint16_t i = 0; i < *size; ++i) {
		if (!pportclass->Putchar(data[i])) {
			return false;
		}
	}
	return true;
}

bool device_LPT::Seek(uint32_t* pos, [[maybe_unused]] uint32_t type)
{
	*pos = 0;
	return true;
}

void device_LPT::Close() {}

uint16_t device_LPT::GetInformation()
{
	return 0x80A0;
}

static const char* lptname[ParallelMaxPorts] = {"LPT1", "LPT2", "LPT3"};

device_LPT::device_LPT(uint8_t _num, CParallel* pp) : pportclass(pp), num(_num)
{
	assert(_num < ParallelMaxPorts);
	SetName(lptname[_num]);
}

// PIC event plumbing

static void Parallel_EventHandler(uint32_t val)
{
	const uint32_t parclassid = val & 0x3;
	if (parclassid < ParallelMaxPorts && parallelPortObjects[parclassid]) {
		parallelPortObjects[parclassid]->handleEvent(
		        static_cast<uint16_t>(val >> 2));
	}
}

void CParallel::setEvent(uint16_t type, float duration)
{
	PIC_AddEvent(Parallel_EventHandler, duration, (type << 2) | port_nr);
}

void CParallel::removeEvent(uint16_t type)
{
	PIC_RemoveSpecificEvents(Parallel_EventHandler, (type << 2) | port_nr);
}

void CParallel::handleEvent(uint16_t type)
{
	handleUpperEvent(type);
}

// IO port handlers

static io_val_t PARALLEL_Read(io_port_t port, [[maybe_unused]] io_width_t width)
{
	for (uint8_t i = 0; i < ParallelMaxPorts; ++i) {
		if (parallel_baseaddr[i] == (port & 0xfffc) && parallelPortObjects[i]) {
			io_val_t retval = 0xff;
			switch (port & 0x7) {
			case 0: retval = parallelPortObjects[i]->Read_PR(); break;
			case 1: retval = parallelPortObjects[i]->Read_SR(); break;
			case 2: retval = parallelPortObjects[i]->Read_COM(); break;
			default: break;
			}
#if PARALLEL_DEBUG
			const char* const dbgtext[] = {"DAT", "STA", "COM", "???"};
			parallelPortObjects[i]->log_par(
			        parallelPortObjects[i]->dbg_cregs,
			        "read  0x%2x from %s.",
			        retval,
			        dbgtext[port & 3]);
#endif
			return retval;
		}
	}
	return 0xff;
}

static void PARALLEL_Write(io_port_t port, io_val_t val, [[maybe_unused]] io_width_t width)
{
	const auto val8 = static_cast<uint8_t>(val);

	for (uint8_t i = 0; i < ParallelMaxPorts; ++i) {
		if (parallel_baseaddr[i] == (port & 0xfffc) && parallelPortObjects[i]) {
#if PARALLEL_DEBUG
			const char* const dbgtext[] = {"DAT", "IOS", "CON", "???"};
			parallelPortObjects[i]->log_par(
			        parallelPortObjects[i]->dbg_cregs,
			        "write 0x%2x to %s.",
			        val8,
			        dbgtext[port & 3]);
			if (parallelPortObjects[i]->dbg_plaindr && !(port & 0x3)) {
				fprintf(parallelPortObjects[i]->debugfp, "%c", val8);
			}
#endif
			switch (port & 0x3) {
			case 0: parallelPortObjects[i]->Write_PR(val8); return;
			case 1: parallelPortObjects[i]->Write_IOSEL(val8); return;
			case 2: parallelPortObjects[i]->Write_CON(val8); return;
			default: return;
			}
		}
	}
}

#if PARALLEL_DEBUG
#include <cstdarg>

void CParallel::log_par(bool active, const char* format, ...)
{
	if (!active) {
		return;
	}
	// copied from DEBUG_SHOWMSG
	char buf[512];
	buf[0] = 0;
	snprintf(buf, sizeof(buf), "%12.3f ", PIC_FullIndex());
	va_list msg;
	va_start(msg, format);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), format, msg);
	va_end(msg);
	// Add newline if not present
	const size_t len = strlen(buf);
	if (len && buf[len - 1] != '\n') {
		safe_strcat(buf, "\r\n");
	}
	fputs(buf, debugfp);
}
#endif

// CParallel

CParallel::CParallel([[maybe_unused]] CommandLine* cmd, uint8_t portnr, uint8_t initirq)
        : port_nr(portnr),
          base(parallel_baseaddr[portnr]),
          irq(initirq)
{
	assert(portnr < ParallelMaxPorts);

#if PARALLEL_DEBUG
	dbg_data         = cmd->FindExist("dbgdata", false);
	dbg_putchar      = cmd->FindExist("dbgput", false);
	dbg_cregs        = cmd->FindExist("dbgregs", false);
	dbg_plainputchar = cmd->FindExist("dbgputplain", false);
	dbg_plaindr      = cmd->FindExist("dbgdataplain", false);

	if (cmd->FindExist("dbgall", false)) {
		dbg_data = dbg_putchar = dbg_cregs = true;
		dbg_plainputchar = dbg_plaindr = false;
	}

	if (dbg_data || dbg_putchar || dbg_cregs || dbg_plainputchar || dbg_plaindr) {
		debugfp = CAPTURE_CreateFile(CaptureType::ParallelLog);
	} else {
		debugfp = nullptr;
	}

	if (!debugfp) {
		dbg_data = dbg_putchar = dbg_plainputchar = dbg_cregs = false;
	} else {
		std::string cleft;
		cmd->GetStringRemain(cleft);
		log_par(true,
		        "Parallel%d: BASE %xh, initstring \"%s\"\r\n\r\n",
		        portnr + 1,
		        base,
		        cleft.c_str());
	}
#endif

	LOG_MSG("PARALLEL: Port LPT%u is at %xh", portnr + 1, base);

	for (uint8_t i = 0; i < 3; ++i) {
		WriteHandler[i].Install(i + base, PARALLEL_Write, io_width_t::byte);
		ReadHandler[i].Install(i + base, PARALLEL_Read, io_width_t::byte);
	}

	BIOS_SetLPTPort(portnr, base);

	mydosdevice = new device_LPT(portnr, this);
	DOS_AddDevice(mydosdevice);

	InstallationSuccessful = true;
}

CParallel::~CParallel()
{
	BIOS_SetLPTPort(port_nr, 0);
	if (mydosdevice) {
		// DOS_DelDevice() deletes the device itself.
		DOS_DelDevice(mydosdevice);
		mydosdevice = nullptr;
	}
}

uint8_t CParallel::getPrinterStatus()
{
	//  7      not busy
	//  6      acknowledge
	//  5      out of paper
	//  4      selected
	//  3      I/O error
	//  2-1    unused
	//  0      timeout
	uint8_t statusreg = Read_SR();
	statusreg ^= 0x48;
	return statusreg & ~0x7;
}

static void RunIdleTime(uint32_t milliseconds)
{
	const auto time = GetTicks() + milliseconds;
	while (GetTicks() < time) {
		CALLBACK_Idle();
	}
}

void CParallel::initialize()
{
	Write_IOSEL(0x55); // output mode
	Write_CON(0x08);   // init low
	Write_PR(0);
	RunIdleTime(10);
	Write_CON(0x0c); // init high
	RunIdleTime(500);
}

CParallel* parallelPortObjects[ParallelMaxPorts] = {nullptr, nullptr, nullptr};

// ParallelPorts -- owns the configured ports

class ParallelPorts {
public:
	ParallelPorts(const ParallelPorts&)            = delete;
	ParallelPorts& operator=(const ParallelPorts&) = delete;

	ParallelPorts(SectionProp* section)
	{
		assert(section);

#if C_PRINTER
		bool printer_used = false;
#endif
		// default ports & interrupts
		const uint8_t defaultirq[ParallelMaxPorts] = {7, 5, 12};

		char pname[] = "parallelx";

		for (uint8_t i = 0; i < ParallelMaxPorts; ++i) {
			// If a parallel port is already occupied by another
			// device (e.g. a Disney Sound Source on LPT1), skip it.
			// Added 2012-02-10 by Alun Bestor.
			uint16_t biosAddress = BIOS_ADDRESS_LPT1;
			switch (i) {
			case 1: biosAddress = BIOS_ADDRESS_LPT2; break;
			case 2: biosAddress = BIOS_ADDRESS_LPT3; break;
			case 0:
			default: biosAddress = BIOS_ADDRESS_LPT1; break;
			}
			if (mem_readw(biosAddress) != 0) {
				LOG_MSG("PARALLEL: LPT%u already taken, skipping",
				        i + 1);
				continue;
			}

			pname[8] = static_cast<char>('1' + i);
			CommandLine cmd(0, section->GetString(pname));

			std::string str;
			cmd.FindCommand(1, str);
			lowcase(str);

#ifdef C_DIRECTLPT
			if (str == "reallpt") {
				auto cdlpt = new CDirectLPT(i, defaultirq[i], &cmd);
				if (cdlpt->InstallationSuccessful) {
					parallelPortObjects[i] = cdlpt;
				} else {
					delete cdlpt;
					parallelPortObjects[i] = nullptr;
				}
			} else
#endif
			        if (str == "file") {
				auto cflpt = new CFileLPT(i, defaultirq[i], &cmd);
				if (cflpt->InstallationSuccessful) {
					parallelPortObjects[i] = cflpt;
				} else {
					delete cflpt;
					parallelPortObjects[i] = nullptr;
				}
			} else
#if C_PRINTER
			        if (str == "printer") {
				if (printer_used) {
					// only one parallel port may host the
					// printer
					LOG_WARNING("PARALLEL: Printer already attached to another port, skipping LPT%u",
					            i + 1);
					parallelPortObjects[i] = nullptr;
					continue;
				}
				auto cprd = new CPrinterRedir(i, defaultirq[i], &cmd);
				if (cprd->InstallationSuccessful) {
					parallelPortObjects[i] = cprd;
					printer_used           = true;
				} else {
					LOG_WARNING("PARALLEL: Printer is not enabled");
					delete cprd;
					parallelPortObjects[i] = nullptr;
				}
			} else
#endif
			        if (has_false(str) || str == "disabled") {
				parallelPortObjects[i] = nullptr;
			} else {
				LOG_WARNING("PARALLEL: Invalid type '%s' for LPT%u",
				            str.c_str(),
				            i + 1);
				parallelPortObjects[i] = nullptr;
			}
		}
	}

	~ParallelPorts()
	{
		for (auto& port : parallelPortObjects) {
			delete port;
			port = nullptr;
		}
	}
};

static std::unique_ptr<ParallelPorts> parallel_ports = {};

void PARALLEL_Init()
{
	auto section = get_section("parallel");
	assert(section);

	parallel_ports = std::make_unique<ParallelPorts>(section);
}

void PARALLEL_Destroy()
{
	parallel_ports = {};
}

static void notify_parallel_setting_updated([[maybe_unused]] SectionProp& section,
                                            [[maybe_unused]] const std::string& prop_name)
{
	PARALLEL_Destroy();
	PARALLEL_Init();
}

static void add_parallel_config_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto pstring = section.AddString("parallel1", WhenIdle, "disabled");
	pstring->SetHelp(
	        "Set the type of device connected to the LPT1 parallel port\n"
	        "('disabled' by default). Possible values:\n"
	        "\n"
	        "  disabled:  Disables the port.\n"
	        "  printer:   Emulates a printer attached to the port.\n"
	        "  file:      Redirects the port's output to a file. Parameters:\n"
	        "               dev:<device>  write to a device or file\n"
	        "               append:<file> append to a file\n"
	        "               timeout:<ms>  auto-flush delay in milliseconds\n"
	        "  reallpt:   Passes through to a real parallel port (not on macOS).\n");

	pstring = section.AddString("parallel2", WhenIdle, "disabled");
	pstring->SetHelp("See 'parallel1'.");

	pstring = section.AddString("parallel3", WhenIdle, "disabled");
	pstring->SetHelp("See 'parallel1'.");
}

void PARALLEL_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("parallel");
	section->AddUpdateHandler(notify_parallel_setting_updated);

	add_parallel_config_settings(*section);
}
