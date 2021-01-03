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
#include "omvp_gst.h"

typedef struct _OMVPGstImpl {
  GstElement *play;
  gchar *audio_caps_str;
  gchar *video_caps_str;
  gchar *audio_tags_str;
  gchar *video_tags_str;
  guint bus_watch_id;
  gpointer instance;
  gulong handler_id;
  gdouble volume;
  gboolean mute;
  OMVPGstCallback callback;
  gpointer callback_data;
} OMVPGstImpl;

static gboolean _omvp_gst_bus_callback(
  GstBus *bus, GstMessage *message, gpointer data);
static void _omvp_gst_on_new_frame(gpointer priv, gpointer user_data);

static gboolean _omvp_gst_bus_callback(
  GstBus *bus, GstMessage *message, gpointer data) {
  OMVPGstImpl *gst_impl;

  (void)bus;
  gst_impl = (OMVPGstImpl *)data;

  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_TAG:
      {
        GstTagList *tags;
        gchar *str;
        gssize len;
        if (gst_impl->audio_tags_str && gst_impl->video_tags_str) {
          break;
        }
        gst_message_parse_tag(message, &tags);
        str = gst_tag_list_to_string(tags);
        len = strlen(str);
        g_debug("GST_MESSAGE_TAG %p |%s| |%s|",
          (void *)gst_impl, GST_MESSAGE_SRC_NAME(message), str);
        if (g_strstr_len(str, len, "audio-codec")) {
          g_free(gst_impl->audio_tags_str);
          gst_impl->audio_tags_str = str;
        } else if (g_strstr_len(str, len, "video-codec")) {
          g_free(gst_impl->video_tags_str);
          gst_impl->video_tags_str = str;
        } else {
          g_free(str);
        }
        gst_tag_list_unref(tags);
      }
      break;
    case GST_MESSAGE_INFO:
      {
        GError *err;
        gchar *str;
        gst_message_parse_info(message, &err, &str);
        g_debug("GST_MESSAGE_INFO %p |%s| |%s| |%s|",
          (void *)gst_impl, GST_MESSAGE_SRC_NAME(message), err->message, str);
        g_error_free(err);
        g_free(str);
      }
      break;
    case GST_MESSAGE_WARNING:
      {
        GError *err;
        gchar *str;
        gst_message_parse_warning(message, &err, &str);
        g_debug("GST_MESSAGE_WARNING %p |%s| |%s| |%s|",
          (void *)gst_impl, GST_MESSAGE_SRC_NAME(message), err->message, str);
        g_error_free(err);
        g_free(str);
      }
      break;
    case GST_MESSAGE_ERROR:
      {
        GError *err;
        gchar *str;
        gst_message_parse_error(message, &err, &str);
        g_debug("GST_MESSAGE_ERROR %p |%s| |%s| |%s|",
          (void *)gst_impl, GST_MESSAGE_SRC_NAME(message), err->message, str);
        g_error_free(err);
        g_free(str);
        if (gst_impl->callback) {
          gst_impl->callback(OMVP_GST_CALLBACK_ID_ERROR,
            gst_impl->callback_data);
        }
      }
      break;
    default:
#if 0
      g_debug("GST_MESSAGE %p |%s| |%s|",
        (void *)gst_impl, GST_MESSAGE_SRC_NAME(message),
        GST_MESSAGE_TYPE_NAME(message));
#endif
      break;
  }

  return TRUE;
}

static void _omvp_gst_on_new_frame(gpointer priv, gpointer user_data) {
  OMVPGstImpl *gst_impl;

  (void)priv;

  g_assert(user_data);

  gst_impl = (OMVPGstImpl *)user_data;

  g_assert(gst_impl->callback);

  gst_impl->callback(OMVP_GST_CALLBACK_ID_NEW_FRAME, gst_impl->callback_data);
}

OMVPGst omvp_gst_open(const gchar *proxy_uri, const gchar *uri,
  ClutterActor *texture, gboolean scan, gint scan_width, gint scan_height,
  OMVPGstCallback callback, gpointer user_data) {

  OMVPGstImpl *gst_impl;
  gchar *protocol;
  gchar *location;
  gchar *real_uri;
  GstElement *play;
  GstElement *scalesink;
  GstElement *scale;
  GstElement *sink;
  GstPad *pad;
  GstPad *ghostpad;
  GstCaps *caps;
  GstBus *bus;

  gst_impl = g_malloc0(sizeof(OMVPGstImpl));
  gst_impl->play = play = gst_element_factory_make("playbin", "play");
  if (gst_uri_is_valid(uri)) {
    protocol = gst_uri_get_protocol(uri);
    if (protocol) {
      location = gst_uri_get_location(uri);
      if (location) {
        if (proxy_uri) {
          if (strcmp(protocol, "ortp") == 0) {
            g_free(protocol);
            protocol = g_strdup("rtp");
          }
          real_uri = g_strdup_printf("%s/%s/%s", proxy_uri, protocol,
            location);
        } else {
          if (!gst_uri_protocol_is_supported(GST_URI_SRC, protocol)) {
            if (strcmp(protocol, "rtp") == 0) {
              g_free(protocol);
              protocol = g_strdup("ortp");
            }
          }
          real_uri = g_strdup_printf("%s://%s", protocol, location);
        }
        g_free(location);
      } else {
        real_uri = g_strdup(uri);
      }
      g_free(protocol);
    } else {
      real_uri = g_strdup(uri);
    }
  } else {
    real_uri = g_strdup(uri);
  }
  g_object_set(G_OBJECT(play), "uri", real_uri, NULL);
  scale = gst_element_factory_make("videoscale", "scale");
#if CLUTTER_GST_MAJOR_VERSION > 2
  {
    ClutterGstVideoSink *gst_video_sink;
    ClutterContent *content;
    content = clutter_actor_get_content(texture);
    if (content) {
      gst_video_sink = clutter_gst_content_get_sink(
        CLUTTER_GST_CONTENT(content));
    } else {
      gst_video_sink = clutter_gst_video_sink_new();
      content = clutter_gst_content_new_with_sink(gst_video_sink);
      clutter_actor_set_content(texture, content);
    }
    sink = GST_ELEMENT(gst_video_sink);
    if (callback) {
      gst_impl->instance = sink;
      g_object_add_weak_pointer(gst_impl->instance, &gst_impl->instance);
      gst_impl->callback = callback;
      gst_impl->callback_data = user_data;
      gst_impl->handler_id =
        g_signal_connect(sink, "new-frame", G_CALLBACK(_omvp_gst_on_new_frame),
          gst_impl);
    }
  }
#else
  {
    sink = gst_element_factory_make("autocluttersink", NULL);
    g_object_set(sink, "texture", texture, NULL);
    if (callback) {
      gst_impl->instance = texture;
      g_object_add_weak_pointer(gst_impl->instance, &gst_impl->instance);
      gst_impl->callback = callback;
      gst_impl->callback_data = user_data;
      gst_impl->handler_id =
        g_signal_connect(texture, "pixbuf-change",
          G_CALLBACK(_omvp_gst_on_new_frame), gst_impl);
    }
  }
#endif
  scalesink = gst_bin_new("scalesink");
  gst_bin_add_many(GST_BIN(scalesink), scale, sink, NULL);
  pad = gst_element_get_static_pad(scale, "sink");
  ghostpad = gst_ghost_pad_new("sink", pad);
  gst_element_add_pad(scalesink, ghostpad);
  gst_object_unref(GST_OBJECT(pad));
  if (scan) {
    guint flags;
    caps = gst_caps_new_simple("video/x-raw",
      "width", G_TYPE_INT, scan_width,
      "height", G_TYPE_INT, scan_height, NULL);
    gst_element_link_filtered(scale, sink, caps);
    gst_caps_unref(caps);
    g_object_get(play, "flags", &flags, NULL);
    flags &= (~0x00000002);
    g_object_set(play, "flags", flags, NULL);
  } else {
    GstElement *convertaudiosink;
    GstElement *convert;
    GstElement *resample;
    GstElement *audiosink;
    convertaudiosink = gst_bin_new("convertaudiosink");
    convert = gst_element_factory_make("audioconvert", "convert");
    resample = gst_element_factory_make("audioresample", "resample");
    audiosink = gst_element_factory_make("autoaudiosink", "audiosink");
    gst_bin_add_many(GST_BIN(convertaudiosink),
      convert, resample, audiosink, NULL);
    pad = gst_element_get_static_pad(convert, "sink");
    ghostpad = gst_ghost_pad_new("sink", pad);
    gst_element_add_pad(convertaudiosink, ghostpad);
    gst_object_unref(GST_OBJECT(pad));
    gst_element_link_many(convert, resample, audiosink, NULL);
    g_object_set(play, "audio-sink", convertaudiosink, NULL);
    gst_element_link(scale, sink);
  }
  g_object_set(play, "video-sink", scalesink, NULL);

  /* mute property doesn't work in some platform. */
  gst_impl->mute = TRUE;
  g_object_set(play, "volume", 0.0f, NULL);

  bus = gst_element_get_bus(play);
  gst_impl->bus_watch_id =
    gst_bus_add_watch(bus, _omvp_gst_bus_callback, gst_impl);
  gst_object_unref(bus);

  gst_element_set_state(play, GST_STATE_PLAYING);

  g_debug("omvp_gst_open uri(%s) real_uri(%s) texture(%p) gst(%p)",
    uri, real_uri, (void *)texture, (void *)gst_impl);
  g_free(real_uri);

  return (OMVPGst)gst_impl;
}

gint omvp_gst_cancel_new_frame_callback(OMVPGst gst) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);

  gst_impl = (OMVPGstImpl *)gst;
  if (gst_impl) {
    if (gst_impl->instance && gst_impl->handler_id) {
      g_signal_handler_disconnect(gst_impl->instance, gst_impl->handler_id);
      gst_impl->instance = NULL;
      gst_impl->handler_id = 0;
    }
  }

  return 0;
}

gint omvp_gst_close(OMVPGst gst) {
  OMVPGstImpl *gst_impl;
  gst_impl = (OMVPGstImpl *)gst;
  if (gst_impl) {
    if (gst_impl->instance && gst_impl->handler_id) {
      g_signal_handler_disconnect(gst_impl->instance, gst_impl->handler_id);
      gst_impl->instance = NULL;
      gst_impl->handler_id = 0;
      gst_impl->callback = NULL;
      gst_impl->callback_data = NULL;
    }
    gst_element_set_state(gst_impl->play, GST_STATE_NULL);
    g_source_remove(gst_impl->bus_watch_id);
    g_free(gst_impl->audio_caps_str);
    g_free(gst_impl->video_caps_str);
    g_free(gst_impl->audio_tags_str);
    g_free(gst_impl->video_tags_str);
    gst_object_unref(gst_impl->play);
    g_free(gst_impl);
  }

  g_debug("omvp_gst_close gst(%p)", gst);

  return 0;
}

gint omvp_gst_set_mute(OMVPGst gst, gboolean mute) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  gst_impl->mute = mute;
  if (mute) {
    g_object_set(gst_impl->play, "volume", 0.0f, NULL);
  } else {
    g_object_set(gst_impl->play, "volume", gst_impl->volume, NULL);
  }

  return 0;
}

gboolean omvp_gst_get_mute(OMVPGst gst) {
  OMVPGstImpl *gst_impl;
  gboolean mute;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  mute = gst_impl->mute;

  return mute;
}

gint omvp_gst_set_volume(OMVPGst gst, gdouble volume) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  gst_impl->volume = volume;
  if (!gst_impl->mute) {
    g_object_set(gst_impl->play, "volume", volume, NULL);
  }

  return 0;
}

gdouble omvp_gst_get_volume(OMVPGst gst) {
  OMVPGstImpl *gst_impl;
  gdouble volume;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  volume = gst_impl->volume;

  return volume;
}

gint omvp_gst_get_num_audio(OMVPGst gst) {
  OMVPGstImpl *gst_impl;
  gint num_audio;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  g_object_get(gst_impl->play, "n-audio", &num_audio, NULL);

  return num_audio;
}

gint omvp_gst_set_current_audio(OMVPGst gst, gint current_audio) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  g_object_set(gst_impl->play, "current-audio", current_audio, NULL);

  return 0;
}

gint omvp_gst_get_current_audio(OMVPGst gst) {
  OMVPGstImpl *gst_impl;
  gint current_audio;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  g_object_get(gst_impl->play, "current-audio", &current_audio, NULL);

  return current_audio;
}

gchar *omvp_gst_get_audio_caps_str(OMVPGst gst) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  if (!gst_impl->audio_caps_str) {
    GstElement *sink;
    GstPad *pad;
    GstCaps *caps;
    g_object_get(gst_impl->play, "audio-sink", &sink, NULL);
    pad = gst_element_get_static_pad(sink, "sink");
    caps = gst_pad_get_current_caps(pad);
    if (caps) {
      gst_impl->audio_caps_str = gst_caps_to_string(caps);
      gst_caps_unref(caps);
    }
    gst_object_unref(GST_OBJECT(pad));
    g_object_unref(sink);
  }

  return gst_impl->audio_caps_str;
}

gchar *omvp_gst_get_video_caps_str(OMVPGst gst) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;
  if (!gst_impl->video_caps_str) {
    GstElement *sink;
    GstPad *pad;
    GstCaps *caps;
    g_object_get(gst_impl->play, "video-sink", &sink, NULL);
    pad = gst_element_get_static_pad(sink, "sink");
    caps = gst_pad_get_current_caps(pad);
    if (caps) {
      gst_impl->video_caps_str = gst_caps_to_string(caps);
      gst_caps_unref(caps);
    }
    gst_object_unref(GST_OBJECT(pad));
    g_object_unref(sink);
  }

  return gst_impl->video_caps_str;
}

gchar *omvp_gst_get_audio_tags_str(OMVPGst gst) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;

  return gst_impl->audio_tags_str;
}

gchar *omvp_gst_get_video_tags_str(OMVPGst gst) {
  OMVPGstImpl *gst_impl;

  g_assert(gst);
  gst_impl = (OMVPGstImpl *)gst;

  return gst_impl->video_tags_str;
}
