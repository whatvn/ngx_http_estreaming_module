
/*******************************************************************************
 * output_m3u8.h - A library for writing M3U8 playlists.

 * Author:
 *      - codeshop
 *      - Isaev Ivan  
 *      - hungnguyen
 * As we develop this module for nginx, and nginx implement of varios libc function
 * is almost faster, we should use all of its functions
 ******************************************************************************/

#define R_360P 1
#define R_480P 2
#define R_720P 3

void replace(char *o_string, size_t capacity, const char *s_string, const char *r_string) {
    char *ch;
    size_t s_len, r_len;

    if (o_string == NULL || s_string == NULL || r_string == NULL || capacity == 0) {
        return;
    }

    s_len = strlen(s_string);
    r_len = strlen(r_string);
    if (s_len == 0) {
        return;
    }

    while ((ch = strstr(o_string, s_string)) != NULL) {
        char buffer[1024];
        size_t prefix_len = (size_t) (ch - o_string);
        int written;

        if (capacity > sizeof (buffer)) {
            capacity = sizeof (buffer);
        }

        written = snprintf(buffer, capacity, "%.*s%s%s",
                (int) prefix_len, o_string, r_string, ch + s_len);
        if (written < 0) {
            return;
        }

        ngx_cpystrn((u_char *) o_string, (u_char *) buffer, capacity);

        if ((size_t) written >= capacity || r_len > s_len) {
            // avoid repeated truncation loops when replacement grows output
            return;
        }
    }
}

int mp4_create_m3u8(struct mp4_context_t *mp4_context, struct bucket_t * bucket,
        struct mp4_split_options_t *options, int width, ngx_str_t path) {
    hls_conf_t *conf = ngx_http_get_module_loc_conf(mp4_context->r, ngx_http_estreaming_module);
    int result = 0;
    u_char *buffer = NULL;
    buffer = (u_char *) ngx_palloc(mp4_context->r->pool, 1024 * 256);
    u_char *p = buffer;
    u_char *buf_end = buffer + (1024 * 256);
    char extra[1024] = "";
#define APPEND_TO_M3U8(...)                                                     \
    do {                                                                         \
        p = ngx_slprintf(p, buf_end, __VA_ARGS__);                              \
        if (p >= buf_end) {                                                      \
            return 0;                                                            \
        }                                                                        \
    } while (0)
    if (mp4_context->r->args.data) {
        size_t copy_len = mp4_context->r->args.len;
        if (copy_len > sizeof (extra) - 2) {
            copy_len = sizeof (extra) - 2;
        }
        extra[0] = '?';
        ngx_memcpy(extra + 1, (const char *) mp4_context->r->args.data, copy_len);
        extra[copy_len + 1] = '\0';
    }

    /* #EXTM3U
    #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1280000,RESOLUTION=640x360
    http://221.132.35.210:9090/vod/hai.m3u8?adbr=true&vr=360p
    #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=2560000,RESOLUTION=854x480
    http://221.132.35.210:9090/vod/hai.m3u8?adbr=true&vr=480p
    #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=7680000, RESOLUTION=1080x720
    hhttp://221.132.35.210:9090/vod/hai.m3u8
     */
    char *rewrite = NULL;
    APPEND_TO_M3U8("#EXTM3U\n");
    if (conf->hls_proxy.data != NULL) {
        rewrite = (char *) ngx_palloc(mp4_context->r->pool,
                ngx_strlen(conf->hls_proxy.data) + ngx_strlen(mp4_context->file->name.data) +
                ngx_strlen(mp4_context->r->headers_in.server.data) - mp4_context->root + 7);
    }
    char *filename = NULL;
    if (!conf->relative) {
        filename = (char *) ngx_palloc(mp4_context->r->pool, ngx_strlen(mp4_context->file->name.data) + ngx_strlen(mp4_context->r->headers_in.server.data) - mp4_context->root + 7);
        strcpy(filename, "http://");
        strcat(filename, (const char *) (mp4_context->r->headers_in.server.data));
        strcat(filename, (const char *) (mp4_context->file->name.data + mp4_context->root));
    } else {
        char *name = strrchr((const char *) mp4_context->file->name.data, '/') + 1;
        filename = (char *) ngx_palloc(mp4_context->r->pool, name - (const char *) mp4_context->file->name.data);
        strcpy(filename, (const char *) name);
    }
    char *ext = strrchr(filename, '.');
    *ext = 0;
    // get video width, height
    if (!moov_build_index(mp4_context, mp4_context->moov)) return 0;
    moov_t const *moov = mp4_context->moov;
    if (!options->adbr && !options->org) {
        APPEND_TO_M3U8("#EXT-X-ALLOW-CACHE:NO\n");
        if (width >= 1920) {
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("adbr/360p/%s.m3u8%s\n", filename, extra);
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3120000,RESOLUTION=854x480,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("adbr/480p/%s.m3u8%s\n", filename, extra);
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5120000,RESOLUTION=1280x720,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("adbr/720p/%s.m3u8%s\n", filename, extra);
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=7680000,RESOLUTION=1920x1080,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("org/%s.m3u8%s\n", filename, extra);

        } else if (width >= 1280) {
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("adbr/360p/%s.m3u8%s\n", filename, extra);
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3120000,RESOLUTION=854x480,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("adbr/480p/%s.m3u8%s\n", filename, extra);
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5120000,RESOLUTION=1280x720,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("org/%s.m3u8%s\n", filename, extra);

        } else if (width >= 854) {
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("adbr/360p/%s.m3u8%s\n", filename);
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3120000,RESOLUTION=854x480,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("org/%s.m3u8%s\n", filename, extra);
        } else {
            APPEND_TO_M3U8("#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            APPEND_TO_M3U8("org/%s.m3u8%s\n", filename, extra);
        }
        result = 1;
    } else {
        // http://developer.apple.com/library/ios/#technotes/tn2288/_index.html
        //    if (!options->fragment_track_id) {
        //        unsigned int audio_tracks;
        //        unsigned int track_id;
        //        audio_tracks = 0;
        //        track_id = 0;
        //        for (track_id = 0; track_id < moov->tracks_; ++track_id) {
        //            if (moov->traks_[track_id]->mdia_->hdlr_->handler_type_ == FOURCC('s', 'o', 'u', 'n')) ++audio_tracks;
        //        }
        //        if (audio_tracks > 1) {
        //            unsigned int i = 0;
        //            for (track_id = 0; track_id < moov->tracks_; ++track_id) {
        //                if (moov->traks_[track_id]->mdia_->hdlr_->handler_type_ == FOURCC('s', 'o', 'u', 'n')) {
        //                    p = ngx_sprintf(p, "#EXT-X-MEDIA:TYPE=VIDEO,GROUP-ID=\"track\",NAME=\"%d\",DEFAULT=%s,URI=\"%s?audio=%d%s\"\n",
        //                            track_id, i ? "YES" : "NO",
        //                            filename, track_id, extra);
        //                    ++i;
        //                }
        //            }
        //            unsigned int bitrate = ((trak_bitrate(moov->traks_[0]) + 999) / 1000) * 1000;
        //
        //            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,VIDEO=\"track\",BANDWIDTH=%ud\n", bitrate);
        //            p = ngx_sprintf(p, "%s?audio=%d%s\n", filename, 1, extra);
        //        }
        //    }



        /*
         remove query string as we rewrite it later
         */
        /* if proxy address is provided, use it
         * can be use proxy address to point request to redirect (CDN) server
         */
        if (conf->hls_proxy.data != NULL) {
            ngx_http_core_loc_conf_t *clcf;
            clcf = ngx_http_get_module_loc_conf(mp4_context->r, ngx_http_core_module);
            strcpy(rewrite, "http://");
            strcat(rewrite, (const char *) conf->hls_proxy.data);

            if (ngx_strlen((char *) clcf->name.data) > 1) {
                strcat(rewrite, (const char *) clcf->name.data);
            }
        }
        if (options->adbr) {
            replace(extra, sizeof (extra), "adbr=true&", "");
            if (rewrite) {
                strcat(rewrite, (const char*) "/adbr");
            }
        } else {
            replace(extra, sizeof (extra), "org=true", "");
            if (rewrite) {
                strcat(rewrite, (const char*) "/org");
            }
        }

        switch (options->video_resolution) {

            case R_360P:
                replace(extra, sizeof (extra), "vr=360p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/360p");
                break;
            case R_480P:
                replace(extra, sizeof (extra), "vr=480p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/480p");
                //rewrite = "adbr/480p/";
                break;
            case R_720P:
                replace(extra, sizeof (extra), "vr=720p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/720p");
                break;
        }
        /* use ngx_strstr instead of strstr */
        while (ngx_strstr(extra, "?&")) {
            replace(extra, sizeof (extra), "?&", "?");
        }
        if (ngx_strlen(extra) == 1) {
            extra[0] = '\0';
        }
        trak_t const *trak = moov->traks_[0];
        samples_t *cur = trak->samples_;
        samples_t *prev = cur;
        samples_t *last = trak->samples_ + trak->samples_size_ + 1;
        APPEND_TO_M3U8("#EXT-X-TARGETDURATION:%ud\n", conf->length + 3);
        APPEND_TO_M3U8("#EXT-X-MEDIA-SEQUENCE:0\n");
        APPEND_TO_M3U8("#EXT-X-VERSION:4\n");
        //        p = ngx_sprintf(p, "#EXT-X-VERSION:3\n");
        uint32_t i = 0, prev_i = 0;
        while (cur != last) {
            if (!cur->is_smooth_ss_) {
                ++cur;
                continue;
            }
            if (prev != cur) {
                float duration = (float) ((cur->pts_ - prev->pts_) / (float) trak->mdia_->mdhd_->timescale_) + 0.0005;
                if (duration >= (float) conf->length || cur + 1 == last) {
                    APPEND_TO_M3U8("#EXTINF:%.3f,\n", duration);
                    if (conf->hls_proxy.data != NULL) {
                        APPEND_TO_M3U8("%s/%uD/%s.ts%s\n", rewrite, prev_i, filename, extra);
                    } else {
                        APPEND_TO_M3U8("%uD/%s.ts%s\n", prev_i, filename, extra);
                    }
                    prev = cur;
                    prev_i = i;
                    ++result;
                }
            }
            ++i;
            ++cur;
        }
        APPEND_TO_M3U8("#EXT-X-ENDLIST\n");
    }
    bucket_insert(bucket, buffer, p - buffer);
#undef APPEND_TO_M3U8
    /*
    if (buffer) {
        ngx_pfree(mp4_context->r->pool, buffer);
    }
    if (filename != NULL) {
        ngx_pfree(mp4_context->r->pool, filename);
    }
    if (rewrite != NULL) {
        ngx_pfree(mp4_context->r->pool, rewrite);
    }
     */
    return result;
}

// End Of File
