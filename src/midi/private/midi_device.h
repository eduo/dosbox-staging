// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_DEVICE_H
#define DOSBOX_MIDI_DEVICE_H

#include <cstdint>
#include <memory>
#include <string>

#include "midi/midi.h"

namespace MidiDeviceName {

// Internal synths
constexpr auto FluidSynth  = "fluidsynth";
constexpr auto SoundCanvas = "soundcanvas";
constexpr auto Mt32        = "mt32";

// External devices
constexpr auto Alsa      = "alsa";
constexpr auto CoreAudio = "coreaudio";
constexpr auto CoreMidi  = "coremidi";
constexpr auto Win32     = "win32";
} // namespace MidiDeviceName

// Generic interface for all MIDI devices. MIDI devices can be either internal
// (our three MIDI synths: SoundCanvas, MT-32, and FluidSynth; see
// `midi_synth.cpp`), or external (various OS-specific ways to output raw MIDI
// data from DOSBox Staging).
class MidiDevice {
public:
	enum class Type { Internal, External };

	virtual ~MidiDevice() = default;

	virtual std::string GetName() const = 0;
	virtual Type GetType() const        = 0;

	virtual void SendMidiMessage(const MidiMessage& msg)      = 0;
	virtual void SendSysExMessage(uint8_t* sysex, size_t len) = 0;
};

#if C_BOXER
// Boxer drives MIDI itself. It picks the output device from its own
// preferences and UI (BXMIDISynth, BXExternalMIDIDevice, or MT-32 through
// Boxer's own MT32Emu framework), and it can swap in an MT-32-capable device
// mid-session when it sniffs MT-32 SysEx -- none of which upstream's device
// list can express. So rather than patching midi.cpp's four send sites as the
// 0.78 fork did, Boxer supplies a MidiDevice of its own, the same way
// BXGFXBridge.mm supplies a RenderBackend.
//
// These are the `mididevice` values that hand the MPU-401 stream to Boxer.
// Everything else -- 'port', 'coremidi', 'coreaudio' -- still reaches
// upstream's own devices, so a user who wants raw host MIDI or the macOS
// SoundFont synth can still ask for it. See FINDINGS.md, D38.
namespace MidiDeviceName {
// 'auto': let Boxer decide, and autodetect MT-32 music from the SysEx stream.
// This is also Boxer's default, replacing upstream's 'port'.
constexpr auto BoxerAuto = "auto";

// 'generalmidi': force General MIDI, defeating the MT-32 autodetection.
// Boxer's shipped "General MIDI.conf" profile used to spell this 'coreaudio',
// which now means upstream's own CoreAudio synth instead (D38).
constexpr auto BoxerGeneralMidi = "generalmidi";

// MidiDeviceName::Mt32 ('mt32') is Boxer's too: upstream's MT-32 is gated out
// (C_MT32EMU 0, D11) precisely because Boxer supplies its own.
} // namespace MidiDeviceName

// Implemented by Boxer in BXCoalfaceAudio.mm. Returns nullptr if `name` is not
// one of the names above, so midi.cpp can fall through to its own devices.
std::unique_ptr<MidiDevice> BOXER_CreateMidiDevice(const std::string& name,
                                                   const std::string& config);

// Called instead when MIDI output is switched off ('mididevice = none'), so
// that Boxer drops its own device rather than leaving the last one attached
// and audible.
void BOXER_NotifyMidiDisabled();
#endif // C_BOXER

// Send All Notes Off and Reset All Controllers to all MIDI channels for this
// device.
void MIDI_Reset(MidiDevice* device);

MidiDevice* MIDI_GetCurrentDevice();

#endif // DOSBOX_MIDI_DEVICE_H
