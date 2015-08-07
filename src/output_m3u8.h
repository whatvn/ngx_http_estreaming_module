
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

void replace(char * o_string, char * s_string, char * r_string) {
    //a buffer variable to do all replace things
    char buffer[20];
    //to store the pointer returned from strstr
    char * ch;

    //first exit condition
    if (!(ch = strstr(o_string, s_string)))
        return;

    //copy all the content to buffer before the first occurrence of the search string
    strncpy(buffer, o_string, ch - o_string);

    //prepare the buffer for appending by adding a null to the end of it
    buffer[ch - o_string] = 0;

    //append using sprintf function
    sprintf(buffer + (ch - o_string), "%s%s", r_string, ch + strlen(s_string));

    //empty o_string for copying
    o_string[0] = 0;
    strcpy(o_string, buffer);
    //pass recursively to replace other occurrences
    return replace(o_string, s_string, r_string);
}

int mp4_create_m3u8(struct mp4_context_t *mp4_context, struct bucket_t * bucket,
        struct mp4_split_options_t *options, int width, ngx_str_t path) {
    hls_conf_t *conf = ngx_http_get_module_loc_conf(mp4_context->r, ngx_http_estreaming_module);
    int result = 0;
    u_char *buffer = NULL;
    buffer = (u_char *) ngx_palloc(mp4_context->r->pool, 1024 * 256);
    u_char *p = buffer;
    char extra[100] = "";
    if (mp4_context->r->args.data) {
        extra[0] = '?';
        strncpy(extra + 1, (const char *) mp4_context->r->args.data, mp4_context->r->args.len < 100 ? mp4_context->r->args.len : 100);
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
    p = ngx_sprintf(p, "#EXTM3U\n");
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
        p = ngx_sprintf(p, "#EXT-X-ALLOW-CACHE:NO\n");
        if (width >= 1920) {
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "adbr/360p/%s.m3u8%s\n", filename, extra);
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3120000,RESOLUTION=854x480,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "adbr/480p/%s.m3u8%s\n", filename, extra);
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5120000,RESOLUTION=1280x720,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "adbr/720p/%s.m3u8%s\n", filename, extra);
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=7680000,RESOLUTION=1920x1080,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "org/%s.m3u8%s\n", filename, extra);

        } else if (width >= 1280) {
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "adbr/360p/%s.m3u8%s\n", filename, extra);
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3120000,RESOLUTION=854x480,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "adbr/480p/%s.m3u8%s\n", filename, extra);
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5120000,RESOLUTION=1280x720,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "org/%s.m3u8%s\n", filename, extra);

        } else if (width >= 854) {
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "adbr/360p/%s.m3u8%s\n", filename);
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3120000,RESOLUTION=854x480,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "org/%s.m3u8%s\n", filename, extra);
        } else {
            p = ngx_sprintf(p, "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1560000,RESOLUTION=640x360,CODECS=\"mp4a.40.2, avc1.4d4015\"\n");
            p = ngx_sprintf(p, "org/%s.m3u8%s\n", filename, extra);
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
            replace(extra, "adbr=true&", "");
            if (rewrite) {
                strcat(rewrite, (const char*) "/adbr");
            }
        } else {
            replace(extra, "org=true", "");
            if (rewrite) {
                strcat(rewrite, (const char*) "/org");
            }
        }

        switch (options->video_resolution) {

            case R_360P:
                replace(extra, "vr=360p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/360p");
                break;
            case R_480P:
                replace(extra, "vr=480p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/480p");
                //rewrite = "adbr/480p/";
                break;
            case R_720P:
                replace(extra, "vr=720p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/720p");
                break;
        }
        /* use ngx_strstr instead of strstr */
        while (ngx_strstr(extra, "?&")) {
            replace(extra, "?&", "?");
        }
        if (ngx_strlen(extra) == 1) {
            extra[0] = '\0';
        }
        trak_t const *trak = moov->traks_[0];
        samples_t *cur = trak->samples_;
        samples_t *prev = cur;
        samples_t *last = trak->samples_ + trak->samples_size_ + 1;
        p = ngx_sprintf(p, "#EXT-X-TARGETDURATION:%ud\n", conf->length + 3);
        p = ngx_sprintf(p, "#EXT-X-MEDIA-SEQUENCE:0\n");
        p = ngx_sprintf(p, "#EXT-X-VERSION:4\n");
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
                    p = ngx_sprintf(p, "#EXTINF:%.3f,\n", duration);
                    if (conf->hls_proxy.data != NULL) {
                        p = ngx_sprintf(p, "%s/%uD/%s.ts%s\n", rewrite, prev_i, filename, extra);
                    } else {
                        p = ngx_sprintf(p, "%uD/%s.ts%s\n", prev_i, filename, extra);
                    }
                    prev = cur;
                    prev_i = i;
                    ++result;
                }
            }
            ++i;
            ++cur;
        }
        p = ngx_sprintf(p, "#EXT-X-ENDLIST\n");
    }
    bucket_insert(bucket, buffer, p - buffer);
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
