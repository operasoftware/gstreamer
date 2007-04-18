/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim@fluendo.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <string.h>

#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>

#include "rtpsource.h"

GST_DEBUG_CATEGORY_STATIC (rtp_source_debug);
#define GST_CAT_DEFAULT rtp_source_debug

#define RTP_MAX_PROBATION_LEN	32

/* signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0
};

/* GObject vmethods */
static void rtp_source_finalize (GObject * object);

/* static guint rtp_source_signals[LAST_SIGNAL] = { 0 }; */

G_DEFINE_TYPE (RTPSource, rtp_source, G_TYPE_OBJECT);

static void
rtp_source_class_init (RTPSourceClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = rtp_source_finalize;

  GST_DEBUG_CATEGORY_INIT (rtp_source_debug, "rtpsource", 0, "RTP Source");
}

static void
rtp_source_init (RTPSource * src)
{
  /* sources are initialy on probation until we receive enough valid RTP
   * packets or a valid RTCP packet */
  src->validated = FALSE;
  src->probation = RTP_DEFAULT_PROBATION;

  src->payload = 0;
  src->clock_rate = -1;
  src->packets = g_queue_new ();

  src->stats.jitter = 0;
  src->stats.transit = -1;
  src->stats.curr_sr = 0;
  src->stats.curr_rr = 0;
}

static void
rtp_source_finalize (GObject * object)
{
  RTPSource *src;
  GstBuffer *buffer;

  src = RTP_SOURCE_CAST (object);

  while ((buffer = g_queue_pop_head (src->packets)))
    gst_buffer_unref (buffer);
  g_queue_free (src->packets);

  G_OBJECT_CLASS (rtp_source_parent_class)->finalize (object);
}

/**
 * rtp_source_new:
 * @ssrc: an SSRC
 *
 * Create a #RTPSource with @ssrc.
 *
 * Returns: a new #RTPSource. Use g_object_unref() after usage.
 */
RTPSource *
rtp_source_new (guint32 ssrc)
{
  RTPSource *src;

  src = g_object_new (RTP_TYPE_SOURCE, NULL);
  src->ssrc = ssrc;

  return src;
}

/**
 * rtp_source_set_callbacks:
 * @src: an #RTPSource
 * @cb: callback functions
 * @user_data: user data
 *
 * Set the callbacks for the source.
 */
void
rtp_source_set_callbacks (RTPSource * src, RTPSourceCallbacks * cb,
    gpointer user_data)
{
  g_return_if_fail (RTP_IS_SOURCE (src));

  src->callbacks.push_rtp = cb->push_rtp;
  src->callbacks.clock_rate = cb->clock_rate;
  src->user_data = user_data;
}

/**
 * rtp_source_set_as_csrc:
 * @src: an #RTPSource
 *
 * Configure @src as a CSRC, this will validate the RTpSource.
 */
void
rtp_source_set_as_csrc (RTPSource * src)
{
  g_return_if_fail (RTP_IS_SOURCE (src));

  src->validated = TRUE;
  src->is_csrc = TRUE;
}

/**
 * rtp_source_set_rtp_from:
 * @src: an #RTPSource
 * @address: the RTP address to set
 *
 * Set that @src is receiving RTP packets from @address. This is used for
 * collistion checking.
 */
void
rtp_source_set_rtp_from (RTPSource * src, GstNetAddress * address)
{
  g_return_if_fail (RTP_IS_SOURCE (src));

  src->have_rtp_from = TRUE;
  memcpy (&src->rtp_from, address, sizeof (GstNetAddress));
}

/**
 * rtp_source_set_rtcp_from:
 * @src: an #RTPSource
 * @address: the RTCP address to set
 *
 * Set that @src is receiving RTCP packets from @address. This is used for
 * collistion checking.
 */
void
rtp_source_set_rtcp_from (RTPSource * src, GstNetAddress * address)
{
  g_return_if_fail (RTP_IS_SOURCE (src));

  src->have_rtcp_from = TRUE;
  memcpy (&src->rtcp_from, address, sizeof (GstNetAddress));
}

static GstFlowReturn
push_packet (RTPSource * src, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;

  /* push queued packets first if any */
  while (!g_queue_is_empty (src->packets)) {
    GstBuffer *buffer = GST_BUFFER_CAST (g_queue_pop_head (src->packets));

    GST_DEBUG ("pushing queued packet");
    if (src->callbacks.push_rtp)
      src->callbacks.push_rtp (src, buffer, src->user_data);
    else
      gst_buffer_unref (buffer);
  }
  GST_DEBUG ("pushing new packet");
  /* push packet */
  if (src->callbacks.push_rtp)
    ret = src->callbacks.push_rtp (src, buffer, src->user_data);
  else
    gst_buffer_unref (buffer);

  return ret;
}

static gint
get_clock_rate (RTPSource * src, guint8 payload)
{
  if (payload != src->payload) {
    gint clock_rate = -1;

    if (src->callbacks.clock_rate)
      clock_rate = src->callbacks.clock_rate (src, payload, src->user_data);

    GST_DEBUG ("new payload %d, got clock-rate %d", payload, clock_rate);

    src->clock_rate = clock_rate;
    src->payload = payload;
  }
  return src->clock_rate;
}

static void
calculate_jitter (RTPSource * src, GstBuffer * buffer,
    RTPArrivalStats * arrival)
{
  GstClockTime current;
  guint32 rtparrival, transit, rtptime;
  gint32 diff;
  gint clock_rate;
  guint8 pt;

  /* get arrival time */
  if ((current = arrival->time) == GST_CLOCK_TIME_NONE)
    goto no_time;

  pt = gst_rtp_buffer_get_payload_type (buffer);

  /* get clockrate */
  if ((clock_rate = get_clock_rate (src, pt)) == -1)
    goto no_clock_rate;

  rtptime = gst_rtp_buffer_get_timestamp (buffer);

  /* convert arrival time to RTP timestamp units */
  rtparrival = gst_util_uint64_scale_int (current, clock_rate, GST_SECOND);

  /* transit time is difference with RTP timestamp */
  transit = rtparrival - rtptime;
  /* get diff with previous transit time */
  if (src->stats.transit != -1)
    diff = transit - src->stats.transit;
  else
    diff = 0;
  src->stats.transit = transit;
  if (diff < 0)
    diff = -diff;
  /* update jitter */
  src->stats.jitter += diff - ((src->stats.jitter + 8) >> 4);

  src->stats.prev_rtptime = src->stats.last_rtptime;
  src->stats.last_rtptime = rtparrival;

  GST_DEBUG ("rtparrival %u, rtptime %u, clock-rate %d, diff %d, jitter: %u",
      rtparrival, rtptime, clock_rate, diff, src->stats.jitter);

  return;

  /* ERRORS */
no_time:
  {
    GST_WARNING ("cannot get current time");
    return;
  }
no_clock_rate:
  {
    GST_WARNING ("cannot get clock-rate for pt %d", pt);
    return;
  }
}

/**
 * rtp_source_process_rtp:
 * @src: an #RTPSource
 * @buffer: an RTP buffer
 *
 * Let @src handle the incomming RTP @buffer.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
rtp_source_process_rtp (RTPSource * src, GstBuffer * buffer,
    RTPArrivalStats * arrival)
{
  GstFlowReturn result = GST_FLOW_OK;

  g_return_val_if_fail (RTP_IS_SOURCE (src), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), GST_FLOW_ERROR);

  /* if we are still on probation, check seqnum */
  if (src->probation) {
    guint16 seqnr, expected;

    expected = src->stats.max_seqnr + 1;

    /* when in probation, we require consecutive seqnums */
    seqnr = gst_rtp_buffer_get_seq (buffer);
    if (seqnr == expected) {
      /* expected packet */
      src->probation--;
      src->stats.max_seqnr = seqnr;

      GST_DEBUG ("probation: seqnr %d == expected %d", seqnr, expected);
    } else {
      GST_DEBUG ("probation: seqnr %d != expected %d", seqnr, expected);
      src->probation = RTP_DEFAULT_PROBATION;
      src->stats.max_seqnr = seqnr;
    }
  }
  if (src->probation) {
    GstBuffer *q;

    GST_DEBUG ("probation %d: queue buffer", src->probation);
    /* when still in probation, keep packets in a list. */
    g_queue_push_tail (src->packets, buffer);
    /* remove packets from queue if there are too many */
    while (g_queue_get_length (src->packets) > RTP_MAX_PROBATION_LEN) {
      q = g_queue_pop_head (src->packets);
      gst_object_unref (q);
    }
  } else {
    /* we are not in probation */
    src->stats.octetsreceived += arrival->payload_len;
    src->stats.bytesreceived += arrival->bytes;
    src->stats.packetsreceived++;
    src->is_sender = TRUE;

    GST_DEBUG ("PC: %" G_GUINT64_FORMAT ", OC: %" G_GUINT64_FORMAT,
        src->stats.packetsreceived, src->stats.octetsreceived);

    /* calculate jitter */
    calculate_jitter (src, buffer, arrival);

    /* we're ready to push the RTP packet now */
    result = push_packet (src, buffer);
  }
  return result;
}

/**
 * rtp_source_process_bye:
 * @src: an #RTPSource
 * @reason: the reason for leaving
 *
 * Notify @src that a BYE packet has been received. This will make the source
 * inactive.
 */
void
rtp_source_process_bye (RTPSource * src, const gchar * reason)
{
  g_return_if_fail (RTP_IS_SOURCE (src));

  GST_DEBUG ("marking SSRC %08x as BYE, reason: %s", src->ssrc,
      GST_STR_NULL (reason));

  /* copy the reason and mark as received_bye */
  g_free (src->bye_reason);
  src->bye_reason = g_strdup (reason);
  src->received_bye = TRUE;
}

/**
 * rtp_source_send_rtp:
 * @src: an #RTPSource
 * @buffer: an RTP buffer
 *
 * Send an RTP @buffer originating from @src. This will make @src a sender.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
rtp_source_send_rtp (RTPSource * src, GstBuffer * buffer)
{
  GstFlowReturn result = GST_FLOW_OK;

  g_return_val_if_fail (RTP_IS_SOURCE (src), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), GST_FLOW_ERROR);

  /* we are a sender now */
  src->is_sender = TRUE;

  /* push packet */
  if (src->callbacks.push_rtp)
    result = src->callbacks.push_rtp (src, buffer, src->user_data);
  else
    gst_buffer_unref (buffer);

  return result;
}

/**
 * rtp_source_process_sr:
 * @src: an #RTPSource
 * @ntptime: the NTP time
 * @rtptime: the RTP time
 * @packet_count: the packet count
 * @octet_count: the octect count
 *
 * Update the sender report in @src.
 */
void
rtp_source_process_sr (RTPSource * src, guint64 ntptime, guint32 rtptime,
    guint32 packet_count, guint32 octet_count)
{
  RTPSenderReport *curr;
  gint curridx;

  g_return_if_fail (RTP_IS_SOURCE (src));

  GST_DEBUG ("got SR packet: SSRC %08x, NTP %" G_GUINT64_FORMAT
      ", RTP %u, PC %u, OC %u", src->ssrc, ntptime, rtptime, packet_count,
      octet_count);

  curridx = src->stats.curr_sr ^ 1;
  curr = &src->stats.sr[curridx];

  /* update current */
  curr->is_valid = TRUE;
  curr->ntptime = ntptime;
  curr->rtptime = rtptime;
  curr->packet_count = packet_count;
  curr->octet_count = octet_count;

  /* make current */
  src->stats.curr_sr = curridx;
}

/**
 * rtp_source_process_rb:
 * @src: an #RTPSource
 * @fractionlost: fraction lost since last SR/RR
 * @packetslost: the cumululative number of packets lost
 * @exthighestseq: the extended last sequence number received
 * @jitter: the interarrival jitter
 * @lsr: the last SR packet from this source
 * @dlsr: the delay since last SR packet
 *
 * Update the report block in @src.
 */
void
rtp_source_process_rb (RTPSource * src, guint8 fractionlost, gint32 packetslost,
    guint32 exthighestseq, guint32 jitter, guint32 lsr, guint32 dlsr)
{
  RTPReceiverReport *curr;
  gint curridx;

  g_return_if_fail (RTP_IS_SOURCE (src));

  GST_DEBUG ("got RB packet %d: SSRC %08x, FL %u"
      ", PL %u, HS %u, JITTER %u, LSR %u, DLSR %u", src->ssrc, fractionlost,
      packetslost, exthighestseq, jitter, lsr, dlsr);

  curridx = src->stats.curr_rr ^ 1;
  curr = &src->stats.rr[curridx];

  /* update current */
  curr->is_valid = TRUE;
  curr->fractionlost = fractionlost;
  curr->packetslost = packetslost;
  curr->exthighestseq = exthighestseq;
  curr->jitter = jitter;
  curr->lsr = lsr;
  curr->dlsr = dlsr;

  /* make current */
  src->stats.curr_rr = curridx;
}
