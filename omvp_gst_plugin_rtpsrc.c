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

#include <string.h>
#include "config.h"
#include "omvp_gst_plugin_rtpsrc.h"

GST_DEBUG_CATEGORY_STATIC(omvp_rtpsrc_debug);
#define GST_CAT_DEFAULT omvp_rtpsrc_debug

enum {
  PROP_0,
  PROP_URI,
  PROP_LAST
};

#define DEFAULT_PROP_URI (NULL)

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
  GST_PAD_SRC, GST_PAD_SOMETIMES, GST_STATIC_CAPS("application/x-rtp"));

static gboolean gst_omvp_rtpsrc_parse_uri(const gchar *uristr, gchar **host,
  guint16 *port);
static GstURIType gst_omvp_rtpsrc_uri_get_type(GType type);
static const gchar *const *gst_omvp_rtpsrc_uri_get_protocols(GType type);
static gchar *gst_omvp_rtpsrc_uri_get_uri(GstURIHandler *handler);
static gboolean gst_omvp_rtpsrc_uri_set_uri(GstURIHandler *handler,
  const gchar *uri, GError **error);
static void gst_omvp_rtpsrc_uri_handler_init(gpointer g_iface,
  gpointer iface_data);
static void gst_omvp_rtpsrc_set_property(GObject *object, guint prop_id,
  const GValue *value, GParamSpec *pspec);
static void gst_omvp_rtpsrc_get_property(GObject *object, guint prop_id,
  GValue *value, GParamSpec *pspec);
static void gst_omvp_rtpsrc_finalize(GObject *gobject);
static void gst_omvp_rtpsrc_rtpbin_pad_added_cb(GstElement *element,
  GstPad *pad, gpointer data);
static gboolean gst_omvp_rtpsrc_start(GstOMVPRtpSrc *rtpsrc);
static GstStateChangeReturn gst_omvp_rtpsrc_change_state(GstElement *element,
  GstStateChange transition);

static gboolean gst_omvp_rtpsrc_parse_uri(const gchar *uristr, gchar **host,
  guint16 *port) {
  gchar *protocol, *location_start;
  gchar *location, *location_end;
  gchar *colptr;

  protocol = gst_uri_get_protocol(uristr);
  if (!protocol) {
    goto no_protocol;
  }
  if (strcmp(protocol, "ortp") != 0) {
    goto wrong_protocol;
  }
  g_free(protocol);

  location_start = gst_uri_get_location(uristr);
  if (!location_start) {
    return FALSE;
  }

  GST_DEBUG("got location '%s'", location_start);

  location = g_strstr_len(location_start, -1, "@");
  if (location == NULL) {
    location = location_start;
  }
  else {
    location += 1;
  }

  if (location[0] == '[') {
    GST_DEBUG("parse IPV6 address '%s'", location);
    location_end = strchr(location, ']');
    if (location_end == NULL) {
      goto wrong_address;
    }

    *host = g_strndup(location + 1, location_end - location - 1);
    colptr = strrchr(location_end, ':');
  } else {
    GST_DEBUG("parse IPV4 address '%s'", location);
    colptr = strrchr(location, ':');

    if (colptr != NULL) {
      *host = g_strndup(location, colptr - location);
    } else {
      *host = g_strdup(location);
    }
  }
  GST_DEBUG("host set to '%s'", *host);

  if (colptr != NULL) {
    *port = g_ascii_strtoll(colptr + 1, NULL, 10);
  } else {
    *port = 0;
  }
  g_free(location_start);

  return TRUE;

no_protocol:
  {
    GST_ERROR("error parsing uri %s: no protocol", uristr);
    return FALSE;
  }
wrong_protocol:
  {
    GST_ERROR("error parsing uri %s: wrong protocol (%s != ortp)", uristr,
      protocol);
    g_free(protocol);
    return FALSE;
  }
wrong_address:
  {
    GST_ERROR("error parsing uri %s", uristr);
    g_free(location);
    return FALSE;
  }
}

#define gst_omvp_rtpsrc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(GstOMVPRtpSrc, gst_omvp_rtpsrc, GST_TYPE_BIN,
  G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER,
    gst_omvp_rtpsrc_uri_handler_init))

static void gst_omvp_rtpsrc_class_init(GstOMVPRtpSrcClass *klass) {
  GObjectClass *oclass = G_OBJECT_CLASS(klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);

  oclass->set_property = gst_omvp_rtpsrc_set_property;
  oclass->get_property = gst_omvp_rtpsrc_get_property;
  oclass->finalize = gst_omvp_rtpsrc_finalize;

  g_object_class_install_property(oclass, PROP_URI,
    g_param_spec_string("uri", "URI", "URI of the media to play",
      DEFAULT_PROP_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template(gstelement_class,
    gst_static_pad_template_get(&src_template));

  gstelement_class->change_state =
    GST_DEBUG_FUNCPTR(gst_omvp_rtpsrc_change_state);

  gst_element_class_set_static_metadata(gstelement_class, "omvprtpsrc",
    "Generic/Bin/Src", "OMVP RTP Src", "Taeho Oh <ohhara@postech.edu>");

  GST_DEBUG_CATEGORY_INIT(omvp_rtpsrc_debug, "omvprtpsrc", 0, "OMVP RTP Src");
}

static void gst_omvp_rtpsrc_init(GstOMVPRtpSrc *rtpsrc) {
  rtpsrc->uri = NULL;
  rtpsrc->udpsrc = NULL;
  rtpsrc->rtpbin = NULL;
  rtpsrc->n_pads = 0;

  GST_DEBUG_OBJECT(rtpsrc, "omvprtpsrc initialized");
}

static GstURIType gst_omvp_rtpsrc_uri_get_type(GType type) {
  (void)type;
  return GST_URI_SRC;
}

static const gchar *const *gst_omvp_rtpsrc_uri_get_protocols(GType type) {
  static const gchar *protocols[] = { "ortp", NULL };

  (void)type;

  return protocols;
}

static gchar *gst_omvp_rtpsrc_uri_get_uri(GstURIHandler *handler) {
  GstOMVPRtpSrc *rtpsrc = GST_OMVP_RTPSRC(handler);

  return g_strdup(rtpsrc->uri);
}

static gboolean gst_omvp_rtpsrc_uri_set_uri(GstURIHandler *handler,
  const gchar *uri, GError **error) {
  GstOMVPRtpSrc *rtpsrc = (GstOMVPRtpSrc *)handler;

  (void)error;

  g_object_set(G_OBJECT(rtpsrc), "uri", uri, NULL);

  return TRUE;
}

static void gst_omvp_rtpsrc_uri_handler_init(gpointer g_iface,
  gpointer iface_data) {
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *)g_iface;

  (void)iface_data;

  iface->get_type = gst_omvp_rtpsrc_uri_get_type;
  iface->get_protocols = gst_omvp_rtpsrc_uri_get_protocols;
  iface->get_uri = gst_omvp_rtpsrc_uri_get_uri;
  iface->set_uri = gst_omvp_rtpsrc_uri_set_uri;
}

static void gst_omvp_rtpsrc_set_property(GObject *object, guint prop_id,
  const GValue *value, GParamSpec *pspec) {
  GstOMVPRtpSrc *rtpsrc = GST_OMVP_RTPSRC(object);

  switch (prop_id) {
    case PROP_URI:
      g_free(rtpsrc->uri);
      rtpsrc->uri = g_strdup(g_value_get_string(value));
      if (rtpsrc->udpsrc) {
        gchar *host;
        guint16 port;
        if (gst_omvp_rtpsrc_parse_uri(rtpsrc->uri, &host, &port)) {
          gchar *udpsrc_uri;
          udpsrc_uri = g_strdup_printf("udp://%s:%u", host, port);
          g_object_set(G_OBJECT(rtpsrc->udpsrc), "uri", udpsrc_uri, NULL);
          g_free(host);
          g_free(udpsrc_uri);
        }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_omvp_rtpsrc_get_property(GObject *object, guint prop_id,
  GValue *value, GParamSpec *pspec) {
  GstOMVPRtpSrc *rtpsrc = GST_OMVP_RTPSRC(object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string(value, rtpsrc->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_omvp_rtpsrc_finalize(GObject *gobject) {
  GstOMVPRtpSrc *rtpsrc = GST_OMVP_RTPSRC(gobject);

  g_free(rtpsrc->uri);
  rtpsrc->uri = NULL;
  rtpsrc->udpsrc = NULL;
  rtpsrc->rtpbin = NULL;
  rtpsrc->n_pads = 0;

  G_OBJECT_CLASS(parent_class)->finalize(gobject);
}

static void gst_omvp_rtpsrc_rtpbin_pad_added_cb(GstElement *element,
  GstPad *pad, gpointer data) {
  gchar *name;
  GstOMVPRtpSrc *rtpsrc = (GstOMVPRtpSrc *)data;

  (void)element;

  name = gst_pad_get_name(pad);
  GST_DEBUG_OBJECT(rtpsrc, "adding a pad %s", name);

  if (strncmp(name, "recv_rtp_src_", strlen("recv_rtp_src_")) != 0) {
    g_free(name);
    GST_DEBUG_OBJECT(rtpsrc, "Pad name does not start with recv_rtp_src_");
    return;
  }
  g_free(name);

  if (GST_PAD_DIRECTION(pad) == GST_PAD_SINK) {
    GST_DEBUG_OBJECT(rtpsrc, "Pad is not src pad");
    return;
  }

  gst_object_ref(pad);

  if (rtpsrc->n_pads) {
    GST_DEBUG_OBJECT(rtpsrc, "Ignore SSRC");
    gst_ghost_pad_set_target(GST_GHOST_PAD(rtpsrc->ghostpad), pad);
    gst_object_unref(pad);
    return;
  }

  rtpsrc->ghostpad = gst_ghost_pad_new("src", pad);
  gst_pad_set_active(rtpsrc->ghostpad, TRUE);
  gst_element_add_pad(GST_ELEMENT(rtpsrc), rtpsrc->ghostpad);
  gst_object_unref(pad);

  rtpsrc->n_pads++;

  gst_element_no_more_pads(GST_ELEMENT(rtpsrc));

  return;
}

static gboolean gst_omvp_rtpsrc_start(GstOMVPRtpSrc *rtpsrc) {
  gchar *host;
  guint16 port;
  GstCaps *caps;

  GST_DEBUG_OBJECT(rtpsrc, "Creating elements");

  rtpsrc->udpsrc = gst_element_factory_make("udpsrc", NULL);
  if (!rtpsrc->udpsrc) {
    return FALSE;
  }

  rtpsrc->rtpbin = gst_element_factory_make("rtpbin", NULL);
  if (!rtpsrc->rtpbin) {
    gst_object_unref(rtpsrc->udpsrc);
    rtpsrc->udpsrc = NULL;
    return FALSE;
  }

  caps = gst_caps_new_simple("application/x-rtp",
    "media", G_TYPE_STRING, "video",
    "clock-rate", G_TYPE_INT, 90000,
    "payload", G_TYPE_INT, 33,
    NULL);
  g_object_set(G_OBJECT(rtpsrc->udpsrc), "caps", caps, NULL);
  gst_caps_unref(caps);

  if (gst_omvp_rtpsrc_parse_uri(rtpsrc->uri, &host, &port)) {
    gchar *udpsrc_uri;
    udpsrc_uri = g_strdup_printf("udp://%s:%u", host, port);
    g_object_set(G_OBJECT(rtpsrc->udpsrc), "uri", udpsrc_uri, NULL);
    g_free(host);
    g_free(udpsrc_uri);
  }

  gst_bin_add_many(GST_BIN(rtpsrc), rtpsrc->udpsrc, rtpsrc->rtpbin, NULL);
  gst_element_link_pads(rtpsrc->udpsrc, "src", rtpsrc->rtpbin,
    "recv_rtp_sink_0");

  g_signal_connect(rtpsrc->rtpbin, "pad-added",
    G_CALLBACK(gst_omvp_rtpsrc_rtpbin_pad_added_cb), rtpsrc);

  if (!gst_element_sync_state_with_parent(rtpsrc->udpsrc)) {
    GST_ERROR_OBJECT(rtpsrc, "Could not set udpsrc to playing");
  }
  if (!gst_element_sync_state_with_parent(rtpsrc->rtpbin)) {
    GST_ERROR_OBJECT(rtpsrc, "Could not set rtpbin to playing");
  }

  return TRUE;
}

static GstStateChangeReturn gst_omvp_rtpsrc_change_state(GstElement *element,
  GstStateChange transition) {
  GstOMVPRtpSrc *rtpsrc = GST_OMVP_RTPSRC(element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT(rtpsrc, "Configuring omvprtpsrc");
      if (!gst_omvp_rtpsrc_start(rtpsrc)) {
        GST_DEBUG_OBJECT(rtpsrc, "Start failed");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
}

gboolean omvp_gst_plugin_rtpsrc_init(GstPlugin *plugin) {
  gboolean ret;

  ret = gst_element_register(plugin, "omvprtpsrc", GST_RANK_NONE,
    GST_TYPE_OMVP_RTPSRC);

  return ret;
}
