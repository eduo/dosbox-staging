/*
 * boxer_hooks.h - Boxer-DOSBox Integration Hook Infrastructure
 *
 * This header defines the IBoxerDelegate interface and hook macros that allow
 * DOSBox Staging to delegate control to Boxer for rendering, input handling,
 * file I/O, shell operations, MIDI, printing, and more.
 *
 * ARCHITECTURE:
 *   - IBoxerDelegate: Abstract interface with 86 integration point methods
 *   - g_boxer_delegate: Global pointer set by Boxer before emulation starts
 *   - BOXER_HOOK_*: Macros for safe hook invocation with default fallbacks
 *
 * THREAD SAFETY:
 *   All hook methods must be thread-safe. Most are called from the emulation
 *   thread, but some (like processEvents) may be called from UI threads.
 *
 * PERFORMANCE:
 *   Critical hooks (e.g., runLoopShouldContinue) are called ~10,000 times/sec
 *   and must complete in <1μs to avoid emulation performance degradation.
 *
 * Copyright (c) 2013 Alun Bestor and contributors. All rights reserved.
 * This source file is released under the GNU General Public License 2.0.
 */

#ifndef BOXER_HOOKS_H
#define BOXER_HOOKS_H

#ifdef BOXER_INTEGRATED

#include "boxer_types.h"
#include <cstdio>

// ============================================================================
// IBoxerDelegate - Abstract Interface for Boxer Integration
// ============================================================================

/**
 * @brief Abstract interface for Boxer integration callbacks
 *
 * DOSBox calls these methods to delegate control to Boxer for rendering,
 * input, file I/O, shell operations, and more. Boxer implements this
 * interface and sets g_boxer_delegate before starting emulation.
 *
 * All methods must be implemented by Boxer. Default implementations are
 * not provided - the delegate pointer is checked before invocation.
 */
class IBoxerDelegate {
public:
    virtual ~IBoxerDelegate() = default;

    // ========================================================================
    // Emulation Lifecycle (5 points) - CRITICAL
    // ========================================================================

    /**
     * @brief Check if emulation loop should continue
     * @return true to continue, false to abort immediately
     * @performance MUST complete in <1μs (called ~10,000/sec)
     * @thread-safety MUST be safe from emulation thread
     * @critical This is THE emergency abort mechanism
     *
     * This is the most frequently called hook and the most critical.
     * When the user closes the window or quits Boxer, this returns false
     * to immediately stop emulation. Without this, DOSBox loops forever.
     *
     * Implementation must use atomic flag (no locks) for thread safety.
     */
    virtual bool runLoopShouldContinue() = 0;

    /**
     * @brief Called when emulation loop is about to start
     * @param context_info Optional context data from Boxer
     *
     * Boxer uses this to initialize rendering resources (Metal textures),
     * audio buffers, and input device state before emulation begins.
     */
    virtual void runLoopWillStartWithContextInfo(void* context_info) = 0;

    /**
     * @brief Called when emulation loop has finished
     * @param context_info Optional context data from Boxer
     *
     * Boxer uses this to save game state, clean up resources, and
     * update UI to show emulation has stopped.
     */
    virtual void runLoopDidFinishWithContextInfo(void* context_info) = 0;

    /**
     * @brief Called before shutdown to clean up resources
     *
     * Boxer releases Metal rendering resources, closes audio devices,
     * and performs final cleanup before program exit.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Handle DOSBox title change with emulation stats
     * @param cycles Current CPU cycles setting
     * @param frameskip Current frameskip value
     * @param paused true if emulation is paused
     *
     * Maps to legacy GFX_SetTitle. DOSBox updates title to show CPU
     * cycles, frameskip, and pause state. Boxer formats and displays
     * this in its window title.
     */
    virtual void handleDOSBoxTitleChange(Bit32s cycles, int frameskip, bool paused) = 0;

    // ========================================================================
    // Rendering Pipeline (15 points) - CRITICAL
    // ========================================================================

    /**
     * @brief Process pending events (keyboard, mouse, window)
     * @return true if events were processed, false on error
     *
     * Maps to legacy GFX_Events macro. Called frequently to handle
     * window events, input, and keep UI responsive.
     */
    virtual bool processEvents() = 0;

    /**
     * @brief Process events if enough time has elapsed
     * @return true if events were processed
     *
     * Maps to legacy GFX_MaybeProcessEvents. Throttles event processing
     * to avoid excessive overhead during intense emulation.
     */
    virtual bool MaybeProcessEvents() = 0;

    /**
     * @brief Start a new frame
     * @param[out] frameBuffer Pointer to receive pixel buffer address
     * @param[out] pitch Row stride in bytes
     * @return true if frame should be rendered, false to skip
     *
     * Maps to legacy GFX_StartUpdate. DOSBox calls this before rendering
     * a frame. Boxer provides a buffer for DOSBox to draw into.
     *
     * @performance Called 60-70 times/sec, should be fast
     */
    virtual bool startFrame(Bit8u** frameBuffer, int& pitch) = 0;

    /**
     * @brief Finish the current frame and present it
     * @param changedLines Bitmask of which scanlines changed (for optimization)
     *
     * Maps to legacy GFX_EndUpdate. DOSBox has finished rendering.
     * Boxer uploads texture to GPU and presents to screen.
     *
     * @performance Called 60-70 times/sec, must be fast
     */
    virtual void finishFrame(const uint16_t* changedLines) = 0;

    /**
     * @brief Prepare for new frame size/format
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @param gfx_flags Graphics mode flags
     * @param scalex Horizontal scaling factor
     * @param scaley Vertical scaling factor
     * @param callback Callback for mode changes
     * @param pixel_aspect Pixel aspect ratio
     * @return DOSBox internal mode ID
     *
     * Maps to legacy GFX_SetSize. Called when DOS program changes video mode.
     * Boxer reallocates render targets to match new dimensions.
     */
    virtual Bitu prepareForFrameSize(Bitu width, Bitu height, Bitu gfx_flags,
                                     double scalex, double scaley,
                                     GFX_CallBack_t callback,
                                     double pixel_aspect) = 0;

    /**
     * @brief Get ideal output mode for given flags
     * @param flags Graphics mode capability flags
     * @return Recommended output mode ID
     *
     * Maps to legacy GFX_GetBestMode. DOSBox queries which rendering
     * mode to use (indexed color, RGB, etc). Boxer returns its preference.
     */
    virtual Bitu idealOutputMode(Bitu flags) = 0;

    /**
     * @brief Get RGB value for palette entry
     * @param red Red component (0-255)
     * @param green Green component (0-255)
     * @param blue Blue component (0-255)
     * @return Packed RGB value in platform format
     *
     * Maps to legacy GFX_GetRGB. For indexed color modes, converts
     * palette entries to RGB. Boxer handles endianness and format.
     */
    virtual Bitu getRGBPaletteEntry(Bit8u red, Bit8u green, Bit8u blue) = 0;

    /**
     * @brief Set shader for rendering
     * @param shaderSource Shader source code (GLSL/Metal)
     *
     * Maps to legacy GFX_SetShader. DOSBox can request special shaders
     * for CRT effects, scaling filters, etc. Boxer compiles and applies.
     */
    virtual void setShader(const char* shaderSource) = 0;

    /**
     * @brief Apply current rendering strategy
     *
     * Called from render.cpp to notify Boxer to reconfigure rendering
     * pipeline based on current settings (scaler, aspect ratio, etc).
     */
    virtual void applyRenderingStrategy() = 0;

    /**
     * @brief Get display refresh rate
     * @return Refresh rate in Hz (typically 60)
     *
     * Used by DOSBox for frame timing. Boxer queries the actual
     * display refresh rate from macOS.
     */
    virtual int GetDisplayRefreshRate() = 0;

    // ========================================================================
    // Graphics Modes (7 points) - Special video mode support
    // ========================================================================

    /**
     * @brief Get current Hercules tint mode
     * @return Tint mode index (0=white, 1=green, 2=amber)
     *
     * Hercules monochrome graphics can be tinted. Boxer stores this
     * preference and applies it during rendering.
     */
    virtual Bit8u herculesTintMode() = 0;

    /**
     * @brief Set Hercules tint mode
     * @param mode Tint mode index
     */
    virtual void setHerculesTintMode(Bit8u mode) = 0;

    /**
     * @brief Get CGA composite hue offset
     * @return Hue offset in degrees (0-360)
     *
     * CGA composite mode can adjust hue to fix color accuracy.
     * Boxer stores this as a user preference.
     */
    virtual double CGACompositeHueOffset() = 0;

    /**
     * @brief Set CGA composite hue offset
     * @param offset Hue offset in degrees
     */
    virtual void setCGACompositeHueOffset(double offset) = 0;

    /**
     * @brief Get CGA component mode (RGB vs composite)
     * @return true if RGB mode, false if composite
     *
     * CGA can output RGB (sharp) or composite (artifact colors).
     * Boxer stores this preference.
     */
    virtual Bit8u CGAComponentMode() = 0;

    /**
     * @brief Set CGA component mode
     * @param mode 1 for RGB, 0 for composite
     */
    virtual void setCGAComponentMode(Bit8u mode) = 0;

    // ========================================================================
    // Shell Integration (15 points)
    // ========================================================================

    /**
     * @brief Called when DOS shell is about to start
     * @param shell Pointer to DOS_Shell instance
     *
     * Boxer suppresses its launcher UI and prepares for command input.
     */
    virtual void shellWillStart(DOS_Shell* shell) = 0;

    /**
     * @brief Called when DOS shell has finished
     * @param shell Pointer to DOS_Shell instance
     * @param exit_code Shell exit code
     *
     * Boxer cleans up and returns to launcher UI.
     */
    virtual void shellDidFinish(DOS_Shell* shell, int exit_code) = 0;

    /**
     * @brief Called when AUTOEXEC.BAT is about to run
     * @param shell Pointer to DOS_Shell instance
     *
     * Boxer tracks autoexec execution for progress indication.
     */
    virtual void shellWillStartAutoexec(DOS_Shell* shell) = 0;

    /**
     * @brief Called when control returns to DOS prompt
     * @param shell Pointer to DOS_Shell instance
     *
     * Boxer shows prompt UI and enables command input.
     */
    virtual void didReturnToShell(DOS_Shell* shell) = 0;

    /**
     * @brief Check if Boxer wants to handle a command
     * @param shell Pointer to DOS_Shell instance
     * @param cmd Command name
     * @param args Command arguments
     * @return true if Boxer handled it, false to let DOSBox execute
     *
     * Allows Boxer to intercept commands like CD-ROM mounting,
     * special game launchers, etc.
     */
    virtual bool shellShouldRunCommand(DOS_Shell* shell, const char* cmd, const char* args) = 0;

    /**
     * @brief Called before shell reads command input
     * @param shell Pointer to DOS_Shell instance
     * @param handle File handle being read from
     *
     * Notifies Boxer that shell is waiting for input.
     */
    virtual void shellWillReadCommandInputFromHandle(DOS_Shell* shell, Bit16u handle) = 0;

    /**
     * @brief Called after shell reads command input
     * @param shell Pointer to DOS_Shell instance
     * @param handle File handle that was read
     *
     * Notifies Boxer that input was received.
     */
    virtual void shellDidReadCommandInputFromHandle(DOS_Shell* shell, Bit16u handle) = 0;

    /**
     * @brief Allow Boxer to modify command input
     * @param shell Pointer to DOS_Shell instance
     * @param[in,out] cmd Command buffer (can be modified)
     * @param[in,out] cursorPosition Cursor position
     * @param[in,out] executeImmediately Whether to execute now
     * @return true if Boxer modified any parameters
     *
     * Boxer can inject commands, modify input, or trigger immediate execution.
     */
    virtual bool handleShellCommandInput(DOS_Shell* shell, char* cmd,
                                        Bitu* cursorPosition,
                                        bool* executeImmediately) = 0;

    /**
     * @brief Check if Boxer has pending commands to execute
     * @param shell Pointer to DOS_Shell instance
     * @return true if commands are queued
     *
     * Boxer can queue commands to execute (e.g., from drag-and-drop).
     */
    virtual bool hasPendingCommandsForShell(DOS_Shell* shell) = 0;

    /**
     * @brief Execute next pending command from Boxer
     * @param shell Pointer to DOS_Shell instance
     * @return true if command was executed
     *
     * Dequeues and executes one command from Boxer's command queue.
     */
    virtual bool executeNextPendingCommandForShell(DOS_Shell* shell) = 0;

    /**
     * @brief Check if shell should display startup messages
     * @param shell Pointer to DOS_Shell instance
     * @return true to show messages, false to suppress
     *
     * Boxer typically suppresses DOSBox startup messages for cleaner UX.
     */
    virtual bool shellShouldDisplayStartupMessages(DOS_Shell* shell) = 0;

    /**
     * @brief Called before executing a program/batch file
     * @param shell Pointer to DOS_Shell instance
     * @param canonicalPath Full DOS path to executable
     * @param arguments Command-line arguments
     *
     * Boxer applies game-specific configs, loads save states, etc.
     */
    virtual void shellWillExecuteFileAtDOSPath(DOS_Shell* shell,
                                              const char* canonicalPath,
                                              const char* arguments) = 0;

    /**
     * @brief Called after program execution finished
     * @param shell Pointer to DOS_Shell instance
     * @param canonicalPath Full DOS path to executable
     *
     * Boxer saves state, updates UI, re-enables controls.
     */
    virtual void shellDidExecuteFileAtDOSPath(DOS_Shell* shell,
                                             const char* canonicalPath) = 0;

    /**
     * @brief Called when batch file starts
     * @param shell Pointer to DOS_Shell instance
     * @param canonicalPath Full DOS path to batch file
     * @param arguments Command-line arguments
     *
     * Boxer tracks batch file nesting for debugging.
     */
    virtual void shellWillBeginBatchFile(DOS_Shell* shell,
                                        const char* canonicalPath,
                                        const char* arguments) = 0;

    /**
     * @brief Called when batch file ends
     * @param shell Pointer to DOS_Shell instance
     * @param canonicalPath Full DOS path to batch file
     *
     * Boxer updates batch file stack.
     */
    virtual void shellDidEndBatchFile(DOS_Shell* shell,
                                     const char* canonicalPath) = 0;

    /**
     * @brief Check if shell should continue processing
     * @param shell Pointer to DOS_Shell instance
     * @return false to exit shell immediately
     *
     * Allows Boxer to abort shell (e.g., on window close).
     */
    virtual bool shellShouldContinue(DOS_Shell* shell) = 0;

    // ========================================================================
    // Drive and File I/O (11 points)
    // ========================================================================

    /**
     * @brief Verify if path is allowed to be mounted
     * @param path Host filesystem path
     * @return true if mounting is allowed
     *
     * Security check - Boxer can prevent mounting system directories.
     */
    virtual bool shouldMountPath(const char* path) = 0;

    /**
     * @brief Check if file should be visible to DOS
     * @param name Filename
     * @return true to show, false to hide
     *
     * Hides macOS metadata files (.DS_Store, ._*) from DOS programs
     * to prevent corruption and confusion.
     *
     * @critical Security and data integrity hook
     */
    virtual bool shouldShowFileWithName(const char* name) = 0;

    /**
     * @brief Check if DOS should have write access to path
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @return true if write allowed, false for read-only
     *
     * Critical security hook - prevents DOS from corrupting macOS files.
     * Boxer enforces write protection on system paths.
     *
     * @critical Security hook
     */
    virtual bool shouldAllowWriteAccessToPath(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Called when drive is mounted
     * @param driveIndex Drive letter index (0=A, 1=B, etc)
     *
     * Boxer updates drive list UI.
     */
    virtual void driveDidMount(Bit8u driveIndex) = 0;

    /**
     * @brief Called when drive is unmounted
     * @param driveIndex Drive letter index
     *
     * Boxer updates drive list UI.
     */
    virtual void driveDidUnmount(Bit8u driveIndex) = 0;

    /**
     * @brief Called when DOS creates a file
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     *
     * Boxer can track file creation for save game detection.
     */
    virtual void didCreateLocalFile(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Called when DOS deletes a file
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     *
     * Boxer can track deletions for undo functionality.
     */
    virtual void didRemoveLocalFile(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Open local file with Boxer's file handling
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @param mode fopen() mode string ("rb", "wb", etc)
     * @return FILE* pointer or nullptr on error
     *
     * Allows Boxer to intercept file opens for special handling
     * (e.g., copy-on-write for game preservation).
     */
    virtual FILE* openLocalFile(const char* path, DOS_Drive* drive, const char* mode) = 0;

    /**
     * @brief Remove local file
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @return true on success
     *
     * Wrapper for file deletion with Boxer's permission checks.
     */
    virtual bool removeLocalFile(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Move/rename local file
     * @param fromPath Source path
     * @param toPath Destination path
     * @param drive DOSBox drive object
     * @return true on success
     *
     * Allows Boxer to track file moves for save game management.
     */
    virtual bool moveLocalFile(const char* fromPath, const char* toPath, DOS_Drive* drive) = 0;

    /**
     * @brief Create local directory
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @return true on success
     */
    virtual bool createLocalDir(const char* path, DOS_Drive* drive) = 0;

    // ========================================================================
    // Additional File I/O (continuation of 11 points)
    // ========================================================================

    /**
     * @brief Remove local directory
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @return true on success
     */
    virtual bool removeLocalDir(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Get file/directory statistics
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @param[out] outStatus Pointer to stat structure to fill
     * @return true on success
     *
     * Wrapper for stat() with Boxer's permission checks.
     */
    virtual bool getLocalPathStats(const char* path, DOS_Drive* drive, struct stat* outStatus) = 0;

    /**
     * @brief Check if local directory exists
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @return true if exists and is directory
     */
    virtual bool localDirectoryExists(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Check if local file exists
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @return true if exists and is file
     */
    virtual bool localFileExists(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Open directory for enumeration
     * @param path Host filesystem path
     * @param drive DOSBox drive object
     * @return Directory handle or nullptr on error
     *
     * Opens directory and returns opaque handle for iteration.
     */
    virtual DIR_Handle openLocalDirectory(const char* path, DOS_Drive* drive) = 0;

    /**
     * @brief Close directory handle
     * @param handle Directory handle from openLocalDirectory
     */
    virtual void closeLocalDirectory(DIR_Handle handle) = 0;

    /**
     * @brief Get next entry from directory
     * @param handle Directory handle
     * @param[out] outName Buffer for filename (at least 256 bytes)
     * @param[out] isDirectory Set to true if entry is directory
     * @return true if entry retrieved, false at end
     *
     * Iterates directory entries, filtering per shouldShowFileWithName.
     */
    virtual bool getNextDirectoryEntry(DIR_Handle handle, char* outName, bool& isDirectory) = 0;

    // ========================================================================
    // Input Handling (16 points)
    // ========================================================================

    /**
     * @brief Set mouse capture state
     * @param active true to capture mouse, false to release
     *
     * Maps to legacy Mouse_AutoLock. When true, mouse is confined to
     * window and hidden. Boxer handles cursor visibility.
     */
    virtual void setMouseActive(bool active) = 0;

    /**
     * @brief Notify of mouse movement
     * @param x X coordinate (normalized 0.0-1.0)
     * @param y Y coordinate (normalized 0.0-1.0)
     *
     * Boxer forwards mouse events from Cocoa to DOSBox.
     */
    virtual void mouseMovedToPoint(float x, float y) = 0;

    /**
     * @brief Set joystick active state
     * @param active true if joystick is being used
     *
     * Boxer can show joystick indicator in UI.
     */
    virtual void setJoystickActive(bool active) = 0;

    /**
     * @brief Get remaining space in keyboard buffer
     * @return Number of key events that can be buffered
     *
     * Used to check if buffer has room before sending keys.
     */
    virtual Bitu keyboardBufferRemaining() = 0;

    /**
     * @brief Check if keyboard layout is loaded
     * @return true if layout loaded successfully
     */
    virtual bool keyboardLayoutLoaded() = 0;

    /**
     * @brief Get current keyboard layout name
     * @return Layout code (e.g., "us", "de", "fr")
     */
    virtual const char* keyboardLayoutName() = 0;

    /**
     * @brief Check if keyboard layout is supported
     * @param code Layout code to check
     * @return true if supported
     */
    virtual bool keyboardLayoutSupported(const char* code) = 0;

    /**
     * @brief Check if keyboard layout translation is active
     * @return true if active
     */
    virtual bool keyboardLayoutActive() = 0;

    /**
     * @brief Set keyboard layout translation active state
     * @param active true to enable translation
     */
    virtual void setKeyboardLayoutActive(bool active) = 0;

    /**
     * @brief Set Num Lock LED state
     * @param active true if Num Lock is on
     *
     * Boxer syncs LED state with macOS keyboard LEDs.
     */
    virtual void setNumLockActive(bool active) = 0;

    /**
     * @brief Set Caps Lock LED state
     * @param active true if Caps Lock is on
     */
    virtual void setCapsLockActive(bool active) = 0;

    /**
     * @brief Set Scroll Lock LED state
     * @param active true if Scroll Lock is on
     */
    virtual void setScrollLockActive(bool active) = 0;

    /**
     * @brief Get preferred keyboard layout from OS
     * @return Layout code for current macOS keyboard
     *
     * Boxer queries macOS for current input source and maps to
     * DOSBox layout code.
     */
    virtual const char* preferredKeyboardLayout() = 0;

    /**
     * @brief Check if should continue listening for key events
     * @return false to interrupt keyboard polling
     *
     * Used in INT16 keyboard handler. Boxer can return false to
     * break out of keyboard wait loops (e.g., on window close).
     */
    virtual bool continueListeningForKeyEvents() = 0;

    /**
     * @brief Get count of keys in paste buffer
     * @return Number of BIOS keycodes available
     *
     * Boxer can queue up keycodes for pasting text into DOS.
     */
    virtual Bitu numKeyCodesInPasteBuffer() = 0;

    /**
     * @brief Get next keycode from paste buffer
     * @param[out] outKeyCode Pointer to receive BIOS keycode
     * @param consumeKey true to remove from buffer, false to peek
     * @return true if keycode was available
     *
     * Retrieves next key from Boxer's paste buffer. Used to implement
     * clipboard paste functionality.
     */
    virtual bool getNextKeyCodeInPasteBuffer(Bit16u* outKeyCode, bool consumeKey) = 0;

    // ========================================================================
    // Printer/Parallel Port (6 points)
    // ========================================================================

    /**
     * @brief Read data from printer port
     * @param port Port number (0=LPT1, 1=LPT2, 2=LPT3)
     * @param iolen I/O operation length
     * @return Data byte
     *
     * Reads from parallel port data register (0x378, 0x278, 0x3BC).
     */
    virtual Bitu PRINTER_readdata(Bitu port, Bitu iolen) = 0;

    /**
     * @brief Write data to printer port
     * @param port Port number
     * @param val Data byte to write
     * @param iolen I/O operation length
     *
     * Writes to parallel port data register. Boxer forwards to
     * virtual printer for rendering.
     */
    virtual void PRINTER_writedata(Bitu port, Bitu val, Bitu iolen) = 0;

    /**
     * @brief Read printer status register
     * @param port Port number
     * @param iolen I/O operation length
     * @return Status byte
     *
     * Returns printer status (busy, paper out, etc). Boxer emulates
     * a always-ready printer.
     */
    virtual Bitu PRINTER_readstatus(Bitu port, Bitu iolen) = 0;

    /**
     * @brief Write printer control register
     * @param port Port number
     * @param val Control byte
     * @param iolen I/O operation length
     *
     * Controls strobe, auto-feed, init signals. Boxer interprets
     * these to trigger page breaks, form feeds, etc.
     */
    virtual void PRINTER_writecontrol(Bitu port, Bitu val, Bitu iolen) = 0;

    /**
     * @brief Read printer control register
     * @param port Port number
     * @param iolen I/O operation length
     * @return Control byte
     */
    virtual Bitu PRINTER_readcontrol(Bitu port, Bitu iolen) = 0;

    /**
     * @brief Check if printer is initialized
     * @param port Port number
     * @return true if printer is available
     *
     * Used by DOSBox to check if printer redirection is active.
     */
    virtual bool PRINTER_isInited(Bitu port) = 0;

    // ========================================================================
    // Audio/MIDI (8 points)
    // ========================================================================

    /**
     * @brief Check if MIDI output is available
     * @return true if MIDI device is connected
     *
     * Maps to legacy MIDI_Available macro. Boxer returns true if
     * CoreMIDI is initialized and has output devices.
     */
    virtual bool MIDIAvailable() = 0;

    /**
     * @brief Send MIDI message
     * @param data MIDI message bytes
     * @param length Message length
     *
     * Sends MIDI channel message or system message to Boxer's
     * CoreMIDI output. Boxer routes to selected MIDI device.
     */
    virtual void sendMIDIMessage(const uint8_t* data, size_t length) = 0;

    /**
     * @brief Send MIDI System Exclusive message
     * @param data SysEx data (excluding F0/F7 markers)
     * @param length Data length
     *
     * Sends large MIDI data dumps (instrument configs, samples, etc).
     * Boxer buffers and transmits via CoreMIDI.
     */
    virtual void sendMIDISysex(const uint8_t* data, size_t length) = 0;

    /**
     * @brief Get suggested MIDI handler name
     * @return Handler name string (e.g., "coremidi", "fluidsynth")
     *
     * Boxer returns "coremidi" to use macOS MIDI routing.
     */
    virtual const char* suggestMIDIHandler() = 0;

    /**
     * @brief Called when MIDI subsystem will restart
     *
     * Notifies Boxer to save MIDI state before reset.
     */
    virtual void MIDIWillRestart() = 0;

    /**
     * @brief Called when MIDI subsystem has restarted
     *
     * Notifies Boxer to restore MIDI connections.
     */
    virtual void MIDIDidRestart() = 0;

    /**
     * @brief Get master volume level
     * @return Volume (0.0 = muted, 1.0 = full)
     *
     * Returns Boxer's master volume setting. DOSBox uses this to
     * scale all audio output.
     */
    virtual float masterVolume() = 0;

    /**
     * @brief Called when DOSBox updates volume levels
     *
     * Notifies Boxer that mixer levels changed (e.g., SB volume command).
     * Boxer can update volume UI.
     */
    virtual void updateVolumes() = 0;

    // ========================================================================
    // Messages, Logging, Error Handling (3 points)
    // ========================================================================

    /**
     * @brief Get localized string for key
     * @param key Message key (e.g., "SHELL_CMD_HELP")
     * @return Localized string or nullptr for default
     *
     * Maps to legacy localizedStringForKey. Allows Boxer to provide
     * localized messages (French, German, etc) for DOSBox UI.
     */
    virtual const char* localizedStringForKey(const char* key) = 0;

    /**
     * @brief Log message
     * @param message Message to log
     *
     * Maps to legacy GFX_ShowMsg. DOSBox logging goes to Boxer's
     * log viewer instead of console.
     */
    virtual void log(const char* message) = 0;

    /**
     * @brief Fatal error - terminate emulation
     * @param message Error message
     *
     * Maps to legacy E_Exit macro. DOSBox encountered unrecoverable
     * error. Boxer shows error dialog and exits gracefully.
     */
    virtual void die(const char* message) = 0;

    // ========================================================================
    // Capture Support (1 point)
    // ========================================================================

    /**
     * @brief Open capture file (screenshot, video, audio)
     * @param filename Filename to create
     * @param mode File mode
     * @return FILE* pointer or nullptr
     *
     * Maps to legacy OpenCaptureFile. DOSBox wants to save screenshot
     * or video. Boxer chooses save location and manages files.
     */
    virtual FILE* openCaptureFile(const char* filename, const char* mode) = 0;
};

// ============================================================================
// Global Delegate Registration
// ============================================================================

/**
 * @brief Global delegate pointer
 *
 * Boxer sets this to its delegate implementation before starting emulation.
 * DOSBox checks if non-nullptr before calling any hooks.
 *
 * Thread safety: Boxer must set this before starting DOSBox threads.
 * Once set, it should not be changed until emulation stops.
 */
extern IBoxerDelegate* g_boxer_delegate;

// ============================================================================
// Hook Invocation Macros
// ============================================================================

/**
 * @brief Invoke hook that returns bool with safe default
 *
 * If delegate is set, calls the hook. Otherwise returns true (continue).
 * Use for optional hooks where absence means "yes/continue".
 *
 * Example:
 *   if (BOXER_HOOK_BOOL(shellShouldContinue, shell)) {
 *       // Continue processing
 *   }
 */
#define BOXER_HOOK_BOOL(name, ...) \
    (g_boxer_delegate ? g_boxer_delegate->name(__VA_ARGS__) : true)

/**
 * @brief Invoke critical hook that returns bool
 *
 * Similar to BOXER_HOOK_BOOL but logs error if delegate is missing.
 * Use for hooks that should always be implemented.
 *
 * Example:
 *   if (!BOXER_HOOK_BOOL_REQUIRED(runLoopShouldContinue)) {
 *       break; // Abort emulation
 *   }
 */
#define BOXER_HOOK_BOOL_REQUIRED(name, ...) \
    (g_boxer_delegate ? g_boxer_delegate->name(__VA_ARGS__) : \
        (fprintf(stderr, "BOXER ERROR: Required hook '" #name "' called without delegate\n"), true))

/**
 * @brief Invoke hook that returns void
 *
 * Checks if delegate exists before calling. Safe to call even if
 * delegate is not set (becomes a no-op).
 *
 * Example:
 *   BOXER_HOOK_VOID(shellDidFinish, shell, exit_code);
 */
#define BOXER_HOOK_VOID(name, ...) \
    do { if (g_boxer_delegate) g_boxer_delegate->name(__VA_ARGS__); } while(0)

/**
 * @brief Invoke hook that returns a value
 *
 * If delegate is set, calls hook and returns its value.
 * Otherwise returns the provided default value.
 *
 * Example:
 *   int refresh_rate = BOXER_HOOK_VALUE(GetDisplayRefreshRate, 60);
 */
#define BOXER_HOOK_VALUE(name, default_val, ...) \
    (g_boxer_delegate ? g_boxer_delegate->name(__VA_ARGS__) : (default_val))

/**
 * @brief Invoke hook that returns a pointer
 *
 * Similar to BOXER_HOOK_VALUE but specifically for pointer returns.
 * Returns nullptr if delegate is not set.
 *
 * Example:
 *   FILE* f = BOXER_HOOK_PTR(openLocalFile, path, drive, "rb");
 */
#define BOXER_HOOK_PTR(name, ...) \
    (g_boxer_delegate ? g_boxer_delegate->name(__VA_ARGS__) : nullptr)

#endif // BOXER_INTEGRATED

#endif // BOXER_HOOKS_H
