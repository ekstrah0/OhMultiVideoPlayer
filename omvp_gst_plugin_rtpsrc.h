/*
  Oh! Multi Video Player
  Copyright (C) 2016 Taeho Oh <ohhara@postech.edu>

  This file is part of Oh! Multi Video Player.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _OMVP_GST_PLUGIN_RTPSRC_H_
#define _OMVP_GST_PLUGIN_RTPSRC_H_

#include <gst/gst.h>

#define GST_TYPE_OMVP_RTPSRC (gst_omvp_rtpsrc_get_type())
#define GST_OMVP_RTPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OMVP_RTPSRC, GstOMVPRtpSrc))
#define GST_OMVP_RTPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OMVP_RTPSRC, GstOMVPRtpSrcClass))
#define GST_IS_OMVP_RTPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OMVP_RTPSRC))
#define GST_IS_OMVP_RTPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OMVP_RTPSRC))
#define GST_OMVP_RTPSRC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_OMVP_RTPSRC, GstOMVPRtpSrcClass))

typedef struct _GstOMVPRtpSrcClass GstOMVPRtpSrcClass;
typedef struct _GstOMVPRtpSrc GstOMVPRtpSrc;

struct _GstOMVPRtpSrcClass {
  GstBinClass parent_class;
};

struct _GstOMVPRtpSrc {
  GstBin parent_instance;

  gchar *uri;
  GstElement *udpsrc;
  GstElement *rtpbin;
  GstPad *ghostpad;
  gint n_pads;
};

extern GType gst_omvp_rtpsrc_get_type(void);
extern gboolean omvp_gst_plugin_rtpsrc_init(GstPlugin *plugin);

#endif /* _OMVP_GST_PLUGIN_RTPSRC_H_ */
