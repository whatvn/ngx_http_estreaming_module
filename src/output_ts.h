/*******************************************************************************
 output_ts.h - A library for reading and writing Fragmented MPEG4.

 *  * Author:
 *      - codeshop
 *      - Isaev Ivan  
 *      - hungnguyen
 * This use too much libc function, have to convert all to nginx standard function
 * 
 ******************************************************************************/

#define NOPTS_VALUE INT64_C(0x8000000000000000)

#define START_PID       100
#define PMT_PID         0x1000
#define SERVICE_ID      0x0001
#define TS_PACKET_SIZE  188

// for audio, a PES packet header is generated every MAX_PES_HEADER_FREQ packets
#define MAX_PES_HEADER_FREQ 32
#define MAX_PES_PAYLOAD_SIZE ((MAX_PES_HEADER_FREQ - 1) * 184 + 170)

// or when the dts delta is over AUDIO_DELTA
#define AUDIO_DELTA (500 * (90000 / 1000))

// resend PAT/PMT every 100ms
//#define PAT_DELTA (100 * (90000 / 1000))
#define PAT_DELTA (60 * 1000 * (90000 / 1000))

static void write_pts(uint8_t *q, int fourbits, int64_t pts) {
    int val = val = fourbits << 4 | (((pts >> 30) & 0x07) << 1) | 1;
    *q++ = val;
    val = (((pts >> 15) & 0x7fff) << 1) | 1;
    *q++ = val >> 8;
    *q++ = val;
    val = (((pts) & 0x7fff) << 1) | 1;
    *q++ = val >> 8;
    *q++ = val;
}

static u_int convert_to_nal(unsigned char const *first,
        unsigned char const *last,
        unsigned char *dst) {
#if 1
    // check if data is already in nal format. Shouldn't be necessary and this
    // is only a hack for Live Smooth Streaming
    if (read_32(first) == 0x00000001) {
        memcpy(dst, first, last - first);
        return 1;
    }
#endif

    while (first < last) {
        uint32_t packet_len = read_32(first);
        if (packet_len > (uint32_t) (last - first)) return 0;
        first += 4;

        write_32(dst, 0x00000001);
        dst += 4;
        //        uint32_t nal_type = first[0] & 0x1f;
        //        if (nal_type != 9) {
        //            write_32(dst, 0x00000001);
        //            dst += 4;
        //            memcpy(dst, first, packet_len);
        //            dst += packet_len;
        //        }
        memcpy(dst, first, packet_len);
        first += packet_len;
        dst += packet_len;
    }
    return 1;
}

static uint32_t const crc32[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
    0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
    0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
    0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
    0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
    0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
    0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
    0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
    0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
    0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
    0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
    0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
    0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
    0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
    0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
    0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
    0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
    0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
    0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
    0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
    0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
    0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static uint32_t get_crc32(uint32_t crc, const uint8_t *buffer, size_t length) {
    uint8_t const *first = buffer;
    uint8_t const *last = buffer + length;
    while (first != last) {
        crc = (crc << 8) ^ crc32[(crc >> 24) ^ (uint32_t) (*first++)];
    }

    return crc;
}

struct mpegts_muxer_t;

struct mpegts_stream_t {
    struct mpegts_muxer_t *muxer_;
    int is_video_;
    int pid_;
    int cc_;

    int payload_index_;
    uint64_t payload_dts_;
    uint64_t payload_pts_;
    uint8_t payload_[MAX_PES_PAYLOAD_SIZE];

    u_int packets_;
    sample_entry_t const *sample_entry_;
};
typedef struct mpegts_stream_t mpegts_stream_t;

static mpegts_stream_t *mpegts_stream_init(struct mp4_context_t *mp4_context, struct mpegts_muxer_t *muxer,
        int is_video, int pid,
        sample_entry_t const *sample_entry) {
    mpegts_stream_t *mpegts_stream =
            (mpegts_stream_t *) ngx_pcalloc(mp4_context->r->pool, sizeof (mpegts_stream_t));

    mpegts_stream->muxer_ = muxer;
    mpegts_stream->is_video_ = is_video;
    mpegts_stream->pid_ = pid;
    mpegts_stream->cc_ = 0;
    mpegts_stream->payload_index_ = 0;
    mpegts_stream->payload_dts_ = NOPTS_VALUE;
    mpegts_stream->payload_pts_ = NOPTS_VALUE;
    mpegts_stream->packets_ = 0;
    mpegts_stream->sample_entry_ = sample_entry;

    return mpegts_stream;
}

static void mpegts_stream_exit(struct mp4_context_t *mp4_context, mpegts_stream_t *stream) {
    ngx_pfree(mp4_context->r->pool, stream);
}

struct fragment_t {
    trak_t *trak;
    samples_t *first;
    samples_t *last;
    struct mpegts_stream_t *stream;
};
typedef struct fragment_t fragment_t;

struct mpegts_muxer_t {
    bucket_t *bucket_;
    mp4_context_t *mp4_context_;
    int pcr_pid_; // same as video pid
    u_int fragment_size_;
    fragment_t *fragment_;
    uint64_t next_pat_;
    int pat_cc_;
    int pmt_cc_;
};
typedef struct mpegts_muxer_t mpegts_muxer_t;

static mpegts_muxer_t *mpegts_muxer_init(struct mp4_context_t *mp4_context, bucket_t *bucket, fragment_t *fragment, u_int fragment_size) {
    mpegts_muxer_t *mpegts_muxer = (mpegts_muxer_t *) ngx_pcalloc(mp4_context->r->pool, sizeof (mpegts_muxer_t));

    mpegts_muxer->mp4_context_ = mp4_context;
    mpegts_muxer->bucket_ = bucket;
    mpegts_muxer->pcr_pid_ = 0x1fff;
    mpegts_muxer->fragment_size_ = fragment_size;
    mpegts_muxer->fragment_ = fragment;
    mpegts_muxer->next_pat_ = NOPTS_VALUE;
    mpegts_muxer->pat_cc_ = 0;
    mpegts_muxer->pmt_cc_ = 0;

    return mpegts_muxer;
}

static void mpegts_muxer_exit(struct mp4_context_t *mp4_context, mpegts_muxer_t *mpegts_muxer) {
    u_int i;
    for (i = 0; i < mpegts_muxer->fragment_size_; ++i) {
        mpegts_stream_exit(mp4_context, mpegts_muxer->fragment_[i].stream);
    }

    ngx_pfree(mp4_context->r->pool, mpegts_muxer);
}

// Program Association Table lists all the programs available in the transport
// stream.

static void mpegts_muxer_write_pat(mpegts_muxer_t *mpegts_muxer) {
    unsigned char packet[TS_PACKET_SIZE];
    uint8_t *q = packet;
    uint8_t *section_start;
    uint8_t *section_end;

    const int section_payload_len = 4;
    unsigned int crc = -1;
    const int PAT_PID = 0x0000;
    const int pat_table_id = 0x00;
    const int default_transport_stream_id = 0x0001;

    // packet header
    q = write_8(q, 0x47);
    q = write_16(q, 0x4000 | PAT_PID);
#if 0
    q = write_8(q, 0x10 | mpegts_muxer->pat_cc_);
#else
    q = write_8(q, 0x30 | mpegts_muxer->pat_cc_);
    q = write_8(q, 1);
    q = write_8(q, 0x80);
#endif
    mpegts_muxer->pat_cc_ = (mpegts_muxer->pat_cc_ + 1) & 0xf;
    q = write_8(q, 0);

    // section header
    section_start = q;
    q = write_8(q, pat_table_id);
    // 5 byte header + 4 byte CRC
    q = write_16(q, 0xb000 | (section_payload_len + 5 + 4));
    // transport stream id
    q = write_16(q, default_transport_stream_id);

    *q++ = 0xc1;
    *q++ = 0x00;
    *q++ = 0x00;

    // section payload
    q = write_16(q, SERVICE_ID);
    q = write_16(q, 0xe000 | PMT_PID);
    section_end = q;

    // crc
    crc = get_crc32(crc, section_start, section_end - section_start);
    q = write_32(q, crc);
    memset(q, 0xff, packet + TS_PACKET_SIZE - q);

    bucket_insert(mpegts_muxer->bucket_, packet, TS_PACKET_SIZE);
}

// Program Map Tables contain information about programs.

static void mpegts_muxer_write_pmt(mpegts_muxer_t *mpegts_muxer) {
    unsigned char packet[TS_PACKET_SIZE];
    uint8_t *q = packet;
    uint8_t *section_start;
    uint8_t *section_end;
    const int pmt_table_id = 0x02;
    unsigned int crc = -1;

    int section_payload_len = 4;
    section_payload_len += mpegts_muxer->fragment_size_ * 5;

    // packet header
    q = write_8(q, 0x47);
    q = write_16(q, 0x4000 | PMT_PID);
#if 0
    q = write_8(q, 0x10 | mpegts_muxer->pmt_cc_);
#else
    q = write_8(q, 0x30 | mpegts_muxer->pmt_cc_);
    q = write_8(q, 1);
    q = write_8(q, 0x80);
#endif
    mpegts_muxer->pmt_cc_ = (mpegts_muxer->pmt_cc_ + 1) & 0xf;
    q = write_8(q, 0);

    // section header
    section_start = q;
    q = write_8(q, pmt_table_id);
    // 5 byte header + 4 byte CRC
    q = write_16(q, 0xb000 | (section_payload_len + 5 + 4));
    // service identifier
    q = write_16(q, SERVICE_ID);

    *q++ = 0xc1;
    *q++ = 0x00;
    *q++ = 0x00;

    // section payload
    q = write_16(q, 0xe000 | mpegts_muxer->pcr_pid_);
    q = write_16(q, 0xf000);

    u_int i = 0;
    for (; i < mpegts_muxer->fragment_size_; ++i) {
        switch (mpegts_muxer->fragment_[i].trak->mdia_->hdlr_->handler_type_) {
            case FOURCC('s', 'o', 'u', 'n'): q = write_8(q, 0x0f);
                break;
            case FOURCC('v', 'i', 'd', 'e'): q = write_8(q, 0x1b);
                break;
        }
        q = write_16(q, 0xe000 | mpegts_muxer->fragment_[i].stream->pid_);
        q = write_16(q, 0xf000);
    }

    section_end = q;

    // crc
    crc = get_crc32(crc, section_start, section_end - section_start);
    q = write_32(q, crc);
    memset(q, 0xff, packet + TS_PACKET_SIZE - q);

    bucket_insert(mpegts_muxer->bucket_, packet, TS_PACKET_SIZE);
}

static void write_header(mpegts_muxer_t *mpegts_muxer) {
    u_int i = 0;
    for (; i < mpegts_muxer->fragment_size_; ++i) {
        if (mpegts_muxer->fragment_[i].trak->mdia_->hdlr_->handler_type_ == FOURCC('v', 'i', 'd', 'e')) {
            mpegts_muxer->pcr_pid_ = mpegts_muxer->fragment_[i].stream->pid_;
            break;
        }
    }
}

static int packetized_packets(mpegts_stream_t *mpegts_stream,
        uint64_t dts, uint64_t pts,
        unsigned int payload_size) {
    u_int write_discontinuity_indicator = mpegts_stream->packets_ == 0;

    // calculate overhead
    u_int once = 0;

    if (mpegts_stream->pid_ == mpegts_stream->muxer_->pcr_pid_) once += 8;
    else if (write_discontinuity_indicator) once += 2;

    // PES header start code, stream id
    once += 4;
    if (pts != NOPTS_VALUE) {
        once += 5;
        if (dts != NOPTS_VALUE && dts != pts) once += 5;
    }
    // packet length, 0x80, flags, header_len
    once += 5;

    if (payload_size <= 184 - once) return 1;

    return 1 + ((payload_size - (184 - once) + 184 - 1) / 184);
}

static void write_packet(mpegts_stream_t *mpegts_stream,
        bucket_t *bucket, uint64_t dts, uint64_t pts,
        unsigned char const *payload, int payload_size) {
    unsigned char *buf;
    unsigned char *q;

    u_int write_discontinuity_indicator = mpegts_stream->packets_ == 0;
    u_int is_start = 1;
    u_int packets;

    mpegts_muxer_t *mpegts_muxer = mpegts_stream->muxer_;

    //  const int max_delay = 90000 / 25;
    //  const int max_delay = 90000 / 2;
    const int max_delay = 90000;
    if (dts != NOPTS_VALUE) dts += max_delay;
    if (pts != NOPTS_VALUE) pts += max_delay;

    if (mpegts_muxer->next_pat_ == NOPTS_VALUE ||
            (dts != NOPTS_VALUE && dts >= mpegts_muxer->next_pat_)) {
        mpegts_muxer_write_pat(mpegts_muxer);
        mpegts_muxer_write_pmt(mpegts_muxer);
        mpegts_muxer->next_pat_ = dts + PAT_DELTA;
    }

    // reserve the exact number of packets we need for this payload
    packets = packetized_packets(mpegts_stream, dts, pts, payload_size);
    size_t out_size = packets * TS_PACKET_SIZE;
    unsigned char *out_buf = (unsigned char *) malloc(out_size);
    if (out_buf == NULL) return;
    buf = out_buf;

    while (payload_size) {
        int write_pcr = is_start &&
                mpegts_stream->pid_ == mpegts_stream->muxer_->pcr_pid_;
        int val;

        // prepare packet header
        q = buf;

        // sync byte
        *q++ = 0x47;

        val = (mpegts_stream->pid_ >> 8);
        if (is_start) {
            // payload unit start indicator
            val |= 0x40;
        }

        // three one-bit flags
        *q++ = val;

        // 13-bit Packet Identifier
        *q++ = mpegts_stream->pid_;

        // 4-bit continuity counter
        *q++ = 0x10 | mpegts_stream->cc_ |
                ((write_pcr || write_discontinuity_indicator) ? 0x20 : 0);
        mpegts_stream->cc_ = (mpegts_stream->cc_ + 1) & 0xf;

        if (write_pcr) {
            int64_t pcr = dts - max_delay + 1;

            // Adaptation Field Length
            *q++ = 7;

            // Adaptation field contains a PCR field
            *q++ = 0x10;

            // Program clock reference
            *q++ = (unsigned char) (pcr >> 25);
            *q++ = (unsigned char) (pcr >> 17);
            *q++ = (unsigned char) (pcr >> 9);
            *q++ = (unsigned char) (pcr >> 1);
            *q++ = (unsigned char) ((pcr & 1) << 7);
            *q++ = 0;
        } else if (write_discontinuity_indicator) {
            // Adaptation Field Length
            *q++ = 1;
            *q++ = 0x80;
            write_discontinuity_indicator = 0;
        }

        if (write_discontinuity_indicator) {
            buf[5] |= 0x80;
            write_discontinuity_indicator = 0;
        }

        // random access indicator
        if (mpegts_stream->packets_ == 0) buf[5] |= 0x40;

        if (is_start) {
            // Packetized Elementary Stream Headers
            // See: http://www.mpucoder.com/DVD/pes-hdr.html

            int header_len;
            int flags;
            int len;

            // PES header start code
            *q++ = 0x00;
            *q++ = 0x00;
            *q++ = 0x01;

            // stream id
            if (mpegts_stream->is_video_)
                *q++ = 0xe0;
            else
                *q++ = 0xbd; // private_stream_1 (for AAC)

            header_len = 0;
            flags = 0;

            // PTS
            if (pts != NOPTS_VALUE) {
                header_len += 5;
                flags |= 0x80;

                if (dts != NOPTS_VALUE && dts != pts) {
                    // DTS
                    header_len += 5;
                    flags |= 0x40;
                }
            }

            len = payload_size + header_len + 3;
            if (len > 0xffff)
                len = 0;
            // packet length
            *q++ = len >> 8;
            *q++ = len;

            *q++ = 0x80;
            *q++ = flags;
            *q++ = header_len;

            if (flags & 0x80) {
                write_pts(q, flags >> 6, pts);
                q += 5;
            }

            if (flags & 0x40) {
                write_pts(q, 1, dts);
                q += 5;
            }

            is_start = 0;
        }

        {
            // header size
            int header_len = q - buf;

            // data len
            int len = TS_PACKET_SIZE - header_len;
            int stuffing_len = TS_PACKET_SIZE - header_len;
            if (len > payload_size) len = payload_size;

            stuffing_len -= len;

            if (stuffing_len > 0) {
                // add stuffing with AFC
                if (buf[3] & 0x20) {
                    // increase size of AFC
                    int afc_len = buf[4] + 1;
                    memmove(buf + 4 + afc_len + stuffing_len,
                            buf + 4 + afc_len,
                            header_len - (4 + afc_len));
                    buf[4] += stuffing_len;
                    memset(buf + 4 + afc_len, 0xff, stuffing_len);
                } else {
                    // add stuffing
                    memmove(buf + 4 + stuffing_len, buf + 4, header_len - 4);
                    buf[3] |= 0x20;
                    buf[4] = stuffing_len - 1;
                    if (stuffing_len >= 2) {
                        buf[5] = 0x00;
                        memset(buf + 6, 0xff, stuffing_len - 2);
                    }
                }
            }

#if 0
            if (write_discontinuity_indicator && (buf[3] & 0x20)) {
                buf[5] |= 0x80;
                write_discontinuity_indicator = 0;
            }
#endif

            memcpy(buf + TS_PACKET_SIZE - len, payload, len);
            payload += len;
            payload_size -= len;
            ++mpegts_stream->packets_;
        }
        buf += TS_PACKET_SIZE;
    }

    if (buf != (unsigned char const *) out_buf + out_size) {
        printf("write_packet: incorrect number of packets\n");
        //    uint n;
        //    uint chunk_size;
        //    for (n = 0; n < out_size; n += 1024) {
        //        chunk_size = out_size - n > 1024 ? 1024 : out_size - n;
        //        bucket_insert(bucket, out_buf + n, chunk_size);
    }
    bucket_insert(bucket, out_buf, out_size);
    free(out_buf);
}

static void flush_audio_packet(mpegts_stream_t *mpegts_stream,
        bucket_t *bucket) {
    if (mpegts_stream->payload_index_) {
        write_packet(mpegts_stream,
                bucket,
                mpegts_stream->payload_dts_,
                mpegts_stream->payload_pts_,
                mpegts_stream->payload_,
                mpegts_stream->payload_index_);

        mpegts_stream->payload_index_ = 0;
        mpegts_stream->payload_dts_ = NOPTS_VALUE;
        mpegts_stream->payload_pts_ = NOPTS_VALUE;
    }
}

static void write_video_packet(mpegts_stream_t *mpegts_stream,
        bucket_t *bucket,
        uint64_t dts, uint64_t pts,
        unsigned char const *first,
        unsigned char const *last) {
    static const unsigned char aud_nal[6] = {
        0x00, 0x00, 0x00, 0x01, 0x09, 0xe0
    };

    u_int size = last - first + sizeof (aud_nal);
    if (size < 50) return;

    if (mpegts_stream->packets_ == 0) {
        size += 4 + mpegts_stream->sample_entry_->sps_length_ +
                4 + mpegts_stream->sample_entry_->pps_length_;
    }

    unsigned char *buf = (unsigned char *) malloc(size + 10);
    if (buf == NULL) return;
    unsigned char *p = buf;

    memcpy(p, aud_nal, sizeof (aud_nal));
    p += sizeof (aud_nal);

    if (mpegts_stream->packets_ == 0) {
        // sps
        p = write_32(p, 1);
        memcpy(p, mpegts_stream->sample_entry_->sps_,
                mpegts_stream->sample_entry_->sps_length_);
        p += mpegts_stream->sample_entry_->sps_length_;

        // pps
        p = write_32(p, 1);
        memcpy(p, mpegts_stream->sample_entry_->pps_,
                mpegts_stream->sample_entry_->pps_length_);
        p += mpegts_stream->sample_entry_->pps_length_;
    }

    if (convert_to_nal(first, last, p)) {
        write_packet(mpegts_stream, bucket, dts, pts, buf, size);
    }

    free(buf);
}

static void write_audio_packet(mpegts_stream_t *mpegts_stream,
        bucket_t *bucket,
        uint64_t dts, uint64_t pts,
        unsigned char const *first,
        unsigned char const *last) {
    while (first != last) {
        unsigned int size = MAX_PES_PAYLOAD_SIZE - mpegts_stream->payload_index_;
        int flush = 0;

        if (size > (unsigned int) (last - first)) size = last - first;
        memcpy(mpegts_stream->payload_ + mpegts_stream->payload_index_, first, size);

        first += size;
        mpegts_stream->payload_index_ += size;

        if (mpegts_stream->payload_index_ == MAX_PES_PAYLOAD_SIZE) flush = 1;

        if (mpegts_stream->payload_dts_ != NOPTS_VALUE && dts != NOPTS_VALUE &&
                dts - mpegts_stream->payload_dts_ >= AUDIO_DELTA) flush = 1;

        if (flush) flush_audio_packet(mpegts_stream, bucket);
    }
}

////////////////////////////////////////////////////////////////////////////////

samples_t *get_next(trak_t const *trak, samples_t *sample, samples_t *last, u_int seconds, unsigned int *last_chunk) {
    if (sample == last) return sample;

    samples_t *next = sample + 1;
    u_int i = 0;
    while (next != last) {
        if (!next->is_smooth_ss_) {
            ++next;
            continue;
        }
        float duration = (float) ((next->pts_ - sample->pts_) / (float) trak->mdia_->mdhd_->timescale_) + 0.0005;
        if (!(*last_chunk) && duration >= seconds) break;
        if (*last_chunk && *last_chunk == i) break;
        ++next;
        ++i;
    }
    if (!(*last_chunk)) *last_chunk = i;
    return next;
}

int output_ts(struct mp4_context_t *mp4_context, struct bucket_t *bucket, struct mp4_split_options_t const *options) {
    hls_conf_t *conf = ngx_http_get_module_loc_conf(mp4_context->r, ngx_http_estreaming_module);
    u_int audio = options->fragment_track_id ? options->fragment_track_id : 1;
    uint32_t mark_video = FOURCC('v', 'i', 'd', 'e'), mark_sound = FOURCC('s', 'o', 'u', 'n');

    moov_t const *moov = mp4_context->moov;
    if (!moov_build_index(mp4_context, mp4_context->moov)) return 0;

    uint32_t track_id, i, audio_tracks = 0, last_track = 0, last_chunk = 0, max_fragment_size = 2;

    fragment_t fragment[max_fragment_size];
    for (track_id = 0; track_id < max_fragment_size; ++track_id) fragment[track_id].trak = NULL;

    for (track_id = 0; track_id < moov->tracks_; ++track_id) {
        MP4_INFO("track_id %d", track_id);

        trak_t const *trak = moov->traks_[track_id];
        samples_t *sample = trak->samples_;
        if (!sample) {
            MP4_ERROR("%s", "sample is null");
            return 0;
        }
        samples_t *last = trak->samples_ + trak->samples_size_;

        u_int i = 0;

        while (sample != last) {
            if (!sample->is_smooth_ss_) {
                ++sample;
                continue;
            }

            int end = 0;

            if (trak->mdia_->hdlr_->handler_type_ == mark_sound) {
                if (track_id != audio) {
                    end = 2;
                    break;
                }
                if (options->fragment_start == i) {
                    ++audio_tracks;
                    end = 1;
                }
            } else if (trak->mdia_->hdlr_->handler_type_ == mark_video && options->fragment_start == i) {
                end = 1;
            }

            if (end == 1 && last_track < max_fragment_size) {
                fragment[last_track].trak = moov->traks_[track_id];
                fragment[last_track].first = sample;
                fragment[last_track].last = get_next(trak, sample, last, conf->length, &last_chunk);
                //MP4_INFO("fragment begin %ld end %ld", sample->pts_, fragment[last_track].last->pts_);
                ++last_track;
            }
            if (end) break;
            ++sample;
            ++i;
        }
    }

    if (!fragment[0].trak) {
        MP4_ERROR("%s", "no video fragment");
        return 0;
    }

    u_int fragment_size = 1 + audio_tracks;
    if (fragment_size > max_fragment_size) fragment_size = max_fragment_size;

    for (i = 0; i < fragment_size; ++i) {
        if (fragment[i].trak == NULL) continue;
        samples_t *next = fragment[i].first;
        // convert time values to 90KHz clock
        while (1) {
            next->pts_ = trak_time_to_moov_time(next->pts_, 90000, fragment[i].trak->mdia_->mdhd_->timescale_);
            next->cto_ = trak_time_to_moov_time(next->cto_, 90000, fragment[i].trak->mdia_->mdhd_->timescale_);
            if (next == fragment[i].last) break;
            ++next;
        }
        MP4_INFO("fragment %u begin %ld end %ld", i, fragment[i].first->pos_, fragment[i].last->pos_);
    }
    {
        mpegts_muxer_t *muxer = mpegts_muxer_init(mp4_context, bucket, fragment, fragment_size);

        for (i = 0; i < fragment_size; ++i) {
            if (fragment[i].trak == NULL) continue;
            if (fragment[i].trak->mdia_->hdlr_->handler_type_ == mark_sound) {
                fragment[i].stream = mpegts_stream_init(mp4_context, muxer, 0, START_PID + i, &fragment[i].trak->mdia_->minf_->stbl_->stsd_->sample_entries_[0]);
            } else if (fragment[i].trak->mdia_->hdlr_->handler_type_ == mark_video)
                fragment[i].stream = mpegts_stream_init(mp4_context, muxer, 1, START_PID + i, &fragment[i].trak->mdia_->minf_->stbl_->stsd_->sample_entries_[0]);
        }

        write_header(muxer);

        uint64_t offset = 0xFFFFFFFFFFFFFFFFULL;
        unsigned char *data = NULL;
        {
            uint64_t pos_end = 0;
            for (i = 0; i < fragment_size; ++i) {
                if (fragment[i].trak == NULL) continue;
                uint64_t size = fragment[i].last->pos_ - fragment[i].first->pos_;
                uint64_t limit = 0;
                if (fragment[i].trak->mdia_->hdlr_->handler_type_ == mark_sound) limit = 1024 * 1024 * 10;
                else if (fragment[i].trak->mdia_->hdlr_->handler_type_ == mark_video) limit = 1024 * 1024 * 50;
                if (size > limit) {
                    MP4_ERROR("segment %d is too big: %ld - %ld", i, fragment[i].first->pos_, fragment[i].last->pos_);
                    return 0;
                }
                if (fragment[i].first->pos_ < offset) offset = fragment[i].first->pos_;
                if (fragment[i].last->pos_ > pos_end) pos_end = fragment[i].last->pos_;
            }
            //MP4_INFO("fragment start %"PRIi64" end %"PRIi64, offset, pos_end);
            if (!pos_end || offset == 0xFFFFFFFFFFFFFFFFULL) return 0; // sanity check
            mp4_read(mp4_context, &data, pos_end - offset, offset);
            if (!data) return 0;
        }

        int order = -1;
        while (1) {
            u_int to_break = 1;
            for (i = 0; i < fragment_size; ++i) {
                if (fragment[i].trak == NULL) continue;
                if (fragment[i].first != fragment[i].last) to_break = 0;
//                if (fragment[i].first == fragment[i].last && to_break == 0) to_break = 1;
            }
            if (to_break) break;

            uint64_t min_dts = 0xFFFFFFFFFFFFFFFFULL;
            int new_order = order;
            for (i = 0; i < fragment_size; ++i) {
                if (fragment[i].trak != NULL && fragment[i].first != fragment[i].last) {
                    if (min_dts > fragment[i].first->pts_) {
                        min_dts = fragment[i].first->pts_;
                        new_order = i;
                    }
                }
            }
            if (order != -1 && order != new_order && fragment[order].trak->mdia_->hdlr_->handler_type_ == mark_sound)
                flush_audio_packet(fragment[order].stream, muxer->bucket_);
            order = new_order;
            if (order == -1) break;
            if (order > (int) max_fragment_size) break;

            uint64_t dts0 = fragment[order].first->pts_;
            uint64_t pts = fragment[order].first->pts_ + fragment[order].first->cto_;

            uint64_t sample_pos = fragment[order].first->pos_;
            u_int sample_size = fragment[order].first->size_;

#ifdef _DEBUG
            MP4_INFO("track=%d dts=%"PRIi64" pts=%"PRIi64" data=%"PRIu64":%u\n", order, dts0, pts, sample_pos + sample_size, sample_size);
#endif

            unsigned char *data_local = data + (sample_pos - offset);

            if (fragment[order].trak->mdia_->hdlr_->handler_type_ == mark_sound) {
                if (fragment[order].stream->payload_dts_ == NOPTS_VALUE) {
                    fragment[order].stream->payload_dts_ = dts0;
                    fragment[order].stream->payload_pts_ = pts;
                }

                uint8_t adts[7];
                sample_entry_get_adts(&fragment[order].trak->mdia_->minf_->stbl_->stsd_->sample_entries_[0], sample_size, adts);
                write_audio_packet(fragment[order].stream, muxer->bucket_, NOPTS_VALUE, NOPTS_VALUE, adts, adts + 7);

                write_audio_packet(fragment[order].stream, muxer->bucket_, dts0, pts, data_local, data_local + sample_size);

                if (fragment[order].first + 1 == fragment[order].last) flush_audio_packet(fragment[order].stream, muxer->bucket_);
            } else if (fragment[order].trak->mdia_->hdlr_->handler_type_ == mark_video)
                write_video_packet(fragment[order].stream, muxer->bucket_, dts0, pts, data_local, data_local + sample_size);

            ++fragment[order].first;
        }

        for (i = 0; i < fragment_size; ++i) {
            ngx_pfree(mp4_context->r->pool, data);
        }

        mpegts_muxer_exit(mp4_context, muxer);
    }

    return 1;
}

// End Of File