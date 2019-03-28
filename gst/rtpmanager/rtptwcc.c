/* GStreamer
 * Copyright (C)  2019 Pexip (http://pexip.com/)
 *   @author: Havard Graff <havard@pexip.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "rtptwcc.h"

GST_DEBUG_CATEGORY_EXTERN (rtp_session_debug);
#define GST_CAT_DEFAULT rtp_session_debug

#define TWCC_DEBUG 0

#if TWCC_DEBUG
static void
print_bits (gpointer ptr, guint bytes)
{
  guint8 *d = ptr;
  for (guint b = 0; b < bytes; b++) {
    g_print ("| ");
    for (guint i = 0; i < 8; i++) {
      g_print ("%d ", (d[b] >> (7 - i)) & 1 ? 1 : 0);
    }
  }
  g_print ("\n");
}
#endif /* TWCC_DEBUG */


struct _RTPTWCCManager
{
  GArray *recv_packets;

  guint8 fb_pkt_count;
  gint32 last_seqnum;

  GArray *sent_packets;
  GArray *parsed_packets;

  guint16 send_seqnum;
};

typedef enum
{
  RTP_TWCC_PACKET_STATUS_NOT_RECV = 0,
  RTP_TWCC_PACKET_STATUS_SMALL_DELTA = 1,
  RTP_TWCC_PACKET_STATUS_LARGE_NEGATIVE_DELTA = 2,
} RTPTWCCPacketStatus;

typedef enum
{
  RTP_TWCC_CHUNK_TYPE_RUN_LENGTH = 0,
  RTP_TWCC_CHUNK_TYPE_STATUS_VECTOR = 1,
} RTPTWCCChunkType;

typedef struct
{
  guint8 base_seqnum[2];
  guint8 packet_count[2];
  guint8 base_time[3];
  guint8 fb_pkt_count[1];
} RTPTWCCHeader;


typedef struct
{
  GstClockTime ts;
  guint16 seqnum;

  GstClockTimeDiff delta_ts;
  RTPTWCCPacketStatus status;
  guint16 missing_run;
  guint equal_run;
} RecvPacket;

typedef struct
{
  GstClockTime ts;
  guint16 seqnum;
  guint size;
} SentPacket;

#pragma pack(push)              /* push current alignment to stack */
#pragma pack(1)                 /* set alignment to 1 byte boundary */
typedef union
{
  struct
  {
    unsigned:7;
    unsigned chunk_type:1;      /* RTP_TWCC_CHUNK_TYPE */
    unsigned:8;
  };

  struct
  {
    unsigned:5;
    unsigned packet_status_symbol:2;
    unsigned chunk_type:1;
    unsigned:8;
  } RunLength;

  struct
  {
    unsigned:6;
    unsigned symbol_size:1;
    unsigned chunk_type:1;
    unsigned:8;
  } StatusVector;

  guint16 data;

} TWCCPacketChunk;
#pragma pack(pop)               /* restore original alignment from stack */

#define MAX_TS_DELTA (63750 * GST_USECOND)
#define REF_TIME_UNIT (64 * GST_MSECOND)
#define DELTA_UNIT (250 * GST_USECOND)


RTPTWCCManager *
rtp_twcc_manager_new (void)
{
  RTPTWCCManager *twcc = g_new0 (RTPTWCCManager, 1);

  twcc->recv_packets = g_array_new (FALSE, FALSE, sizeof (RecvPacket));

  twcc->sent_packets = g_array_new (FALSE, FALSE, sizeof (RecvPacket));
  twcc->parsed_packets = g_array_new (FALSE, FALSE, sizeof (RecvPacket));

  twcc->last_seqnum = -1;

  return twcc;
}

void
rtp_twcc_manager_free (RTPTWCCManager * twcc)
{
  g_array_unref (twcc->recv_packets);
  g_array_unref (twcc->sent_packets);
  g_array_unref (twcc->parsed_packets);
}

static void
recv_packet_init (RecvPacket * packet, RTPPacketInfo * pinfo)
{
  memset (packet, 0, sizeof (RecvPacket));
  packet->seqnum = pinfo->twcc_seqnum;
  packet->ts = pinfo->running_time;
}

void
rtp_twcc_manager_recv_packet (RTPTWCCManager * twcc, RTPPacketInfo * pinfo)
{
  RecvPacket packet;

  /* we don't want duplicates, because they mess us up, pretending to
     be a huuuge gap in seqnum (65535) */
  if (pinfo->twcc_seqnum == twcc->last_seqnum) {
    GST_WARNING ("duplicate twcc seqnum (%u) received!", pinfo->twcc_seqnum);
    return;
  }

  /* store the packet for Transport-wide RTCP feedback message */
  recv_packet_init (&packet, pinfo);
  g_array_append_val (twcc->recv_packets, packet);
  twcc->last_seqnum = pinfo->twcc_seqnum;
  GST_DEBUG ("Received twcc-seqnum: %u, marker: %d",
      packet.seqnum, pinfo->marker);
}

static gint
_twcc_seqnum_sort (gconstpointer a, gconstpointer b)
{
  gint32 seqa = ((RecvPacket *) a)->seqnum;
  gint32 seqb = ((RecvPacket *) b)->seqnum;
  gint res = seqa - seqb;
  if (res < -65000)
    res = 1;
  if (res > 65000)
    res = -1;
  return res;
}

static void
rtp_twcc_write_recv_deltas (guint8 * fci_data, GArray * twcc_packets)
{
  guint i;
  for (i = 0; i < twcc_packets->len; i++) {
    RecvPacket *pkt = &g_array_index (twcc_packets, RecvPacket, i);
    gint64 delta = pkt->delta_ts / DELTA_UNIT;

    if (pkt->status == RTP_TWCC_PACKET_STATUS_SMALL_DELTA) {
      GST_WRITE_UINT8 (fci_data, delta);
      fci_data += 1;
    } else if (pkt->status == RTP_TWCC_PACKET_STATUS_LARGE_NEGATIVE_DELTA) {
      GST_WRITE_UINT16_BE (fci_data, delta);
      fci_data += 2;
    }
  }
}

static void
rtp_twcc_write_run_length_chunk (GArray * packet_chunks,
    RTPTWCCPacketStatus status, guint run_length)
{
  TWCCPacketChunk chunk;
  guint written = 0;
  while (written < run_length) {
    guint len = MIN (run_length - written, 8191);
    GST_DEBUG ("Writing a run-lenght of %u with status %u", len, status);
    GST_WRITE_UINT16_BE (&chunk.data, len);
    chunk.RunLength.chunk_type = RTP_TWCC_CHUNK_TYPE_RUN_LENGTH;
    chunk.RunLength.packet_status_symbol = status;
    g_array_append_val (packet_chunks, chunk);
    written += len;
  }
}

/* | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 */

typedef struct
{
  GArray *packet_chunks;
  TWCCPacketChunk chunk;
  guint num_bits;
  gint start_pos;
  gint pos;
} ChunkBitWriter;

static void
chunk_bit_writer_reset (ChunkBitWriter * writer)
{
  writer->chunk.data = 0;
  writer->chunk.chunk_type = RTP_TWCC_CHUNK_TYPE_STATUS_VECTOR;
  writer->chunk.StatusVector.symbol_size = writer->num_bits - 1; /* 0 for 1 bit, 1 for 2 bits */
  writer->pos = writer->start_pos;
}

static void
chunk_bit_writer_configure (ChunkBitWriter * writer, guint num_bits)
{
  writer->num_bits = num_bits;
  writer->start_pos = 13;       /* 2 bits used for chunk_type and symbol_size */
  chunk_bit_writer_reset (writer);
}

static gboolean
chunk_bit_writer_is_empty (ChunkBitWriter * writer)
{
  return writer->start_pos == writer->pos;
}

static guint
chunk_bit_writer_get_available_slots (ChunkBitWriter * writer)
{
  return writer->pos / writer->num_bits + 1;
}

static guint
chunk_bit_writer_get_total_slots (ChunkBitWriter * writer)
{
  return writer->start_pos / writer->num_bits + 1;
}

static void
chunk_bit_writer_flush (ChunkBitWriter * writer)
{
  /* don't append a chunk if no bits have been written */
  if (writer->pos < writer->start_pos) {
    g_array_append_val (writer->packet_chunks, writer->chunk);
    chunk_bit_writer_reset (writer);
  }
}

static void
chunk_bit_writer_init (ChunkBitWriter * writer,
    GArray * packet_chunks, guint num_bits)
{
  writer->packet_chunks = packet_chunks;
  chunk_bit_writer_configure (writer, num_bits);
}

#define SET_BITS(data, value, pos, num_bits) \
    (data |= ((value) & ((1 << (num_bits)) - 1)) << ((pos) * (num_bits)))

#define GET_BITS(data, pos, num_bits) \
    (((data) & (((1 << (num_bits)) - 1) << ((pos) * (num_bits)))) >> ((pos) * (num_bits)))

static void
chunk_bit_writer_write (ChunkBitWriter * writer, RTPTWCCPacketStatus status)
{
  guint pos;

  pos = writer->pos;
  pos += 8;
  if (pos > 15)
    pos -= 16;
  pos /= writer->num_bits;

  SET_BITS (writer->chunk.data, status, pos, writer->num_bits);

  GST_LOG ("WRITE status: %d pos: %d writer->pos: %u bits: %d",
      status, pos, writer->pos, writer->num_bits);
#if TWCC_DEBUG
  print_bits (&writer->chunk, 2);
#endif

  writer->pos -= writer->num_bits;
  if (writer->pos < 0) {
    chunk_bit_writer_flush (writer);
  }
}

static void
rtp_twcc_write_status_vector_chunk (ChunkBitWriter * writer,
    RecvPacket * pkt)
{
  if (pkt->missing_run > 0) {
    guint available = chunk_bit_writer_get_available_slots (writer);
    guint total = chunk_bit_writer_get_total_slots (writer);
    if (pkt->missing_run > (available + total)) {
      /* here it is better to finish up the current status-chunk and then
         go for run-length */
      for (guint i = 0; i < available; i++) {
        chunk_bit_writer_write (writer, RTP_TWCC_PACKET_STATUS_NOT_RECV);
      }
      rtp_twcc_write_run_length_chunk (writer->packet_chunks,
          RTP_TWCC_PACKET_STATUS_NOT_RECV, pkt->missing_run - available);
    } else {
      for (guint i = 0; i < pkt->missing_run; i++) {
        chunk_bit_writer_write (writer, RTP_TWCC_PACKET_STATUS_NOT_RECV);
      }
    }
  }

  chunk_bit_writer_write (writer, pkt->status);
}

typedef struct
{
  RecvPacket *equal;
} RunLengthHelper;

static void
run_lenght_helper_update (RunLengthHelper * rlh, RecvPacket * pkt)
{
  /* all status equal run */
  if (rlh->equal == NULL) {
    rlh->equal = pkt;
    rlh->equal->equal_run = 0;
  }

  if (rlh->equal->status == pkt->status) {
    rlh->equal->equal_run++;
  } else {
    rlh->equal = pkt;
    rlh->equal->equal_run = 1;
  }

  /* for missing packets we reset */
  if (pkt->missing_run > 0) {
    rlh->equal = NULL;
  }
}

static void
rtp_twcc_write_chunks (GArray * packet_chunks,
    GArray * twcc_packets, guint num_bits)
{
  ChunkBitWriter writer;
  guint i;
  guint bits_per_chunks = 7 * num_bits;

  chunk_bit_writer_init (&writer, packet_chunks, num_bits);

  for (i = 0; i < twcc_packets->len; i++) {
    RecvPacket *pkt = &g_array_index (twcc_packets, RecvPacket, i);
    guint remaining_packets = twcc_packets->len - i;

    /* we can only start a run-length chunk if the status-chunk is
       completed */
    if (chunk_bit_writer_is_empty (&writer)) {
      /* first write in any preceeding gaps, we use run-length
         if it would take up more than one chunk (14/7) */
      if (pkt->missing_run > bits_per_chunks) {
        rtp_twcc_write_run_length_chunk (packet_chunks,
            RTP_TWCC_PACKET_STATUS_NOT_RECV, pkt->missing_run);
      }

      /* we have a run of the same status, write a run-length chunk and skip
         to the next point */
      if (pkt->equal_run > bits_per_chunks ||
          pkt->equal_run == remaining_packets) {
        rtp_twcc_write_run_length_chunk (packet_chunks,
            pkt->status, pkt->equal_run);
        i += pkt->equal_run - 1;
        continue;
      }
    }

    GST_DEBUG ("i=%u: Writing a %u-bit vector of status: %u",
        i, num_bits, pkt->status);
    rtp_twcc_write_status_vector_chunk (&writer, pkt);
  }
  chunk_bit_writer_flush (&writer);
}

void
rtp_twcc_manager_add_fci (RTPTWCCManager * twcc, GstRTCPPacket * packet)
{
  RecvPacket *first, *last, *prev;
  guint16 packet_count;
  guint i;
  GArray *packet_chunks = g_array_new (FALSE, FALSE, 2);
  RTPTWCCHeader header;
  guint header_size = sizeof (RTPTWCCHeader);
  guint packet_chunks_size;
  guint recv_deltas_size = 0;
  guint16 fci_length;
  guint16 fci_chunks;
  guint8 *fci_data;
  guint8 *fci_data_ptr;
  RunLengthHelper rlh = { NULL };
  guint num_bits_for_status_vector = 1;

  g_array_sort (twcc->recv_packets, _twcc_seqnum_sort);

  /* get first and last packet */
  first = &g_array_index (twcc->recv_packets, RecvPacket, 0);
  last = &g_array_index (twcc->recv_packets, RecvPacket, twcc->recv_packets->len - 1);

  packet_count = last->seqnum - first->seqnum + 1;

  GST_WRITE_UINT16_BE (header.base_seqnum, first->seqnum);
  GST_WRITE_UINT16_BE (header.packet_count, packet_count);
  GST_WRITE_UINT24_BE (header.base_time, first->ts / REF_TIME_UNIT);
  GST_WRITE_UINT8 (header.fb_pkt_count, twcc->fb_pkt_count++);

  /* calculate all deltas and check for gaps etc */
  prev = first;
  for (i = 0; i < twcc->recv_packets->len; i++) {
    RecvPacket *pkt = &g_array_index (twcc->recv_packets, RecvPacket, i);
    if (i != 0)
      pkt->missing_run = pkt->seqnum - prev->seqnum - 1;

    pkt->delta_ts = GST_CLOCK_DIFF (prev->ts, pkt->ts);
    if (pkt->delta_ts < 0 || pkt->delta_ts > MAX_TS_DELTA) {
      pkt->status = RTP_TWCC_PACKET_STATUS_LARGE_NEGATIVE_DELTA;
      recv_deltas_size += 2;
      num_bits_for_status_vector = 2;
    } else {
      pkt->status = RTP_TWCC_PACKET_STATUS_SMALL_DELTA;
      recv_deltas_size += 1;
    }
    run_lenght_helper_update (&rlh, pkt);

    GST_DEBUG ("pkt: #%u, delta_ts: %" GST_TIME_FORMAT
        " missing_run: %u, status: %u", pkt->seqnum,
        GST_TIME_ARGS (pkt->delta_ts), pkt->missing_run, pkt->status);
    prev = pkt;
  }

  rtp_twcc_write_chunks (packet_chunks, twcc->recv_packets,
      num_bits_for_status_vector);

  packet_chunks_size = packet_chunks->len * 2;
  fci_length = header_size + packet_chunks_size + recv_deltas_size;
  fci_chunks = (fci_length  - 1) / sizeof (guint32) + 1;

  if (!gst_rtcp_packet_fb_set_fci_length (packet, fci_chunks))
    g_assert_not_reached ();

  fci_data = gst_rtcp_packet_fb_get_fci (packet);
  fci_data_ptr = fci_data;

  memcpy (fci_data_ptr, &header, header_size);
  fci_data_ptr += header_size;

  memcpy (fci_data_ptr, packet_chunks->data, packet_chunks_size);
  fci_data_ptr += packet_chunks_size;

  rtp_twcc_write_recv_deltas (fci_data_ptr, twcc->recv_packets);

#if TWCC_DEBUG
  g_print ("\nheader:\n");
  gst_util_dump_mem ((guint8 *) & header, header_size);

  g_print ("\npacket_chunks:\n");
  print_bits (packet_chunks->data, packet_chunks_size);
  gst_util_dump_mem ((guint8 *) packet_chunks->data, packet_chunks_size);

  g_print ("\nfull fci:\n");
  gst_util_dump_mem (fci_data, fci_length);
#endif

  g_array_unref (packet_chunks);

  /* FIXME */
  g_array_set_size (twcc->recv_packets, 0);
}


static void
sent_packet_init (SentPacket * packet, RTPPacketInfo * pinfo)
{
  packet->ts = pinfo->running_time;
  packet->seqnum = pinfo->twcc_seqnum;
  packet->size = pinfo->payload_len;
}


void
rtp_twcc_manager_send_packet (RTPTWCCManager * twcc,
    RTPPacketInfo * pinfo, guint8 ext_id)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  SentPacket packet;
  guint16 data;

  sent_packet_init (&packet, pinfo);
  g_array_append_val (twcc->sent_packets, packet);

  pinfo->data = gst_buffer_make_writable (pinfo->data);
  gst_rtp_buffer_map (pinfo->data, GST_MAP_READWRITE, &rtp);

  GST_WRITE_UINT16_BE (&data, twcc->send_seqnum++);
  gst_rtp_buffer_add_extension_onebyte_header (&rtp,
      ext_id, &data, 2);

  gst_rtp_buffer_unmap (&rtp);
}


static void
_add_twcc_packet (GArray * twcc_packets, guint16 seqnum, guint status)
{
  RecvPacket packet;
  memset (&packet, 0, sizeof (RecvPacket));
  packet.seqnum = seqnum;
  packet.status = status;
  g_array_append_val (twcc_packets, packet);
}

static guint
_parse_run_length_chunk (TWCCPacketChunk * chunk, GArray * twcc_packets,
    guint16 seqnum_offset, guint remaining_packets)
{
  guint status_code = chunk->RunLength.packet_status_symbol;
  guint run_length = chunk->data & ~0xE0;       /* mask out the 3 last bits */
  run_length = MIN (remaining_packets, GST_READ_UINT16_BE (&run_length));

  if (status_code != RTP_TWCC_PACKET_STATUS_NOT_RECV) {
    for (guint i = 0; i < run_length; i++) {
      _add_twcc_packet (twcc_packets, seqnum_offset + i, status_code);
    }
  }

  return run_length;
}

static guint
_parse_status_vector_chunk (TWCCPacketChunk * chunk, GArray * twcc_packets,
    guint16 seqnum_offset, guint remaining_packets)
{
  guint symbol_size = chunk->StatusVector.symbol_size + 1;
  guint num_bits = MIN (remaining_packets, 14 / symbol_size);

  for (guint i = 0; i < num_bits; i++) {
    guint status_code;
    guint pos = 21 - (i * symbol_size);
    if (pos > 15)
      pos -= 16;
    pos /= symbol_size;
    status_code = GET_BITS (chunk->data, pos, symbol_size);
    if (status_code != RTP_TWCC_PACKET_STATUS_NOT_RECV) {
      _add_twcc_packet (twcc_packets, seqnum_offset + i, status_code);
    }
  }

  return num_bits;
}

static void
_append_structure_to_value_array (GValueArray * array, GstStructure * s)
{
  GValue *val;
  g_value_array_append (array, NULL);
  val = g_value_array_get_nth (array, array->n_values - 1);
  g_value_init (val, GST_TYPE_STRUCTURE);
  g_value_take_boxed (val, s);
}

static void
_structure_take_value_array (GstStructure * s,
    const gchar * field_name, GValueArray * array)
{
  GValue value = G_VALUE_INIT;
  g_value_init (&value, G_TYPE_VALUE_ARRAY);
  g_value_take_boxed (&value, array);
  gst_structure_take_value (s, field_name, &value);
  g_value_unset (&value);
}

GstStructure *
rtp_twcc_parse_fci (guint8 * fci_data, guint fci_length)
{
  guint16 base_seqnum;
  guint16 packet_count;
  GstClockTime base_time;
  guint8 fb_pkt_count;
  GstStructure *ret = NULL;
  GArray *twcc_packets;
  guint packets_parsed = 0;
  guint fci_parsed;
  GstClockTime abs_time;
  GValueArray *array;
  guint i;

  if (fci_length < 10)
    return NULL;

  base_seqnum = GST_READ_UINT16_BE (&fci_data[0]);
  packet_count = GST_READ_UINT16_BE (&fci_data[2]);
  base_time = GST_READ_UINT24_BE (&fci_data[4]) * REF_TIME_UNIT;
  fb_pkt_count = fci_data[7];

  GST_DEBUG ("Parsed TWCC feedback: base_seqnum: #%u, packet_count: %u, "
      "base_time %" GST_TIME_FORMAT " fb_pkt_count: %u",
      base_seqnum, packet_count, GST_TIME_ARGS (base_time), fb_pkt_count);

  twcc_packets = g_array_sized_new (FALSE, FALSE,
      sizeof (RecvPacket), packet_count);

  fci_parsed = 8;
  while (packets_parsed < packet_count && (fci_parsed + 1) < fci_length) {
    TWCCPacketChunk *chunk = (TWCCPacketChunk *) & fci_data[fci_parsed];
    guint seqnum_offset = base_seqnum + packets_parsed;
    guint remaining_packets = packet_count - packets_parsed;

    if (chunk->chunk_type == RTP_TWCC_CHUNK_TYPE_RUN_LENGTH) {
      packets_parsed += _parse_run_length_chunk (chunk,
          twcc_packets, seqnum_offset, remaining_packets);
    } else {
      packets_parsed += _parse_status_vector_chunk (chunk,
          twcc_packets, seqnum_offset, remaining_packets);
    }
    fci_parsed += 2;
  }

  GST_DEBUG ("Parsed TWCC feedback: packets_parsed: %u, twcc_packets: %u "
      "fci_parsed: %u/%u", packets_parsed, twcc_packets->len,
      fci_parsed, fci_length);

  array = g_value_array_new (1);
  abs_time = base_time;
  for (i = 0; i < twcc_packets->len && fci_parsed < fci_length; i++) {
    RecvPacket *pkt = &g_array_index (twcc_packets, RecvPacket, i);
    GstStructure *pkt_s;

    if (pkt->status == RTP_TWCC_PACKET_STATUS_SMALL_DELTA) {
      abs_time += fci_data[fci_parsed] * DELTA_UNIT;
      fci_parsed += 1;
    } else if (pkt->status == RTP_TWCC_PACKET_STATUS_LARGE_NEGATIVE_DELTA) {
      abs_time += GST_READ_UINT16_BE (&fci_data[fci_parsed]) * DELTA_UNIT;
      fci_parsed += 2;
    }
    pkt->ts = abs_time;

    GST_INFO ("parsed: pkt: #%u, ts: %" GST_TIME_FORMAT " status: %u",
        pkt->seqnum, GST_TIME_ARGS (pkt->ts), pkt->status);
    pkt_s = gst_structure_new ("RecvPacket",
        "seqnum", G_TYPE_UINT, pkt->seqnum,
        "timestamp", G_TYPE_UINT64, pkt->ts, NULL);
    _append_structure_to_value_array (array, pkt_s);
  }
  g_array_unref (twcc_packets);

  ret = gst_structure_new_empty ("RTPTWCCPackets");
  _structure_take_value_array (ret, "packets", array);

  return ret;
}
