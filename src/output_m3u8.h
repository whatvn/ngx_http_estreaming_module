
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

char *replace_str(char *str, char *old, char *new) {
    char *ret, *r;
    char *p, *q;
    /* use ngx_strlen instead of strlen*/
    size_t oldlen = ngx_strlen(old);
    size_t count, retlen, newlen = ngx_strlen(new);

    if (oldlen != newlen) {
        /* use ngx_strstr instead of strstr*/
        for (count = 0, p = str; (q = ngx_strstr(p, old)) != NULL; p = q + oldlen)
            count++;
        /* this is undefined if p - str > PTRDIFF_MAX */
        retlen = p - str + ngx_strlen(p) + count * (newlen - oldlen);
    } else
        retlen = ngx_strlen(str);

    if ((ret = malloc(retlen + 1)) == NULL)
        return NULL;

    /* use ngx_strstr instead of strstr*/
    for (r = ret, p = str; (q = ngx_strstr(p, old)) != NULL; p = q + oldlen) {
        /* this is undefined if q - p > PTRDIFF_MAX */
        ptrdiff_t l = q - p;
        ngx_memcpy(r, p, l);
        r += l;
        /* use ngx_memcpy instead of memcpy */
        ngx_memcpy(r, new, newlen);
        r += newlen;
    }
    strcpy(r, p);

    return ret;
}

int mp4_create_m3u8(struct mp4_context_t *mp4_context, struct bucket_t * bucket,
        struct mp4_split_options_t *options, int width) {
    hls_conf_t *conf = ngx_http_get_module_loc_conf(mp4_context->r, ngx_http_estreaming_module);
    int result = 0;
    u_char *buffer = (u_char *) ngx_palloc(mp4_context->r->pool, 1024 * 256);
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
    char *rewrite;
    p = ngx_sprintf(p, "#EXTM3U\n");
    if (conf->hls_proxy.data == NULL) {
        rewrite = (char *) ngx_palloc(mp4_context->r->pool,
                ngx_strlen(mp4_context->file->name.data) +
                ngx_strlen(mp4_context->r->headers_in.server.data) - mp4_context->root + 7);
    } else {
        rewrite = (char *) ngx_palloc(mp4_context->r->pool,
                ngx_strlen(conf->hls_proxy.data) + ngx_strlen(mp4_context->file->name.data) +
                ngx_strlen(mp4_context->r->headers_in.server.data) - mp4_context->root + 7);
    }
    char *filename;
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
//        printf("Request for master playlist\n");
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
            p = ngx_sprintf(p, "org/%s.m3u8%s\n", filename,extra);

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
        printf("Request for child playlist, resolution: %d\n", options->video_resolution);
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
        char *query_string = "";
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
            if (options->adbr) {
                query_string = replace_str(extra, "adbr=true&", "");
                strcat(rewrite, "/adbr");
            } else {
                query_string = replace_str(extra, "org=true", "");
                strcat(rewrite, "/org");
            }
        }
        
        switch (options->video_resolution) {
            case R_360P:
                query_string = replace_str(query_string, "vr=360p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/360p");
                break;
            case R_480P:
                query_string = replace_str(query_string, "vr=480p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/480p");
                //rewrite = "adbr/480p/";
                break;
            case R_720P:
                query_string = replace_str(query_string, "vr=720p", "");
                if (conf->hls_proxy.data != NULL) strcat(rewrite, "/720p");
                break;
        }
        /* use ngx_strstr instead of strstr */
        while (ngx_strstr(query_string, "?&")) {
            query_string = replace_str(query_string, "?&", "?");
        }
        if (ngx_strlen(query_string) == 1) {
            query_string = "";
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
                if (duration >=  (float) conf->length || cur + 1 == last) {
                    p = ngx_sprintf(p, "#EXTINF:%.3f,\n", duration);
                    if (conf->hls_proxy.data != NULL) {
                        p = ngx_sprintf(p, "%s/%uD/%s.ts%s\n", rewrite, prev_i, filename, query_string);
                    } else {
                        p = ngx_sprintf(p, "%uD/%s.ts%s\n", prev_i, filename, query_string);
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
    ngx_pfree(mp4_context->r->pool, buffer);
    ngx_pfree(mp4_context->r->pool, filename);
    if (rewrite) ngx_pfree(mp4_context->r->pool, rewrite);
    return result;
}

// End Of File
