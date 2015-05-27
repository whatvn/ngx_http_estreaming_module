/*******************************************************************************
 mp4_io.h - A library for general MPEG4 I/O.

 For licensing see the LICENSE file
******************************************************************************/

#define ATOM_PREAMBLE_SIZE 8

#define FOURCC(a, b, c, d) ((uint32_t)(a) << 24) + \
  ((uint32_t)(b) << 16) + \
  ((uint32_t)(c) << 8) + \
  ((uint32_t)(d))

#define MP4_INFO(fmt, ...); \
  mp4_log_trace(mp4_context, NGX_LOG_INFO, "%s.%d: (info) "fmt, remove_path(__FILE__), __LINE__, __VA_ARGS__);

#define MP4_WARNING(fmt, ...) \
  mp4_log_trace(mp4_context, NGX_LOG_WARN, "%s.%d: (warning) "fmt, remove_path(__FILE__), __LINE__, __VA_ARGS__);

#define MP4_ERROR(fmt, ...) \
  mp4_log_trace(mp4_context, NGX_LOG_CRIT, "%s.%d: (error) "fmt, remove_path(__FILE__), __LINE__, __VA_ARGS__);

struct unknown_atom_t {
    void *atom_;
    struct unknown_atom_t *next_;
};
typedef struct unknown_atom_t unknown_atom_t;

struct mvhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint64_t creation_time_;      // seconds since midnite, Jan .1 1904 (UTC)
    uint64_t modification_time_;  // seconds since midnite, Jan .1 1904 (UTC)
    uint32_t timescale_;          // time units that pass in one second
    uint64_t duration_;           // duration of the longest track
    uint32_t rate_;               // preferred playback rate (16.16)
    uint16_t volume_;             // preferred playback volume (8.8)
    uint16_t reserved1_;
    uint32_t reserved2_[2];
    uint32_t matrix_[9];
    uint32_t predefined_[6];
    uint32_t next_track_id_;
};
typedef struct mvhd_t mvhd_t;

struct trak_t {
    struct unknown_atom_t *unknown_atoms_;
    struct tkhd_t *tkhd_;
    struct mdia_t *mdia_;
    struct edts_t *edts_;

    unsigned int chunks_size_;
    struct chunks_t *chunks_;

    unsigned int samples_size_;
    struct samples_t *samples_;
};
typedef struct trak_t trak_t;

struct tkhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint64_t creation_time_;      // seconds since midnite, Jan .1 1904 (UTC)
    uint64_t modification_time_;  // seconds since midnite, Jan .1 1904 (UTC)
    uint32_t track_id_;
    uint32_t reserved_;
    uint64_t duration_;           // duration of this track (mvhd.timescale)
    uint32_t reserved2_[2];
    uint16_t layer_;              // front-to-back ordering
    uint16_t predefined_;
    uint16_t volume_;             // relative audio volume (8.8)
    uint16_t reserved3_;
    uint32_t matrix_[9];          // transformation matrix
    uint32_t width_;              // visual presentation width (16.16)
    uint32_t height_;             // visual presentation height (16.16)
};
typedef struct tkhd_t tkhd_t;

struct mdia_t {
    struct unknown_atom_t *unknown_atoms_;
    struct mdhd_t *mdhd_;
    struct hdlr_t *hdlr_;
    struct minf_t *minf_;
};
typedef struct mdia_t mdia_t;

struct elst_table_t {
    uint64_t segment_duration_;
    int64_t media_time_;
    int16_t media_rate_integer_;
    int16_t media_rate_fraction_;
};
typedef struct elst_table_t elst_table_t;

struct elst_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entry_count_;
    struct elst_table_t *table_;
};
typedef struct elst_t elst_t;

struct edts_t {
    struct unknown_atom_t *unknown_atoms_;
    struct elst_t *elst_;
};
typedef struct edts_t edts_t;

struct mdhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint64_t creation_time_;      // seconds since midnite, Jan .1 1904 (UTC)
    uint64_t modification_time_;  // seconds since midnite, Jan .1 1904 (UTC)
    uint32_t timescale_;          // time units that pass in one second
    uint64_t duration_;           // duration of this media
    unsigned int language_[3];    // language code for this media (ISO 639-2/T)
    uint16_t predefined_;
};
typedef struct mdhd_t mdhd_t;

struct hdlr_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t predefined_;
    uint32_t handler_type_;       // format of the contents ('vide', 'soun', ...)
    uint32_t reserved1_;
    uint32_t reserved2_;
    uint32_t reserved3_;
    char *name_;                  // human-readable name for the track type (UTF8)
};
typedef struct hdlr_t hdlr_t;

struct minf_t {
    struct unknown_atom_t *unknown_atoms_;
    struct vmhd_t *vmhd_;
    struct smhd_t *smhd_;
    struct dinf_t *dinf_;
    struct stbl_t *stbl_;
};
typedef struct minf_t minf_t;

struct vmhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint16_t graphics_mode_;      // composition mode (0=copy)
    uint16_t opcolor_[3];
};
typedef struct vmhd_t vmhd_t;

struct smhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint16_t balance_;            // place mono audio tracks in stereo space (8.8)
    uint16_t reserved_;
};
typedef struct smhd_t smhd_t;

struct dinf_t {
    struct dref_t *dref_;         // declares the location of the media info
};
typedef struct dinf_t dinf_t;

struct dref_table_t {
    unsigned int flags_;          // 0x000001 is self contained
    char *name_;                  // name is a URN
    char *location_;              // location is a URL
};
typedef struct dref_table_t dref_table_t;

struct dref_t {
    unsigned int version_;
    unsigned int flags_;
    unsigned int entry_count_;
    dref_table_t *table_;
};
typedef struct dref_t dref_t;

struct stbl_t {
    struct unknown_atom_t *unknown_atoms_;
    struct stsd_t *stsd_;         // sample description
    struct stts_t *stts_;         // decoding time-to-sample
    struct stss_t *stss_;         // sync sample
    struct stsc_t *stsc_;         // sample-to-chunk
    struct stsz_t *stsz_;         // sample size
    struct stco_t *stco_;         // chunk offset
    struct ctts_t *ctts_;         // composition time-to-sample
};
typedef struct stbl_t stbl_t;

struct stsd_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct sample_entry_t *sample_entries_;
};
typedef struct stsd_t stsd_t;

struct video_sample_entry_t {
    uint16_t version_;
    uint16_t revision_level_;
    uint32_t vendor_;
    uint32_t temporal_quality_;
    uint32_t spatial_quality_;
    uint16_t width_;
    uint16_t height_;
    uint32_t horiz_resolution_;   // pixels per inch (16.16)
    uint32_t vert_resolution_;    // pixels per inch (16.16)
    uint32_t data_size_;
    uint16_t frame_count_;        // number of frames in each sample
    uint8_t compressor_name_[32]; // informative purposes (pascal string)
    uint16_t depth_;              // images are in colour with no alpha (24)
    int16_t color_table_id_;
};

struct audio_sample_entry_t {
    uint16_t version_;
    uint16_t revision_;
    uint32_t vendor_;
    uint16_t channel_count_;      // mono(1), stereo(2)
    uint16_t sample_size_;        // (bits)
    uint16_t compression_id_;
    uint16_t packet_size_;
    uint32_t samplerate_;         // sampling rate (16.16)
};

struct sample_entry_t {
    unsigned int len_;
    uint32_t fourcc_;
    unsigned char *buf_;

    struct video_sample_entry_t *video_;
    struct audio_sample_entry_t *audio_;

    unsigned int codec_private_data_length_;
    unsigned char const *codec_private_data_;

    // avcC
    unsigned int nal_unit_length_;
    unsigned int sps_length_;
    unsigned char *sps_;
    unsigned int pps_length_;
    unsigned char *pps_;

    // sound (WAVEFORMATEX) structure
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;

    unsigned int samplerate_hi_;
    unsigned int samplerate_lo_;

    // esds
    unsigned int max_bitrate_;
    unsigned int avg_bitrate_;
};
typedef struct sample_entry_t sample_entry_t;

struct stts_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct stts_table_t *table_;
};
typedef struct stts_t stts_t;

struct stts_table_t {
    uint32_t sample_count_;
    uint32_t sample_duration_;
};
typedef struct stts_table_t stts_table_t;

struct stss_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    uint32_t *sample_numbers_;
};
typedef struct stss_t stss_t;

struct stsc_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct stsc_table_t *table_;
};
typedef struct stsc_t stsc_t;

struct stsc_table_t {
    uint32_t chunk_;
    uint32_t samples_;
    uint32_t id_;
};
typedef struct stsc_table_t stsc_table_t;

struct stsz_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t sample_size_;
    uint32_t entries_;
    uint32_t *sample_sizes_;
};
typedef struct stsz_t stsz_t;

struct stco_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    uint64_t *chunk_offsets_;

    void *stco_inplace_;          // newly generated stco (patched inplace)
};
typedef struct stco_t stco_t;

struct ctts_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t entries_;
    struct ctts_table_t *table_;
};
typedef struct ctts_t ctts_t;

struct ctts_table_t {
    uint32_t sample_count_;
    uint32_t sample_offset_;
};
typedef struct ctts_table_t ctts_table_t;

struct samples_t {
    uint64_t pts_;                // decoding/presentation time
    unsigned int size_;           // size in bytes
    uint64_t pos_;                // byte offset
    unsigned int cto_;            // composition time offset

    unsigned int is_smooth_ss_: 1; // sync sample for smooth streaming
};
typedef struct samples_t samples_t;

struct chunks_t {
    unsigned int sample_;         // number of the first sample in the chunk
    unsigned int size_;           // number of samples in the chunk
    int id_;                      // not used
    uint64_t pos_;                // start byte position of chunk
};
typedef struct chunks_t chunks_t;

struct mvex_t {
    struct unknown_atom_t *unknown_atoms_;
    unsigned int tracks_;
    struct trex_t *trexs_[MAX_TRACKS];
};
typedef struct mvex_t mvex_t;

struct trex_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t track_id_;
    uint32_t default_sample_description_index_;
    uint32_t default_sample_duration_;
    uint32_t default_sample_size_;
    uint32_t default_sample_flags_;
};
typedef struct trex_t trex_t;

struct moof_t {
    struct unknown_atom_t *unknown_atoms_;
    struct mfhd_t *mfhd_;
    unsigned int tracks_;
    struct traf_t *trafs_[MAX_TRACKS];
};
typedef struct moof_t moof_t;

struct mfhd_t {
    unsigned int version_;
    unsigned int flags_;
    // the ordinal number of this fragment, in increasing order
    uint32_t sequence_number_;
};
typedef struct mfhd_t mfhd_t;

struct traf_t {
    struct unknown_atom_t *unknown_atoms_;
    struct tfhd_t *tfhd_;
    struct trun_t *trun_;
    struct uuid0_t *uuid0_;
    struct uuid1_t *uuid1_;
};
typedef struct traf_t traf_t;

struct tfhd_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t track_id_;
    // all the following are optional fields
    uint64_t base_data_offset_;
    uint32_t sample_description_index_;
    uint32_t default_sample_duration_;
    uint32_t default_sample_size_;
    uint32_t default_sample_flags_;
};
typedef struct tfhd_t tfhd_t;

struct tfra_table_t {
    uint64_t time_;
    uint64_t moof_offset_;
    uint32_t traf_number_;
    uint32_t trun_number_;
    uint32_t sample_number_;
};
typedef struct tfra_table_t tfra_table_t;

struct tfra_t {
    unsigned int version_;
    unsigned int flags_;
    uint32_t track_id_;
    unsigned int length_size_of_traf_num_;
    unsigned int length_size_of_trun_num_;
    unsigned int length_size_of_sample_num_;
    uint32_t number_of_entry_;
    struct tfra_table_t *table_;
};
typedef struct tfra_t tfra_t;

struct mfra_t {
    struct unknown_atom_t *unknown_atoms_;
    unsigned int tracks_;
    struct tfra_t *tfras_[MAX_TRACKS];
};
typedef struct mfra_t mfra_t;

struct trun_table_t {
    uint32_t sample_duration_;
    uint32_t sample_size_;
    uint32_t sample_flags_;
    uint32_t sample_composition_time_offset_;
};
typedef struct trun_table_t trun_table_t;

struct trun_t {
    unsigned int version_;
    unsigned int flags_;
    // the number of samples being added in this fragment; also the number of rows
    // in the following table (the rows can be empty)
    uint32_t sample_count_;
    // is added to the implicit or explicit data_offset established in the track
    // fragment header
    int32_t data_offset_;
    // provides a set of flags for the first sample only of this run
    uint32_t first_sample_flags_;

    trun_table_t *table_;

    struct trun_t *next_;
};
typedef struct trun_t trun_t;

struct uuid0_t {
    uint64_t pts_;
    uint64_t duration_;
};
typedef struct uuid0_t uuid0_t;

struct uuid1_t {
    unsigned int entries_;
    uint64_t pts_[2];
    uint64_t duration_[2];
};
typedef struct uuid1_t uuid1_t;

// random access structure similar to mfra, but with size field
struct rxs_t {
    uint64_t time_;
    uint64_t offset_;
    uint64_t size_;
};
typedef struct rxs_t rxs_t;

#define MP4_ELEMENTARY_STREAM_DESCRIPTOR_TAG   3
#define MP4_DECODER_CONFIG_DESCRIPTOR_TAG      4
#define MP4_DECODER_SPECIFIC_DESCRIPTOR_TAG    5

#define MP4_MPEG4Audio                      0x40
#define MP4_MPEG2AudioMain                  0x66
#define MP4_MPEG2AudioLowComplexity         0x67
#define MP4_MPEG2AudioScaleableSamplingRate 0x68
#define MP4_MPEG2AudioPart3                 0x69
#define MP4_MPEG1Audio                      0x6b

enum mp4_open_flags {
    MP4_OPEN_MOOV = 0x00000001,
    MP4_OPEN_MOOF = 0x00000002,
    MP4_OPEN_MDAT = 0x00000004,
    MP4_OPEN_MFRA = 0x00000008,
    MP4_OPEN_ALL  = 0x0000000f
};
typedef enum mp4_open_flags mp4_open_flags;

static void stbl_exit(struct stbl_t *atom);
static void moov_exit(moov_t *atom);
static void tkhd_exit(tkhd_t *tkhd);
static void mdhd_exit(struct mdhd_t *mdhd);
static void hdlr_exit(struct hdlr_t *atom);
static void vmhd_exit(struct vmhd_t *atom);
static void smhd_exit(struct smhd_t *atom);
static void hdlr_exit(struct hdlr_t *atom);
static void dref_exit(dref_t *atom);
static void dref_table_exit(dref_table_t *entry);
static void sample_entry_exit(sample_entry_t *sample_entry);
static unsigned int stss_get_nearest_keyframe(struct stss_t const *stss, unsigned int sample);
static void trak_exit(trak_t *trak);
static void mvex_exit(mvex_t *atom);
static void mdia_exit(mdia_t *atom);
static void edts_exit(edts_t *edts);
static void minf_exit(struct minf_t *atom);
static void dinf_exit(dinf_t *atom);
static void trex_exit(trex_t *atom);

static void *moov_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size);

static uint64_t atoi64(const char *val) {
#ifdef WIN32
  return _atoi64(val);
#else
  return strtoll(val, NULL, 10);
#endif
}

static const char *remove_path(const char *path) {
  const char *p = strrchr(path, DIR_SEPARATOR);
  if(p != NULL && *p != '\0') {
    return p + 1;
  }

  return path;
}

static void mp4_log_trace(const mp4_context_t *mp4_context, ngx_uint_t level, const char *fmt, ...) {
  va_list arglist;
  va_start(arglist, fmt);

  char out[255];

  vsprintf(out, fmt, arglist);

  ngx_log_error(level, mp4_context->r->connection->log, 0, out);

  va_end(arglist);
}

static int64_t seconds_since_1970(void) {
  return time(NULL);
}

static int64_t seconds_since_1904(void) {
  return seconds_since_1970() + 2082844800;
}

static unsigned int read_8(unsigned char const *buffer) {
  return buffer[0];
}

static unsigned char *write_8(unsigned char *buffer, unsigned int v) {
  buffer[0] = (uint8_t)v;

  return buffer + 1;
}

static uint16_t read_16(unsigned char const *buffer) {
  return (buffer[0] << 8) |
         (buffer[1] << 0);
}

static unsigned char *write_16(unsigned char *buffer, unsigned int v) {
  buffer[0] = (uint8_t)(v >> 8);
  buffer[1] = (uint8_t)(v >> 0);

  return buffer + 2;
}

static unsigned int read_24(unsigned char const *buffer) {
  return (buffer[0] << 16) |
         (buffer[1] << 8) |
         (buffer[2] << 0);
}

static uint32_t read_32(unsigned char const *buffer) {
  return (buffer[0] << 24) |
         (buffer[1] << 16) |
         (buffer[2] << 8) |
         (buffer[3] << 0);
}

static unsigned char *write_32(unsigned char *buffer, uint32_t v) {
  buffer[0] = (uint8_t)(v >> 24);
  buffer[1] = (uint8_t)(v >> 16);
  buffer[2] = (uint8_t)(v >> 8);
  buffer[3] = (uint8_t)(v >> 0);

  return buffer + 4;
}

static uint64_t read_64(unsigned char const *buffer) {
  return ((uint64_t)(read_32(buffer)) << 32) + read_32(buffer + 4);
}

static unsigned char *write_64(unsigned char *buffer, uint64_t v) {
  write_32(buffer + 0, (uint32_t)(v >> 32));
  write_32(buffer + 4, (uint32_t)(v >> 0));

  return buffer + 8;
}

static ngx_int_t mp4_read(mp4_context_t *mp4_context, u_char **buffer, size_t size, off_t pos) {
    if(mp4_context->buffer_size < size) {
        mp4_context->buffer_size = mp4_context->alignment ? (size / (off_t)4096 + 1) * 4096 : size;
        ngx_pfree(mp4_context->r->pool, mp4_context->buffer);
        mp4_context->buffer = 0;
        MP4_INFO("new buffer size: %zu", mp4_context->buffer_size);
    }

    if(mp4_context->buffer) {
        off_t start = mp4_context->file->offset - (off_t)mp4_context->buffer_size;
        if(pos + (off_t)size <= mp4_context->file->offset && pos > start) {
            *buffer = mp4_context->buffer + pos - start;
            mp4_context->offset += size;
            return NGX_OK;
        } else {
            ngx_pfree(mp4_context->r->pool, mp4_context->buffer);
            mp4_context->buffer = 0;
        }
    }

    off_t pos_align = mp4_context->alignment ? (pos / (off_t)4096) * 4096 : pos;
    if(pos != pos_align) mp4_context->buffer_size += 4096;

    if(pos_align + (off_t)mp4_context->buffer_size > mp4_context->filesize) {
        if(mp4_context->buffer_size - pos_align < 0) {
            return NGX_ERROR;
        }
        mp4_context->buffer_size = (size_t)(mp4_context->filesize - pos_align);
    }

    if(mp4_context->buffer == NULL) {
        mp4_context->buffer = ngx_palloc(mp4_context->r->pool, mp4_context->buffer_size);
        if (mp4_context->buffer == NULL) {
            return NGX_ERROR;
        }
    }

    ssize_t n = ngx_read_file(mp4_context->file, mp4_context->buffer, mp4_context->buffer_size, pos_align);

    if(n == NGX_ERROR) {
        return NGX_ERROR;
    }

    if((size_t) n != mp4_context->buffer_size) {
        MP4_ERROR("read only %zu of %zu from \"%s\"", n, mp4_context->buffer_size, mp4_context->file->name.data);
        return NGX_ERROR;
    }
    mp4_context->file->offset = pos_align + n;
    mp4_context->offset += size;

    *buffer = mp4_context->buffer + (pos_align == pos ? 0 : pos - pos_align);
    return NGX_OK;
}

int mp4_atom_read_header(mp4_context_t *mp4_context, mp4_atom_t *atom) {
  u_char *atom_header = 0;

  atom->start_ = mp4_context->offset;
  if(mp4_read(mp4_context, &atom_header, 8, atom->start_) == NGX_ERROR) {
    MP4_ERROR("%s", "Error reading atom header\n");
    return 0;
  }
  atom->short_size_ = read_32(&atom_header[0]);
  atom->type_ = read_32(&atom_header[4]);

  if(atom->short_size_ == 1) {
    if(mp4_read(mp4_context, &atom_header, 8, mp4_context->offset) == NGX_ERROR) {
      MP4_ERROR("%s", "Error reading extended atom header\n");
      return 0;
    }
    atom->size_ = read_64(&atom_header[0]);
  } else {
    atom->size_ = atom->short_size_;
  }

  MP4_INFO("Atom(%c%c%c%c,%"PRIu64")\n",
           atom->type_ >> 24, atom->type_ >> 16,
           atom->type_ >> 8, atom->type_,
           atom->size_);

  if(atom->size_ < ATOM_PREAMBLE_SIZE) {
    MP4_ERROR("%s", "Error: invalid atom size\n");
    return 0;
  }

  return 1;
}

int mp4_atom_write_header(unsigned char *outbuffer, mp4_atom_t const *atom) {
  int write_box64 = atom->short_size_ == 1 ? 1 : 0;

  if(write_box64) write_32(outbuffer, 1);
  else write_32(outbuffer, (uint32_t)atom->size_);

  write_32(outbuffer + 4, atom->type_);

  if(write_box64) {
    write_64(outbuffer + 8, atom->size_);
    return 16;
  } else return 8;
}

u_char *read_box(mp4_context_t *mp4_context, struct mp4_atom_t *atom) {
  hls_conf_t *conf = ngx_http_get_module_loc_conf(mp4_context->r, ngx_http_estreaming_module);
  if(atom->size_ > conf->max_buffer_size) {
    MP4_ERROR("mp4 moov atom is too large: %zu, you may want to increase hls_mp4_max_buffer_size", atom->size_);
    return 0;
  }

  u_char *box_data = 0;

  if(mp4_read(mp4_context, &box_data, atom->size_, atom->start_) == NGX_ERROR) {
    MP4_ERROR("Error reading %c%c%c%c atom\n",
              atom->type_ >> 24, atom->type_ >> 16,
              atom->type_ >> 8, atom->type_);
    return 0;
  }
  mp4_context->offset -= 8;

  return box_data;
}

static mp4_context_t *mp4_context_init(ngx_http_request_t *r, ngx_file_t *file, off_t filesize) {
  hls_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_http_estreaming_module);
  mp4_context_t *mp4_context = (mp4_context_t *)ngx_pcalloc(r->pool, sizeof(mp4_context_t));

  mp4_context->r = r;
  mp4_context->file = file;
  mp4_context->filesize = filesize;

  memset(&mp4_context->ftyp_atom, 0, sizeof(struct mp4_atom_t));
  memset(&mp4_context->moov_atom, 0, sizeof(struct mp4_atom_t));
  memset(&mp4_context->mdat_atom, 0, sizeof(struct mp4_atom_t));

  mp4_context->moov_data = 0;

  mp4_context->moov = 0;
  mp4_context->buffer = 0;
  mp4_context->buffer_size = conf->buffer_size;
  mp4_context->alignment = 0;

  return mp4_context;
}

static void mp4_context_exit(struct mp4_context_t *mp4_context) {
  if(mp4_context->moov_data) ngx_pfree(mp4_context->r->pool, mp4_context->moov_data);
  if(mp4_context->moov) moov_exit(mp4_context->moov);
  if(mp4_context->buffer) ngx_pfree(mp4_context->r->pool, mp4_context->buffer);
  ngx_pfree(mp4_context->r->pool, mp4_context);
}

static mp4_context_t *mp4_open(ngx_http_request_t *r, ngx_file_t *file, int64_t filesize, mp4_open_flags flags) {
  mp4_context_t *mp4_context = mp4_context_init(r, file, filesize);
  if(!mp4_context) return 0;
  while(!mp4_context->moov_atom.size_ || !mp4_context->mdat_atom.size_) {
    struct mp4_atom_t leaf_atom;

    if(!mp4_atom_read_header(mp4_context, &leaf_atom))
      break;

    switch(leaf_atom.type_) {
    case FOURCC('f', 't', 'y', 'p'):
      mp4_context->ftyp_atom = leaf_atom;
      break;
    case FOURCC('m', 'o', 'o', 'v'):
      mp4_context->moov_atom = leaf_atom;
      mp4_context->moov_data = read_box(mp4_context, &mp4_context->moov_atom);
      if(mp4_context->moov_data == NULL) {
        MP4_ERROR("%s", "No moov data\n");
        mp4_context_exit(mp4_context);
        return 0;
      }

      mp4_context->moov = (moov_t *)
                          moov_read(mp4_context, NULL,
                                    mp4_context->moov_data + ATOM_PREAMBLE_SIZE,
                                    mp4_context->moov_atom.size_ - ATOM_PREAMBLE_SIZE);

      if(mp4_context->moov == 0 || mp4_context->moov->mvhd_ == 0) {
        MP4_ERROR("%s", "Error parsing moov header\n");
        mp4_context_exit(mp4_context);
        return 0;
      }
      break;
    case FOURCC('m', 'd', 'a', 't'):
      mp4_context->mdat_atom = leaf_atom;
      break;
    }

    mp4_context->offset = leaf_atom.start_ + leaf_atom.size_;
  }

  return mp4_context;
}

static void mp4_close(struct mp4_context_t *mp4_context) {
  mp4_context_exit(mp4_context);
}

////////////////////////////////////////////////////////////////////////////////

static struct unknown_atom_t *unknown_atom_init() {
  unknown_atom_t *atom = (unknown_atom_t *)malloc(sizeof(unknown_atom_t));
  atom->atom_ = 0;
  atom->next_ = 0;

  return atom;
}

static void unknown_atom_exit(unknown_atom_t *atom) {
  while(atom) {
    unknown_atom_t *next = atom->next_;
    free(atom->atom_);
    free(atom);
    atom = next;
  }
}

static moov_t *moov_init() {
  moov_t *moov = (moov_t *)malloc(sizeof(moov_t));
  moov->unknown_atoms_ = 0;
  moov->mvhd_ = 0;
  moov->tracks_ = 0;
  moov->mvex_ = 0;

  moov->is_indexed_ = 0;

  return moov;
}

static void moov_exit(moov_t *atom) {
  unsigned int i;
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->mvhd_) {
    free(atom->mvhd_);
  }
  for(i = 0; i != atom->tracks_; ++i) {
    trak_exit(atom->traks_[i]);
  }
  if(atom->mvex_) {
    mvex_exit(atom->mvex_);
  }
  free(atom);
}

static trak_t *trak_init() {
  trak_t *trak = (trak_t *)malloc(sizeof(trak_t));
  trak->unknown_atoms_ = 0;
  trak->tkhd_ = 0;
  trak->mdia_ = 0;
  trak->edts_ = 0;
  trak->chunks_size_ = 0;
  trak->chunks_ = 0;
  trak->samples_size_ = 0;
  trak->samples_ = 0;

//  trak->fragment_pts_ = 0;

  return trak;
}

static void trak_exit(trak_t *trak) {
  if(trak->unknown_atoms_) {
    unknown_atom_exit(trak->unknown_atoms_);
  }
  if(trak->tkhd_) {
    tkhd_exit(trak->tkhd_);
  }
  if(trak->mdia_) {
    mdia_exit(trak->mdia_);
  }
  if(trak->edts_) {
    edts_exit(trak->edts_);
  }
  if(trak->chunks_) {
    free(trak->chunks_);
  }
  if(trak->samples_) {
    free(trak->samples_);
  }
  free(trak);
}

static mvhd_t *mvhd_init() {
  unsigned int i;
  mvhd_t *atom = (mvhd_t *)malloc(sizeof(mvhd_t));

  atom->version_ = 1;
  atom->flags_ = 0;
  atom->creation_time_ =
    atom->modification_time_ = seconds_since_1904();
  atom->timescale_ = 10000000;
  atom->duration_ = 0;
  atom->rate_ = (1 << 16);
  atom->volume_ = (1 << 8);
  atom->reserved1_ = 0;
  for(i = 0; i != 2; ++i) {
    atom->reserved2_[i] = 0;
  }
  for(i = 0; i != 9; ++i) {
    atom->matrix_[i] = 0;
  }
  atom->matrix_[0] = 0x00010000;
  atom->matrix_[4] = 0x00010000;
  atom->matrix_[8] = 0x40000000;
  for(i = 0; i != 6; ++i) {
    atom->predefined_[i] = 0;
  }
  atom->next_track_id_ = 1;

  return atom;
}

static tkhd_t *tkhd_init() {
  unsigned int i;
  tkhd_t *tkhd = (tkhd_t *)malloc(sizeof(tkhd_t));

  tkhd->version_ = 1;
  tkhd->flags_ = 7;           // track_enabled, track_in_movie, track_in_preview
  tkhd->creation_time_ =
    tkhd->modification_time_ = seconds_since_1904();
  tkhd->track_id_ = 0;
  tkhd->reserved_ = 0;
  tkhd->duration_ = 0;
  for(i = 0; i != 2; ++i) {
    tkhd->reserved2_[i] = 0;
  }
  tkhd->layer_ = 0;
  tkhd->predefined_ = 0;
  tkhd->volume_ = (1 << 8) + 0;
  tkhd->reserved3_ = 0;
  for(i = 0; i != 9; ++i) {
    tkhd->matrix_[i] = 0;
  }
  tkhd->matrix_[0] = 0x00010000;
  tkhd->matrix_[4] = 0x00010000;
  tkhd->matrix_[8] = 0x40000000;
  tkhd->width_ = 0;
  tkhd->height_ = 0;

  return tkhd;
}

static void tkhd_exit(tkhd_t *tkhd) {
  free(tkhd);
}

static struct mdia_t *mdia_init() {
  mdia_t *atom = (mdia_t *)malloc(sizeof(mdia_t));
  atom->unknown_atoms_ = 0;
  atom->mdhd_ = 0;
  atom->hdlr_ = 0;
  atom->minf_ = 0;

  return atom;
}

static void mdia_exit(mdia_t *atom) {
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->mdhd_) {
    mdhd_exit(atom->mdhd_);
  }
  if(atom->hdlr_) {
    hdlr_exit(atom->hdlr_);
  }
  if(atom->minf_) {
    minf_exit(atom->minf_);
  }
  free(atom);
}

static elst_t *elst_init() {
  elst_t *elst = (elst_t *)malloc(sizeof(elst_t));

  elst->version_ = 1;
  elst->flags_ = 0;
  elst->entry_count_ = 0;
  elst->table_ = 0;

  return elst;
}

static void elst_exit(elst_t *elst) {
  if(elst->table_) {
    free(elst->table_);
  }
  free(elst);
}

static edts_t *edts_init() {
  edts_t *edts = (edts_t *)malloc(sizeof(edts_t));

  edts->unknown_atoms_ = 0;
  edts->elst_ = 0;

  return edts;
}

static void edts_exit(edts_t *edts) {
  if(edts->unknown_atoms_) {
    unknown_atom_exit(edts->unknown_atoms_);
  }
  if(edts->elst_) {
    elst_exit(edts->elst_);
  }
  free(edts);
}

static mdhd_t *mdhd_init() {
  unsigned int i;
  mdhd_t *mdhd = (mdhd_t *)malloc(sizeof(mdhd_t));

  mdhd->version_ = 1;
  mdhd->flags_ = 0;
  mdhd->creation_time_ =
    mdhd->modification_time_ = seconds_since_1904();
  mdhd->timescale_ = 10000000;
  mdhd->duration_ = 0;
  for(i = 0; i != 3; ++i) {
    mdhd->language_[i] = 0x7f;
  }
  mdhd->predefined_ = 0;

  return mdhd;
}

static void mdhd_exit(struct mdhd_t *mdhd) {
  free(mdhd);
}

static hdlr_t *hdlr_init() {
  hdlr_t *atom = (hdlr_t *)malloc(sizeof(hdlr_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->predefined_ = 0;
  atom->handler_type_ = 0;
  atom->reserved1_ = 0;
  atom->reserved2_ = 0;
  atom->reserved3_ = 0;
  atom->name_ = 0;

  return atom;
}

static void hdlr_exit(struct hdlr_t *atom) {
  if(atom->name_) {
    free(atom->name_);
  }
  free(atom);
}

static struct minf_t *minf_init() {
  struct minf_t *atom = (struct minf_t *)malloc(sizeof(struct minf_t));
  atom->unknown_atoms_ = 0;
  atom->vmhd_ = 0;
  atom->smhd_ = 0;
  atom->dinf_ = 0;
  atom->stbl_ = 0;

  return atom;
}

static void minf_exit(struct minf_t *atom) {
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->vmhd_) {
    vmhd_exit(atom->vmhd_);
  }
  if(atom->smhd_) {
    smhd_exit(atom->smhd_);
  }
  if(atom->dinf_) {
    dinf_exit(atom->dinf_);
  }
  if(atom->stbl_) {
    stbl_exit(atom->stbl_);
  }
  free(atom);
}

static vmhd_t *vmhd_init() {
  unsigned int i;
  vmhd_t *atom = (vmhd_t *)malloc(sizeof(vmhd_t));

  atom->version_ = 0;
  atom->flags_ = 1;
  atom->graphics_mode_ = 0;
  for(i = 0; i != 3; ++i) {
    atom->opcolor_[i] = 0;
  }

  return atom;
}

static void vmhd_exit(struct vmhd_t *atom) {
  free(atom);
}

static smhd_t *smhd_init() {
  smhd_t *atom = (smhd_t *)malloc(sizeof(smhd_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->balance_ = 0;
  atom->reserved_ = 0;

  return atom;
}

static void smhd_exit(struct smhd_t *atom) {
  free(atom);
}

static dinf_t *dinf_init() {
  dinf_t *atom = (dinf_t *)malloc(sizeof(dinf_t));

  atom->dref_ = 0;

  return atom;
}

static void dinf_exit(dinf_t *atom) {
  if(atom->dref_) {
    dref_exit(atom->dref_);
  }
  free(atom);
}

static dref_t *dref_init() {
  dref_t *atom = (dref_t *)malloc(sizeof(dref_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entry_count_ = 0;
  atom->table_ = 0;

  return atom;
}

static void dref_exit(dref_t *atom) {
  unsigned int i;
  for(i = 0; i != atom->entry_count_; ++i) {
    dref_table_exit(&atom->table_[i]);
  }
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

static void dref_table_init(dref_table_t *entry) {
  entry->flags_ = 0;
  entry->name_ = 0;
  entry->location_ = 0;
}

static void dref_table_exit(dref_table_t *entry) {
  if(entry->name_) {
    free(entry->name_);
  }
  if(entry->location_) {
    free(entry->location_);
  }
}

static struct stbl_t *stbl_init() {
  struct stbl_t *atom = (struct stbl_t *)malloc(sizeof(struct stbl_t));
  atom->unknown_atoms_ = 0;
  atom->stsd_ = 0;
  atom->stts_ = 0;
  atom->stss_ = 0;
  atom->stsc_ = 0;
  atom->stsz_ = 0;
  atom->stco_ = 0;
  atom->ctts_ = 0;

  return atom;
}

static void ctts_exit(struct ctts_t *atom) {
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

static void stsd_exit(struct stsd_t *atom) {
  unsigned int i;
  for(i = 0; i != atom->entries_; ++i) {
    sample_entry_t *sample_entry = &atom->sample_entries_[i];
    sample_entry_exit(sample_entry);
  }
  if(atom->sample_entries_) {
    free(atom->sample_entries_);
  }
  free(atom);
}

static unsigned int stbl_get_nearest_keyframe(struct stbl_t const *stbl,
    unsigned int sample) {
  // If the sync atom is not present, all samples are implicit sync samples.
  if(!stbl->stss_)
    return sample;

  return stss_get_nearest_keyframe(stbl->stss_, sample);
}

static stsd_t *stsd_init() {
  stsd_t *atom = (stsd_t *)malloc(sizeof(stsd_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->sample_entries_ = 0;

  return atom;
}

static void sample_entry_init(sample_entry_t *sample_entry) {
  sample_entry->len_ = 0;
  sample_entry->buf_ = 0;
  sample_entry->codec_private_data_length_ = 0;
  sample_entry->codec_private_data_ = 0;

  sample_entry->video_ = 0;
  sample_entry->audio_ = 0;
//sample_entry->hint_ = 0;

  sample_entry->nal_unit_length_ = 0;
  sample_entry->sps_length_ = 0;
  sample_entry->sps_ = 0;
  sample_entry->pps_length_ = 0;
  sample_entry->pps_ = 0;

  sample_entry->wFormatTag = 0;
  sample_entry->nChannels = 2;
  sample_entry->nSamplesPerSec = 44100;
  sample_entry->nAvgBytesPerSec = 0;
  sample_entry->nBlockAlign = 0;
  sample_entry->wBitsPerSample = 16;

  sample_entry->max_bitrate_ = 0;
  sample_entry->avg_bitrate_ = 0;
}

void sample_entry_assign(sample_entry_t *lhs, sample_entry_t const *rhs) {
  memcpy(lhs, rhs, sizeof(sample_entry_t));
  if(rhs->buf_ != NULL) {
    lhs->buf_ = (unsigned char *)malloc(rhs->len_);
    memcpy(lhs->buf_, rhs->buf_, rhs->len_);
  }
}

static void sample_entry_exit(sample_entry_t *sample_entry) {
  if(sample_entry->buf_) {
    free(sample_entry->buf_);
  }

  if(sample_entry->video_) {
    free(sample_entry->video_);
  }
  if(sample_entry->audio_) {
    free(sample_entry->audio_);
  }
}

static const uint32_t aac_samplerates[] = {
  96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
  16000, 12000, 11025,  8000,  7350,     0,     0,     0
};

static int mp4_samplerate_to_index(unsigned int samplerate) {
  unsigned int i;
  for(i = 0; i != 13; ++i) {
    if(aac_samplerates[i] == samplerate)
      return i;
  }
  return 4;
}

// Create an ADTS frame header
static void sample_entry_get_adts(sample_entry_t const *sample_entry,
                                  unsigned int sample_size, uint8_t *buf) {
  unsigned int syncword = 0xfff;
  unsigned int ID = 0; // MPEG-4
  unsigned int layer = 0;
  unsigned int protection_absent = 1;
  // 0 = Main profile AAC MAIN
  // 1 = Low Complexity profile (LC) AAC LC
  // 2 = Scalable Sample Rate profile (SSR) AAC SSR
  // 3 = (reserved) AAC LTP
  unsigned int profile = 1;
  unsigned int sampling_frequency_index =
    mp4_samplerate_to_index(sample_entry->nSamplesPerSec);
  unsigned int private_bit = 0;
  unsigned int channel_configuration = sample_entry->nChannels;
  unsigned int original_copy = 0;
  unsigned int home = 0;
  unsigned int copyright_identification_bit = 0;
  unsigned int copyright_identification_start = 0;
  unsigned int aac_frame_length = 7 + sample_size;
  unsigned int adts_buffer_fullness = 0x7ff;
  unsigned int no_raw_data_blocks_in_frame = 0;
  unsigned char buffer[8];

  uint64_t adts = 0;
  adts = (adts << 12) | syncword;
  adts = (adts << 1) | ID;
  adts = (adts << 2) | layer;
  adts = (adts << 1) | protection_absent;
  adts = (adts << 2) | profile;
  adts = (adts << 4) | sampling_frequency_index;
  adts = (adts << 1) | private_bit;
  adts = (adts << 3) | channel_configuration;
  adts = (adts << 1) | original_copy;
  adts = (adts << 1) | home;
  adts = (adts << 1) | copyright_identification_bit;
  adts = (adts << 1) | copyright_identification_start;
  adts = (adts << 13) | aac_frame_length;
  adts = (adts << 11) | adts_buffer_fullness;
  adts = (adts << 2) | no_raw_data_blocks_in_frame;

  write_64(buffer, adts);

  memcpy(buf, buffer + 1, 7);
}

static stts_t *stts_init() {
  stts_t *atom = (stts_t *)malloc(sizeof(stts_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->table_ = 0;

  return atom;
}

static void stts_exit(struct stts_t *atom) {
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

static unsigned int stts_get_sample(struct stts_t const *stts, uint64_t time) {
  unsigned int stts_index = 0;
  unsigned int stts_count;

  unsigned int ret = 0;
  uint64_t time_count = 0;

  for(; stts_index != stts->entries_; ++stts_index) {
    unsigned int sample_count = stts->table_[stts_index].sample_count_;
    unsigned int sample_duration = stts->table_[stts_index].sample_duration_;
    if(time_count + (uint64_t)sample_duration * (uint64_t)sample_count >= time) {
      stts_count = (unsigned int)((time - time_count + sample_duration - 1) / sample_duration);
      time_count += (uint64_t)stts_count * (uint64_t)sample_duration;
      ret += stts_count;
      break;
    } else {
      time_count += (uint64_t)sample_duration * (uint64_t)sample_count;
      ret += sample_count;
    }
  }
  return ret;
}

static uint64_t stts_get_time(struct stts_t const *stts, unsigned int sample) {
  uint64_t ret = 0;
  unsigned int stts_index = 0;
  unsigned int sample_count = 0;

  for(;;) {
    unsigned int table_sample_count = stts->table_[stts_index].sample_count_;
    unsigned int table_sample_duration = stts->table_[stts_index].sample_duration_;
    if(sample_count + table_sample_count > sample) {
      unsigned int stts_count = (sample - sample_count);
      ret += (uint64_t)stts_count * (uint64_t)table_sample_duration;
      break;
    } else {
      sample_count += table_sample_count;
      ret += (uint64_t)table_sample_count * (uint64_t)table_sample_duration;
      stts_index++;
    }
  }
  return ret;
}

static struct stss_t *stss_init() {
  stss_t *atom = (stss_t *)malloc(sizeof(stss_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->sample_numbers_ = 0;

  return atom;
}

static void stss_exit(struct stss_t *atom) {
  if(atom->sample_numbers_) {
    free(atom->sample_numbers_);
  }
  free(atom);
}

static unsigned int stss_get_nearest_keyframe(struct stss_t const *stss, unsigned int sample) {
  // scan the sync samples to find the key frame that precedes the sample number
  unsigned int i;
  unsigned int table_sample = 0;
  for(i = 0; i != stss->entries_; ++i) {
    table_sample = stss->sample_numbers_[i];
    if(table_sample >= sample)
      break;
  }
  if(table_sample == sample)
    return table_sample;
  else
    return stss->sample_numbers_[i - 1];
}

static stsc_t *stsc_init() {
  stsc_t *atom = (stsc_t *)malloc(sizeof(stsc_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->table_ = 0;

  return atom;
}

static void stsc_exit(struct stsc_t *atom) {
  if(atom->table_) {
    free(atom->table_);
  }
  free(atom);
}

static stsz_t *stsz_init() {
  stsz_t *atom = (stsz_t *)malloc(sizeof(stsz_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->sample_size_ = 0;
  atom->entries_ = 0;
  atom->sample_sizes_ = 0;

  return atom;
}

static void stsz_exit(struct stsz_t *atom) {
  if(atom->sample_sizes_) {
    free(atom->sample_sizes_);
  }
  free(atom);
}

static stco_t *stco_init() {
  stco_t *atom = (stco_t *)malloc(sizeof(stco_t));

  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->chunk_offsets_ = 0;

  return atom;
}

static void stco_exit(stco_t *atom) {
  if(atom->chunk_offsets_) {
    free(atom->chunk_offsets_);
  }
  free(atom);
}

static struct ctts_t *ctts_init() {
  struct ctts_t *atom = (struct ctts_t *)malloc(sizeof(struct ctts_t));
  atom->version_ = 0;
  atom->flags_ = 0;
  atom->entries_ = 0;
  atom->table_ = 0;

  return atom;
}

static unsigned int ctts_get_samples(struct ctts_t const *ctts) {
  unsigned int samples = 0;
  unsigned int entries = ctts->entries_;
  unsigned int i;
  for(i = 0; i != entries; ++i) {
    unsigned int sample_count = ctts->table_[i].sample_count_;
    samples += sample_count;
  }

  return samples;
}

static uint64_t moov_time_to_trak_time(uint64_t t, uint32_t moov_time_scale,
                                       uint32_t trak_time_scale) {
  return t * (uint64_t)trak_time_scale / moov_time_scale;
}

static uint64_t trak_time_to_moov_time(uint64_t t, uint32_t moov_time_scale,
                                       uint32_t trak_time_scale) {
  return t * (uint64_t)moov_time_scale / trak_time_scale;
}

static mvex_t *mvex_init() {
  mvex_t *mvex = (mvex_t *)malloc(sizeof(mvex_t));
  mvex->unknown_atoms_ = 0;
  mvex->tracks_ = 0;

  return mvex;
}

static void mvex_exit(mvex_t *atom) {
  unsigned int i;
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  for(i = 0; i != atom->tracks_; ++i) {
    trex_exit(atom->trexs_[i]);
  }
  free(atom);
}

static trex_t *trex_init() {
  trex_t *trex = (trex_t *)malloc(sizeof(trex_t));

  trex->version_ = 0;
  trex->flags_ = 0;
  trex->track_id_ = 0;
  trex->default_sample_description_index_ = 0;
  trex->default_sample_duration_ = 0;
  trex->default_sample_size_ = 0;
  trex->default_sample_flags_ = 0;

  return trex;
}

static void trex_exit(trex_t *atom) {
  free(atom);
}

static void stbl_exit(struct stbl_t *atom) {
  if(atom->unknown_atoms_) {
    unknown_atom_exit(atom->unknown_atoms_);
  }
  if(atom->stsd_) {
    stsd_exit(atom->stsd_);
  }
  if(atom->stts_) {
    stts_exit(atom->stts_);
  }
  if(atom->stss_) {
    stss_exit(atom->stss_);
  }
  if(atom->stsc_) {
    stsc_exit(atom->stsc_);
  }
  if(atom->stsz_) {
    stsz_exit(atom->stsz_);
  }
  if(atom->stco_) {
    stco_exit(atom->stco_);
  }
  if(atom->ctts_) {
    ctts_exit(atom->ctts_);
  }

  free(atom);
}

// End Of File
