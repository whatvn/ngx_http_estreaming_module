/*******************************************************************************
 moov.h - A library for splitting Quicktime/MPEG4 files.
 *  * Author:
 *      - codeshop
 *      - Isaev Ivan  
 *      - hungnguyen
 For licensing see the LICENSE file
 ******************************************************************************/

enum input_format_t {
    INPUT_FORMAT_MP4,
    INPUT_FORMAT_FLV
};
typedef enum input_format_t input_format_t;

//typedef struct {

struct mp4_split_options_t {
    float start;
    uint64_t start_integer;
    float end;
    int fragments;
    enum input_format_t input_format;
    uint32_t fragment_bitrate;
    uint32_t fragment_track_id;
    uint64_t fragment_start;
    // add adbr
    int adbr;
    int org;
    int video_resolution;
    char *hash;
};
typedef struct mp4_split_options_t mp4_split_options_t;

/* Returns true when the test string is a prefix of the input */
int starts_with(const char *input, const char *test) {
    while (*input && *test) {
        if (*input != *test)
            return 0;
        ++input;
        ++test;
    }

    return *test == '\0';
}

/* Returns true when the test string is a suffix of the input */
int ends_with(const char *input, const char *test) {
    const char *it = input + strlen(input);
    const char *pit = test + strlen(test);
    while (it != input && pit != test) {
        if (*it != *pit)
            return 0;
        --it;
        --pit;
    }

    return pit == test;
}

////////////////////////////////////////////////////////////////////////////////

// reported by everwanna:
// av out of sync because:
// audio track 0 without stss, seek to the exact time.
// video track 1 with stss, seek to the nearest key frame time.
//
// fixed:
// first pass we get the new aligned times for traks with an stss present
// second pass is for traks without an stss

static int get_aligned_start_and_end(struct mp4_context_t const *mp4_context,
        unsigned int start, unsigned int end,
        unsigned int *trak_sample_start,
        unsigned int *trak_sample_end) {
    unsigned int pass;
    struct moov_t *moov = mp4_context->moov;
    uint32_t moov_time_scale = moov->mvhd_->timescale_;

    for (pass = 0; pass != 2; ++pass) {
        unsigned int i;
        for (i = 0; i != moov->tracks_; ++i) {
            struct trak_t *trak = moov->traks_[i];
            struct stbl_t *stbl = trak->mdia_->minf_->stbl_;
            uint32_t trak_time_scale = trak->mdia_->mdhd_->timescale_;

            // 1st pass: stss present, 2nd pass: no stss present
            if (pass == 0 && !stbl->stss_)
                continue;
            if (pass == 1 && stbl->stss_)
                continue;

            // get start
            if (start == 0) {
                trak_sample_start[i] = start;
            } else {
                start = stts_get_sample(stbl->stts_,
                        moov_time_to_trak_time(start, moov_time_scale, trak_time_scale));

                MP4_INFO("start=%u (trac time)\n", start);
                MP4_INFO("start=%.2f (seconds)\n",
                        stts_get_time(stbl->stts_, start) / (float) trak_time_scale);

                start = stbl_get_nearest_keyframe(stbl, start + 1) - 1;
                MP4_INFO("start=%u (zero based keyframe)\n", start);
                trak_sample_start[i] = start;
                start = (unsigned int) (trak_time_to_moov_time(
                        stts_get_time(stbl->stts_, start), moov_time_scale, trak_time_scale));
                MP4_INFO("start=%u (moov time)\n", start);
                MP4_INFO("start=%.2f (seconds)\n", start / (float) moov_time_scale);
            }

            // get end
            if (end == 0) {
                // The default is till-the-end of the track
                trak_sample_end[i] = trak->samples_size_;
            } else {
                end = stts_get_sample(stbl->stts_,
                        moov_time_to_trak_time(end, moov_time_scale, trak_time_scale));
                MP4_INFO("end=%u (trac time)\n", end);
                MP4_INFO("end=%.2f (seconds)\n",
                        stts_get_time(stbl->stts_, end) / (float) trak_time_scale);

                if (end >= trak->samples_size_) {
                    end = trak->samples_size_;
                } else {
                    end = stbl_get_nearest_keyframe(stbl, end + 1) - 1;
                }
                MP4_INFO("end=%u (zero based keyframe)\n", end);
                trak_sample_end[i] = end;
                //          MP4_INFO("endframe=%u, samples_size_=%u\n", end, trak->samples_size_);
                end = (unsigned int) trak_time_to_moov_time(
                        stts_get_time(stbl->stts_, end), moov_time_scale, trak_time_scale);
                MP4_INFO("end=%u (moov time)\n", end);
                MP4_INFO("end=%.2f (seconds)\n", end / (float) moov_time_scale);
            }
        }
    }

    MP4_INFO("start=%u\n", start);
    MP4_INFO("end=%u\n", end);

    if (end && start >= end) {
        return 0;
    }

    return 1;
}

////////////////////////////////////////////////////////////////////////////////

mp4_split_options_t *mp4_split_options_init(ngx_http_request_t *r) {
    mp4_split_options_t *options = (mp4_split_options_t *) ngx_pcalloc(r->pool, sizeof (mp4_split_options_t));
    options->start = 0.0;
    options->start_integer = 0;
    options->end = 0.0;
    options->fragments = 0;
    options->input_format = INPUT_FORMAT_MP4;
    options->fragment_bitrate = 0;
    options->fragment_track_id = 0;
    options->fragment_start = 0;
    options->hash = NULL;

    return options;
}

int mp4_split_options_set(ngx_http_request_t *r, struct mp4_split_options_t *options,
        const char *args_data,
        unsigned int args_size) {
    int result = 1;
#define KEY_EQ(lit) (key_len == (sizeof(lit) - 1) && strncmp(key, lit, sizeof(lit) - 1) == 0)
#define VAL_EQ(lit) (val_len == (int) (sizeof(lit) - 1) && strncmp(val, lit, sizeof(lit) - 1) == 0)

    {
        hls_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_http_estreaming_module);
        const char *first = args_data;
        const char *last = first + args_size + 1;

        if (*first == '?') ++first;

        {
            char const *key = first;
            char const *val = NULL;
            int is_key = 1;
            size_t key_len = 0;

            while (first != last) {
                // the args_data is not necessarily 0 terminated, so fake it
                int ch = (first == last - 1) ? '\0' : *first;
                switch (ch) {
                    case '=':
                        val = first + 1;
                        key_len = first - key;
                        is_key = 0;
                        break;
                    case '&':
                    case '\0':
                        if (!is_key) {
                            // make sure the value is zero-terminated (for strtod,atoi64)
                            int val_len = first - val;
                            char *valz = (char *) malloc(val_len + 1);
                            memcpy(valz, val, val_len);
                            valz[val_len] = '\0';

                            if (KEY_EQ("start")) {
                                options->start = (float) (strtod(valz, NULL));
                                options->start_integer = atoi64(valz);
                            } else if (KEY_EQ("end")) {
                                options->end = (float) (strtod(valz, NULL));
                            } else if (KEY_EQ("bitrate")) {
                                options->fragment_bitrate = (unsigned int) (atoi64(valz));
                            } else if (KEY_EQ("org")) {
                                if (VAL_EQ("true")) {
                                    options->org = 1;
                                }
                            } else if (KEY_EQ("video")) {
                                options->fragments = 1;
                                options->fragment_start = atoi64(valz);
                            } else if (KEY_EQ("audio")) {
                                options->fragment_track_id = atoi64(valz);
                            } else if (KEY_EQ("length")) {
                                conf->length = (ngx_uint_t) atoi64(valz);
                            } else if (KEY_EQ("hash")) {
                                if (val_len > 16) {
                                    val_len = 16;
                                    valz[val_len] = 0;
                                }
                                options->hash = ngx_pcalloc(r->pool, val_len + 1);
                                strcpy(options->hash, valz);
                            } else if (KEY_EQ("input")) {
                                if (VAL_EQ("flv")) {
                                    options->input_format = INPUT_FORMAT_FLV;
                                }
                            } else if (KEY_EQ("vr")) {
                                if (VAL_EQ("360p")) {
                                    options->video_resolution = 1; //360p
                                } else if (VAL_EQ("480p")) {
                                    options->video_resolution = 2; //480p
                                } else if (VAL_EQ("720p")) {
                                    options->video_resolution = 3; //720p
                                }
                            } else if (KEY_EQ("adbr")) {
                                if (VAL_EQ("true")) {
                                    options->adbr = 1;
                                }
                            }
                            free(valz);
                        }
                        key = first + 1;
                        val = NULL;
                        is_key = 1;
                        break;
                }
                ++first;
            }
        }
    }

    return result;
#undef KEY_EQ
#undef VAL_EQ
}

void mp4_split_options_exit(ngx_http_request_t *r, struct mp4_split_options_t *options) {
    if (options == NULL) return;

    if (options->hash) ngx_pfree(r->pool, options->hash);

    ngx_pfree(r->pool, options);
    options = NULL;
}

extern int mp4_split(struct mp4_context_t *mp4_context,
        unsigned int *trak_sample_start,
        unsigned int *trak_sample_end,
        mp4_split_options_t const *options) {
    int result;

    float start_time = options->start;
    float end_time = options->end;

    moov_build_index(mp4_context, mp4_context->moov);

    {
        struct moov_t const *moov = mp4_context->moov;
        uint32_t moov_time_scale = moov->mvhd_->timescale_;
        unsigned int start = (unsigned int) (start_time * moov_time_scale + 0.5f);
        unsigned int end = (unsigned int) (end_time * moov_time_scale + 0.5f);

        // for every trak, convert seconds to sample (time-to-sample).
        // adjust sample to keyframe
        result = get_aligned_start_and_end(mp4_context, start, end,
                trak_sample_start, trak_sample_end);
    }

    return result;
}

uint64_t get_filesize(const char *path) {
    struct stat status;
    if (stat(path, &status)) {
        printf("get_file_length(%s) stat: ", path);
        perror(NULL);
        return 0;
    }
    return status.st_size;
}

// End Of File
