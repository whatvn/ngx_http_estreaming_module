/* 
 * File:   ngx_http_adaptive_streaming.h
 * Author:  - ffmpeg developers
 *          - Hung Nguyen
 *
 * Created on January 23, 2015, 11:33 AM
 */

#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/old_pix_fmts.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>


#define DIR_SEPARATOR '/'
#define NGX_HTTP_STREAMING_MODULE_VIDEO_SEGMENT_NOT_FOUND 3
#define NGX_HTTP_STREAMING_MODULE_STREAM_NOT_FOUND 4
#define NGX_HTTP_STREAMING_MODULE_NO_DECODER 5
#define NGX_STREAMING_CHUNK_MAX_SIZE 192200*2

typedef struct {
    ngx_shm_zone_t *cache;
    ngx_http_complex_value_t cache_key;
    ngx_uint_t cache_min_uses;
    ngx_array_t *cache_valid;
    ngx_path_t *temp_path;
    size_t big_file_size;
} adaptive_cache;

typedef struct {
    unsigned char *data;
    int len;
    ngx_pool_t *pool;
} video_buffer;

typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;

uint64_t flatten_chain(ngx_chain_t *out, ngx_pool_t *pool, u_char **buf) {
    off_t bsize;
    ngx_chain_t *out_ptr;
    u_char *ret_ptr;
    uint64_t flattenSize = 0;
    //    u_char *temp;

    out_ptr = out;
    while (out_ptr) {
        if (!out_ptr->buf->in_file) {
            bsize = ngx_buf_size(out_ptr->buf);
            flattenSize += bsize;
        }
        out_ptr = out_ptr->next;
    }
    *buf = ngx_palloc(pool, flattenSize);
    ret_ptr = *buf;
    out_ptr = out;
    while (out_ptr) {
        bsize = ngx_buf_size(out_ptr->buf);
        if (!out_ptr->buf->in_file) {
            ngx_memcpy(ret_ptr, out_ptr->buf->pos, (size_t) bsize);
            ret_ptr += bsize;
        }
        out_ptr = out_ptr->next;
    }
    return flattenSize;
}

/* read callback function for IOContext*/
static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    video_buffer *bd = (video_buffer *) opaque;
    buf_size = FFMIN(buf_size, bd->len);
    //    printf("ptr:%p size:%d\n", bd->data, bd->len);
    ngx_memcpy(buf, bd->data, buf_size);
    bd->data += buf_size;
    //    bd->ptr += buf_size;
    bd->len -= buf_size;
    return buf_size;
}

static int open_input_file(ngx_pool_t *pool, ngx_chain_t *chain, int width,
        int height, AVFormatContext *ifmt_ctx, AVIOContext **io_read_context) { /* Reserve height for future */
    int ret;
    unsigned int i;
    /* move input format to local scope*/
    AVInputFormat *infmt;
    video_buffer *source;
    AVDictionary *vdec_opt = NULL;
    unsigned char *exchange_area_read = NULL;
    u_char *buf = NULL;
    source = ngx_pcalloc(pool, sizeof (video_buffer));
    source->data = NULL;
    source->len = 0;
    source->pool = pool;
    size_t _size = 4096;
    exchange_area_read = (unsigned char *) av_mallocz(_size * sizeof(unsigned char));
    size_t chain_size = flatten_chain(chain, pool, &buf);
    source->data = buf;
    source->len = chain_size;
    *io_read_context = avio_alloc_context(exchange_area_read, _size, 0, (void *) source, read_packet, NULL, NULL);
    infmt = av_find_input_format("mpegts");
    ifmt_ctx->pb = *io_read_context;
    if ((ret = avformat_open_input(&ifmt_ctx, "filename.ts", infmt, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    int dec_ = 0;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        codec_ctx->delay = 5;
        codec_ctx->thread_count = 0;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
                || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                av_dict_set(&vdec_opt, "vprofile", "baseline", 0);
            }
            ret = avcodec_open2(codec_ctx,
                    avcodec_find_decoder(codec_ctx->codec_id), &vdec_opt);

            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }

            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO && dec_ == 0) {
                /* setting to test if it gains any performance hit*/
                if (codec_ctx->codec->capabilities & CODEC_CAP_TRUNCATED)
                    ifmt_ctx->video_codec->capabilities |= CODEC_CAP_TRUNCATED;
                int video_width = codec_ctx->width;
                av_log(NULL, AV_LOG_DEBUG, "source video w:%d, request w:%d\n", video_width, width);
                if (video_width <= width) return -1;
                dec_ = 1;
            }
        }
    }
    if (vdec_opt) {
        av_dict_free(&vdec_opt);
    }
    av_dump_format(ifmt_ctx, 0, "filename", 0);
    /*
    if (io_read_context->buffer) av_freep(&io_read_context->buffer);
    if (io_read_context) av_freep(&io_read_context);
     */
    //if (buf) ngx_pfree(pool, buf);
    if (source) ngx_pfree(pool, source);
    return 0;
}

static int write_adbr_packet(void *opaque, unsigned char *buf, int buf_size) {
    int old_size;
    video_buffer *destination = (video_buffer *) opaque;

    if (destination->data == NULL) {
        destination->data = ngx_pcalloc(destination->pool, NGX_STREAMING_CHUNK_MAX_SIZE * sizeof (*destination));
    }
    old_size = destination->len;
    destination->len += buf_size;
    ngx_memmove(destination->data + old_size, buf, buf_size * sizeof (unsigned char));
    return buf_size;
}

static int prepare_output_encoder(ngx_http_request_t *req, video_buffer *destination,
        AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, int width, int height,
        AVIOContext **io_context) {
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    //    AVIOContext *io_context;
    int ret;
    unsigned int i;
    int buffer_size;
    AVDictionary *option = NULL;
    AVDictionary *format_option = NULL;
    unsigned char *exchange_area_write;
    buffer_size = 1024;
    exchange_area_write = (unsigned char *) av_mallocz(buffer_size * sizeof (unsigned char));
    *io_context = avio_alloc_context(exchange_area_write, buffer_size, 1, (void *) destination, NULL, write_adbr_packet, NULL);
    ofmt_ctx->pb = *io_context;
    ofmt_ctx->oformat = av_guess_format("mpegts", NULL, NULL);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    /* setting providers
     * and some metadata
     */
    av_dict_set(&ofmt_ctx->metadata, "service_name", "nginx http estreaming module ", 0);
    av_dict_set(&ofmt_ctx->metadata, "service_provider", "hls chunk produced by nginx_http_estreaming_module - version 0.1", 0);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }
        in_stream = ifmt_ctx->streams[i];
        dec_ctx = in_stream->codec;
        enc_ctx = out_stream->codec;
        out_stream->duration = in_stream->duration;
        out_stream->metadata = in_stream->metadata;
        out_stream->start_time = in_stream->start_time;
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            enc_ctx->width = width;
            enc_ctx->height = height;
            out_stream->avg_frame_rate = in_stream->avg_frame_rate;
            out_stream->r_frame_rate = in_stream->r_frame_rate;
            enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
            enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
            out_stream->time_base = in_stream->time_base;
            enc_ctx->has_b_frames = dec_ctx->has_b_frames;
            enc_ctx->frame_number = dec_ctx->frame_number;
            enc_ctx->skip_frame = AVDISCARD_NONE;
            if (width == 640) {
                enc_ctx->bit_rate = 1000 * 1000;
                enc_ctx->qmin = 30;
                enc_ctx->qmax = 100;
            } else if (width == 854) {
                enc_ctx->bit_rate = 2000 * 1000;
                enc_ctx->qmin = 25;
                enc_ctx->qmax = 90;
            } else {
                enc_ctx->bit_rate = 3000 * 1000;
                enc_ctx->qmin = 20;
                enc_ctx->qmax = 80;
            }
            enc_ctx->bit_rate_tolerance = 0;
            enc_ctx->rc_max_rate = 0;
            enc_ctx->rc_buffer_size = 0;
            enc_ctx->gop_size = 25;
            enc_ctx->keyint_min = 50;
            enc_ctx->max_b_frames = 0;
            enc_ctx->b_frame_strategy = 1;
            enc_ctx->coder_type = 0;
            enc_ctx->me_cmp = 1;
            enc_ctx->me_range = 16;
            enc_ctx->scenechange_threshold = 0;
            //            if ((LIBAVCODEC_VERSION_MAJOR <= 56) || !(LIBAVCODEC_VERSION_MINOR <= 8)) {
            //                enc_ctx->me_method = ME_ITER;
            //            }
            enc_ctx->me_subpel_quality = 4;
            enc_ctx->i_quant_factor = 1;
            enc_ctx->qcompress = 0;
            enc_ctx->max_qdiff = 4;
            // license features
            enc_ctx->thread_count = 0;
            enc_ctx->flags |= CODEC_FLAG_LOOP_FILTER;
            if (av_dict_set(&option, "vsync", "0", 0) < 0)
                av_log(NULL, AV_LOG_ERROR, "cannot set vsync option\n");
            if (av_dict_set(&option, "mpegts_copyts", "1", 0) < 0)
                av_log(NULL, AV_LOG_ERROR, "cannot set mpegts_copyts option\n");
            av_dict_set(&option, "x264opts", "bframes=16:keyint=50:min-keyint=50:no-scenecut", 0);
            av_dict_set(&option, "preset", "medium", 0);
            av_dict_set(&option, "r", "24", 0);
            av_dict_set(&option, "vprofile", "baseline", 0);
            av_dict_set(&option, "level", "3.0", 0);
            av_dict_set(&option, "tune", "zerolatency", 0);
            avcodec_open2(enc_ctx, encoder, &option);
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
            av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            /* if this stream must be remuxed */
            ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
                    ifmt_ctx->streams[i]->codec);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
                return ret;
            }
        }
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    /* init muxer, write output file header */
    av_dict_set(&format_option, "mpegts_copyts", "1", 0);
    av_dict_set(&format_option, "copy_ts", "1", 0);
    av_dict_set(&format_option, "vsync", "0", 0);
    ret = avformat_write_header(ofmt_ctx, &format_option);
    av_dict_free(&format_option);
    av_dict_free(&option);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    printf("Prepare output encoder ok\n");
    return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
        AVCodecContext *enc_ctx, const char *filter_spec) {
    char args[512];
    int ret = 0;
    AVFilter *buffersrc = NULL;
    AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    av_log(NULL, AV_LOG_DEBUG, "Filter frame\n");
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        snprintf(args, sizeof (args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                //                dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                dec_ctx->width, dec_ctx->height, AV_PIX_FMT_YUV420P,
                dec_ctx->time_base.num, dec_ctx->time_base.den,
                dec_ctx->sample_aspect_ratio.num,
                dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                (uint8_t*) & enc_ctx->pix_fmt, sizeof (enc_ctx->pix_fmt),
                AV_OPT_SEARCH_CHILDREN);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }

    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if (!dec_ctx->channel_layout)
            dec_ctx->channel_layout =
                av_get_default_channel_layout(dec_ctx->channels);
        snprintf(args, sizeof (args),
                "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
                dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
                av_get_sample_fmt_name(dec_ctx->sample_fmt),
                dec_ctx->channel_layout);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                (uint8_t*) & enc_ctx->sample_fmt, sizeof (enc_ctx->sample_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                (uint8_t*) & enc_ctx->channel_layout,
                sizeof (enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                (uint8_t*) & enc_ctx->sample_rate, sizeof (enc_ctx->sample_rate),
                AV_OPT_SEARCH_CHILDREN);

        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }

    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
            &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    /*
    printf("filter video ok\n");
    */
    return ret;
}

static int init_filters(int width, int height, AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, FilteringContext *filter_ctx) {

    //    const char *filter_spec;
    char filter_spec[512];
    unsigned int i;
    int ret;
    //    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof (*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph = NULL;
        if (!ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            continue;
        } else {
            snprintf(filter_spec, sizeof (filter_spec),
                    "scale=iw*min(%d/iw\\,%d/ih):ih*min(%d/iw\\,%d/ih)"
                    ", pad=%d:%d:(%d-iw*min(%d/iw\\,"
                    "%d/ih))/2:(%d-ih*min(%d/iw\\,%d/ih))/2",
                    width, height, width, height, width, height, width, width, height,
                    height, width, height);
            ret = init_filter(&filter_ctx[i], ifmt_ctx->streams[i]->codec,
                    ofmt_ctx->streams[i]->codec, filter_spec);
            if (ret)
                return ret;
        }
    }
    printf("init filter ok\n");
    return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame, AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx) {
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;

    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
            (ifmt_ctx->streams[stream_index]->codec->codec_type ==
            AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

    if (!got_frame)
        got_frame = &got_frame_local;

    av_log(NULL, AV_LOG_DEBUG, "Encoding frame\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);

    ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
            filt_frame, got_frame);
    av_frame_free(&filt_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing
     * the most important thing when muxing mpegts stream is:
     * never change or optimize timestamp
     */
    enc_pkt.stream_index = stream_index;
    /*
    if (enc_pkt.pts != AV_NOPTS_VALUE) {
        av_packet_rescale_ts(&enc_pkt,
                ifmt_ctx->streams[stream_index]->time_base,
                ofmt_ctx->streams[stream_index]->time_base);
    }
     */
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index,
        AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, FilteringContext *filter_ctx) {
    int ret;
    AVFrame *filt_frame;
    //av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
            frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }
    /* pull filtered frames from the filtergraph */
    while (1) {
        filt_frame = av_frame_alloc();
        if (!filt_frame) {
            ret = AVERROR(ENOMEM);
            break;
        }
        av_log(NULL, AV_LOG_DEBUG, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
                filt_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }
        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(filt_frame, stream_index, NULL, ifmt_ctx, ofmt_ctx);
        if (ret < 0)
            break;
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index, AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx) {
    int ret;
    int got_frame;

    if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
            CODEC_CAP_DELAY))
        return 0;

    while (1) {
        //av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame(NULL, stream_index, &got_frame, ifmt_ctx, ofmt_ctx);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

static int flush_decoder(unsigned int stream_index, AVFormatContext *ifmt_ctx) {
    int ret;
    int got_frame;
    AVFrame *frame = NULL;
    AVPacket packet = {.data = NULL, .size = 0};
    /*
        if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
                CODEC_CAP_DELAY))
            return 0;
     */
    while (1) {
        frame = av_frame_alloc();
        ret = avcodec_decode_video2(ifmt_ctx->streams[stream_index]->codec, frame,
                &got_frame, &packet);
        av_frame_free(&frame);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u decoder\n", stream_index);
    }
    return ret;
}

int ngx_estreaming_adaptive_bitrate(ngx_http_request_t *req, ngx_chain_t *chain,
        video_buffer *destination, mp4_split_options_t *options) {
    int ret = 0;
    AVPacket packet = {.data = NULL, .size = 0};
    AVFrame *frame = NULL;
    /* move global var into local scope */
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    FilteringContext *filter_ctx = NULL;
    AVIOContext *io_read_context = NULL;
    AVIOContext *io_write_context = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    unsigned int skipped = 0;
    int got_frame;
    int height = 360;
    int width = 640;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    // setup video resolution
    if (options->video_resolution == 3) {
        // 720p
        width = 1280;
        height = 720;
    } else if (options->video_resolution == 2) {
        // 480p
        width = 854;
        height = 480;
    }
    /* allocate memory for input format context*/
    ifmt_ctx = avformat_alloc_context();
    if ((ret = open_input_file(req->pool, chain, width,
            height, ifmt_ctx, &io_read_context)) < 0) {
        goto end;
    }

    /* allocate memory for output context */
    ofmt_ctx = avformat_alloc_context();
    if ((ret = prepare_output_encoder(req, destination, ifmt_ctx, ofmt_ctx, width, height, &io_write_context)) < 0)
        goto end;
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof (*filter_ctx));
    if ((ret = init_filters(width, height, ifmt_ctx, ofmt_ctx, filter_ctx)) < 0)
        goto end;
    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO) {
            if (filter_ctx[stream_index].filter_graph) {
                frame = av_frame_alloc();
                if (!frame) {
                    ret = AVERROR(ENOMEM);
                    break;
                }
                dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
                        avcodec_decode_audio4;
                ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
                        &got_frame, &packet);
                if (ret < 0) {
                    av_frame_free(&frame);
                    av_log(NULL, AV_LOG_ERROR, "Error occurred: No: %d, %s\n", ret, av_err2str(ret));
                    av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                    break;
                }
                if (got_frame) {
                    frame->pts = av_frame_get_best_effort_timestamp(frame);
                    ret = filter_encode_write_frame(frame, stream_index, ifmt_ctx, ofmt_ctx, filter_ctx);
                    av_frame_free(&frame);
                    if (ret < 0)
                        goto end;
                } else {
                    ++skipped;
                    av_frame_free(&frame);
                }
            }
        } else {
            /* remux this frame without reencoding */
            //            av_packet_rescale_ts(&packet,
            //                    ifmt_ctx->streams[stream_index]->time_base,
            //                    ofmt_ctx->streams[stream_index]->time_base);
            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
            if (ret < 0)
                goto end;
        }
        av_free_packet(&packet);
    }
    // decode and encode delay frame
    for (i = skipped; i > 0; i--) {
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO) {
            frame = av_frame_alloc();
            ret = avcodec_decode_video2(ifmt_ctx->streams[stream_index]->codec
                    , frame, &got_frame, &packet);
            if (got_frame) {
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                ret = filter_encode_write_frame(frame, stream_index, ifmt_ctx, ofmt_ctx, filter_ctx);
                av_frame_free(&frame);
                if (ret < 0) {
                    goto end;
                }
            }
        }
    }
    /* flush filters and encoders/decoders */
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        /* flush filter */
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i, ifmt_ctx, ofmt_ctx, filter_ctx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }
        /* flush encoder */
        /* we do not encode audio frame so just flush video encoder*/
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            ret = flush_decoder(i, ifmt_ctx);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Flushing decoder failed\n");
                goto end;
            }
            ret = flush_encoder(i, ifmt_ctx, ofmt_ctx);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
                goto end;
            }
        }
    }
    av_write_trailer(ofmt_ctx);
end:
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec) {
        avcodec_close(ofmt_ctx->streams[i]->codec);
        //        }
        avcodec_close(ifmt_ctx->streams[i]->codec);
        if (filter_ctx && filter_ctx[i].filter_graph) {
            avfilter_graph_free(&filter_ctx[i].filter_graph);
        }
    }
    if (filter_ctx) av_free(filter_ctx);
    if (io_read_context) av_free(io_read_context);
    if (ifmt_ctx) avformat_close_input(&ifmt_ctx);
    /*
    if (chain_memory) {
        ngx_pfree(req->pool, chain_memory);
    }
    if (exchange_area_write) {
        ngx_pfree(req->pool, exchange_area_write);
    }
    av_freep(&ofmt_ctx->pb->buffer);
    if (ofmt_ctx && ofmt_ctx->pb && ofmt_ctx->nb_streams > 0) av_freep(&ofmt_ctx->pb);
    */
    if (io_write_context) av_free(io_write_context);
    if (ofmt_ctx && ofmt_ctx->nb_streams > 0) avformat_free_context(ofmt_ctx);
    av_free_packet(&packet);
    av_frame_free(&frame);

    return ret ? 1 : 0;
}
