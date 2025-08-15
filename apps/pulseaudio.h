// apps/pulseaudio.h
#pragma once

#ifdef _WIN32
// Minimal PulseAudio Simple API subset implemented by a Windows shim.
// Only what detect-live/enroll-live need.

#include <cstddef>
#include <cstdint>

// pa_sample_format_t (subset)
enum {
	PA_SAMPLE_INVALID = -1,
	PA_SAMPLE_U8 = 0,
	PA_SAMPLE_ALAW,
	PA_SAMPLE_ULAW,
	PA_SAMPLE_S16LE, // we support this
};

// Stream direction (subset)
enum {
	PA_STREAM_NODIRECTION = 0,
	PA_STREAM_PLAYBACK = 1,
	PA_STREAM_RECORD = 2,
};

// Error codes (subset)
enum {
	PA_OK = 0,
	PA_ERR_INTERNAL = -1,
	PA_ERR_INVALID = -2
};

struct pa_sample_spec {
	int format;		  // PA_SAMPLE_*
	uint32_t rate;	  // Hz
	uint8_t channels; // 1 or 2 (we’ll support both)
};

// Opaque in real PA; defined in the shim
struct pa_simple;

pa_simple* pa_simple_new(
	const char* /*server*/,
	const char* /*name*/,
	int dir,
	const char* /*dev*/,
	const char* /*stream_name*/,
	const pa_sample_spec* ss,
	void* /*channel_map*/,
	void* /*buffer_attr*/,
	int* error);

void pa_simple_free(pa_simple* s);

// Blocking read of exactly 'bytes' into 'data'
int pa_simple_read(pa_simple* s, void* data, size_t bytes, int* error);

#else
// Non-Windows: use real PulseAudio
#include <pulse/error.h>
#include <pulse/simple.h>
#endif
