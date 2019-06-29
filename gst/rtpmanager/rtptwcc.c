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
#include "rtptwcc.h"
#include <gst/rtp/gstrtcpbuffer.h>

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
  guint mtu;
  guint max_packets_per_rtcp;
  GArray *recv_packets;

  guint8 fb_pkt_count;
  gint32 last_seqnum;

  GArray *sent_packets;
  GArray *parsed_packets;
  GQueue *rtcp_buffers;

  guint64 recv_sender_ssrc;
  guint64 recv_media_ssrc;

  guint16 send_seqnum;

  gboolean first_fci_parse;
  guint16 expected_seqnum;
};

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

  gint64 delta;
  RTPTWCCPacketStatus status;
  guint16 missing_run;
  guint equal_run;
} RecvPacket;

typedef struct
{
  GstClockTime ts;
  GstClockTime socket_ts;
  GstClockTime remote_ts;
  guint16 seqnum;
  guint size;
  gboolean lost;
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
rtp_twcc_manager_new (guint mtu)
{
  RTPTWCCManager *twcc = g_new0 (RTPTWCCManager, 1);

  twcc->recv_packets = g_array_new (FALSE, FALSE, sizeof (RecvPacket));

  twcc->sent_packets = g_array_new (FALSE, FALSE, sizeof (SentPacket));
  twcc->parsed_packets = g_array_new (FALSE, FALSE, sizeof (RecvPacket));

  twcc->rtcp_buffers = g_queue_new ();

  twcc->last_seqnum = -1;
  twcc->recv_media_ssrc = -1;
  twcc->recv_sender_ssrc = -1;

  rtp_twcc_manager_set_mtu (twcc, mtu);

  twcc->first_fci_parse = TRUE;

  return twcc;
}

void
rtp_twcc_manager_free (RTPTWCCManager * twcc)
{
  g_array_unref (twcc->recv_packets);
  g_array_unref (twcc->sent_packets);
  g_array_unref (twcc->parsed_packets);
  g_queue_free_full (twcc->rtcp_buffers, (GDestroyNotify)gst_buffer_unref);
  g_free (twcc);
}

static void
recv_packet_init (RecvPacket * packet, RTPPacketInfo * pinfo)
{
  memset (packet, 0, sizeof (RecvPacket));
  packet->seqnum = pinfo->twcc_seqnum;
  packet->ts = pinfo->running_time;
}

void
rtp_twcc_manager_set_mtu (RTPTWCCManager * twcc, guint mtu)
{
  twcc->mtu = mtu;

  /* the absolute worst case is that 7 packets uses
     header (4 * 4 * 4) 32 bytes) and 
     packet_chunk 2 bytes +  
     recv_deltas (2 * 7) 14 bytes */
  twcc->max_packets_per_rtcp = ((twcc->mtu - 32) * 7) / (2 + 14);
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

    if (pkt->status == RTP_TWCC_PACKET_STATUS_SMALL_DELTA) {
      GST_WRITE_UINT8 (fci_data, pkt->delta);
      fci_data += 1;
    } else if (pkt->status == RTP_TWCC_PACKET_STATUS_LARGE_NEGATIVE_DELTA) {
      GST_WRITE_UINT16_BE (fci_data, pkt->delta);
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
    GST_LOG ("Writing a run-lenght of %u with status %u", len, status);
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
  writer->chunk.StatusVector.symbol_size = writer->num_bits - 1;        /* 0 for 1 bit, 1 for 2 bits */
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
rtp_twcc_write_status_vector_chunk (ChunkBitWriter * writer, RecvPacket * pkt)
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
  /* for missing packets we reset */
  if (pkt->missing_run > 0) {
    rlh->equal = NULL;
  }

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
      if (pkt->missing_run == 0 &&
          (pkt->equal_run > bits_per_chunks ||
              pkt->equal_run == remaining_packets)) {
        rtp_twcc_write_run_length_chunk (packet_chunks,
            pkt->status, pkt->equal_run);
        i += pkt->equal_run - 1;
        continue;
      }
    }

    GST_LOG ("i=%u: Writing a %u-bit vector of status: %u",
        i, num_bits, pkt->status);
    rtp_twcc_write_status_vector_chunk (&writer, pkt);
  }
  chunk_bit_writer_flush (&writer);
}

static void
rtp_twcc_manager_add_fci (RTPTWCCManager * twcc, GstRTCPPacket * packet)
{
  RecvPacket *first, *last, *prev;
  guint16 packet_count;
  GstClockTime base_time;
  GstClockTime ts_rounded;
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
  GstClockTimeDiff delta_ts;
  gint64 delta_ts_rounded;

  g_array_sort (twcc->recv_packets, _twcc_seqnum_sort);

  /* get first and last packet */
  first = &g_array_index (twcc->recv_packets, RecvPacket, 0);
  last =
      &g_array_index (twcc->recv_packets, RecvPacket,
      twcc->recv_packets->len - 1);

  packet_count = last->seqnum - first->seqnum + 1;
  base_time = first->ts / REF_TIME_UNIT;

  GST_WRITE_UINT16_BE (header.base_seqnum, first->seqnum);
  GST_WRITE_UINT16_BE (header.packet_count, packet_count);
  GST_WRITE_UINT24_BE (header.base_time, base_time);
  GST_WRITE_UINT8 (header.fb_pkt_count, twcc->fb_pkt_count);

  base_time *= REF_TIME_UNIT;
  ts_rounded = base_time;

  GST_DEBUG ("Created TWCC feedback: base_seqnum: #%u, packet_count: %u, "
      "base_time %" GST_TIME_FORMAT " fb_pkt_count: %u",
      first->seqnum, packet_count, GST_TIME_ARGS (base_time),
      twcc->fb_pkt_count);
  twcc->fb_pkt_count++;

  /* calculate all deltas and check for gaps etc */
  prev = first;
  for (i = 0; i < twcc->recv_packets->len; i++) {
    RecvPacket *pkt = &g_array_index (twcc->recv_packets, RecvPacket, i);
    if (i != 0) {
      pkt->missing_run = pkt->seqnum - prev->seqnum - 1;
    }

    delta_ts = GST_CLOCK_DIFF (ts_rounded, pkt->ts);
    pkt->delta = delta_ts / DELTA_UNIT;
    delta_ts_rounded = pkt->delta * DELTA_UNIT;
    ts_rounded += delta_ts_rounded;

    if (delta_ts_rounded < 0 || delta_ts_rounded > MAX_TS_DELTA) {
      pkt->status = RTP_TWCC_PACKET_STATUS_LARGE_NEGATIVE_DELTA;
      recv_deltas_size += 2;
      num_bits_for_status_vector = 2;
    } else {
      pkt->status = RTP_TWCC_PACKET_STATUS_SMALL_DELTA;
      recv_deltas_size += 1;
    }
    run_lenght_helper_update (&rlh, pkt);

    GST_LOG ("pkt: #%u, ts: %" GST_TIME_FORMAT
        " ts_rounded: %" GST_TIME_FORMAT
        " delta_ts: %" GST_STIME_FORMAT
        " delta_ts_rounded: %" GST_STIME_FORMAT
        " missing_run: %u, status: %u", pkt->seqnum,
        GST_TIME_ARGS (pkt->ts), GST_TIME_ARGS (ts_rounded),
        GST_STIME_ARGS (delta_ts), GST_STIME_ARGS (delta_ts_rounded),
        pkt->missing_run, pkt->status);
    prev = pkt;
  }

  rtp_twcc_write_chunks (packet_chunks, twcc->recv_packets,
      num_bits_for_status_vector);

  packet_chunks_size = packet_chunks->len * 2;
  fci_length = header_size + packet_chunks_size + recv_deltas_size;
  fci_chunks = (fci_length - 1) / sizeof (guint32) + 1;

  if (!gst_rtcp_packet_fb_set_fci_length (packet, fci_chunks)) {
    GST_ERROR ("Could not fit: %u packets", packet_count);
    g_assert_not_reached ();
  }

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
rtp_twcc_manager_create_feedback (RTPTWCCManager * twcc)
{
  GstBuffer *buf;
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;

  buf = gst_rtcp_buffer_new (twcc->mtu);

  gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcp);

  gst_rtcp_buffer_add_packet (&rtcp, GST_RTCP_TYPE_RTPFB, &packet);

  gst_rtcp_packet_fb_set_type (&packet, GST_RTCP_RTPFB_TYPE_TWCC);
  if (twcc->recv_sender_ssrc != 1)
    gst_rtcp_packet_fb_set_sender_ssrc (&packet, twcc->recv_sender_ssrc);
  gst_rtcp_packet_fb_set_media_ssrc (&packet, twcc->recv_media_ssrc);

  rtp_twcc_manager_add_fci (twcc, &packet);

  gst_rtcp_buffer_unmap (&rtcp);

  g_queue_push_tail (twcc->rtcp_buffers, buf);
}

/* we have calculated a (very pessimistic) max-packets per RTCP feedback,
   so this is to make sure we don't exceed that */
static gboolean
_exceeds_max_packets (RTPTWCCManager * twcc, guint16 seqnum)
{
  RecvPacket *first, *last;
  guint16 packet_count;

  if (twcc->recv_packets->len == 0)
    return FALSE;

  /* find the delta betwen first stored packet and this seqnum */
  first = &g_array_index (twcc->recv_packets, RecvPacket, 0);
  packet_count = seqnum - first->seqnum + 1;
  if (packet_count > twcc->max_packets_per_rtcp)
    return TRUE;

  /* then find the delta between last stored packet and this seqnum */
  last =
      &g_array_index (twcc->recv_packets, RecvPacket,
      twcc->recv_packets->len - 1);
  packet_count = seqnum - (last->seqnum + 1);
  if (packet_count > twcc->max_packets_per_rtcp)
    return TRUE;

  return FALSE;
}

/* in this case we could have lost the packet with the marker bit,
   so with a large (30) amount of packets, lost packets and still no marker,
   we send a feedback anyway */
static gboolean
_many_packets_some_lost (RTPTWCCManager * twcc, guint16 seqnum)
{
  RecvPacket *first;
  guint16 packet_count;
  guint received_packets = twcc->recv_packets->len;
  if (received_packets == 0)
    return FALSE;

  first = &g_array_index (twcc->recv_packets, RecvPacket, 0);
  packet_count = seqnum - first->seqnum + 1;
  /* packet-count larger than recevied-packets means we have lost packets */
  if (packet_count >= 30 && packet_count > received_packets)
    return TRUE;

  return FALSE;
}

gboolean
rtp_twcc_manager_recv_packet (RTPTWCCManager * twcc, RTPPacketInfo * pinfo)
{
  gboolean send_feedback = FALSE;
  RecvPacket packet;
  guint16 seqnum = pinfo->twcc_seqnum;

  /* if this packet would exceed the capacity of our MTU, we create a feedback
     with the current packets, and start over with this one */
  if (_exceeds_max_packets (twcc, seqnum)) {
    GST_INFO ("twcc-seqnum: %u would overflow max packets: %u, create feedback"
        " with current packets", seqnum, twcc->max_packets_per_rtcp);
    rtp_twcc_manager_create_feedback (twcc);
    send_feedback = TRUE;
  }

  /* we can have multiple ssrcs here, so just pick the first one */
  if (twcc->recv_media_ssrc == -1)
    twcc->recv_media_ssrc = pinfo->ssrc;

  /* store the packet for Transport-wide RTCP feedback message */
  recv_packet_init (&packet, pinfo);
  g_array_append_val (twcc->recv_packets, packet);
  twcc->last_seqnum = seqnum;
  GST_LOG ("Receive: twcc-seqnum: %u, marker: %d, ts: %" GST_TIME_FORMAT,
      seqnum, pinfo->marker, GST_TIME_ARGS (pinfo->running_time));

  if (pinfo->marker || _many_packets_some_lost (twcc, seqnum)) {
    rtp_twcc_manager_create_feedback (twcc);
    send_feedback = TRUE;
  }

  return send_feedback;
}

static void
_change_rtcp_fb_sender_ssrc (GstBuffer * buf, guint32 sender_ssrc)
{
  GstRTCPBuffer rtcp = GST_RTCP_BUFFER_INIT;
  GstRTCPPacket packet;
  gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcp);
  gst_rtcp_buffer_get_first_packet (&rtcp, &packet);
  gst_rtcp_packet_fb_set_sender_ssrc (&packet, sender_ssrc);
  gst_rtcp_buffer_unmap (&rtcp);
}

GstBuffer *
rtp_twcc_manager_get_feedback (RTPTWCCManager * twcc, guint sender_ssrc)
{
  GstBuffer *buf;
  buf = g_queue_pop_head (twcc->rtcp_buffers);

  if (buf && twcc->recv_sender_ssrc != sender_ssrc) {
    _change_rtcp_fb_sender_ssrc (buf, sender_ssrc);
    twcc->recv_sender_ssrc = sender_ssrc;
  }

  return buf;
}

static void
sent_packet_init (SentPacket * packet, RTPPacketInfo * pinfo)
{
  packet->ts = pinfo->running_time;
  packet->seqnum = pinfo->twcc_seqnum;
  packet->size = pinfo->payload_len;
  packet->remote_ts = GST_CLOCK_TIME_NONE;
  packet->socket_ts = GST_CLOCK_TIME_NONE;
  packet->lost = FALSE;
}

void
rtp_twcc_manager_send_packet (RTPTWCCManager * twcc,
    RTPPacketInfo * pinfo, guint8 ext_id)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  SentPacket packet;
  guint16 data;

  pinfo->twcc_seqnum = twcc->send_seqnum++;

  sent_packet_init (&packet, pinfo);
  g_array_append_val (twcc->sent_packets, packet);

  pinfo->data = gst_buffer_make_writable (pinfo->data);
  gst_rtp_buffer_map (pinfo->data, GST_MAP_READWRITE, &rtp);

  GST_WRITE_UINT16_BE (&data, pinfo->twcc_seqnum);
  gst_rtp_buffer_add_extension_onebyte_header (&rtp, ext_id, &data, 2);

  GST_LOG ("Send: twcc-seqnum: %u, marker: %d, ts: %" GST_TIME_FORMAT,
      pinfo->twcc_seqnum, pinfo->marker, GST_TIME_ARGS (pinfo->running_time));

  gst_rtp_buffer_unmap (&rtp);
}

void
rtp_twcc_manager_set_send_packet_ts (RTPTWCCManager * twcc,
    guint packet_id, GstClockTime ts)
{
  SentPacket *pkt = NULL;
  pkt = &g_array_index (twcc->sent_packets, SentPacket, packet_id);
  if (pkt) {
    pkt->socket_ts = ts;
    GST_DEBUG ("assigning: pkt-id: %u to packet: %u", packet_id, pkt->seqnum);
  }
}

static void
_add_twcc_packet (GArray * twcc_packets, guint16 seqnum, guint status)
{
  RTPTWCCPacket packet;
  memset (&packet, 0, sizeof (RTPTWCCPacket));
  packet.local_ts = GST_CLOCK_TIME_NONE;
  packet.remote_ts = GST_CLOCK_TIME_NONE;
  packet.local_delta = GST_CLOCK_STIME_NONE;
  packet.remote_delta = GST_CLOCK_STIME_NONE;
  packet.delta_delta = GST_CLOCK_STIME_NONE;
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

  for (guint i = 0; i < run_length; i++) {
    _add_twcc_packet (twcc_packets, seqnum_offset + i, status_code);
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
    _add_twcc_packet (twcc_packets, seqnum_offset + i, status_code);
  }

  return num_bits;
}

GArray *
rtp_twcc_manager_parse_fci (RTPTWCCManager * twcc,
    guint8 * fci_data, guint fci_length)
{
  GArray *twcc_packets;
  guint16 base_seqnum;
  guint16 packet_count;
  GstClockTime base_time;
  GstClockTime ts_rounded;
  guint8 fb_pkt_count;
  guint packets_lost;
  guint packets_parsed = 0;
  guint fci_parsed;
  guint i;

  if (fci_length < 10) {
    GST_WARNING ("Malformed TWCC RTCP feedback packet");
    return NULL;
  }

  base_seqnum = GST_READ_UINT16_BE (&fci_data[0]);
  packet_count = GST_READ_UINT16_BE (&fci_data[2]);
  base_time = GST_READ_UINT24_BE (&fci_data[4]) * REF_TIME_UNIT;
  fb_pkt_count = fci_data[7];

  GST_DEBUG ("Parsed TWCC feedback: base_seqnum: #%u, packet_count: %u, "
      "base_time %" GST_TIME_FORMAT " fb_pkt_count: %u",
      base_seqnum, packet_count, GST_TIME_ARGS (base_time), fb_pkt_count);

  if (twcc->first_fci_parse) {
    twcc->expected_seqnum = base_seqnum;
    twcc->first_fci_parse = FALSE;
  }

  packets_lost = base_seqnum - twcc->expected_seqnum;
  twcc_packets = g_array_sized_new (FALSE, FALSE,
      sizeof (RTPTWCCPacket), packets_lost + packet_count);

  for (i = 0; i < packets_lost; i++) {
    _add_twcc_packet (twcc_packets, twcc->expected_seqnum + i,
        RTP_TWCC_PACKET_STATUS_NOT_RECV);
  }
  twcc->expected_seqnum = base_seqnum + packet_count;

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

  ts_rounded = base_time;
  for (i = 0; i < twcc_packets->len; i++) {
    RTPTWCCPacket *pkt = &g_array_index (twcc_packets, RTPTWCCPacket, i);
    SentPacket *first = NULL;
    gint16 delta = 0;
    GstClockTimeDiff delta_ts;

    if (pkt->status == RTP_TWCC_PACKET_STATUS_SMALL_DELTA) {
      delta = fci_data[fci_parsed];
      fci_parsed += 1;
    } else if (pkt->status == RTP_TWCC_PACKET_STATUS_LARGE_NEGATIVE_DELTA) {
      delta = GST_READ_UINT16_BE (&fci_data[fci_parsed]);
      fci_parsed += 2;
    }

    if (fci_parsed > fci_length) {
      GST_WARNING ("Malformed TWCC RTCP feedback packet");
      g_array_set_size (twcc_packets, 0);
      break;
    }

    if (pkt->status != RTP_TWCC_PACKET_STATUS_NOT_RECV) {
      delta_ts = delta * DELTA_UNIT;
      ts_rounded += delta_ts;
      pkt->remote_ts = ts_rounded;

      GST_LOG ("pkt: #%u, remote_ts: %" GST_TIME_FORMAT
          " delta_ts: %" GST_STIME_FORMAT
          " status: %u", pkt->seqnum,
          GST_TIME_ARGS (pkt->remote_ts), GST_STIME_ARGS (delta_ts),
          pkt->status);
    }

    if (twcc->sent_packets->len > 0)
      first = &g_array_index (twcc->sent_packets, SentPacket, 0);
    if (first) {
      SentPacket *found = NULL;
      guint sent_idx = pkt->seqnum - first->seqnum;
      if (sent_idx < twcc->sent_packets->len)
        found = &g_array_index (twcc->sent_packets, SentPacket, sent_idx);

      if (found && found->seqnum == pkt->seqnum) {
        if (GST_CLOCK_TIME_IS_VALID (found->socket_ts)) {
          pkt->local_ts = found->socket_ts;
        } else {
          pkt->local_ts = found->ts;
        }
        pkt->size = found->size;

        GST_LOG ("matching pkt: #%u with local_ts: %" GST_TIME_FORMAT
            " size: %u", pkt->seqnum, GST_TIME_ARGS (pkt->local_ts), pkt->size);
      }
    }
  }

  return twcc_packets;
}
