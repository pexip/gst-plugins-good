/* GStreamer
 * Copyright (C) 2019 Pexip (http://pexip.com/)
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

#ifndef __RTP_TWCC_H__
#define __RTP_TWCC_H__

#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include "rtpstats.h"

typedef struct _RTPTWCCManager RTPTWCCManager;

RTPTWCCManager * rtp_twcc_manager_new (void);
void rtp_twcc_manager_free (RTPTWCCManager * twcc);

void rtp_twcc_manager_recv_packet (RTPTWCCManager * twcc, RTPPacketInfo * pinfo);
void rtp_twcc_manager_send_packet (RTPTWCCManager * twcc,
    RTPPacketInfo * pinfo, guint8 ext_id);

void rtp_twcc_manager_add_fci (RTPTWCCManager * twcc, GstRTCPPacket * packet);

GstStructure * rtp_twcc_parse_fci (guint8 * fci_data, guint fci_length);

#endif /* __RTP_TWCC_H__ */
