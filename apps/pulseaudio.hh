// apps/pulseaudio.hh
#pragma once
#include "pulseaudio.h"
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace pulseaudio {
	namespace pa {
		class simple_record_stream {
			pa_simple* s_{};

		public:
			template <class... Args>
			explicit simple_record_stream(Args&&...) {
				pa_sample_spec ss{PA_SAMPLE_S16LE, 16000, 1};
				int err = 0;
				s_ = pa_simple_new(nullptr, "snowman", PA_STREAM_RECORD, nullptr, "record", &ss, nullptr, nullptr, &err);
				if (!s_ || err) throw std::runtime_error("pa_simple_new(record) failed");
			}
			int read(void* data, std::size_t bytes) {
				int err = 0;
				int r = pa_simple_read(s_, data, bytes, &err);
				if (r < 0 || err) throw std::runtime_error("pa_simple_read failed");
				return r;
			}
			template<typename T>
			void read(std::vector<T>& samples) {
				const size_t bytes_needed = 1024 * sizeof(T); // Read ~1024 samples at a time
				samples.resize(1024);
				int err = 0;
				int r = pa_simple_read(s_, samples.data(), bytes_needed, &err);
				if (r < 0 || err) throw std::runtime_error("pa_simple_read failed");
			}
			~simple_record_stream() {
				if (s_) pa_simple_free(s_);
			}
		};

		class simple_playback_stream {
		public:
			template <class... Args>
			explicit simple_playback_stream(Args&&...) {}
			int write(const void*, std::size_t) { return 0; } // stub playback
			template<typename T>
			int write(const std::vector<T>&) { return 0; } // stub playback for vectors
		};
	} // namespace pa
} // namespace pulseaudio
