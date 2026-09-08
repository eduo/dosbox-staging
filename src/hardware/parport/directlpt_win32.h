/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// include guard
#ifndef DOSBOX_DIRECTLPT_WIN32_H
#define DOSBOX_DIRECTLPT_WIN32_H

#include "dosbox.h"
#include "config/setup.h"

#if C_DIRECTLPT
#ifdef WIN32



#define DIRECTLPT_AVAILIBLE
#include "parport.h"
//#include <windows.h>


class CDirectLPT : public CParallel {
public:
	//HANDLE driverHandle;
	uint32_t realbaseaddress;
	uint8_t originalECPControlReg;
	
	CDirectLPT(
			uint32_t nr,
			uint8_t initIrq,
			CommandLine* cmd
            );
	

	~CDirectLPT();
	
	bool interruptflag;
	bool isECP;
	// InstallationSuccessful is inherited from CParallel.
	bool ack_polarity;

	uint8_t Read_PR();
	uint8_t Read_COM();
	uint8_t Read_SR();

	void Write_PR(uint8_t);
	void Write_CON(uint8_t);
	void Write_IOSEL(uint8_t);
	bool Putchar(uint8_t);

	void handleUpperEvent(uint16_t type);
};

#endif	// WIN32
#endif	// C_DIRECTLPT
#endif	// include guard
