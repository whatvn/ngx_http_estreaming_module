/*******************************************************************************
 mp4_reader.h - A library for reading MPEG4.

 For licensing see the LICENSE file
******************************************************************************/

struct atom_t {
  uint32_t type_;
  uint32_t short_size_;
  uint64_t size_;
  unsigned char *start_;
  unsigned char *end_;
};
typedef struct atom_t atom_t;

struct atom_read_list_t {
    uint32_t type_;
    int (*destination_)(struct mp4_context_t const *mp4_context,
                        void *parent, void *child);
    void *(*reader_)(struct mp4_context_t const *mp4_context,
                     void *parent, unsigned char *buffer, uint64_t size);
};
typedef struct atom_read_list_t atom_read_list_t;

static void atom_print(mp4_context_t const *mp4_context, atom_t const *atom) {
  MP4_INFO("Atom(%c%c%c%c,%"PRIu64")\n",
           atom->type_ >> 24,
           atom->type_ >> 16,
           atom->type_ >> 8,
           atom->type_,
           atom->size_);
}

static unsigned char *
atom_read_header(mp4_context_t const *mp4_context, unsigned char *buffer,
                 atom_t *atom) {
  atom->start_ = buffer;
  atom->short_size_ = read_32(buffer);
  atom->type_ = read_32(buffer + 4);

  if(atom->short_size_ == 1)
    atom->size_ = read_64(buffer + 8);
  else
    atom->size_ = atom->short_size_;

  atom->end_ = atom->start_ + atom->size_;

  atom_print(mp4_context, atom);

  if(atom->size_ < ATOM_PREAMBLE_SIZE) {
    MP4_ERROR("Error: invalid atom size %zu\n", atom->size_);
    return 0;
  }

  return buffer + ATOM_PREAMBLE_SIZE + (atom->short_size_ == 1 ? 8 : 0);
}

static struct unknown_atom_t *unknown_atom_add_atom(struct unknown_atom_t *parent, void *atom) {
  size_t size = read_32((const unsigned char *)atom);
  if(size > 1024 * 1024 || size < ATOM_PREAMBLE_SIZE) return 0;

  unknown_atom_t *unknown = unknown_atom_init();
  unknown->atom_ = malloc(size);
  memcpy(unknown->atom_, atom, size);

  {
    unknown_atom_t **adder = &parent;
    while(*adder != NULL) {
      adder = &(*adder)->next_;
    }
    *adder = unknown;
  }

  return parent;
}

static int atom_reader(struct mp4_context_t const *mp4_context,
                       struct atom_read_list_t *atom_read_list,
                       unsigned int atom_read_list_size,
                       void *parent,
                       unsigned char *buffer, uint64_t size) {
  atom_t leaf_atom;
  unsigned char *buffer_start = buffer;

  while(buffer < buffer_start + size) {
    unsigned int i;
    buffer = atom_read_header(mp4_context, buffer, &leaf_atom);

    if(buffer == NULL) {
      return 0;
    }

    for(i = 0; i != atom_read_list_size; ++i) {
      if(leaf_atom.type_ == atom_read_list[i].type_) {
        break;
      }
    }

    if(i == atom_read_list_size) {
      // add to unkown chunks
      (*(unknown_atom_t **)parent) =
        unknown_atom_add_atom(*(unknown_atom_t **)(parent), buffer - ATOM_PREAMBLE_SIZE);
        if(!parent) return 0;
    } else {
      void *child =
        atom_read_list[i].reader_(mp4_context, parent, buffer,
                                  leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
      if(!child)
        break;
      if(!atom_read_list[i].destination_(mp4_context, parent, child))
        break;
    }
    buffer = leaf_atom.end_;
  }

  if(buffer < buffer_start + size) {
    return 0;
  }

  return 1;
}

static void *ctts_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  ctts_t *atom;

  if(size < 8)
    return 0;

  atom = ctts_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entries_ = read_32(buffer + 4);

  if(size < 8 + atom->entries_ * sizeof(ctts_table_t))
    return 0;

  buffer += 8;

  atom->table_ = (ctts_table_t *)(malloc(atom->entries_ * sizeof(ctts_table_t)));

  for(i = 0; i != atom->entries_; ++i) {
    atom->table_[i].sample_count_ = read_32(buffer + 0);
    atom->table_[i].sample_offset_ = read_32(buffer + 4);
    buffer += 8;
  }

  return atom;
}

static void *stco_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  stco_t *atom;

  if(size < 8)
    return 0;

  atom = stco_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entries_ = read_32(buffer + 4);
  buffer += 8;

  if(size < 8 + atom->entries_ * sizeof(uint32_t))
    return 0;

  atom->chunk_offsets_ = (uint64_t *)malloc(atom->entries_ * sizeof(uint64_t));
  for(i = 0; i != atom->entries_; ++i) {
    atom->chunk_offsets_[i] = read_32(buffer);
    buffer += 4;
  }

  return atom;
}

static void *co64_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  stco_t *atom;

  if(size < 8)
    return 0;

  atom = stco_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entries_ = read_32(buffer + 4);
  buffer += 8;

  if(size < 8 + atom->entries_ * sizeof(uint64_t))
    return 0;

  atom->chunk_offsets_ = (uint64_t *)malloc(atom->entries_ * sizeof(uint64_t));
  for(i = 0; i != atom->entries_; ++i) {
    atom->chunk_offsets_[i] = read_64(buffer);
    buffer += 8;
  }

  return atom;
}

static void *stsz_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  stsz_t *atom;

  if(size < 12) {
    MP4_ERROR("%s", "Error: not enough bytes for stsz atom\n");
    return 0;
  }

  atom = stsz_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->sample_size_ = read_32(buffer + 4);
  atom->entries_ = read_32(buffer + 8);
  buffer += 12;

  // fix for clayton.mp4, it mistakenly says there is 1 entry
//  if(atom->sample_size_ && atom->entries_)
//    atom->entries_ = 0;

  if(!atom->sample_size_) {
    if(size < 12 + atom->entries_ * sizeof(uint32_t)) {
      MP4_ERROR("%s", "Error: stsz.entries don't match with size\n");
      stsz_exit(atom);
      return 0;
    }

    atom->sample_sizes_ = (uint32_t *)malloc(atom->entries_ * sizeof(uint32_t));
    for(i = 0; i != atom->entries_; ++i) {
      atom->sample_sizes_[i] = read_32(buffer);
      buffer += 4;
    }
  }

  return atom;
}

static void *stsc_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  stsc_t *atom;

  if(size < 8)
    return 0;

  atom = stsc_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entries_ = read_32(buffer + 4);

  if(size < 8 + atom->entries_ * sizeof(stsc_table_t))
    return 0;

  buffer += 8;

  // reserve space for one extra entry as when splitting the video we may have to
  // split the first entry
  atom->table_ = (stsc_table_t *)(malloc((atom->entries_ + 1) * sizeof(stsc_table_t)));

  for(i = 0; i != atom->entries_; ++i) {
    atom->table_[i].chunk_ = read_32(buffer + 0) - 1; // Note: we use zero based
    atom->table_[i].samples_ = read_32(buffer + 4);
    atom->table_[i].id_ = read_32(buffer + 8);
    buffer += 12;
  }

  return atom;
}

static void *stss_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  stss_t *atom;

  if(size < 8)
    return 0;

  atom = stss_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entries_ = read_32(buffer + 4);

  if(size < 8 + atom->entries_ * sizeof(uint32_t))
    return 0;

  buffer += 8;

  atom->sample_numbers_ = (uint32_t *)malloc(atom->entries_ * sizeof(uint32_t));

  for(i = 0; i != atom->entries_; ++i) {
    atom->sample_numbers_[i] = read_32(buffer);
    buffer += 4;
  }

  return atom;
}

static int mp4_read_desc_len(unsigned char **buffer) {
  uint32_t len = 0;
  unsigned int bytes = 0;
  for(;;) {
    unsigned int c = read_8(*buffer + bytes);
    len <<= 7;
    len |= (c & 0x7f);
    if(++bytes == 4)
      break;
    if(!(c & 0x80))
      break;
  }

  *buffer += bytes;

  return len;
}

static int esds_read(mp4_context_t const *mp4_context,
                     sample_entry_t *sample_entry,
                     unsigned char *buffer, uint64_t size) {
  unsigned int tag;
  unsigned int len;

  unsigned int object_type_id;
  unsigned int stream_type;
  unsigned int buffer_size_db;

  if(size < 9)
    return 0;

  /* unsigned char version = */ read_8(buffer);
  /* unsigned int flags = */ read_24(buffer + 1);

  buffer += 4;

  // ES_DescrTag
  tag = read_8(buffer);
  buffer += 1;
  if(tag == MP4_ELEMENTARY_STREAM_DESCRIPTOR_TAG) {
    len = mp4_read_desc_len(&buffer);
    MP4_INFO("Elementary Stream Descriptor: len=%u\n", len);
    buffer += 3;
  } else {
    MP4_INFO("Elementary Stream Descriptor: len=%u\n", 2);
    buffer += 2;
  }

  tag = read_8(buffer);
  buffer += 1;
  len = mp4_read_desc_len(&buffer);
  MP4_INFO("MPEG: tag=%u len=%u\n", tag, len);

  // Decoder config descr Tag
  if(tag != MP4_DECODER_CONFIG_DESCRIPTOR_TAG) {
    MP4_INFO("Decoder Config Descriptor: len=%u\n", len);
    return 0;
  }

  object_type_id = read_8(buffer);
  buffer += 1; // object_type_id

  stream_type = read_8(buffer);
  buffer += 1; // stream_type

  buffer_size_db = read_24(buffer);
  buffer += 3; // buffer_size_db

  sample_entry->max_bitrate_ = read_32(buffer);
  buffer += 4;

  sample_entry->avg_bitrate_ = read_32(buffer);
  buffer += 4; // avg_bitrate

  MP4_INFO("%s", "Decoder Configuration Descriptor:\n");
  MP4_INFO("  object_type_id=$%02x\n", object_type_id);
  MP4_INFO("  stream_type=%u\n", stream_type);
  MP4_INFO("  buffer_size_db=%u\n", buffer_size_db);
  MP4_INFO("  max_bitrate=%u\n", sample_entry->max_bitrate_);
  MP4_INFO("  avg_bitrate=%u\n", sample_entry->avg_bitrate_);

  switch(object_type_id) {
  case MP4_MPEG2AudioMain:
  case MP4_MPEG2AudioLowComplexity:
  case MP4_MPEG2AudioScaleableSamplingRate:
  case MP4_MPEG4Audio:
    sample_entry->wFormatTag = 0x00ff;  // WAVE_FORMAT_RAW_AAC1
    break;
  case MP4_MPEG1Audio:
  case MP4_MPEG2AudioPart3:
    sample_entry->wFormatTag = 0x0055;  // WAVE_FORMAT_MP3
    break;
  default:
    break;
  }

  if(!sample_entry->nAvgBytesPerSec) {
    unsigned int bitrate = sample_entry->avg_bitrate_;
    if(!bitrate)
      bitrate = sample_entry->max_bitrate_;
    sample_entry->nAvgBytesPerSec = bitrate / 8;
  }

  tag = read_8(buffer);
  buffer += 1;
  len = mp4_read_desc_len(&buffer);
  MP4_INFO("MPEG: tag=%u len=%u\n", tag, len);
  // Decoder specific descr Tag
  if(tag == MP4_DECODER_SPECIFIC_DESCRIPTOR_TAG) {
    MP4_INFO("Decoder Specific Info Descriptor: len=%u\n", len);
    sample_entry->codec_private_data_length_ = len;
    sample_entry->codec_private_data_ = buffer;
  }

  return 1;
}

static int
stsd_parse_vide(mp4_context_t const *mp4_context,
                trak_t *UNUSED(trak),
                sample_entry_t *sample_entry) {
  unsigned char *buffer = sample_entry->buf_;
  unsigned char *buffer_start = buffer;
  uint64_t size = sample_entry->len_;
  unsigned int i;

  if(size < 78) {
    MP4_ERROR("%s", "invalid stsd video size\n");
    return 0;
  }

  // 'ovc1' is immediately followed by additional data and ends with the codec
  // private data, *not* by additional atoms.
  if(sample_entry->fourcc_ == FOURCC('o', 'v', 'c', '1')) {
    // TODO: get start of ovc1 codec private data (25 00 00 01 0F CB 6C 1A ..)
    sample_entry->codec_private_data_ = buffer + 190;
    sample_entry->codec_private_data_length_ = (unsigned int)(size - 190);
    return 1;
  }

  if(size >= 78 + ATOM_PREAMBLE_SIZE) {
    atom_t atom;
    buffer += 78;
    while(buffer < buffer_start + size - ATOM_PREAMBLE_SIZE) {
      buffer = atom_read_header(mp4_context, buffer, &atom);

      if(buffer == NULL) {
        return 0;
      }

      switch(atom.type_) {
      case FOURCC('a', 'v', 'c', 'C'): {
        unsigned int sequence_parameter_sets;
        unsigned int picture_parameter_sets;

        sample_entry->codec_private_data_ = buffer;

        sample_entry->nal_unit_length_ = (read_8(buffer + 4) & 3) + 1;
        sequence_parameter_sets = read_8(buffer + 5) & 0x1f;
        buffer += 6;
        for(i = 0; i != sequence_parameter_sets; ++i) {
          unsigned int sps_length = read_16(buffer);
          buffer += 2;
          sample_entry->sps_ = buffer;
          sample_entry->sps_length_ = sps_length;
          buffer +=  sps_length;
        }

        picture_parameter_sets = read_8(buffer);
        buffer += 1;
        for(i = 0; i != picture_parameter_sets; ++i) {
          unsigned int pps_length = read_16(buffer);
          buffer += 2;
          sample_entry->pps_ = buffer;
          sample_entry->pps_length_ = pps_length;
          buffer += pps_length;
        }
        sample_entry->codec_private_data_length_ =
          (unsigned int)(buffer - sample_entry->codec_private_data_);
      }
      break;
      case FOURCC('e', 's', 'd', 's'):
        if(!esds_read(mp4_context, sample_entry, buffer, atom.size_ - ATOM_PREAMBLE_SIZE)) {
          return 0;
        }
        break;
      default:
        break;
      }
      buffer = atom.end_;
    }
  }

  return 1;
}

static int wave_read(mp4_context_t const *mp4_context,
                     sample_entry_t *sample_entry,
                     unsigned char *buffer, uint64_t size) {
  unsigned char *end = buffer + size;
  while(buffer < end) {
    atom_t atom;
    buffer = atom_read_header(mp4_context, buffer, &atom);

    if(buffer == NULL) {
      return 0;
    }

    switch(atom.type_) {
    case FOURCC('e', 's', 'd', 's'):
      if(!esds_read(mp4_context, sample_entry, buffer, atom.size_ - ATOM_PREAMBLE_SIZE)) {
        return 0;
      }
      break;
    }

    buffer = atom.end_;
  }

  return 1;
}

static int
stsd_parse_soun(mp4_context_t const *mp4_context,
                trak_t *UNUSED(trak),
                sample_entry_t *sample_entry) {
  unsigned char *buffer = sample_entry->buf_;
  unsigned char *buffer_start = buffer;
  uint64_t size = sample_entry->len_;

  unsigned int data_reference_index;
  unsigned int version;
  unsigned int revision;
  unsigned int vendor;
  unsigned int compression_id;
  unsigned int packet_size;

  if(sample_entry->len_ < 28)
    return 0;

  data_reference_index = read_16(buffer + 6);

  version = read_16(buffer + 8);
  revision = read_16(buffer + 10);
  vendor = read_32(buffer + 12);

  sample_entry->nChannels = read_16(buffer + 16);

  if(sample_entry->nChannels == 3)
    sample_entry->nChannels = 6;

  sample_entry->wBitsPerSample = read_16(buffer + 18);

  compression_id = read_16(buffer + 20);
  packet_size = read_16(buffer + 22);

  sample_entry->samplerate_hi_ = read_16(buffer + 24);
  sample_entry->samplerate_lo_ = read_16(buffer + 26);

  sample_entry->nSamplesPerSec = sample_entry->samplerate_hi_;

  MP4_INFO("%s", "Sample Entry:\n");
  MP4_INFO("  data_reference_index=%u\n", data_reference_index);
  MP4_INFO("  version=%u\n", version);
  MP4_INFO("  revision=%u\n", revision);
  MP4_INFO("  vendor=%08x\n", vendor);
  MP4_INFO("  channel_count=%u\n", sample_entry->nChannels);
  MP4_INFO("  sample_size=%u\n", sample_entry->wBitsPerSample);
  MP4_INFO("  compression_id=%u\n", compression_id);
  MP4_INFO("  packet_size=%u\n", packet_size);
  MP4_INFO("  samplerate_hi=%u\n", sample_entry->samplerate_hi_);
  MP4_INFO("  samplerate_lo=%u\n", sample_entry->samplerate_lo_);

  buffer += 28;

  // 'owma' is immediately followed by the codec private data, *not* by
  // additional atoms.
  if(sample_entry->fourcc_ == FOURCC('o', 'w', 'm', 'a')) {
    sample_entry->codec_private_data_ = buffer;
    sample_entry->codec_private_data_length_ = (unsigned int)(size - 28);
//    sample_entry->wFormatTag = read_16(buffer + 0);
//    ...

    return 1;
  }

  if(version >= 1) {
    unsigned int samples_per_packet;
    unsigned int bytes_per_packet;
    unsigned int bytes_per_frame;
    unsigned int bytes_per_sample;

    if(version == 1 && size < 28 + 16) {
      return 0;
    }
    if(version == 2 && size < 28 + 36) {
      return 0;
    }
    if(version > 2) {
      return 0;
    }

    samples_per_packet = read_32(buffer + 0);
    bytes_per_packet = read_32(buffer + 4);
    bytes_per_frame = read_32(buffer + 8);
    bytes_per_sample = read_32(buffer + 12);

    MP4_INFO("  samples_per_packet=%u\n", samples_per_packet);
    MP4_INFO("  bytes_per_packet=%u\n", bytes_per_packet);
    MP4_INFO("  bytes_per_frame=%u\n", bytes_per_frame);
    MP4_INFO("  bytes_per_sample=%u\n", bytes_per_sample);

    if(samples_per_packet > 0) {
      sample_entry->nAvgBytesPerSec =
        (sample_entry->nChannels * sample_entry->nSamplesPerSec *
         bytes_per_packet + samples_per_packet / 2) / samples_per_packet;
      sample_entry->nBlockAlign = (uint16_t)bytes_per_frame;
    } else {
      sample_entry->nAvgBytesPerSec =
        sample_entry->nChannels * sample_entry->nSamplesPerSec *
        sample_entry->wBitsPerSample / 8;
    }

    buffer += version == 1 ? 16 : 36;
  }

  if(buffer - buffer_start >= ATOM_PREAMBLE_SIZE) {
    atom_t atom;
    while(buffer < buffer_start + size - ATOM_PREAMBLE_SIZE) {
      buffer = atom_read_header(mp4_context, buffer, &atom);

      if(buffer == NULL) {
        return 0;
      }

      switch(atom.type_) {
      case FOURCC('w', 'a', 'v', 'e'):
        if(!wave_read(mp4_context, sample_entry, buffer, atom.size_ - ATOM_PREAMBLE_SIZE)) {
          return 0;
        }
        break;
      case FOURCC('f', 'r', 'm', 'a'):
        break;
      case FOURCC('e', 's', 'd', 's'):
        if(!esds_read(mp4_context, sample_entry, buffer, atom.size_ - ATOM_PREAMBLE_SIZE)) {
          return 0;
        }
        break;
      default:
        break;
      }

      buffer = atom.end_;
    }
  }

  return 1;
}

static int stsd_parse(mp4_context_t const *mp4_context,
                      trak_t *trak, stsd_t *stsd) {
  unsigned int i;
  for(i = 0; i != stsd->entries_; ++i) {
    sample_entry_t *sample_entry = &stsd->sample_entries_[i];
    switch(trak->mdia_->hdlr_->handler_type_) {
    case FOURCC('v', 'i', 'd', 'e'):
      if(!stsd_parse_vide(mp4_context, trak, sample_entry)) {
        return 0;
      }
      break;
    case FOURCC('s', 'o', 'u', 'n'):
      if(!stsd_parse_soun(mp4_context, trak, sample_entry)) {
        return 0;
      }
      break;
    case FOURCC('h', 'i', 'n', 't'):
    default:
      return 1;
    }
  }

  return 1;
}

static void *stts_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  stts_t *atom;

  if(size < 8)
    return 0;

  atom = stts_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entries_ = read_32(buffer + 4);

  if(size < 8 + atom->entries_ * sizeof(stts_table_t))
    return 0;

  buffer += 8;

  atom->table_ = (stts_table_t *)(malloc(atom->entries_ * sizeof(stts_table_t)));

  for(i = 0; i != atom->entries_; ++i) {
    atom->table_[i].sample_count_ = read_32(buffer + 0);
    atom->table_[i].sample_duration_ = read_32(buffer + 4);
    buffer += 8;
  }

  return atom;
}

static void *stsd_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  stsd_t *atom;

  if(size < 8)
    return 0;

  atom = stsd_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entries_ = read_32(buffer + 4);

  buffer += 8;

  atom->sample_entries_ = (sample_entry_t *)(malloc(atom->entries_ * sizeof(sample_entry_t)));

  for(i = 0; i != atom->entries_; ++i) {
    unsigned int j;
    sample_entry_t *sample_entry = &atom->sample_entries_[i];
    sample_entry_init(sample_entry);
    sample_entry->len_ = read_32(buffer) - 8;
    sample_entry->fourcc_ = read_32(buffer + 4);
    sample_entry->buf_ = (unsigned char *)malloc(sample_entry->len_);
    buffer += 8;
    for(j = 0; j != sample_entry->len_; ++j) {
      sample_entry->buf_[j] = (unsigned char)read_8(buffer + j);
    }
    buffer += j;
  }

  return atom;
}

static int stbl_add_stsd(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  stbl_t *stbl = (stbl_t *)parent;
  stbl->stsd_ = (stsd_t *)child;

  return 1;
}

static int stbl_add_stts(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  stbl_t *stbl = (stbl_t *)parent;
  stbl->stts_ = (stts_t *)child;

  return 1;
}

static int stbl_add_stss(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  stbl_t *stbl = (stbl_t *)parent;
  stbl->stss_ = (stss_t *)child;

  return 1;
}

static int stbl_add_stsc(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  stbl_t *stbl = (stbl_t *)parent;
  stbl->stsc_ = (stsc_t *)child;

  return 1;
}

static int stbl_add_stsz(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  stbl_t *stbl = (stbl_t *)parent;
  stbl->stsz_ = (stsz_t *)child;

  return 1;
}

static int stbl_add_stco(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  stbl_t *stbl = (stbl_t *)parent;
  stbl->stco_ = (stco_t *)child;

  return 1;
}

static int stbl_add_ctts(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  stbl_t *stbl = (stbl_t *)parent;
  stbl->ctts_ = (ctts_t *)child;

  return 1;
}


static void *stbl_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  stbl_t *atom = stbl_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('s', 't', 's', 'd'), &stbl_add_stsd, &stsd_read },
    { FOURCC('s', 't', 't', 's'), &stbl_add_stts, &stts_read },
    { FOURCC('s', 't', 's', 's'), &stbl_add_stss, &stss_read },
    { FOURCC('s', 't', 's', 'c'), &stbl_add_stsc, &stsc_read },
    { FOURCC('s', 't', 's', 'z'), &stbl_add_stsz, &stsz_read },
    { FOURCC('s', 't', 'c', 'o'), &stbl_add_stco, &stco_read },
    { FOURCC('c', 'o', '6', '4'), &stbl_add_stco, &co64_read },
    { FOURCC('c', 't', 't', 's'), &stbl_add_ctts, &ctts_read },
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  // check for mandatory atoms
  if(!atom->stsd_) {
    MP4_ERROR("%s", "stbl: missing mandatory stsd\n");
    result = 0;
  }

  if(!atom->stts_) {
    MP4_ERROR("%s", "stbl: missing mandatory stts\n");
    result = 0;
  }

  // Expression Encoder doesn't write mandatory stsz atom
  if(!atom->stsc_) {
    MP4_ERROR("%s", "stbl: missing mandatory stsc\n");
//    result = 0;
  }

  // Expression Encoder doesn't write mandatory stsz atom
  if(!atom->stsz_) {
    MP4_ERROR("%s", "stbl: missing mandatory stsz\n");
//    result = 0;
  }

  // Expression Encoder doesn't write mandatory stco atom
  if(!atom->stco_) {
    MP4_ERROR("%s", "stbl: missing mandatory stco\n");
//    result = 0;
  }

  if(!result) {
    stbl_exit(atom);
    return 0;
  }

  return atom;
}

static void *hdlr_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  hdlr_t *atom;

  if(size < 8)
    return 0;

  atom = hdlr_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->predefined_ = read_32(buffer + 4);
  atom->handler_type_ = read_32(buffer + 8);
  atom->reserved1_ = read_32(buffer + 12);
  atom->reserved2_ = read_32(buffer + 16);
  atom->reserved3_ = read_32(buffer + 20);
  buffer += 24;
  size -= 24;
  if(size > 0) {
    size_t length = (size_t)size;
    atom->name_ = (char *)malloc(length + 1);
    if(atom->predefined_ == FOURCC('m', 'h', 'l', 'r')) {
      length = read_8(buffer);
      buffer += 1;
      if(size < length)
        length = (size_t)size;
    }
    memcpy(atom->name_, buffer, length);
    atom->name_[length] = '\0';
  }

  return atom;
}

static void *vmhd_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  vmhd_t *atom;

  if(size < 12)
    return 0;

  atom = vmhd_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);

  atom->graphics_mode_ = read_16(buffer + 4);
  buffer += 6;
  for(i = 0; i != 3; ++i) {
    atom->opcolor_[i] = read_16(buffer);
    buffer += 2;
  }

  return atom;
}

static void *smhd_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  smhd_t *atom;

  if(size < 8)
    return 0;

  atom = smhd_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);

  atom->balance_ = read_16(buffer + 4);
  atom->reserved_ = read_16(buffer + 6);

  return atom;
}

static int dinf_add_dref(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  dinf_t *dinf = (dinf_t *)parent;
  dinf->dref_ = (dref_t *)child;

  return 1;
}

static void *dref_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  dref_t *atom;

  if(size < 20)
    return 0;

  atom = dref_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);

  atom->entry_count_ = read_32(buffer + 4);
  atom->table_ = atom->entry_count_ == 0 ? NULL : (dref_table_t *)malloc(atom->entry_count_ * sizeof(dref_table_t));
  buffer += 8;

  for(i = 0; i != atom->entry_count_; ++i) {
    dref_table_t *entry = &atom->table_[i];
    uint32_t entry_size = read_32(buffer + 0);
    uint32_t type = read_32(buffer + 4);
    uint32_t flags = read_32(buffer + 8);
    dref_table_init(entry);
    entry->flags_ = flags;
    if(flags != 0x000001) {
      switch(type) {
      case FOURCC('u', 'r', 'n', ' '):
        // read name and location (optional) as UTF8 zero-terminated string
        break;
      case FOURCC('u', 'r', 'l', ' '):
        // read location as UTF8 zero-terminated string
        break;
      }
    }
    buffer += entry_size;
  }

  return atom;
}


static void *dinf_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  dinf_t *atom = dinf_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('d', 'r', 'e', 'f'), &dinf_add_dref, &dref_read },
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  // check for mandatory atoms
  if(!atom->dref_) {
    MP4_ERROR("%s", "dinf: missing dref\n");
    result = 0;
  }

  if(!result) {
    dinf_exit(atom);
    return 0;
  }

  return atom;
}

static int minf_add_vmhd(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  minf_t *minf = (minf_t *)parent;
  minf->vmhd_ = (vmhd_t *)child;

  return 1;
}

static int minf_add_smhd(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  minf_t *minf = (minf_t *)parent;
  minf->smhd_ = (smhd_t *)child;

  return 1;
}

static int minf_add_dinf(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  minf_t *minf = (minf_t *)parent;
  minf->dinf_ = (dinf_t *)child;

  return 1;
}

static int minf_add_stbl(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  minf_t *minf = (minf_t *)parent;
  minf->stbl_ = (stbl_t *)child;

  return 1;
}

static void *minf_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  minf_t *atom = minf_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('v', 'm', 'h', 'd'), &minf_add_vmhd, &vmhd_read },
    { FOURCC('s', 'm', 'h', 'd'), &minf_add_smhd, &smhd_read },
    { FOURCC('d', 'i', 'n', 'f'), &minf_add_dinf, &dinf_read },
    { FOURCC('s', 't', 'b', 'l'), &minf_add_stbl, &stbl_read }
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  // check for mandatory atoms
  if(!atom->stbl_) {
    MP4_ERROR("%s", "minf: missing stbl\n");
    result = 0;
  }

  if(!result) {
    minf_exit(atom);
    return 0;
  }

  return atom;
}


static void *mdhd_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t UNUSED(size)) {
  uint16_t language;
  unsigned int i;

  mdhd_t *mdhd = mdhd_init();
  mdhd->version_ = read_8(buffer + 0);
  mdhd->flags_ = read_24(buffer + 1);
  if(mdhd->version_ == 0) {
    mdhd->creation_time_ = read_32(buffer + 4);
    mdhd->modification_time_ = read_32(buffer + 8);
    mdhd->timescale_ = read_32(buffer + 12);
    mdhd->duration_ = read_32(buffer + 16);
    buffer += 20;
  } else {
    mdhd->creation_time_ = read_64(buffer + 4);
    mdhd->modification_time_ = read_64(buffer + 12);
    mdhd->timescale_ = read_32(buffer + 20);
    mdhd->duration_ = read_64(buffer + 24);
    buffer += 32;
  }

  language = read_16(buffer + 0);
  for(i = 0; i != 3; ++i) {
    mdhd->language_[i] = ((language >> ((2 - i) * 5)) & 0x1f) + 0x60;
  }

  mdhd->predefined_ = read_16(buffer + 2);

  return mdhd;
}


static void *tkhd_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  tkhd_t *tkhd = tkhd_init();

  tkhd->version_ = read_8(buffer + 0);
  tkhd->flags_ = read_24(buffer + 1);
  if(tkhd->version_ == 0) {
    if(size < 92 - 8)
      return 0;

    tkhd->creation_time_ = read_32(buffer + 4);
    tkhd->modification_time_ = read_32(buffer + 8);
    tkhd->track_id_ = read_32(buffer + 12);
    tkhd->reserved_ = read_32(buffer + 16);
    tkhd->duration_ = read_32(buffer + 20);
    buffer += 24;
  } else {
    if(size < 104 - 8)
      return 0;

    tkhd->creation_time_ = read_64(buffer + 4);
    tkhd->modification_time_ = read_64(buffer + 12);
    tkhd->track_id_ = read_32(buffer + 20);
    tkhd->reserved_ = read_32(buffer + 24);
    tkhd->duration_ = read_64(buffer + 28);
    buffer += 36;
  }

  tkhd->reserved2_[0] = read_32(buffer + 0);
  tkhd->reserved2_[1] = read_32(buffer + 4);
  tkhd->layer_ = read_16(buffer + 8);
  tkhd->predefined_ = read_16(buffer + 10);
  tkhd->volume_ = read_16(buffer + 12);
  tkhd->reserved3_ = read_16(buffer + 14);
  buffer += 16;

  for(i = 0; i != 9; ++i) {
    tkhd->matrix_[i] = read_32(buffer);
    buffer += 4;
  }

  tkhd->width_ = read_32(buffer + 0);
  tkhd->height_ = read_32(buffer + 4);

  return tkhd;
}

static int mdia_add_mdhd(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  mdia_t *mdia = (mdia_t *)parent;
  mdia->mdhd_ = (mdhd_t *)child;

  return 1;
}

static int mdia_add_hdlr(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  mdia_t *mdia = (mdia_t *)parent;
  mdia->hdlr_ = (hdlr_t *)child;

  return 1;
}

static int mdia_add_minf(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  mdia_t *mdia = (mdia_t *)parent;
  mdia->minf_ = (minf_t *)child;

  return 1;
}

static void *mdia_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  mdia_t *atom = mdia_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('m', 'd', 'h', 'd'), &mdia_add_mdhd, &mdhd_read },
    { FOURCC('h', 'd', 'l', 'r'), &mdia_add_hdlr, &hdlr_read },
    { FOURCC('m', 'i', 'n', 'f'), &mdia_add_minf, &minf_read }
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  // check for mandatory atoms
  if(!atom->mdhd_) {
    MP4_ERROR("%s", "mdia: missing mdhd\n");
    result = 0;
  }

  if(!atom->hdlr_) {
    MP4_ERROR("%s", "mdia: missing hdlr\n");
    result = 0;
  }

  if(!atom->minf_) {
    MP4_ERROR("%s", "mdia: missing minf\n");
    result = 0;
  }

  if(!result) {
    mdia_exit(atom);
    return 0;
  }

  return atom;
}

static void *elst_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  elst_t *atom;

  if(size < 8)
    return 0;

  atom = elst_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  atom->entry_count_ = read_32(buffer + 4);

  buffer += 8;

  atom->table_ = (elst_table_t *)(malloc(atom->entry_count_ * sizeof(elst_table_t)));

  for(i = 0; i != atom->entry_count_; ++i) {
    elst_table_t *elst_table = &atom->table_[i];
    if(atom->version_ == 0) {
      elst_table->segment_duration_ = read_32(buffer);
      elst_table->media_time_ = (int32_t)read_32(buffer + 4);
      buffer += 8;
    } else {
      elst_table->segment_duration_ = read_64(buffer);
      elst_table->media_time_ = read_64(buffer + 8);
      buffer += 16;
    }

    elst_table->media_rate_integer_ = read_16(buffer);
    elst_table->media_rate_fraction_ = read_16(buffer + 2);
    buffer += 4;
  }

  return atom;
}

static int edts_add_elst(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *elst) {
  edts_t *edts = (edts_t *)parent;
  edts->elst_ = (elst_t *)elst;

  return 1;
}

static void *edts_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  edts_t *atom = edts_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('e', 'l', 's', 't'), &edts_add_elst, &elst_read }
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  if(!result) {
    edts_exit(atom);
    return 0;
  }

  return atom;
}

static int trak_add_tkhd(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *tkhd) {
  trak_t *trak = (trak_t *)parent;
  trak->tkhd_ = (tkhd_t *)tkhd;

  return 1;
}

static int trak_add_mdia(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *mdia) {
  trak_t *trak = (trak_t *)parent;
  trak->mdia_ = (mdia_t *)mdia;

  return 1;
}

static int trak_add_edts(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *edts) {
  trak_t *trak = (trak_t *)parent;
  trak->edts_ = (edts_t *)edts;

  return 1;
}

static void *trak_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  trak_t *atom = trak_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('t', 'k', 'h', 'd'), &trak_add_tkhd, &tkhd_read },
    { FOURCC('m', 'd', 'i', 'a'), &trak_add_mdia, &mdia_read },
    { FOURCC('e', 'd', 't', 's'), &trak_add_edts, &edts_read }
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  // check for mandatory atoms
  if(!atom->tkhd_) {
    MP4_ERROR("%s", "trak: missing tkhd\n");
    result = 0;
  }

  if(!atom->mdia_) {
    MP4_ERROR("%s", "trak: missing mdia\n");
    result = 0;
  }

  if(result && !stsd_parse(mp4_context, atom, atom->mdia_->minf_->stbl_->stsd_)) {
    MP4_ERROR("%s", "trak: error parsing stsd\n");
    result = 0;
  }

  if(!result) {
    trak_exit(atom);
    return 0;
  }

  return atom;
}

static void *mvhd_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  unsigned int i;

  mvhd_t *atom = mvhd_init();
  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);
  if(atom->version_ == 0) {
    if(size < 108 - 8)
      return 0;

    atom->creation_time_ = read_32(buffer + 4);
    atom->modification_time_ = read_32(buffer + 8);
    atom->timescale_ = read_32(buffer + 12);
    atom->duration_ = read_32(buffer + 16);
    buffer += 20;
  } else {
    if(size < 120 - 8)
      return 0;

    atom->creation_time_ = read_64(buffer + 4);
    atom->modification_time_ = read_64(buffer + 12);
    atom->timescale_ = read_32(buffer + 20);
    atom->duration_ = read_64(buffer + 24);
    buffer += 32;
  }
  atom->rate_ = read_32(buffer + 0);
  atom->volume_ = read_16(buffer + 4);
  atom->reserved1_ = read_16(buffer + 6);
  atom->reserved2_[0] = read_32(buffer + 8);
  atom->reserved2_[1] = read_32(buffer + 12);
  buffer += 16;

  for(i = 0; i != 9; ++i) {
    atom->matrix_[i] = read_32(buffer);
    buffer += 4;
  }

  for(i = 0; i != 6; ++i) {
    atom->predefined_[i] = read_32(buffer);
    buffer += 4;
  }

  atom->next_track_id_ = read_32(buffer + 0);

  return atom;
}


static int moov_add_mvhd(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *mvhd) {
  moov_t *moov = (moov_t *)parent;
  moov->mvhd_ = (mvhd_t *)mvhd;

  return 1;
}

static int moov_add_trak(mp4_context_t const *mp4_context,
                         void *parent, void *child) {
  moov_t *moov = (moov_t *)parent;
  trak_t *trak = (trak_t *)child;
  if(moov->tracks_ == MAX_TRACKS) {
    trak_exit(trak);
    return 0;
  }

  if(trak->mdia_->hdlr_->handler_type_ != FOURCC('v', 'i', 'd', 'e') &&
      trak->mdia_->hdlr_->handler_type_ != FOURCC('s', 'o', 'u', 'n')) {
    MP4_INFO("Trak ignored (handler_type=%c%c%c%c, name=%s)\n",
             trak->mdia_->hdlr_->handler_type_ >> 24,
             trak->mdia_->hdlr_->handler_type_ >> 16,
             trak->mdia_->hdlr_->handler_type_ >> 8,
             trak->mdia_->hdlr_->handler_type_,
             trak->mdia_->hdlr_->name_);
    trak_exit(trak);
    return 1; // continue
  }

  // check for tracks that have a duration, but no samples. This happens with
  // Expression Encoder fragmented movie files.
  if(trak->mdia_->minf_->stbl_->stco_ == 0 ||
      (trak->mdia_->minf_->stbl_->stco_->entries_ == 0 &&
       trak->mdia_->mdhd_->duration_)) {
    trak->mdia_->mdhd_->duration_ = 0;
  }

#if 0  // we can't ignore empty tracks, as the fragments may come later
  // ignore empty track (unless LIVE)
  if(trak->mdia_->mdhd_->duration_ == 0 && !moov->mvex_) {
    trak_exit(trak);
    return 1; // continue
  }
#endif

  moov->traks_[moov->tracks_] = trak;
  ++moov->tracks_;

  return 1;
}

static void *trex_read(mp4_context_t const *UNUSED(mp4_context),
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  trex_t *atom = trex_init();

  if(size < 24)
    return 0;

  atom->version_ = read_8(buffer + 0);
  atom->flags_ = read_24(buffer + 1);

  atom->track_id_ = read_32(buffer + 4);
  atom->default_sample_description_index_ = read_32(buffer + 8);
  atom->default_sample_duration_ = read_32(buffer + 12);
  atom->default_sample_size_ = read_32(buffer + 16);
  atom->default_sample_flags_ = read_32(buffer + 20);

  return atom;
}

static int moov_add_mvex(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *mvex) {
  moov_t *moov = (moov_t *)parent;
  moov->mvex_ = (mvex_t *)mvex;

  return 1;
}

static int mvex_add_trex(mp4_context_t const *UNUSED(mp4_context),
                         void *parent, void *child) {
  mvex_t *mvex = (mvex_t *)parent;
  trex_t *trex = (trex_t *)child;
  if(mvex->tracks_ == MAX_TRACKS) {
    trex_exit(trex);
    return 0;
  }

  mvex->trexs_[mvex->tracks_] = trex;
  ++mvex->tracks_;

  return 1;
}

static void *mvex_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  mvex_t *atom = mvex_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('t', 'r', 'e', 'x'), &mvex_add_trex, &trex_read }
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  // check for mandatory atoms
  if(!atom->tracks_) {
    MP4_ERROR("%s", "mvex: missing trex\n");
    result = 0;
  }

  if(!result) {
    mvex_exit(atom);
    return 0;
  }

  return atom;
}

static void *moov_read(mp4_context_t const *mp4_context,
                       void *UNUSED(parent),
                       unsigned char *buffer, uint64_t size) {
  moov_t *atom = moov_init();

  atom_read_list_t atom_read_list[] = {
    { FOURCC('m', 'v', 'h', 'd'), &moov_add_mvhd, &mvhd_read },
    { FOURCC('t', 'r', 'a', 'k'), &moov_add_trak, &trak_read },
    { FOURCC('m', 'v', 'e', 'x'), &moov_add_mvex, &mvex_read }
  };

  int result = atom_reader(mp4_context,
                           atom_read_list,
                           sizeof(atom_read_list) / sizeof(atom_read_list[0]),
                           atom,
                           buffer, size);

  // check for mandatory atoms
  if(!atom->mvhd_) {
    MP4_ERROR("%s", "moov: missing mvhd\n");
    result = 0;
  }

  if(!atom->tracks_) {
    MP4_ERROR("%s", "moov: missing trak\n");
    result = 0;
  }

  if(!result) {
    moov_exit(atom);
    return 0;
  }

  return atom;
}

static int trak_build_index(mp4_context_t const *mp4_context, trak_t *trak) {
  stco_t const *stco = trak->mdia_->minf_->stbl_->stco_;
  unsigned int stco_samples = 0;

  if(stco == NULL || stco->entries_ == 0) return 0;

  trak->chunks_size_ = stco->entries_;
  trak->chunks_ = (chunks_t *)malloc(trak->chunks_size_ * sizeof(chunks_t));

  {
    unsigned int i;
    for(i = 0; i != trak->chunks_size_; ++i) {
      trak->chunks_[i].pos_ = stco->chunk_offsets_[i];
    }
  }

  // process chunkmap:
  stsc_t const *stsc = trak->mdia_->minf_->stbl_->stsc_;
  unsigned int last = trak->chunks_size_;
  unsigned int i = stsc->entries_;
  while(i > 0) {
    unsigned int j;
    --i;
    for(j = stsc->table_[i].chunk_; j < last; j++) {
      trak->chunks_[j].id_ = stsc->table_[i].id_;
      trak->chunks_[j].size_ = stsc->table_[i].samples_;
    }
    last = stsc->table_[i].chunk_;
  }

  // calc pts of chunks:
  stsz_t const *stsz = trak->mdia_->minf_->stbl_->stsz_;
  unsigned int sample_size = stsz->sample_size_;
  unsigned int s = 0;
  {
    unsigned int j;
    for(j = 0; j < trak->chunks_size_; j++) {
      trak->chunks_[j].sample_ = s;
      s += trak->chunks_[j].size_;
    }
  }
  stco_samples = s;

  if(sample_size == 0) {
    trak->samples_size_ = stsz->entries_;
  } else {
    trak->samples_size_ = s;
  }

  // reserve one extra for the end information (like pts and cto).
  trak->samples_ = (samples_t *)calloc(trak->samples_size_ + 1, sizeof(samples_t));

  if(sample_size == 0) {
    unsigned int i;
    for(i = 0; i != trak->samples_size_ ; ++i)
      trak->samples_[i].size_ = stsz->sample_sizes_[i];
  } else {
    unsigned int i;
    for(i = 0; i != trak->samples_size_ ; ++i)
      trak->samples_[i].size_ = sample_size;
  }

  // calc pts:
  stts_t const *stts = trak->mdia_->minf_->stbl_->stts_;
  s = 0;
  uint64_t pts = 0;
  unsigned int entries = stts->entries_;
  unsigned int j;
  for(j = 0; j < entries; j++) {
    unsigned int i;
    unsigned int sample_count = stts->table_[j].sample_count_;
    unsigned int sample_duration = stts->table_[j].sample_duration_;
    for(i = 0; i < sample_count; i++) {
      trak->samples_[s].pts_ = pts;
      ++s;
      pts += sample_duration;
    }
  }
  // write end pts
  trak->samples_[s].pts_ = pts;

  // calc composition times:
  ctts_t const *ctts = trak->mdia_->minf_->stbl_->ctts_;
  if(ctts) {
    unsigned int s = 0;
    unsigned int entries = ctts->entries_;
    unsigned int j;
    unsigned int sample_offset = 0;

    for(j = 0; j != entries; j++) {
      unsigned int i;
      unsigned int sample_count = ctts->table_[j].sample_count_;
      sample_offset = ctts->table_[j].sample_offset_;
      for(i = 0; i < sample_count; i++) {
        if(s == trak->samples_size_) {
          MP4_WARNING("Warning: ctts_get_samples=%u, should be %u\n",
                      ctts_get_samples(ctts), trak->samples_size_);
          break;
        }

        trak->samples_[s].cto_ = sample_offset;
        ++s;
      }
    }
    // write end cto
    trak->samples_[s].cto_ = sample_offset;
  }

  // calc sample offsets
  s = 0;
  uint64_t pos = 0;
  for(j = 0; j < trak->chunks_size_; j++) {
    pos = trak->chunks_[j].pos_;
    unsigned int i;
    for(i = 0; i < trak->chunks_[j].size_; i++) {
      if(s == trak->samples_size_) {
        MP4_WARNING("Warning: stco_get_samples=%u, should be %u\n",
                    stco_samples, trak->samples_size_);
        break;
      }
      trak->samples_[s].pos_ = pos;
      pos += trak->samples_[s].size_;
      ++s;
    }
  }
  trak->samples_[s].pos_ = pos;

  stss_t const *stss = trak->mdia_->minf_->stbl_->stss_;
  if(stss) {
    for(i = 0; i != stss->entries_; ++i) {
      uint32_t s = stss->sample_numbers_[i] - 1;
      trak->samples_[s].is_smooth_ss_ = 1;
    }
  }
  // write end ss
  trak->samples_[trak->samples_size_].is_smooth_ss_ = 1;

  return 1;
}

static void copy_sync_samples_to_audio_track(trak_t *video, trak_t *audio) {
  if(video) {
    samples_t *first = video->samples_;
    samples_t *last = video->samples_ + video->samples_size_;
    samples_t *audio_first = audio->samples_;
    samples_t *audio_last = audio->samples_ + audio->samples_size_;
    while(first != last) {
      if(first->is_smooth_ss_) {
        uint64_t pts = trak_time_to_moov_time(first->pts_,
                                              audio->mdia_->mdhd_->timescale_, video->mdia_->mdhd_->timescale_);
        while(audio_first != audio_last) {
          if(audio_first->pts_ >= pts) {
            audio_first->is_smooth_ss_ = 1;
            break;
          }
          ++audio_first;
        }
      }
      ++first;
    }
  } else {
    // if there is no video track and we don't have sync samples, then insert
    // smooth sync samples every 2 seconds
    samples_t *f = audio->samples_;
    samples_t *l = audio->samples_ + audio->samples_size_;
    uint64_t pts = 0;
    uint64_t increment = 2 * audio->mdia_->mdhd_->timescale_;
    while(f != l) {
      if(f->pts_ >= pts) {
        f->is_smooth_ss_ = 1;
        pts += increment;
      }
      ++f;
    }
  }
}

static int moov_build_index(struct mp4_context_t const *mp4_context,
                            struct moov_t *moov) {
  // Build the track index
  trak_t *video_trak = NULL;
  unsigned int track;

  if(!moov) return 0;
  // already indexed?
  if(moov->is_indexed_) return 1;

  moov->is_indexed_ = 1;

  for(track = 0; track != moov->tracks_; ++track) {
    trak_t *trak = moov->traks_[track];
    switch(trak->mdia_->hdlr_->handler_type_) {
    case FOURCC('s', 'o', 'u', 'n'):
      break;
    case FOURCC('v', 'i', 'd', 'e'):
      video_trak = trak;
      break;
    }
    if(!trak_build_index(mp4_context, trak)) return 0;
  }

  for(track = 0; track != moov->tracks_; ++track) {
    trak_t *trak = moov->traks_[track];
    switch(trak->mdia_->hdlr_->handler_type_) {
    case FOURCC('s', 'o', 'u', 'n'):
      // Copy the sync sample markers for smooth streaming from the video trak
      // to the audio trak in case the audio track doesn't have an 'stss'.
      if(!moov->mvex_ && !trak->mdia_->minf_->stbl_->stss_) {
        copy_sync_samples_to_audio_track(video_trak, trak);
      }
      break;
    }
  }


  return 1;
}

// End Of File
