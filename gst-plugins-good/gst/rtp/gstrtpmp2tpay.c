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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <gst/rtp/gstrtpbuffer.h>

#include "gstrtpmp2tpay.h"

/* elementfactory information */
static const GstElementDetails gst_rtp_mp2t_pay_details =
GST_ELEMENT_DETAILS ("RTP MP2T audio payloader",
    "Codec/Payloader/Network",
    "Payload-encodes MPEG2 TS into RTP packets (RFC 2250)",
    "Wim Taymans <wim@fluendo.com>");

static GstStaticPadTemplate gst_rtp_mp2t_pay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts,"
        "packetsize=(int)188," "systemstream=(boolean)true")
    );

static GstStaticPadTemplate gst_rtp_mp2t_pay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"video\", "
        "payload = (int) " GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate = (int) 90000, " "encoding-name = (string) \"MP2T-ES\"")
    );

static gboolean gst_rtp_mp2t_pay_setcaps (GstBaseRTPPayload * payload,
    GstCaps * caps);
static GstFlowReturn gst_rtp_mp2t_pay_handle_buffer (GstBaseRTPPayload *
    payload, GstBuffer * buffer);

GST_BOILERPLATE (GstRTPMP2TPay, gst_rtp_mp2t_pay, GstBaseRTPPayload,
    GST_TYPE_BASE_RTP_PAYLOAD);

static void
gst_rtp_mp2t_pay_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_rtp_mp2t_pay_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_rtp_mp2t_pay_src_template));
  gst_element_class_set_details (element_class, &gst_rtp_mp2t_pay_details);
}

static void
gst_rtp_mp2t_pay_class_init (GstRTPMP2TPayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseRTPPayloadClass *gstbasertppayload_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasertppayload_class = (GstBaseRTPPayloadClass *) klass;

  gstbasertppayload_class->set_caps = gst_rtp_mp2t_pay_setcaps;
  gstbasertppayload_class->handle_buffer = gst_rtp_mp2t_pay_handle_buffer;
}

static void
gst_rtp_mp2t_pay_init (GstRTPMP2TPay * rtpmp2tpay, GstRTPMP2TPayClass * klass)
{
  GST_BASE_RTP_PAYLOAD (rtpmp2tpay)->clock_rate = 90000;
  GST_BASE_RTP_PAYLOAD_PT (rtpmp2tpay) = GST_RTP_PAYLOAD_MP2T;
}

static gboolean
gst_rtp_mp2t_pay_setcaps (GstBaseRTPPayload * payload, GstCaps * caps)
{
  const char *stname;
  GstStructure *structure;

  structure = gst_caps_get_structure (caps, 0);

  stname = gst_structure_get_name (structure);

  gst_basertppayload_set_options (payload, "video", TRUE, "MP2T-ES", 90000);
  gst_basertppayload_set_outcaps (payload, NULL);

  return TRUE;
}

static GstFlowReturn
gst_rtp_mp2t_pay_handle_buffer (GstBaseRTPPayload * basepayload,
    GstBuffer * buffer)
{
  GstRTPMP2TPay *rtpmp2tpay;
  guint size, payload_len;
  GstBuffer *outbuf;
  guint8 *payload, *data;
  GstClockTime timestamp;
  GstFlowReturn ret;

  rtpmp2tpay = GST_RTP_MP2T_PAY (basepayload);

  size = GST_BUFFER_SIZE (buffer);
  data = GST_BUFFER_DATA (buffer);
  timestamp = GST_BUFFER_TIMESTAMP (buffer);

  /* FIXME, only one MP2T frame per RTP packet for now */
  payload_len = size;

  outbuf = gst_rtp_buffer_new_allocate (payload_len, 0, 0);

  /* copy timestamp */
  GST_BUFFER_TIMESTAMP (outbuf) = timestamp;

  /* get payload */
  payload = gst_rtp_buffer_get_payload (outbuf);

  /* copy data in payload */
  memcpy (payload, data, size);

  gst_buffer_unref (buffer);

  GST_DEBUG_OBJECT (rtpmp2tpay, "pushing buffer of size %d",
      GST_BUFFER_SIZE (outbuf));

  ret = gst_basertppayload_push (basepayload, outbuf);

  return ret;
}

gboolean
gst_rtp_mp2t_pay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtpmp2tpay",
      GST_RANK_NONE, GST_TYPE_RTP_MP2T_PAY);
}
