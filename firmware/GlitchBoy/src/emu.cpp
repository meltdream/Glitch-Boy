
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include "emu.h"

#include <limits.h>
#include <sys/stat.h>
using namespace std;

namespace {

loading_progress_callback_t g_loading_progress_callback = nullptr;
void* g_loading_progress_user = nullptr;
int g_loading_progress_last = -1;

int file_size_from_stat(const std::string& path)
{
    struct stat st = {};
    if (stat(path.c_str(), &st) != 0)
        return -1;
    if (st.st_size < 0 || st.st_size > INT_MAX) {
        printf("File size out of range for %s: %ld\n", path.c_str(), (long)st.st_size);
        return -1;
    }
    return (int)st.st_size;
}

int file_size_from_stream(FILE* f)
{
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0)
        return -1;
    long pos = ftell(f);
    if (pos < 0 || pos > INT_MAX)
        return -1;
    return (int)pos;
}

int file_size_from_path_or_stream(const std::string& path, FILE* f)
{
    int size = file_size_from_stat(path);
    if (size >= 0)
        return size;
    return file_size_from_stream(f);
}

size_t read_fully(FILE* f, void* dst, size_t len)
{
    uint8_t* out = static_cast<uint8_t*>(dst);
    size_t total = 0;
    while (total < len) {
        size_t got = fread(out + total, 1, len - total, f);
        if (got == 0)
            break;
        total += got;
    }
    return total;
}

}

void loading_progress_set_callback(loading_progress_callback_t callback, void* user)
{
    g_loading_progress_callback = callback;
    g_loading_progress_user = user;
    g_loading_progress_last = -1;
}

void loading_progress_clear_callback()
{
    g_loading_progress_callback = nullptr;
    g_loading_progress_user = nullptr;
    g_loading_progress_last = -1;
}

void loading_progress_set(int percent)
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    if (!g_loading_progress_callback ||
        percent == g_loading_progress_last ||
        (g_loading_progress_last >= 0 && percent < g_loading_progress_last))
        return;
    g_loading_progress_last = percent;
    g_loading_progress_callback(percent, g_loading_progress_user);
}

// Flash-backed ROM mapping for NES/SMS images that do not fit in RAM.

#ifdef ESP_PLATFORM
#include <esp_spi_flash.h>
#include <esp_attr.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include "rom/miniz.h"
#include <new>
#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Only one ROM mapping can be active.
spi_flash_mmap_handle_t _file_handle = 0;
static uint8_t* _heap_mapped_file = 0;

static void print_part(const esp_partition_t *pPart)
{
    printf("main: partition type = %d.\n", pPart->type);
    printf("main: partition subtype = %d.\n", pPart->subtype);
    printf("main: partition starting address = %x.\n", pPart->address);
    printf("main: partition size = %x.\n", pPart->size);
    printf("main: partition label = %s.\n", pPart->label);
    printf("main: partition encrypted = %d.\n", pPart->encrypted);
    printf("\n");
}

static void print_parts(esp_partition_iterator_t it)
{
    while (it)
    {
        print_part((esp_partition_t *) esp_partition_get(it));
        it = esp_partition_next(it);
    }
}

typedef struct {
    uint32_t sig;
    uint32_t offset;
    uint32_t len;
    uint32_t flags;
    char name[128-16];
} FlashFile;

#define FSIG ('F' | ('I' << 8) | ('L' << 16) | ('E' << 24))

class CrapFS {
public:
    bool ok() const { return _part != nullptr && _buf != nullptr && _dir != nullptr && _count > 0; }
    #define DIR_BLOCK_SIZE 0x1000   // 4k or 32 entries; map_file_internal clears the cache per ROM
    // skip 64K at start of partition
    static DRAM_ATTR uint8_t g_crapfs_dir_buf[DIR_BLOCK_SIZE];
    static const int COPY_BUF_SIZE = 1024;
    static DRAM_ATTR uint8_t g_crapfs_copy_buf[COPY_BUF_SIZE];

    const esp_partition_t* _part;
    uint8_t *_buf;
    uint8_t *_copy_buf;
    FlashFile* _dir;
    int _count;

    CrapFS() : _part(0), _buf(0), _copy_buf(0), _dir(0), _count(0)
    {
        const esp_partition_t* running = esp_ota_get_running_partition();
        const esp_partition_t* preferred = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app1");
        if (preferred && (!running || preferred->address != running->address))
            _part = preferred;
        if (!_part) {
            esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
            while (it) {
                const esp_partition_t* p = esp_partition_get(it);
                if (!running || p->address != running->address) {
                    _part = p;
                    break;
                }
                it = esp_partition_next(it);
            }
            if (it)
                esp_partition_iterator_release(it);
        }
        if (!_part) {
            printf("CrapFS::CrapFS no writable app partition available (running=%s)\n",
                running ? running->label : "none");
            return;
        }
        if (running) {
            printf("CrapFS::CrapFS running=%s cache=%s\n", running->label, _part->label);
        }
        print_part(_part);

        _buf = g_crapfs_dir_buf;
        _copy_buf = g_crapfs_copy_buf;

        _dir = (FlashFile*)_buf;
        esp_err_t err = esp_partition_read(_part, 0, _buf, DIR_BLOCK_SIZE);
        if (err) {
            printf("CrapFS::CrapFS dir read failed %d\n",err);
            return;
        }

        // init directory if required
        _count = DIR_BLOCK_SIZE/sizeof(FlashFile);
        if (_dir[0].sig != FSIG)
            reformat();

        // dump dir
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig == FSIG)
                printf("%08X %08X %s\n",_dir[i].offset,_dir[i].len,_dir[i].name);
        }
    }

~CrapFS()
{
}

    uint32_t align(uint32_t n)
    {
        return (n + 0xFFFF) & 0xFFFF0000;
    }

    void reformat()
    {
        if (!_part) return;
        uint32_t erased = 0;
        while (erased < _part->size) {
            uint32_t n = _part->size - erased;
            if (n > 0x10000)
                n = 0x10000;
            esp_partition_erase_range(_part, erased, n); // erase entire partition
            erased += n;
            loading_progress_set(8 + (int)(((uint64_t)erased * 12u) / (uint64_t)_part->size));
            vTaskDelay(1);
        }
        if (_buf) memset(_buf,0xFF,DIR_BLOCK_SIZE);       // only use the first sector of the first block
    }
    uint8_t* mmap(const FlashFile* file)
    {
        if (!file)
            return 0;
        printf("CrapFS::mmap mapping %s offset:%08X len:%d\n",file->name,file->offset,file->len);
        void* data = 0;
        if (esp_partition_mmap(_part, file->offset, file->len, SPI_FLASH_MMAP_DATA, (const void**)&data, &_file_handle) == 0)
        {
            printf("CrapFS::mmap mapped to %08X\n",data);
            return (uint8_t*)data;
        }
        return 0;
    }

    // see if the file exists
    FlashFile* find(const std::string& path)
    {
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig == FSIG && strcmp(_dir[i].name,path.c_str()) == 0)
                return _dir + i;
        }
        return NULL;
    }

    int copy(const std::string& path, int offset, int len)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
            printf("CrapFS::copy open failed for %s\n", path.c_str());
            return -1;
        }
        uint8_t* buf = _copy_buf;
        esp_err_t err = ESP_OK;
        int i = 0;
        int last_progress_bucket = -1;
        const int kProgressBuckets = 1; // One progress update per copy.
        loading_progress_set(20);
        while (i < len) {
            int n = len-i;
            if (n > COPY_BUF_SIZE)
                n = COPY_BUF_SIZE;
            size_t got = read_fully(f, buf, (size_t)n);
            if (got != (size_t)n) {
                printf("CrapFS::copy short read %u/%d for %s\n", (unsigned)got, n, path.c_str());
                err = ESP_FAIL;
                break;
            }
            err = esp_partition_write(_part, i + offset, buf, n);
            if (err) {
                printf("CrapFS::copy write failed %d at %08X for %s\n",
                    err, i + offset, path.c_str());
                break;
            }
            i += n;
            if ((i & 0xFFFF) == 0)
                vTaskDelay(1);

            loading_progress_set(20 + (int)(((int64_t)i * 60) / (len > 0 ? len : 1)));
            int bucket = (len > 0) ? (int)(((long long)i * (long long)kProgressBuckets) / (long long)len) : kProgressBuckets;
            if (bucket > kProgressBuckets)
                bucket = kProgressBuckets;
            if ((bucket > 0 || i == len) && bucket != last_progress_bucket) {
                last_progress_bucket = bucket;
                int written_kb = (i + 1023) / 1024;
                int total_kb = (len + 1023) / 1024;
                printf("CrapFS::copy writing %d/%d KB\n", written_kb, total_kb);
            }
        }
        fclose(f);
        return err;
    }

    //
    FlashFile* create(const std::string& path, int len)
    {
        uint32_t start = 0x10000;
        if ((uint32_t)len > (_part->size - start)) {
            printf("CrapFS::create file too large len=%d part=%u\n", len, (unsigned)_part->size);
            return NULL;
        }
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig != FSIG) {
                uint32_t end = align(start + (uint32_t)len);
                if (end > _part->size) {
                    printf("CrapFS::create no space start=%08X len=%d end=%08X part=%08X\n",
                        start, len, end, _part->size);
                    return NULL;
                }
                if (path.size() >= sizeof(_dir[i].name)) {
                    printf("CrapFS::create path too long len=%u for %s\n",
                        (unsigned)path.size(), path.c_str());
                    return NULL;
                }
                _dir[i].sig = FSIG;
                _dir[i].offset = start;
                _dir[i].len = len;  //
                strcpy(_dir[i].name,path.c_str());
                printf("CrapFS::created %s %08X %d\n",_dir[i].name,start,len);
                esp_err_t err = esp_partition_erase_range(_part,0, DIR_BLOCK_SIZE);     // erase dir
                if (err == 0)
                    err = esp_partition_write(_part, 0, _buf, DIR_BLOCK_SIZE);    // update dir
                if (err) {
                    printf("CrapFS::create dir write failed %d\n",err);
                    return NULL;
                }
                int c = copy(path,start,len);
                if (c) {
                    printf("CrapFS::create copy failed %d for %s\n", c, path.c_str());
                    return NULL;
                }
                return _dir+i;
            }
            start = align(_dir[i].offset + _dir[i].len);
        }
        // No free directory entry.
        return NULL;
    }
};

DRAM_ATTR uint8_t CrapFS::g_crapfs_dir_buf[DIR_BLOCK_SIZE];
DRAM_ATTR uint8_t CrapFS::g_crapfs_copy_buf[COPY_BUF_SIZE];

static uint8_t* load_heap_mapped_file(const char* path)
{
    uint8_t* data;
    int actual_len;
    if (Emu::load(path, &data, &actual_len) == 0) {
        _heap_mapped_file = data;
        return data;
    }
    return nullptr;
}

static uint8_t* map_file_internal(const char* path, int len, bool allow_heap_fallback)
{
    if (_file_handle) {
        spi_flash_munmap(_file_handle);
        _file_handle = 0;
    }
    if (_heap_mapped_file) {
        free(_heap_mapped_file);
        _heap_mapped_file = 0;
    }

    const char* fallback_reason = nullptr;
    uint8_t* mapped_ptr = nullptr;

    {
        CrapFS _fs;
        if (!_fs.ok()) {
            fallback_reason = "unavailable";
        } else {
            // Each load replaces the previous cached ROM.
            printf("CrapFS::clearing cache before loading new ROM\n");
            _fs.reformat();

            // Create and map the replacement ROM.
            FlashFile* file = _fs.create(path, len);
            if (!file) {
                fallback_reason = "create failed";
                if (!allow_heap_fallback)
                    printf("CrapFS::create failed for %s\n", path);
            } else {
                mapped_ptr = _fs.mmap(file);
                if (!mapped_ptr) {
                    fallback_reason = "mmap failed";
                    if (!allow_heap_fallback)
                        printf("CrapFS::mmap failed for %s\n", path);
                }
            }
        }
    }

    if (mapped_ptr) {
        _heap_mapped_file = 0;
        return mapped_ptr;
    }

    if (allow_heap_fallback) {
        printf("CrapFS %s for %s, falling back to direct RAM loading\n",
            fallback_reason ? fallback_reason : "failed", path);
        return load_heap_mapped_file(path);
    }

    return nullptr;
}

uint8_t* map_file(const char* path, int len)
{
    return map_file_internal(path, len, true);
}

uint8_t* map_file_strict(const char* path, int len)
{
    return map_file_internal(path, len, false);
}

void unmap_file(uint8_t* ptr)
{
    if (_file_handle) {
        spi_flash_munmap(_file_handle);
        _file_handle = 0;
    }
    if (ptr && ptr == _heap_mapped_file) {
        free(_heap_mapped_file);
        _heap_mapped_file = 0;
    }
}

uint8_t* map_file_scratch(size_t* len)
{
    if (len)
        *len = DIR_BLOCK_SIZE;
    return CrapFS::g_crapfs_dir_buf;
}

FILE* mkfile(const char* path)
{
    return fopen(path,"wb");
}

#else
#include <sys/stat.h>
#include "../miniz.h"

uint8_t* map_file(const char* path, int len)
{
    uint8_t* d;
    Emu::load(path,&d,&len);
    return d;
}

uint8_t* map_file_strict(const char* path, int len)
{
    return map_file(path, len);
}

void unmap_file(uint8_t* ptr)
{
    delete[] ptr;
}

uint8_t* map_file_scratch(size_t* len)
{
    if (len)
        *len = 0;
    return 0;
}


FILE* mkfile(const char* path)
{
    std::string v = path;
    std::string dir = v.substr(0,v.find_last_of("/"));
    mkdir(dir.c_str(), 0755);
    return fopen(path,"wb");
}

#endif

// map one bit array to another
uint32_t generic_map(uint32_t bits, const uint32_t* m)
{
    uint32_t b = 0;
    for (int i = 0; i < 16; i++) {
        if ((0x8000 >> i) & bits)
            b |= m[i];
    }
    return b;
}

// Inflate ROM data to the filesystem before Atari video buffers consume RAM.
int unpack(const char* dst, const uint8_t* d, int len)
{
    printf("unpacking %s\n",dst);

    // Allocate before truncating dst so running out of RAM leaves it intact.
    #define BUF_SIZE 0x8000
    uint8_t* buf = new (std::nothrow) uint8_t[BUF_SIZE];
    if (!buf)
        return -1;  // Smaller windows do not handle arbitrary ZIP files.

    tinfl_decompressor* dec = new (std::nothrow) tinfl_decompressor;
    if (!dec) {
        delete [] buf;
        return -1;
    }

    FILE* f = mkfile(dst);
    if (!f) {
        delete [] buf;
        delete dec;
        return -1;
    }
    size_t in_bytes, out_bytes;
    tinfl_status status;
    int i = 0;

    tinfl_init(dec);
    while (i < len) {
        in_bytes = len-i;
        out_bytes = BUF_SIZE;
        status = tinfl_decompress(dec,d+i,&in_bytes,buf,buf,&out_bytes,11);
        if (out_bytes != fwrite(buf,1,out_bytes,f)) {
            status = TINFL_STATUS_FAILED;
            break;
        }
        i += in_bytes;
    }

    delete [] buf;
    delete dec;
    fclose(f);

    if (status == TINFL_STATUS_FAILED) {
        remove(dst);
        return -1;
    }
    return 0;
}

Emu::Emu(const char* n,int w,int h, int st, int aformat, int cc, int f) :
    name(n),width(w),height(h),standard(st),audio_format(aformat),cc_width(cc),flavor(f)
{
    audio_frequency = standard == NTSC ? VIDEO_NTSC_SCANLINE_RATE_HZ : 15600;
    audio_frame_samples = standard == NTSC
        ? (int)(VIDEO_NTSC_LINES_PER_FRAME << 16)
        : (audio_frequency << 16)/50;
    audio_fraction = 0;
}

Emu::~Emu()
{
}

int Emu::frame_sample_count()
{
    int n = audio_frame_samples + audio_fraction;
    audio_fraction = n & 0xFFFF;
    return n >> 16;
}

int Emu::insert(const std::string& path, int flags, int disk_index)
{
    return -1;
}

const uint32_t* Emu::composite_palette()
{
    return standard ? ntsc_palette() : pal_palette();
}

// determine file type
int Emu::head(const std::string& path, uint8_t* data, int len)
{
    FILE *f = fopen(path.c_str() , "rb");
    if (!f)
        return -1;
    if (data && len > 0) {
        memset(data, 0, len);
        read_fully(f, data, (size_t)len);
    }
    int flen = file_size_from_path_or_stream(path, f);
    fclose(f);
    return flen;
}

int Emu::load(const std::string& path, uint8_t** data, int* len)
{
    *data = 0;
    *len = 0;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        printf("Emu::load failed for %s\n",path.c_str());
        return -1;
    }
    int fsize = file_size_from_path_or_stream(path, f);
    if (fsize < 0) {
        printf("Emu::load failed for %s (size unavailable)\n", path.c_str());
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        printf("Emu::load failed for %s (seek failed)\n", path.c_str());
        fclose(f);
        return -1;
    }
    uint8_t* d = (uint8_t*)malloc(fsize);
    if (!d) { printf("Emu::load failed for %s (out of memory)\n",path.c_str()); fclose(f); return -1; }
    loading_progress_set(20);
    uint8_t* out = d;
    size_t got = 0;
    while (got < (size_t)fsize) {
        size_t want = (size_t)fsize - got;
        if (want > 1024)
            want = 1024;
        size_t n = fread(out + got, 1, want, f);
        if (n == 0)
            break;
        got += n;
        loading_progress_set(20 + (int)(((uint64_t)got * 60u) / (uint64_t)(fsize > 0 ? fsize : 1)));
    }
    fclose(f);
    if (got != (size_t)fsize) {
        printf("Emu::load short read %u/%d bytes %s\n", (unsigned)got, fsize, path.c_str());
        free(d);
        return -1;
    }

    printf("Emu::load %d bytes %s\n",fsize,path.c_str());
    *data = d;
    *len = fsize;
    return 0;
}
