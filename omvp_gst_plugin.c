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

#include "config.h"
#include "omvp_gst_plugin.h"
#include "omvp_gst_plugin_rtpsrc.h"

#if 0

static gboolean omvp_gst_plugin_init(GstPlugin *plugin);

static gboolean omvp_gst_plugin_init(GstPlugin *plugin) {
  if (!omvp_gst_plugin_rtpsrc_init(plugin)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, omvprtp,
  "RTP protocol for " PACKAGE, omvp_gst_plugin_init, PACKAGE_VERSION, "GPL",
  PACKAGE, PACKAGE_URL)

#else

gboolean omvp_gst_plugin_register(void) {
  gboolean ret;

  ret = gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR,
    "omvprtpsrc", "RTP protocol for " PACKAGE, omvp_gst_plugin_rtpsrc_init,
    PACKAGE_VERSION, "GPL", PACKAGE, PACKAGE, PACKAGE_URL);

  return ret;
}

#endif
