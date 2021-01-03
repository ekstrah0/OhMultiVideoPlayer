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

#ifndef _OMVP_GST_H_
#define _OMVP_GST_H_

#include <clutter-gst/clutter-gst.h>

typedef void *OMVPGst;

typedef enum _OMVPGstCallbackID {
  OMVP_GST_CALLBACK_ID_NEW_FRAME,
  OMVP_GST_CALLBACK_ID_ERROR
} OMVPGstCallbackID;

typedef void (*OMVPGstCallback)(OMVPGstCallbackID id, gpointer user_data);

extern OMVPGst omvp_gst_open(const gchar *proxy_uri, const gchar *uri,
  ClutterActor *texture, gboolean scan, gint scan_width, gint scan_height,
  OMVPGstCallback callback, gpointer user_data);
extern gint omvp_gst_cancel_new_frame_callback(OMVPGst gst);
extern gint omvp_gst_close(OMVPGst gst);
extern gint omvp_gst_set_mute(OMVPGst gst, gboolean mute);
extern gboolean omvp_gst_get_mute(OMVPGst gst);
extern gint omvp_gst_set_volume(OMVPGst gst, gdouble volume);
extern gdouble omvp_gst_get_volume(OMVPGst gst);
extern gint omvp_gst_get_num_audio(OMVPGst gst);
extern gint omvp_gst_set_current_audio(OMVPGst gst, gint n_audio);
extern gint omvp_gst_get_current_audio(OMVPGst gst);
extern gchar *omvp_gst_get_audio_caps_str(OMVPGst gst);
extern gchar *omvp_gst_get_video_caps_str(OMVPGst gst);
extern gchar *omvp_gst_get_audio_tags_str(OMVPGst gst);
extern gchar *omvp_gst_get_video_tags_str(OMVPGst gst);

#endif /* _OMVP_GST_H_ */
