// apps/pulseaudio_win.cpp
#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Request at least Windows 7 for WASAPI shared
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <algorithm>
#include <atomic>
#include <audioclient.h>
#include <cmath>
#include <cstring>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <new>
#include <stdexcept>
#include <stdint.h>
#include <vector>
#include <windows.h>

#include "pulseaudio.h" // our C shim
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

// Define required WASAPI GUIDs manually to avoid problematic headers
static const GUID my_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = 
	{ 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID my_KSDATAFORMAT_SUBTYPE_PCM = 
	{ 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

// --------------------- Small helpers ---------------------
static inline int16_t float_to_s16(float x) {
	// clamp to [-1,1], scale
	if (x > 1.0f) x = 1.0f;
	if (x < -1.0f) x = -1.0f;
	float y = x * 32767.0f;
	if (y > 32767.0f) y = 32767.0f;
	if (y < -32768.0f) y = -32768.0f;
	return static_cast<int16_t>(std::lrintf(y));
}

class ComInit {
public:
	ComInit() { hr_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
	~ComInit() {
		if (SUCCEEDED(hr_)) CoUninitialize();
	}

private:
	HRESULT hr_;
};

// --------------------- WASAPI capture ---------------------
struct pa_simple {
	// target format
	unsigned target_rate = 16000;
	// com/wasapi
	ComInit com;
	IMMDeviceEnumerator* enumr = nullptr;
	IMMDevice* device = nullptr;
	IAudioClient* client = nullptr;
	IAudioCaptureClient* cap = nullptr;
	WAVEFORMATEX* mix = nullptr;
	UINT32 bufferFrames = 0;
	bool started = false;

	// conversion state
	unsigned src_rate = 0;
	unsigned src_channels = 0;
	bool src_float = false;

	std::vector<float> src_fifo; // mono @ src_rate
	double t = 0.0;				 // fractional position in src_fifo for resampler
	double step = 1.0;			 // src_rate / target_rate

	std::vector<int16_t> out_fifo; // S16 mono @ 16 kHz

	pa_simple() = default;

	~pa_simple() {
		if (client && started) client->Stop();
		if (cap) cap->Release();
		if (client) client->Release();
		if (device) device->Release();
		if (enumr) enumr->Release();
		if (mix) CoTaskMemFree(mix);
	}

	void init() {
		HRESULT hr;
		// 1) default input device
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
							  __uuidof(IMMDeviceEnumerator), (void**)&enumr);
		if (FAILED(hr)) throw std::runtime_error("MMDeviceEnumerator failed");

		hr = enumr->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
		if (FAILED(hr)) throw std::runtime_error("GetDefaultAudioEndpoint failed");

		hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
		if (FAILED(hr)) throw std::runtime_error("Activate(IAudioClient) failed");

		// 2) choose format
		hr = client->GetMixFormat(&mix);
		if (FAILED(hr) || !mix) throw std::runtime_error("GetMixFormat failed");

		src_rate = mix->nSamplesPerSec;
		src_channels = mix->nChannels;
		src_float = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
		if (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			auto* ex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix);
			src_float = IsEqualGUID(ex->SubFormat, my_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
			if (IsEqualGUID(ex->SubFormat, my_KSDATAFORMAT_SUBTYPE_PCM)) src_float = false;
		}
		if (src_channels == 0) src_channels = 1;

		step = double(src_rate) / double(target_rate);

		// 3) init client
		// Buffer ~100ms
		const REFERENCE_TIME hnsBuffer = 1000000; // 100 ms in 100-ns units
		hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
								hnsBuffer, 0, mix, nullptr);
		if (FAILED(hr)) throw std::runtime_error("IAudioClient::Initialize failed");

		hr = client->GetService(__uuidof(IAudioCaptureClient), (void**)&cap);
		if (FAILED(hr)) throw std::runtime_error("GetService(IAudioCaptureClient) failed");

		hr = client->GetBufferSize(&bufferFrames);
		if (FAILED(hr) || bufferFrames == 0) throw std::runtime_error("GetBufferSize failed");

		hr = client->Start();
		if (FAILED(hr)) throw std::runtime_error("IAudioClient::Start failed");
		started = true;

		src_fifo.reserve(src_rate);		   // ~1 sec
		out_fifo.reserve(target_rate * 2); // ~2 sec
	}

	void pump_once() {
		UINT32 packet = 0;
		if (!cap) return;

		HRESULT hr = cap->GetNextPacketSize(&packet);
		if (FAILED(hr)) return;
		if (packet == 0) return;

		BYTE* data = nullptr;
		UINT32 frames = 0;
		DWORD flags = 0;
		hr = cap->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
		if (FAILED(hr)) return;

		// Convert this packet into mono float @ src_rate
		const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
		if (frames > 0) {
			if (src_float) {
				const float* f = reinterpret_cast<const float*>(data);
				// interleaved float channels
				for (UINT32 i = 0; i < frames; ++i) {
					double acc = 0.0;
					for (unsigned ch = 0; ch < src_channels; ++ch)
						acc += silent ? 0.0f : f[i * src_channels + ch];
					float mono = static_cast<float>(acc / double(src_channels));
					src_fifo.push_back(mono);
				}
			} else {
				// assume PCM 16-bit if not float; other formats are rare in shared mode
				const int16_t* s = reinterpret_cast<const int16_t*>(data);
				for (UINT32 i = 0; i < frames; ++i) {
					int acc = 0;
					for (unsigned ch = 0; ch < src_channels; ++ch)
						acc += silent ? 0 : s[i * src_channels + ch];
					// avg and normalize to [-1,1]
					float mono = float(acc) / float(32768 * (int)src_channels);
					src_fifo.push_back(mono);
				}
			}
		}

		cap->ReleaseBuffer(frames);
	}

	void produce_out_fifo() {
		if (src_fifo.size() < 2) return;

		const double inc = step;
		// Make outputs while we have at least two src samples ahead
		// t is position in src_fifo for next output
		const size_t maxConsumeSafe = src_fifo.size() - 1;
		std::vector<int16_t> tmp;
		tmp.reserve(src_fifo.size());

		while ((size_t)std::floor(t) + 1 < maxConsumeSafe) {
			size_t i = (size_t)std::floor(t);
			float a = src_fifo[i];
			float b = src_fifo[i + 1];
			float frac = static_cast<float>(t - double(i));
			float y = a + (b - a) * frac; // linear interp

			out_fifo.push_back(float_to_s16(y));
			t += inc;
		}

		// Discard fully consumed src samples to avoid unbounded growth.
		size_t consumed = (size_t)std::floor(t);
		if (consumed > 0) {
			t -= double(consumed);
			src_fifo.erase(src_fifo.begin(), src_fifo.begin() + (std::min(consumed, src_fifo.size())));
		}
	}

	// Blocking read of S16 mono @ 16k into dst (bytes)
	int read_blocking(void* dst, size_t bytes) {
		const size_t want_samples = bytes / sizeof(int16_t);
		int16_t* out = reinterpret_cast<int16_t*>(dst);

		size_t produced = 0;
		while (produced < want_samples) {
			// If we already have enough, consume immediately
			if (out_fifo.size() >= (want_samples - produced)) {
				size_t n = want_samples - produced;
				std::memcpy(out + produced, out_fifo.data(), n * sizeof(int16_t));
				out_fifo.erase(out_fifo.begin(), out_fifo.begin() + n);
				produced += n;
				break;
			}

			// Consume what we have
			if (!out_fifo.empty()) {
				size_t n = std::min(out_fifo.size(), want_samples - produced);
				std::memcpy(out + produced, out_fifo.data(), n * sizeof(int16_t));
				out_fifo.erase(out_fifo.begin(), out_fifo.begin() + n);
				produced += n;
				if (produced >= want_samples) break;
			}

			// Not enough -> pump WASAPI once and convert
			pump_once();
			produce_out_fifo();

			// If still nothing came in, sleep a tad to avoid busy loop
			if (out_fifo.empty()) {
				Sleep(5);
			}
		}
		return int(bytes);
	}
};

// --------------------- C shim expected by our wrapper ---------------------
pa_simple* pa_simple_new(const char* /*server*/, const char* /*name*/, int /*dir*/,
						 const char* /*dev*/, const char* /*stream_name*/,
						 const pa_sample_spec* /*ss*/, void*, void*, int* error) {
	try {
		auto* s = new pa_simple();
		s->init();
		if (error) *error = 0;
		return s;
	} catch (const std::exception& e) {
		// Log the actual error to help debugging
		#ifdef _DEBUG
		OutputDebugStringA(("WASAPI initialization failed: " + std::string(e.what()) + "\n").c_str());
		#else
		(void)e; // Suppress unused variable warning in release builds
		#endif
		if (error) *error = -1;
		return nullptr;
	} catch (...) {
		#ifdef _DEBUG
		OutputDebugStringA("WASAPI initialization failed: unknown exception\n");
		#endif
		if (error) *error = -1;
		return nullptr;
	}
}

int pa_simple_read(pa_simple* s, void* data, size_t bytes, int* error) {
	if (!s) {
		if (error) *error = -1;
		return -1;
	}
	try {
		int ret = s->read_blocking(data, bytes);
		if (error) *error = 0;
		return ret;
	} catch (...) {
		if (error) *error = -1;
		return -1;
	}
}

void pa_simple_free(pa_simple* s) {
	delete s;
}

#endif // _WIN32
