// ============================================================================
// FILE: src/boxer/boxer_hooks.cpp
// Stub implementation for Boxer integration hooks
// ============================================================================

#ifdef BOXER_INTEGRATED

#include "boxer/boxer_hooks.h"

// Global delegate pointer - set by Boxer before emulation starts
// When null, all hooks fall back to default behavior via BOXER_HOOK_* macros
IBoxerDelegate* g_boxer_delegate = nullptr;

#endif // BOXER_INTEGRATED

// No implementation code needed here!
// All hooks go through BOXER_HOOK_* macros which check g_boxer_delegate
// Actual implementation is on the Boxer side (Objective-C++)
