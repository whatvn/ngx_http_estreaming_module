/*******************************************************************************
 ngx_http_estreaming - An Nginx module for http adaptive live streaming 
 ******************************************************************************/

#include "ngx_http_estreaming_module.h"
#include "mp4_io.h"
#include "mp4_reader.h"
#include "moov.h"
#include "output_bucket.h"
#include "view_count.h"
#include "output_m3u8.h"
#include "output_ts.h"
#include "ngx_http_adaptive_streaming.h"
#include "mp4_module.h"
#include "ngx_http_mp4_faststart.h"

static void *ngx_http_hls_create_conf(ngx_conf_t * cf) {
    hls_conf_t *conf;
    conf = ngx_pcalloc(cf->pool, sizeof (hls_conf_t));
    if (conf == NULL) {
        return NULL;
    }
    /*
     * set by ngx_pcalloc():
     *
     *     conf->hash = { NULL };
     *     conf->server_names = 0;
     *     conf->keys = NULL;
     */
    conf->length = NGX_CONF_UNSET_UINT;
    conf->relative = NGX_CONF_UNSET;
    conf->buffer_size = NGX_CONF_UNSET_SIZE;
    conf->max_buffer_size = NGX_CONF_UNSET_SIZE;
    conf->hls_proxy.data = NULL;
    conf->hls_proxy.len = 0;
    conf->mp4_buffer_size = NGX_CONF_UNSET_SIZE;
    conf->mp4_max_buffer_size = NGX_CONF_UNSET_SIZE;
    return conf;
}

static ngx_int_t ngx_http_hls_initialization() {
    av_register_all();
    avfilter_register_all();
    av_log_set_level(AV_LOG_ERROR);
    return NGX_OK;
}

static char *ngx_http_hls_merge_conf(ngx_conf_t *cf, void *parent, void *child) {
    hls_conf_t *prev = parent;
    hls_conf_t *conf = child;
    ngx_conf_merge_uint_value(conf->length, prev->length, 8);
    ngx_conf_merge_value(conf->relative, prev->relative, 1);
    ngx_conf_merge_size_value(conf->buffer_size, prev->buffer_size, 512 * 1024);
    ngx_conf_merge_size_value(conf->max_buffer_size, prev->max_buffer_size,
            10 * 1024 * 1024);
    ngx_conf_merge_str_value(conf->hls_proxy, prev->hls_proxy, NULL);
    // merge mp4 module
    ngx_conf_merge_size_value(conf->mp4_buffer_size, prev->mp4_buffer_size, 512 * 1024);
    ngx_conf_merge_size_value(conf->mp4_max_buffer_size, prev->mp4_max_buffer_size,
            10 * 1024 * 1024);
    ngx_conf_merge_off_value(conf->mp4_enhance, prev->mp4_enhance, 0);

    if (conf->length < 1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "video length must be equal or more than 1");
        return NGX_CONF_ERROR;
    }
    return NGX_CONF_OK;
}

static ngx_int_t ngx_estreaming_handler(ngx_http_request_t * r) {
    size_t root;
    ngx_int_t rc;
    ngx_uint_t level;
    ngx_str_t path;
    ngx_open_file_info_t of;
    ngx_http_core_loc_conf_t *clcf;
    video_buffer *destination;

    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD)))
        return NGX_HTTP_NOT_ALLOWED;

    if (r->uri.data[r->uri.len - 1] == '/')
        return NGX_DECLINED;

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK)
        return rc;

    mp4_split_options_t * options = mp4_split_options_init(r);

    if (!ngx_http_map_uri_to_path(r, &path, &root, 1)) {
        mp4_split_options_exit(r, options);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (r->args.len && !mp4_split_options_set(r, options, (const char *) r->args.data, r->args.len)) {
        mp4_split_options_exit(r, options);
        return NGX_DECLINED;
    }
    if (!options) return NGX_DECLINED;
    ngx_log_t * nlog = r->connection->log;
    struct bucket_t * bucket = bucket_init(r);
    int result = 0;
    u_int m3u8 = 0, len_ = 0;
    int64_t duration = 0;
    if (ngx_memcmp(r->exten.data, "mp4", r->exten.len) == 0) {
        return ngx_http_mp4_handler(r);
    } else if (ngx_memcmp(r->exten.data, "m3u8", r->exten.len) == 0) {
        m3u8 = 1;
    } else if (ngx_memcmp(r->exten.data, "len", r->exten.len) == 0) {// this is for length request
        len_ = 1;
    } else if (ngx_memcmp(r->exten.data, "ts", r->exten.len) == 0) {
        // don't do anything 
    } else {
        return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
    }
    // change file name to mp4
    // in order to lookup file in filesystem
    char *ext = strrchr((const char *) path.data, '.');
    strcpy(ext, ".mp4");
    path.len = ((u_char *) ext - path.data) + 4;
    path.data[path.len] = '\0';
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    ngx_memzero(&of, sizeof (ngx_open_file_info_t));
    of.read_ahead = clcf->read_ahead;
    of.directio = NGX_MAX_OFF_T_VALUE;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;
    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool) != NGX_OK) {
        mp4_split_options_exit(r, options);
        switch (of.err) {
            case 0:
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            case NGX_ENOENT:
            case NGX_ENOTDIR:
            case NGX_ENAMETOOLONG:
                level = NGX_LOG_ERR;
                rc = NGX_HTTP_NOT_FOUND;
                break;
            case NGX_EACCES:
                level = NGX_LOG_ERR;
                rc = NGX_HTTP_FORBIDDEN;
                break;
            default:
                level = NGX_LOG_CRIT;
                rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
                break;
        }
        if (rc != NGX_HTTP_NOT_FOUND || clcf->log_not_found) {
            ngx_log_error(level, nlog, of.err,
                    ngx_open_file_n " \"%s\" failed", path.data);
        }

        return rc;
    }

    if (!of.is_file) {
        mp4_split_options_exit(r, options);
        if (ngx_close_file(of.fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, nlog, ngx_errno,
                    ngx_close_file_n " \"%s\" failed", path.data);
        }
        return NGX_DECLINED;
    }
    /* move atom to beginning of file if it's in the last*/
    hls_conf_t *mlcf;
    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_estreaming_module);
    if (mlcf->mp4_enhance == 1) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, nlog, 0,
                "examine mp4 filename: \"%s\"", &path.data);
        if (ngx_http_enable_fast_start(&path, of.fd, r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    ngx_file_t *file = ngx_pcalloc(r->pool, sizeof (ngx_file_t));
    if (file == NULL) {
        mp4_split_options_exit(r, options);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    file->fd = of.fd;
    file->name = path;
    file->log = nlog;
    mp4_context_t *mp4_context = mp4_open(r, file, of.size, MP4_OPEN_MOOV);
    if (!mp4_context) {
        mp4_split_options_exit(r, options);
        ngx_log_error(NGX_LOG_ALERT, nlog, ngx_errno, "mp4_open failed");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    mp4_context->root = root;
    if (m3u8 || len_) {
        int ret, video_width = 0;
        /*
        only request for master playlist need to use ffmpeg api to open video
        this takes time and cpu, we should avoid it as much as possible 
         */
        if ((m3u8) && (options->adbr || options->org)) goto no_ffmpeg;
        AVFormatContext *fmt_ctx = NULL;
        unsigned int i;
//        av_register_all();
        if ((ret = avformat_open_input(&fmt_ctx, (const char*) path.data, NULL, NULL)) < 0) {
            mp4_split_options_exit(r, options);
            mp4_close(mp4_context);
            if (fmt_ctx) avformat_close_input(&fmt_ctx);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
            if (fmt_ctx) avformat_close_input(&fmt_ctx);
            mp4_close(mp4_context);
            mp4_split_options_exit(r, options);
            av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        if (len_) {
            duration = fmt_ctx->duration;
            u_char *buffer = (u_char *) ngx_palloc(mp4_context->r->pool, 10 * sizeof (char));
            u_char * p = buffer;
            if (buffer == NULL) {
                mp4_split_options_exit(r, options);
                mp4_close(mp4_context);
                if (fmt_ctx) avformat_close_input(&fmt_ctx);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            // this is stolen from ffmpeg source code
            if (duration != AV_NOPTS_VALUE) {
                duration = duration + 5000;
                int secs;
                secs = duration / AV_TIME_BASE;
                p = ngx_sprintf(p, "%02d\n", secs);
            } else {
                p = ngx_sprintf(p, "N/A\n");
            }
            bucket_insert(bucket, buffer, p - buffer);
            ngx_pfree(mp4_context->r->pool, buffer);
            if (fmt_ctx) avformat_close_input(&fmt_ctx);
            r->allow_ranges = 0;
            result = 1;
            goto response;
        } else {
            for (i = 0; i < fmt_ctx->nb_streams; i++) {
                AVStream *stream;
                AVCodecContext *codec_ctx;
                stream = fmt_ctx->streams[i];
                codec_ctx = stream->codec;
                if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, nlog, 0,
                            "source video w:%d", codec_ctx->width);
                    if (video_width < codec_ctx->width) {
                        video_width = codec_ctx->width;
                    } else
                        break;
                }
            }
            avformat_close_input(&fmt_ctx);
        }
no_ffmpeg:
        if ((result = mp4_create_m3u8(mp4_context, bucket, options, video_width, path))) {
            char action[50];
            sprintf(action, "ios_playlist&segments=%d", result);
            view_count(mp4_context, (char *) path.data, options ? options->hash : NULL, action);
        }
        r->allow_ranges = 0;
    } else {
        result = output_ts(mp4_context, bucket, options);
        if (!options || !result) {
            mp4_close(mp4_context);
            ngx_log_error(NGX_LOG_ALERT, nlog, ngx_errno, "output_ts failed");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        if (options->adbr) {
            destination = ngx_pcalloc(r->pool, sizeof (video_buffer));
            destination->data = NULL;
            destination->len = 0;
            destination->pool = r->pool;
            if (ngx_estreaming_adaptive_bitrate(r, bucket->first,
                    destination, options) == NGX_OK) {
                ngx_buf_t *b = ngx_pcalloc(r->pool, sizeof (ngx_buf_t));
                if (b == NULL) {
                    return NGX_HTTP_INTERNAL_SERVER_ERROR;
                }
                bucket->first->buf = b;
                bucket->first->next = NULL;
                b->pos = destination->data;
                b->last = destination->data + (destination->len * sizeof (unsigned char));
                b->memory = 1;
                b->last_buf = 1;
                bucket->content_length = destination->len;
            }
            ngx_pfree(r->pool, destination);
        }
        char action[50] = "ios_view";
        view_count(mp4_context, (char *) path.data, options->hash, action);
        r->allow_ranges = 1;
    }
response:
    mp4_close(mp4_context);
    mp4_split_options_exit(r, options);
    result = result == 0 ? 415 : 200;
    r->root_tested = !r->error_page;
    if (result && bucket) {
        nlog->action = "sending mp4 to client";
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, nlog, 0, "content_length: %d", bucket->content_length);
        if (bucket->content_length == 0) return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.content_length_n = bucket->content_length;
        r->headers_out.last_modified_time = of.mtime;
        if (m3u8) {
            r->headers_out.content_type.len = sizeof ("application/vnd.apple.mpegurl") - 1;
            r->headers_out.content_type.data = (u_char *) "application/vnd.apple.mpegurl";
        } else if (len_) {
            r->headers_out.content_type.len = sizeof ("text/html") - 1;
            r->headers_out.content_type.data = (u_char *) "text/html";
        } else {
            r->headers_out.content_type.len = sizeof ("video/MP2T") - 1;
            r->headers_out.content_type.data = (u_char *) "video/MP2T";
        }
        rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            ngx_log_error(NGX_LOG_ALERT, nlog, ngx_errno, ngx_close_file_n "ngx_http_send_header failed");
            return rc;
        }
        return ngx_http_output_filter(r, bucket->first);
    } else return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
}

static char *ngx_estreaming(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *clcf =
            ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_estreaming_handler;
    return NGX_CONF_OK;
}
// End Of File

