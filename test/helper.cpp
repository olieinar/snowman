// helper.cpp
#ifdef _WIN32
// ---- POSIX execinfo stubs for Windows ----
static inline int backtrace(void**, int) { return 0; }
static inline char** backtrace_symbols(void* const*, int) { return nullptr; }
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <execinfo.h>
#endif

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <helper.h>
#include <malloc.h>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <vector>

// =================== MD5 helpers ===================
static std::string bytes_to_hex_colon(const unsigned char* p, size_t n) {
	std::string out;
	out.reserve(n * 3); // "AA:" per byte, last ':' trimmed
	static const char* HEX = "0123456789ABCDEF";
	for (size_t i = 0; i < n; ++i) {
		unsigned char b = p[i];
		out.push_back(HEX[(b >> 4) & 0xF]);
		out.push_back(HEX[b & 0xF]);
		if (i + 1 != n) out.push_back(':');
	}
	return out;
}

#ifndef _WIN32
// Non-Windows: keep OpenSSL
#include <openssl/crypto.h>
#include <openssl/md5.h>
#endif
// ===================================================

// ---- malloc_usable_size portability ----
static inline size_t portable_msize(void* p) {
#ifdef _WIN32
	return _msize(p);
#else
	return malloc_usable_size(p);
#endif
}

// ---- allocator entry points ----
#ifndef _WIN32
extern "C" void* __libc_malloc(size_t);
extern "C" void* __libc_realloc(void*, size_t);
extern "C" void __libc_free(void*);
extern "C" void* __libc_memalign(size_t alignment, size_t size);
#else
#include <cstdlib>
#include <malloc.h>
#define __libc_malloc malloc
#define __libc_realloc realloc
#define __libc_free free
// Avoid _aligned_malloc here to keep free() valid for all allocations we do.
static inline void* __libc_memalign(size_t /*alignment*/, size_t size) {
	return malloc(size);
}
#endif

std::vector<short> read_sample_file(const std::string& filename, bool treat_wave) {
	struct wavHeader { // 44 byte HEADER only
		char RIFF[4];
		int RIFFsize;
		char fmt[8];
		int fmtSize;
		short fmtTag;
		short nchan;
		int fs;
		int avgBps;
		short nBlockAlign;
		short bps;
		char data[4];
		int datasize;
	};

	auto readWavHeader = [](wavHeader* wavhdr, FILE* fi) {
		char* tag = (char*)wavhdr;
		if (fread(wavhdr, 1, 34, fi) != 34) throw std::runtime_error("fread failed");
		if (tag[0] != 'R' || tag[1] != 'I' || tag[2] != 'F' || tag[3] != 'F')
			throw std::runtime_error("not a riff file");
		if (wavhdr->fmtTag != 1)
			throw std::runtime_error("WAV file has encoded data or it is WAVEFORMATEXTENSIBLE");
		if (wavhdr->fmtSize == 14) wavhdr->bps = 16;
		if (wavhdr->fmtSize >= 16) {
			if (fread(&wavhdr->bps, 1, 2, fi) != 2) throw std::runtime_error("fread failed");
		}
		if (wavhdr->fmtSize == 18) {
			short lixo;
			if (fread(&lixo, 1, 2, fi) != 2) throw std::runtime_error("fread failed");
		}
		tag += 36;
		if (fread(tag, 1, 4, fi) != 4) throw std::runtime_error("fread failed");
		while (tag[0] != 'd' || tag[1] != 'a' || tag[2] != 't' || tag[3] != 'a') {
			if (fread(tag, 1, 4, fi) != 4) throw std::runtime_error("fread failed");
			if (ftell(fi) >= long(wavhdr->RIFFsize)) {
				fclose(fi);
				perror("Bad WAV header !");
			}
		}
		if (fread(&wavhdr->datasize, 1, 4, fi) != 4) throw std::runtime_error("fread failed");
	};

	const bool isWav = treat_wave || (filename.size() > 4 && filename.rfind(".wav") == filename.size() - 4);

	FILE* f = fopen(filename.c_str(), "rb");
	if (f == nullptr) throw std::runtime_error("Error opening file");

	std::vector<short> res;
	try {
		if (isWav) {
			wavHeader wavhdr{};
			readWavHeader(&wavhdr, f);
			res.resize(static_cast<size_t>(wavhdr.datasize) / 2);
		} else {
			fseek(f, 0, SEEK_END);
			res.resize(static_cast<size_t>(ftell(f)) / 2);
			rewind(f);
		}
		const size_t want = res.size() * 2;
		size_t got = fread(res.data(), 1, want, f);
		while (got != want) {
			const size_t r = fread(reinterpret_cast<char*>(res.data()) + got, 1, want - got, f);
			if (r == 0) throw std::runtime_error("fread failed: wanted=" + std::to_string(want) + " got=" + std::to_string(got));
			got += r;
		}
	} catch (...) {
		fclose(f);
		throw;
	}
	fclose(f);
	return res;
}

std::string read_sample_file_as_string(const std::string& filename, bool treat_wave) {
	auto data = read_sample_file(filename, treat_wave);
	std::string str_data;
	str_data.resize(data.size() * 2);
	std::memcpy(const_cast<char*>(str_data.data()), data.data(), str_data.size());
	return str_data;
}

bool file_exists(const std::string& name) {
	struct stat buffer;
	return stat(name.c_str(), &buffer) == 0;
}

std::string detect_project_root() {
	std::string prefix;
	for (int i = 0; i < 5; i++) {
		if (file_exists(prefix + "./.git")) return prefix;
		prefix += "../";
	}
	return "";
}

std::string read_file(const std::string& file) {
	std::ifstream f{file, std::ios::binary};
	std::stringstream content;
	content << f.rdbuf();
	return content.str();
}

std::string md5sum(const std::string& data) { return md5sum(data.data(), data.size()); }

std::string md5sum(const void* const data, size_t len) {
#ifdef _WIN32
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_HASH_HANDLE hHash = nullptr;
	DWORD objLen = 0, cb = 0;
	NTSTATUS s;

	s = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0);
	if (s < 0) throw std::runtime_error("BCryptOpenAlgorithmProvider(MD5) failed");

	s = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0);
	if (s < 0) {
		BCryptCloseAlgorithmProvider(hAlg, 0);
		throw std::runtime_error("BCryptGetProperty(OBJECT_LENGTH) failed");
	}

	std::vector<UCHAR> obj(objLen);
	std::vector<UCHAR> hash(16);

	s = BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, nullptr, 0, 0);
	if (s < 0) {
		BCryptCloseAlgorithmProvider(hAlg, 0);
		throw std::runtime_error("BCryptCreateHash failed");
	}

	s = BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
	if (s < 0) {
		BCryptDestroyHash(hHash);
		BCryptCloseAlgorithmProvider(hAlg, 0);
		throw std::runtime_error("BCryptHashData failed");
	}

	s = BCryptFinishHash(hHash, hash.data(), (ULONG)hash.size(), 0);
	BCryptDestroyHash(hHash);
	BCryptCloseAlgorithmProvider(hAlg, 0);
	if (s < 0) throw std::runtime_error("BCryptFinishHash failed");

	return bytes_to_hex_colon(hash.data(), hash.size());
#else
	unsigned char hash[MD5_DIGEST_LENGTH];
	MD5(reinterpret_cast<const unsigned char*>(data), len, hash);
	return bytes_to_hex_colon(hash, MD5_DIGEST_LENGTH);
#endif
}

std::string md5sum_file(const std::string& file) {
	auto content = read_file(file);
	return md5sum(content);
}

static MemoryChecker::snapshot* g_mc_info = nullptr;

void MemoryChecker::stacktrace::capture() {
	auto res = ::backtrace(trace, 50);
	for (int i = res; i < 50; i++)
		trace[i] = nullptr;
}

MemoryChecker::MemoryChecker() { g_mc_info = &info; }
MemoryChecker::~MemoryChecker() { g_mc_info = nullptr; }

void* mc_malloc(size_t size, const void* /*caller*/) {
	if (g_mc_info == nullptr) return __libc_malloc(size);
	void* ptr = __libc_malloc(size);

	g_mc_info->num_malloc++;
	if (ptr == nullptr) {
		g_mc_info->num_malloc_failed++;
		return ptr;
	}
	size = portable_msize(ptr);
	g_mc_info->num_bytes_allocated += size;
	g_mc_info->num_malloc_bytes += size;
	g_mc_info->num_chunks_allocated++;
	if (g_mc_info->num_chunks_allocated > g_mc_info->num_chunks_allocated_max)
		g_mc_info->num_chunks_allocated_max = g_mc_info->num_chunks_allocated;
	if (g_mc_info->num_bytes_allocated > g_mc_info->num_bytes_allocated_max)
		g_mc_info->num_bytes_allocated_max = g_mc_info->num_bytes_allocated;
	return ptr;
}

void* mc_realloc(void* cptr, size_t size, const void* /*caller*/) {
	if (g_mc_info == nullptr) return __libc_realloc(cptr, size);
	auto oldsize = cptr ? portable_msize(cptr) : 0;
	void* ptr = __libc_realloc(cptr, size);

	g_mc_info->num_realloc++;
	if (ptr == nullptr) {
		g_mc_info->num_realloc_failed++;
		return ptr;
	}
	if (cptr != ptr) g_mc_info->num_realloc_moved++;
	size = portable_msize(ptr);

	auto alloc_size = (static_cast<ssize_t>(size) - static_cast<ssize_t>(oldsize));
	g_mc_info->num_realloc_bytes += alloc_size;
	g_mc_info->num_bytes_allocated += alloc_size;
	if (g_mc_info->num_chunks_allocated > g_mc_info->num_chunks_allocated_max)
		g_mc_info->num_chunks_allocated_max = g_mc_info->num_chunks_allocated;
	if (g_mc_info->num_bytes_allocated > g_mc_info->num_bytes_allocated_max)
		g_mc_info->num_bytes_allocated_max = g_mc_info->num_bytes_allocated;
	return ptr;
}

void mc_free(void* ptr, const void* /*caller*/) {
	if (g_mc_info == nullptr) {
		__libc_free(ptr);
		return;
	}
	if (ptr == nullptr) return;
	auto oldsize = portable_msize(ptr);
	__libc_free(ptr);
	g_mc_info->num_free++;
	g_mc_info->num_bytes_allocated -= oldsize;
	g_mc_info->num_chunks_allocated--;
}

void* mc_memalign(size_t alignment, size_t size, const void* /*caller*/) {
	void* ptr = __libc_memalign(alignment, size);
	if (g_mc_info == nullptr) return ptr;

	g_mc_info->num_memalign++;
	if (ptr == nullptr) {
		g_mc_info->num_memalign_failed++;
		return ptr;
	}
	size = portable_msize(ptr);
	g_mc_info->num_bytes_allocated += size;
	g_mc_info->num_memalign_bytes += size;
	g_mc_info->num_chunks_allocated++;
	if (g_mc_info->num_chunks_allocated > g_mc_info->num_chunks_allocated_max)
		g_mc_info->num_chunks_allocated_max = g_mc_info->num_chunks_allocated;
	if (g_mc_info->num_bytes_allocated > g_mc_info->num_bytes_allocated_max)
		g_mc_info->num_bytes_allocated_max = g_mc_info->num_bytes_allocated;
	return ptr;
}

std::ostream& operator<<(std::ostream& str, const MemoryChecker::stacktrace& o) {
	int n = 0;
	for (; n < 49; n++)
		if (o.trace[n + 1] == nullptr) break;
	auto strings = backtrace_symbols(o.trace, n);
	if (strings == NULL) {
		str << "<backtrace_symbols failed>";
		return str;
	}
	for (int j = 0; j < n; j++)
		str << strings[j] << "\n";
	free(strings);
	return str;
}

std::ostream& operator<<(std::ostream& str, const MemoryChecker& o) {
	str << "==== Memory report ====\n";
	str << "num_malloc =               " << o.info.num_malloc << "\n";
	str << "num_malloc_failed =        " << o.info.num_malloc_failed << "\n";
	str << "num_malloc_bytes =         " << o.info.num_malloc_bytes << "\n";
	str << "num_free =                 " << o.info.num_free << "\n";
	str << "num_realloc =              " << o.info.num_realloc << "\n";
	str << "num_realloc_failed =       " << o.info.num_realloc_failed << "\n";
	str << "num_realloc_moved =        " << o.info.num_realloc_moved << "\n";
	str << "num_realloc_bytes =        " << o.info.num_realloc_bytes << "\n";
	str << "num_memalign =             " << o.info.num_memalign << "\n";
	str << "num_memalign_failed =      " << o.info.num_memalign_failed << "\n";
	str << "num_memalign_bytes =       " << o.info.num_memalign_bytes << "\n";
	str << "num_chunks_allocated =     " << o.info.num_chunks_allocated << "\n";
	str << "num_chunks_allocated_max = " << o.info.num_chunks_allocated_max << "\n";
	str << "num_bytes_allocated =      " << o.info.num_bytes_allocated << "\n";
	str << "num_bytes_allocated_max =  " << o.info.num_bytes_allocated_max << "\n";
	return str;
}

#if MEMCHECK_ENABLED
extern "C" void* malloc(size_t size) { return mc_malloc(size, __builtin_return_address(0)); }
extern "C" void* realloc(void* ptr, size_t size) { return mc_realloc(ptr, size, __builtin_return_address(0)); }
extern "C" void* memalign(size_t alignment, size_t size) { return mc_memalign(alignment, size, __builtin_return_address(0)); }
extern "C" void free(void* ptr) { mc_free(ptr, __builtin_return_address(0)); }
extern "C" int posix_memalign(void** memptr, size_t alignment, size_t size) {
	*memptr = memalign(alignment, size);
	if (!*memptr) return ENOMEM;
	return 0;
}
#endif
