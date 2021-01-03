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
#include <clutter-gst/clutter-gst.h>
#include "config.h"
#include "omvp_vids.h"
#include "omvp_gst.h"
#include "omvp_gst_plugin.h"

#define _OMVP_WIDTH_RATIO 16
#define _OMVP_HEIGHT_RATIO 9
#define _OMVP_SCAN_VID_SCALE 0.97f
#define _OMVP_VID_SCALE 1.00f
#define _OMVP_FOCUS_SCALE 1.03f
#define _OMVP_TEX_SIZE 500
#define _OMVP_TEXT_FONT "Monospace Bold 10"
#define _OMVP_SCAN_TEXT_FONT "Monospace Bold 50"

static gchar *_omvp_vids_filename = "omvp.m3u";
static gchar *_omvp_proxy_uri;
static gint _omvp_ani_duration = 300;
static gint _omvp_num_vid_per_row;
static gint _omvp_scan_num_jobs = 1;
static gint _omvp_scan_timeout = 10 * 1000;
static gint _omvp_text_info_timeout = 3 * 1000;
static gint _omvp_scan_width = 480;
static gint _omvp_scan_height = 270;
static gdouble _omvp_default_volume = 0.5f;
static gboolean _omvp_default_mute;
static gboolean _omvp_no_text_info;
static gboolean _omvp_show_debug;
static gboolean _omvp_show_version;
static gboolean _omvp_help_keys;

static GOptionEntry _omvp_entries[] = {
  { "proxy-uri", 'p', 0, G_OPTION_ARG_STRING, &_omvp_proxy_uri,
    "Proxy server uri", "uri" },
  { "ani-duration", 'a', 0, G_OPTION_ARG_INT, &_omvp_ani_duration,
    "Animation duration in milliseconds", "ms" },
  { "num-vid-per-row", 'n', 0, G_OPTION_ARG_INT, &_omvp_num_vid_per_row,
    "Number of videos per row", "videos" },
  { "scan-num-jobs", 'j', 0, G_OPTION_ARG_INT, &_omvp_scan_num_jobs,
    "Number of scanning jobs", "jobs" },
  { "scan-timeout", 't', 0, G_OPTION_ARG_INT, &_omvp_scan_timeout,
    "Scan timeout in milliseconds", "ms" },
  { "text-info-timeout", 'i', 0, G_OPTION_ARG_INT, &_omvp_text_info_timeout,
    "Text info timeout in milliseconds", "ms" },
  { "scan-width", 'w', 0, G_OPTION_ARG_INT, &_omvp_scan_width,
    "Scan video horizontal resolution", "width" },
  { "scan-height", 'h', 0, G_OPTION_ARG_INT, &_omvp_scan_height,
    "Scan video vertical resolution", "height" },
  { "volume", 'v', 0, G_OPTION_ARG_DOUBLE, &_omvp_default_volume,
    "Volume from 0.0(0%) to 1.0(100%)", "volume" },
  { "mute", 'm', 0, G_OPTION_ARG_NONE, &_omvp_default_mute,
    "Mute", NULL },
  { "no-text", 'o', 0, G_OPTION_ARG_NONE, &_omvp_no_text_info,
    "Do not show text", NULL },
  { "show-debug", 'd', 0, G_OPTION_ARG_NONE, &_omvp_show_debug,
    "Show debug text", NULL },
  { "version", 0, 0, G_OPTION_ARG_NONE, &_omvp_show_version,
    "Show version information", NULL },
  { "help-keys", 0, 0, G_OPTION_ARG_NONE, &_omvp_help_keys,
    "Help for shortcut keys", NULL },
  { NULL }
};

typedef enum _OMVPTextInfo {
  OMVP_TEXT_INFO_NONE,
  OMVP_TEXT_INFO_MAIN_ONLY,
  OMVP_TEXT_INFO_MAIN_AND_DEBUG,
  OMVP_TEXT_INFO_MAX
} OMVPTextInfo;

typedef struct _OMVPTexture {
  struct _OMVPPlayer *player;
  gint idx;
  ClutterActor *texture;
  ClutterContent *content;
} OMVPTexture;

typedef struct _OMVPPlayer {
  OMVPVids *vids;

  gfloat stage_org_width;
  gfloat stage_org_height;
  gfloat stage_width;
  gfloat stage_height;
  gfloat stage_x;
  gfloat stage_y;

  gfloat view_pos_x;
  gfloat view_pos_y;
  gint view_num_per_row;

  gfloat org_view_pos_x;
  gfloat org_view_pos_y;
  gint org_view_num_per_row;

  gint num_row;

  ClutterActor *root_actor;

  ClutterActor *focus_actor;

  OMVPTexture o_texture;
  ClutterActor *texture;
  ClutterActor *text;
  gboolean is_texture_showing;
  OMVPGst gst;
  gint vid_idx;

  OMVPTexture *o_scan_textures;
  ClutterActor **scan_textures;
  ClutterActor **scan_texts;
  ClutterActor *root_scan_texts;
  OMVPGst *scan_gsts;
  guint *scan_timeout_ids;
  gint scan_vid_idx;
  gint num_scan_vids;
  gint max_num_scan_vids;
  guint scan_texts_timeout_id;

  gdouble volume;
  gboolean mute;
  OMVPTextInfo text_info;
} OMVPPlayer;

static gint _omvp_remove_transition(ClutterActor *actor, const gchar *name);
static gint _omvp_calc_best_num_vid_per_row(gint num_vids);
static gint _omvp_calc_vid_idx(gint vid_idx_1, gint vid_idx_2,
  gint num_vids);
static gint _omvp_scan_vid_cancel_timeout(OMVPPlayer *player,
  gint scan_vid_idx);
static gboolean _omvp_scan_vid_timeout(gpointer user_data);
static gint _omvp_scan_vid_start(OMVPPlayer *player, gint scan_vid_idx);
static gint _omvp_scan_vid_start_all(OMVPPlayer *player);
static gint _omvp_scan_vid_finish(OMVPPlayer *player, gint scan_vid_idx);
static gint _omvp_scan_vid_finish_all(OMVPPlayer *player);
static gint _omvp_reshape(OMVPPlayer *player);
static gint _omvp_move_focus(OMVPPlayer *player);
static gint _omvp_get_view_bounds(OMVPPlayer *player, gint *pmin_view_pos_x,
  gint *pmax_view_pos_x, gint *pmin_view_pos_y, gint *pmax_view_pos_y);
static gint _omvp_is_focus_visible(OMVPPlayer *player);
static gint _omvp_auto_view(OMVPPlayer *player);
static gint _omvp_refresh_text(OMVPPlayer *player);
static gint _omvp_refresh_texture(OMVPPlayer *player);
static gint _omvp_scan_texts_timer_start(OMVPPlayer *player);
static gint _omvp_scan_texts_timer_cancel_timeout(OMVPPlayer *player);
static gboolean _omvp_scan_texts_timer_timeout(gpointer user_data);
static gboolean _omvp_on_key_press(ClutterActor *actor, ClutterEvent *event,
  gpointer user_data);
static void _omvp_on_allocation_changed(ClutterActor *actor,
  const ClutterActorBox *allocation, ClutterAllocationFlags flags,
  gpointer user_data);
static void _omvp_on_destroy(ClutterActor *actor, gpointer user_data);
static void _omvp_texture_on_callback(OMVPGstCallbackID id,
  gpointer user_data);
static void _omvp_scan_texture_on_callback(OMVPGstCallbackID id,
  gpointer user_data);

static gint _omvp_remove_transition(ClutterActor *actor, const gchar *name) {
  ClutterTransition *transition;
  transition = clutter_actor_get_transition(actor, name);
  if (transition) {
    clutter_transition_set_remove_on_complete(transition, TRUE);
    return 0;
  }
  g_debug("can't find transition(%s)", name);
  return -1;
}

static gint _omvp_calc_best_num_vid_per_row(gint num_vids) {
  gint i;

  i = 1;
  for (;;) {
    if (i * i >= num_vids) {
      break;
    }
    i++;
  }

  return i;
}

static gint _omvp_calc_vid_idx(gint vid_idx_1, gint vid_idx_2,
  gint num_vids) {
  gint vid_idx;
  vid_idx = vid_idx_1 + vid_idx_2;
  if (vid_idx >= num_vids) {
    vid_idx -= num_vids;
  }
  if (vid_idx < 0) {
    vid_idx += num_vids;
  }
  return vid_idx;
}

static gint _omvp_scan_vid_cancel_timeout(OMVPPlayer *player,
  gint scan_vid_idx) {

  if (player->scan_timeout_ids[scan_vid_idx]) {
    g_source_remove(player->scan_timeout_ids[scan_vid_idx]);
    player->scan_timeout_ids[scan_vid_idx] = 0;
  }

  return 0;
}

static gboolean _omvp_scan_vid_timeout(gpointer user_data) {
  OMVPTexture *o_texture;
  OMVPPlayer *player;

  o_texture = (OMVPTexture *)user_data;
  player = o_texture->player;

  _omvp_scan_vid_finish(player, o_texture->idx);
  _omvp_scan_vid_start(player, player->scan_vid_idx);
  player->scan_vid_idx =
    _omvp_calc_vid_idx(player->scan_vid_idx, 1, player->vids->num_vids);

  return FALSE;
}

static gint _omvp_scan_vid_start(OMVPPlayer *player, gint scan_vid_idx) {

  g_assert(player->scan_gsts[scan_vid_idx] == NULL);

  player->scan_gsts[scan_vid_idx] =
    omvp_gst_open(_omvp_proxy_uri, player->vids->uris[scan_vid_idx],
      player->scan_textures[scan_vid_idx], TRUE,
      _omvp_scan_width, _omvp_scan_height, _omvp_scan_texture_on_callback,
      &player->o_scan_textures[scan_vid_idx]);
  player->o_scan_textures[scan_vid_idx].content =
    clutter_actor_get_content(player->scan_textures[scan_vid_idx]);
  player->scan_timeout_ids[scan_vid_idx] =
    clutter_threads_add_timeout(
      _omvp_scan_timeout, _omvp_scan_vid_timeout,
      &player->o_scan_textures[scan_vid_idx]);
  ++player->num_scan_vids;

  return 0;
}

static gint _omvp_scan_vid_start_all(OMVPPlayer *player) {

  while (player->num_scan_vids != player->max_num_scan_vids) {
    _omvp_scan_vid_start(player, player->scan_vid_idx);
    player->scan_vid_idx =
      _omvp_calc_vid_idx(player->scan_vid_idx, 1, player->vids->num_vids);
  }

  return 0;
}

static gint _omvp_scan_vid_finish(OMVPPlayer *player, gint scan_vid_idx) {

  if (player->scan_timeout_ids[scan_vid_idx]) {
    _omvp_scan_vid_cancel_timeout(player, scan_vid_idx);
  }

  if (player->scan_gsts[scan_vid_idx]) {
    omvp_gst_close(player->scan_gsts[scan_vid_idx]);
    player->scan_gsts[scan_vid_idx] = NULL;
    --player->num_scan_vids;
  }

  return 0;
}

static gint _omvp_scan_vid_finish_all(OMVPPlayer *player) {
  gint i;

  for (i = 0; i < player->vids->num_vids; i++) {
    if (player->num_scan_vids == 0) {
      break;
    }
    _omvp_scan_vid_finish(player, i);
  }

  return 0;
}

static gint _omvp_reshape(OMVPPlayer *player) {
  gfloat ra_x, ra_y;
  gdouble ra_scale_x, ra_scale_y;

  ra_scale_x = player->stage_width /
    ((gfloat)player->view_num_per_row * (gfloat)_OMVP_TEX_SIZE);
  ra_scale_y = player->stage_height /
    ((gfloat)player->view_num_per_row * (gfloat)_OMVP_TEX_SIZE);

  ra_x = player->stage_x -
    (player->view_pos_x - (gfloat)player->view_num_per_row * 0.5f + 0.5f) *
    (gfloat)_OMVP_TEX_SIZE * ra_scale_x;
  ra_y = player->stage_y -
    (player->view_pos_y - (gfloat)player->view_num_per_row * 0.5f + 0.5f) *
    (gfloat)_OMVP_TEX_SIZE * ra_scale_y;

  g_debug("ra_x(%f) ra_y(%f) ra_scale_x(%f) ra_scale_y(%f)", ra_x, ra_y,
    ra_scale_x, ra_scale_y);

  clutter_actor_set_scale(player->root_actor, ra_scale_x, ra_scale_y);
  clutter_actor_set_position(player->root_actor, ra_x, ra_y);

  _omvp_remove_transition(player->root_actor, "position");
  _omvp_remove_transition(player->root_actor, "scale-x");
  _omvp_remove_transition(player->root_actor, "scale-y");

  if (player->view_num_per_row == 1) {
    clutter_actor_set_scale(player->texture,
      _OMVP_VID_SCALE, _OMVP_VID_SCALE);
  } else {
    clutter_actor_set_scale(player->texture,
      _OMVP_SCAN_VID_SCALE, _OMVP_SCAN_VID_SCALE);
  }

  return 0;
}

static gint _omvp_move_focus(OMVPPlayer *player) {
  player->is_texture_showing = FALSE;
  _omvp_refresh_texture(player);
  omvp_gst_close(player->gst);
  player->gst = omvp_gst_open(_omvp_proxy_uri,
    player->vids->uris[player->vid_idx], player->texture, FALSE, 0, 0,
    _omvp_texture_on_callback, &player->o_texture);
  player->o_texture.content = clutter_actor_get_content(player->texture);
  omvp_gst_set_volume(player->gst, player->volume);
  omvp_gst_set_mute(player->gst, player->mute);
  clutter_actor_set_position(player->focus_actor,
    (gfloat)(player->vid_idx % _omvp_num_vid_per_row) *
      (gfloat)_OMVP_TEX_SIZE,
    (gfloat)(player->vid_idx / _omvp_num_vid_per_row) *
      (gfloat)_OMVP_TEX_SIZE);
  return 0;
}

static gint _omvp_get_view_bounds(OMVPPlayer *player, gint *pmin_view_pos_x,
  gint *pmax_view_pos_x, gint *pmin_view_pos_y, gint *pmax_view_pos_y) {
  *pmin_view_pos_x = player->view_pos_x -
    (gfloat)(player->view_num_per_row - 1) / 2.0f;
  *pmax_view_pos_x = player->view_pos_x +
    (gfloat)(player->view_num_per_row - 1) / 2.0f;
  *pmin_view_pos_y = player->view_pos_y -
    (gfloat)(player->view_num_per_row - 1) / 2.0f;
  *pmax_view_pos_y = player->view_pos_y +
    (gfloat)(player->view_num_per_row - 1) / 2.0f;

  return 0;
}

static gint _omvp_is_focus_visible(OMVPPlayer *player) {
  gint focus_x;
  gint focus_y;
  gint min_view_pos_x;
  gint max_view_pos_x;
  gint min_view_pos_y;
  gint max_view_pos_y;

  focus_x = player->vid_idx % _omvp_num_vid_per_row;
  focus_y = player->vid_idx / _omvp_num_vid_per_row;
  _omvp_get_view_bounds(player, &min_view_pos_x, &max_view_pos_x,
    &min_view_pos_y, &max_view_pos_y);

  if (min_view_pos_x <= focus_x && focus_x <= max_view_pos_x &&
    min_view_pos_y <= focus_y && focus_y <= max_view_pos_y) {
    return TRUE;
  }

  return FALSE;
}

static gint _omvp_auto_view(OMVPPlayer *player) {
  gfloat pos_offset;
  player->view_pos_x = (gfloat)(player->vid_idx % _omvp_num_vid_per_row);
  player->view_pos_y = (gfloat)(player->vid_idx / _omvp_num_vid_per_row);
  if (player->num_row == 1) {
    pos_offset = player->vids->num_vids -
      ((gfloat)player->view_pos_x + 1.0f + player->view_num_per_row / 2);
  } else {
    pos_offset = _omvp_num_vid_per_row -
      ((gfloat)player->view_pos_x + 1.0f + player->view_num_per_row / 2);
  }
  if (pos_offset < 0.0f) {
    player->view_pos_x += pos_offset;
  }
  pos_offset = (gfloat)player->view_pos_x + 1.0f -
    (player->view_num_per_row + 1) / 2;
  if (pos_offset < 0.0f) {
    player->view_pos_x -= pos_offset;
  }
  pos_offset = player->num_row -
    ((gfloat)player->view_pos_y + 1.0f + player->view_num_per_row / 2);
  if (pos_offset < 0.0f) {
    player->view_pos_y += pos_offset;
  }
  pos_offset = (gfloat)player->view_pos_y + 1.0f -
    (player->view_num_per_row + 1) / 2;
  if (pos_offset < 0.0f) {
    player->view_pos_y -= pos_offset;
  }
  if (!(player->view_num_per_row & 1)) {
    player->view_pos_x += 0.5f;
    player->view_pos_y += 0.5f;
  }
  return 0;
}

static gint _omvp_refresh_text(OMVPPlayer *player) {
  gchar *text;
  text = g_strdup_printf(
    "vid_idx: %d\n"
    "uri: %s\n"
    "volume: %f\n"
    "mute: %u",
    player->vid_idx,
    player->vids->uris[player->vid_idx],
    player->volume,
    player->mute);
  if (player->is_texture_showing) {
    gchar *text2;
    text2 = g_strdup_printf(
      "%s\n"
      "audio_caps: %s\n"
      "audio_tags: %s\n"
      "video_caps: %s\n"
      "video_tags: %s\n"
      "num_audio: %d\n"
      "current_audio: %d\n",
      text,
      omvp_gst_get_audio_caps_str(player->gst),
      omvp_gst_get_audio_tags_str(player->gst),
      omvp_gst_get_video_caps_str(player->gst),
      omvp_gst_get_video_tags_str(player->gst),
      omvp_gst_get_num_audio(player->gst),
      omvp_gst_get_current_audio(player->gst));
    g_free(text);
    text = text2;
  }
  clutter_text_set_text(CLUTTER_TEXT(player->text), text);
  g_free(text);
  switch (player->text_info) {
    case OMVP_TEXT_INFO_NONE:
      clutter_actor_set_opacity(player->text, 0);
      clutter_actor_set_opacity(player->root_scan_texts, 0);
      break;
    case OMVP_TEXT_INFO_MAIN_ONLY:
      clutter_actor_set_opacity(player->text, 0);
      clutter_actor_set_opacity(player->root_scan_texts, 0xff);
      break;
    case OMVP_TEXT_INFO_MAIN_AND_DEBUG:
      clutter_actor_set_opacity(player->text, 0xff);
      clutter_actor_set_opacity(player->root_scan_texts, 0xff);
      break;
    default:
      g_assert(FALSE);
      break;
  }
  return 0;
}

static gint _omvp_refresh_texture(OMVPPlayer *player) {
  if (player->is_texture_showing) {
    clutter_actor_save_easing_state(player->texture);
    clutter_actor_set_easing_duration(player->texture, _omvp_ani_duration);
    clutter_actor_set_easing_mode(player->texture, CLUTTER_LINEAR);
    clutter_actor_set_opacity(player->texture, 0xff);
    clutter_actor_restore_easing_state(player->texture);
    _omvp_remove_transition(player->texture, "opacity");

    if (player->view_num_per_row == 1) {
      clutter_actor_set_scale(player->texture,
        _OMVP_SCAN_VID_SCALE, _OMVP_SCAN_VID_SCALE);
      clutter_actor_save_easing_state(player->texture);
      clutter_actor_set_easing_delay(player->texture, _omvp_ani_duration);
      clutter_actor_set_easing_duration(player->texture, _omvp_ani_duration);
      clutter_actor_set_easing_mode(player->texture, CLUTTER_EASE_OUT_CUBIC);
      clutter_actor_set_scale(player->texture,
        _OMVP_VID_SCALE, _OMVP_VID_SCALE);
      clutter_actor_restore_easing_state(player->texture);
      _omvp_remove_transition(player->texture, "scale-x");
      _omvp_remove_transition(player->texture, "scale-y");
    } else {
      clutter_actor_set_scale(player->texture,
        _OMVP_SCAN_VID_SCALE, _OMVP_SCAN_VID_SCALE);
    }
  } else {
    clutter_actor_set_opacity(player->texture, 0);
  }
  return 0;
}

static gint _omvp_scan_texts_timer_start(OMVPPlayer *player) {
  if (player->text_info == OMVP_TEXT_INFO_MAIN_ONLY) {
    player->scan_texts_timeout_id =
      clutter_threads_add_timeout(_omvp_text_info_timeout,
        _omvp_scan_texts_timer_timeout, player);
  }

  return 0;
}

static gint _omvp_scan_texts_timer_cancel_timeout(OMVPPlayer *player) {
  if (player->scan_texts_timeout_id) {
    g_source_remove(player->scan_texts_timeout_id);
    player->scan_texts_timeout_id = 0;
  }

  return 0;
}

static gboolean _omvp_scan_texts_timer_timeout(gpointer user_data) {
  OMVPPlayer *player;

  player = (OMVPPlayer *)user_data;

  player->scan_texts_timeout_id = 0;
  clutter_actor_set_opacity(player->root_scan_texts, 0);

  return FALSE;
}

static gboolean _omvp_on_key_press(ClutterActor *actor, ClutterEvent *event,
  gpointer user_data) {
  guint key_symbol;
  OMVPPlayer *player;

  player = (OMVPPlayer *)user_data;

  key_symbol = clutter_event_get_key_symbol(event);

  g_debug("on_key_press event key_symbol(%u)",
    clutter_event_get_key_symbol(event));

  switch (key_symbol) {
    case CLUTTER_KEY_F11:
      {
        gboolean fullscreen =
          clutter_stage_get_fullscreen(CLUTTER_STAGE(actor));
        clutter_stage_set_fullscreen(CLUTTER_STAGE(actor), !fullscreen);
        g_usleep(100 * 1000);
      }
      break;
    case CLUTTER_KEY_Left:
      --player->vid_idx;
      if (player->vid_idx < 0) {
        player->vid_idx = player->vids->num_vids - 1;
      }
      g_debug("vid_idx(%d)", player->vid_idx);
      _omvp_move_focus(player);
      if (!_omvp_is_focus_visible(player)) {
        player->view_pos_x -= 1.0f;
        if (!_omvp_is_focus_visible(player)) {
          player->view_pos_x =
            (gfloat)_omvp_num_vid_per_row - 1.0f -
            ((gfloat)player->view_num_per_row - 1.0f) / 2.0f;
          if (!_omvp_is_focus_visible(player)) {
            player->view_pos_y -= 1.0f;
            if (!_omvp_is_focus_visible(player)) {
              _omvp_auto_view(player);
            }
          }
        }
        _omvp_reshape(player);
      }
      break;
    case CLUTTER_KEY_Right:
      ++player->vid_idx;
      if (player->vid_idx >= player->vids->num_vids) {
        player->vid_idx = 0;
      }
      g_debug("vid_idx(%d)", player->vid_idx);
      _omvp_move_focus(player);
      if (!_omvp_is_focus_visible(player)) {
        player->view_pos_x += 1.0f;
        if (!_omvp_is_focus_visible(player)) {
          player->view_pos_x =
            ((gfloat)player->view_num_per_row - 1.0f) / 2.0f;
          if (!_omvp_is_focus_visible(player)) {
            player->view_pos_y += 1.0f;
            if (!_omvp_is_focus_visible(player)) {
              _omvp_auto_view(player);
            }
          }
        }
        _omvp_reshape(player);
      }
      break;
    case CLUTTER_KEY_Up:
      player->vid_idx -= _omvp_num_vid_per_row;
      if (player->vid_idx < 0) {
        player->vid_idx = (player->vids->num_vids - 1) /
          _omvp_num_vid_per_row * _omvp_num_vid_per_row +
          _omvp_num_vid_per_row + player->vid_idx;
        if (player->vid_idx >= player->vids->num_vids) {
          player->vid_idx -= _omvp_num_vid_per_row;
        }
      }
      g_debug("vid_idx(%d)", player->vid_idx);
      _omvp_move_focus(player);
      if (!_omvp_is_focus_visible(player)) {
        player->view_pos_y -= 1.0f;
        if (!_omvp_is_focus_visible(player)) {
          player->view_pos_y =
            (gfloat)player->num_row - 1.0f -
            ((gfloat)player->view_num_per_row - 1.0f) / 2.0f;
          if (!_omvp_is_focus_visible(player)) {
            _omvp_auto_view(player);
          }
        }
        _omvp_reshape(player);
      }
      break;
    case CLUTTER_KEY_Down:
      player->vid_idx += _omvp_num_vid_per_row;
      if (player->vid_idx >= player->vids->num_vids) {
        player->vid_idx %= _omvp_num_vid_per_row;
      }
      g_debug("vid_idx(%d)", player->vid_idx);
      _omvp_move_focus(player);
      if (!_omvp_is_focus_visible(player)) {
        player->view_pos_y += 1.0f;
        if (!_omvp_is_focus_visible(player)) {
          player->view_pos_y =
            ((gfloat)player->view_num_per_row - 1.0f) / 2.0f;
          if (!_omvp_is_focus_visible(player)) {
            _omvp_auto_view(player);
          }
        }
        _omvp_reshape(player);
      }
      break;
    case CLUTTER_KEY_Page_Down:
      player->volume -= 0.02;
      if (player->volume < 0.0) {
        player->volume = 0.0;
      }
      player->mute = FALSE;
      g_debug("volume(%f)", player->volume);
      g_debug("mute(%d)", player->mute);
      omvp_gst_set_volume(player->gst, player->volume);
      omvp_gst_set_mute(player->gst, player->mute);
      break;
    case CLUTTER_KEY_Page_Up:
      player->volume += 0.02;
      if (player->volume > 1.0) {
        player->volume = 1.0;
      }
      player->mute = FALSE;
      g_debug("volume(%f)", player->volume);
      g_debug("mute(%d)", player->mute);
      omvp_gst_set_volume(player->gst, player->volume);
      omvp_gst_set_mute(player->gst, player->mute);
      break;
    case CLUTTER_KEY_KP_Home:
    case CLUTTER_KEY_Home:
      {
        gint current_audio;
        gint num_audio;
        num_audio = omvp_gst_get_num_audio(player->gst);
        current_audio = omvp_gst_get_current_audio(player->gst);
        current_audio++;
        if (current_audio >= num_audio) {
          current_audio = 0;
        }
        g_debug("current_audio(%d)", current_audio);
        omvp_gst_set_current_audio(player->gst, current_audio);
      }
      break;
    case CLUTTER_KEY_KP_End:
    case CLUTTER_KEY_End:
      player->mute = !player->mute;
      g_debug("mute(%d)", player->mute);
      omvp_gst_set_mute(player->gst, player->mute);
      break;
    case CLUTTER_KEY_KP_Add:
    case CLUTTER_KEY_plus:
      player->view_num_per_row--;
      if (player->view_num_per_row == 0) {
        player->view_num_per_row = 1;
      }
      g_debug("zoom(%f)", 1.0f / (gfloat)player->view_num_per_row);
      _omvp_auto_view(player);
      _omvp_reshape(player);
      break;
    case CLUTTER_KEY_KP_Subtract:
    case CLUTTER_KEY_minus:
      player->view_num_per_row++;
      if (player->view_num_per_row >= _omvp_num_vid_per_row) {
        player->view_num_per_row = _omvp_num_vid_per_row;
      }
      g_debug("zoom(%f)", 1.0f / (gfloat)player->view_num_per_row);
      _omvp_auto_view(player);
      _omvp_reshape(player);
      break;
    case CLUTTER_KEY_KP_Enter:
    case CLUTTER_KEY_Return:
      if (player->view_num_per_row == 1) {
        player->view_pos_x = player->org_view_pos_x;
        player->view_pos_y = player->org_view_pos_y;
        player->view_num_per_row = player->org_view_num_per_row;
        if (!_omvp_is_focus_visible(player)) {
          _omvp_auto_view(player);
        }
        _omvp_reshape(player);
      } else {
        player->org_view_pos_x = player->view_pos_x;
        player->org_view_pos_y = player->view_pos_y;
        player->org_view_num_per_row = player->view_num_per_row;
        player->view_pos_x = player->vid_idx % _omvp_num_vid_per_row;
        player->view_pos_y = player->vid_idx / _omvp_num_vid_per_row;
        player->view_num_per_row = 1;
        _omvp_reshape(player);
      }
      break;
    case CLUTTER_KEY_KP_Insert:
    case CLUTTER_KEY_Insert:
      player->text_info++;
      if (player->text_info >= OMVP_TEXT_INFO_MAX) {
        player->text_info = 0;
      }
      break;
    case CLUTTER_KEY_Escape:
      {
        ClutterActor *stage;
        stage = clutter_actor_get_stage(player->root_actor);
        clutter_actor_destroy(stage);
        return TRUE;
      }
      break;
    default:
      return FALSE;
      break;
  }
  _omvp_refresh_text(player);
  _omvp_scan_texts_timer_cancel_timeout(player);
  _omvp_scan_texts_timer_start(player);

  return TRUE;
}

static void _omvp_on_allocation_changed(ClutterActor *actor,
  const ClutterActorBox *allocation, ClutterAllocationFlags flags,
  gpointer user_data) {
  ClutterActor *stage;
  gfloat new_x, new_y, new_width, new_height;
  gfloat stage_width, stage_height;
  OMVPPlayer *player;

  g_debug(
    "on_allocation_changed event x1(%f) y1(%f) x2(%f) y2(%f) flags(%d)",
    allocation->x1, allocation->y1, allocation->x2, allocation->y2, flags);

  stage = actor;
  player = (OMVPPlayer *)user_data;

  clutter_actor_get_size(stage, &stage_width, &stage_height);

  if (stage_width * _OMVP_HEIGHT_RATIO > stage_height * _OMVP_WIDTH_RATIO) {
    new_width = stage_height * _OMVP_WIDTH_RATIO / _OMVP_HEIGHT_RATIO;
    new_height = stage_height;
    new_x = (stage_width - new_width) / 2.0f;
    new_y = 0;
  } else {
    new_width = stage_width;
    new_height = stage_width * _OMVP_HEIGHT_RATIO / _OMVP_WIDTH_RATIO;
    new_x = 0;
    new_y = (stage_height - new_height) / 2.0f;
  }

  player->stage_org_width = stage_width;
  player->stage_org_height = stage_height;
  player->stage_width = new_width;
  player->stage_height = new_height;
  player->stage_x = new_x;
  player->stage_y = new_y;

  _omvp_reshape(player);

  g_debug("new_x(%f) new_y(%f) new_width(%f) new_height(%f)",
    new_x, new_y, new_width, new_height);
  return;
}

static void _omvp_on_destroy(ClutterActor *actor, gpointer user_data) {
  OMVPPlayer *player;
  gint i;

  (void)actor;

  player = (OMVPPlayer *)user_data;

  _omvp_scan_vid_finish_all(player);

  if (player->gst) {
    omvp_gst_close(player->gst);
    player->gst = NULL;
  }

  for (i = 0; i < player->vids->num_vids; i++) {
    g_object_unref(player->o_scan_textures[i].texture);
    if (player->o_scan_textures[i].content) {
      g_object_unref(player->o_scan_textures[i].content);
    }
  }
  g_object_unref(player->o_texture.texture);
  if (player->o_texture.content) {
    g_object_unref(player->o_texture.content);
  }

  g_free(player->scan_gsts);
  g_free(player->scan_texts);
  g_free(player->scan_textures);
  g_free(player->o_scan_textures);
  omvp_vids_close(player->vids);

  clutter_main_quit();
}

static void _omvp_texture_on_callback(OMVPGstCallbackID id,
  gpointer user_data) {
  OMVPTexture *o_texture;
  OMVPPlayer *player;

  o_texture = (OMVPTexture *)user_data;
  player = o_texture->player;

  switch (id) {
    case OMVP_GST_CALLBACK_ID_NEW_FRAME:
      player->is_texture_showing = TRUE;
      _omvp_refresh_texture(player);
      _omvp_refresh_text(player);
      _omvp_scan_texts_timer_cancel_timeout(player);
      _omvp_scan_texts_timer_start(player);

      if (omvp_gst_get_video_caps_str(player->gst) &&
        omvp_gst_get_audio_caps_str(player->gst)) {
        omvp_gst_cancel_new_frame_callback(player->gst);
      }
      break;
    case OMVP_GST_CALLBACK_ID_ERROR:
      _omvp_move_focus(player);
      break;
    default:
      g_assert(FALSE);
      break;
  }
}

static void _omvp_scan_texture_on_callback(OMVPGstCallbackID id,
  gpointer user_data) {
  OMVPTexture *o_texture;
  OMVPPlayer *player;
  ClutterActor *texture;
  gint scan_vid_idx;

  o_texture = (OMVPTexture *)user_data;
  player = o_texture->player;
  texture = o_texture->texture;
  scan_vid_idx = o_texture->idx;

  switch (id) {
    case OMVP_GST_CALLBACK_ID_NEW_FRAME:
      clutter_actor_set_opacity(texture, 0xff);
      break;
    case OMVP_GST_CALLBACK_ID_ERROR:
      break;
    default:
      g_assert(FALSE);
      break;
  }

  if (player->max_num_scan_vids != player->vids->num_vids) {
    _omvp_scan_vid_finish(player, scan_vid_idx);
    _omvp_scan_vid_start(player, player->scan_vid_idx);
    player->scan_vid_idx =
      _omvp_calc_vid_idx(player->scan_vid_idx, 1, player->vids->num_vids);
  }
}

int main(int argc, char *argv[]) {
  GError *error = NULL;
  OMVPPlayer player;
  ClutterActor *stage;
  ClutterColor stage_color = {0, 0, 0, 255};
  ClutterColor focus_color = {255, 255, 0, 255};
  ClutterColor text_color = {255, 255, 255, 255};
  gchar *text;
  PangoAttribute *text_attr;
  PangoAttrList *text_attrs;
  ClutterConstraint *constraint;
  gint i;

  if (clutter_gst_init_with_args(&argc, &argv, "[videos m3u/csv file]",
    _omvp_entries, NULL, &error) != CLUTTER_INIT_SUCCESS) {
    g_print("Failed to initialize: %s\n", error->message);
    g_error_free(error);
    return -1;
  }

  if (_omvp_show_version) {
    g_print(
      PACKAGE_STRING "\n"
      "Written by Taeho Oh <ohhara@postech.edu>\n"
      PACKAGE_URL "\n"
    );
    return 0;
  }

  if (_omvp_help_keys) {
    g_print(
      "Left/Right/Up/Down : Move focus left/right/up/down\n"
      "+/-                : Zoom in/out\n"
      "Enter              : Zoom in fully\n"
      "F11                : Fullscreen/Unfullscreen\n"
      "PgUp/PgDn          : Volume up/down\n"
      "Home               : Change audio\n"
      "End                : Mute/Unmute\n"
      "Insert             : Change text mode(none/main_only/main_debug)\n"
    );
    return 0;
  }

  omvp_gst_plugin_register();

  if (argc > 1) {
    _omvp_vids_filename = argv[1];
  }

  memset(&player, 0, sizeof(OMVPPlayer));
  player.vids = omvp_vids_open(_omvp_vids_filename);
  if (player.vids == NULL) {
    if (strcmp(_omvp_vids_filename, "omvp.m3u") != 0) {
      player.vids = omvp_vids_open("omvp.m3u");
    }
    if (player.vids == NULL) {
      player.vids = omvp_vids_open("omvp.csv");
      if (player.vids == NULL) {
        g_print("Failed to open %s\n", _omvp_vids_filename);
        return -1;
      }
    }
  }
  player.o_scan_textures =
    g_malloc0(sizeof(OMVPTexture) * player.vids->num_vids);
  player.scan_textures =
    g_malloc0(sizeof(ClutterActor *) * player.vids->num_vids);
  player.scan_texts =
    g_malloc0(sizeof(ClutterActor *) * player.vids->num_vids);
  player.scan_gsts = g_malloc0(sizeof(OMVPGst) * player.vids->num_vids);
  player.scan_timeout_ids = g_malloc0(sizeof(guint) * player.vids->num_vids);
  player.volume = _omvp_default_volume;
  player.mute = _omvp_default_mute;
  player.text_info = OMVP_TEXT_INFO_MAIN_ONLY;
  if (_omvp_no_text_info) {
    player.text_info = OMVP_TEXT_INFO_NONE;
  }
  if (_omvp_show_debug) {
    player.text_info = OMVP_TEXT_INFO_MAIN_AND_DEBUG;
  }
  if (!_omvp_num_vid_per_row) {
    _omvp_num_vid_per_row =
      _omvp_calc_best_num_vid_per_row(player.vids->num_vids);
  }

  player.view_pos_x = ((gfloat)(_omvp_num_vid_per_row - 1)) / 2.0f;
  player.view_pos_y = ((gfloat)(_omvp_num_vid_per_row - 1)) / 2.0f;
  player.view_num_per_row = _omvp_num_vid_per_row;

  stage = clutter_stage_new();
  clutter_stage_set_title(CLUTTER_STAGE(stage), PACKAGE_STRING);
  clutter_actor_set_background_color(stage, &stage_color);
  g_signal_connect(stage, "destroy", G_CALLBACK(_omvp_on_destroy), &player);
  g_signal_connect(stage, "key-press-event", G_CALLBACK(_omvp_on_key_press),
    &player);
  g_signal_connect(stage, "allocation-changed",
    G_CALLBACK(_omvp_on_allocation_changed), &player);
  clutter_stage_set_user_resizable(CLUTTER_STAGE(stage), TRUE);

  player.root_actor = clutter_actor_new();
  player.num_row = (player.vids->num_vids - 1) / _omvp_num_vid_per_row + 1;
  clutter_actor_set_position(player.root_actor, 0.0f, 0.0f);
  clutter_actor_set_size(player.root_actor,
    _omvp_num_vid_per_row * _OMVP_TEX_SIZE, player.num_row * _OMVP_TEX_SIZE);
  clutter_actor_add_child(stage, player.root_actor);

  player.focus_actor = clutter_actor_new();
  clutter_actor_set_background_color(player.focus_actor, &focus_color);
  clutter_actor_add_child(player.root_actor, player.focus_actor);
  clutter_actor_set_position(player.focus_actor, 0.0f, 0.0f);
  clutter_actor_set_size(player.focus_actor, _OMVP_TEX_SIZE, _OMVP_TEX_SIZE);
  clutter_actor_set_pivot_point(player.focus_actor, 0.5f, 0.5f);
  clutter_actor_set_scale(player.focus_actor,
    _OMVP_FOCUS_SCALE, _OMVP_FOCUS_SCALE);

  for (i = 0; i < player.vids->num_vids; i++) {
#if CLUTTER_GST_MAJOR_VERSION > 2
    player.scan_textures[i] = clutter_actor_new();
#else
    player.scan_textures[i] =
      g_object_new(CLUTTER_TYPE_TEXTURE, "disable-slicing", TRUE, NULL);
#endif
    player.o_scan_textures[i].player = &player;
    player.o_scan_textures[i].idx = i;
    player.o_scan_textures[i].texture = player.scan_textures[i];
    clutter_actor_set_opacity(player.scan_textures[i], 0);
    clutter_actor_add_child(player.root_actor, player.scan_textures[i]);
    g_object_ref(player.o_scan_textures[i].texture);
    clutter_actor_set_position(player.scan_textures[i],
      (gfloat)(i % _omvp_num_vid_per_row) * (gfloat)_OMVP_TEX_SIZE,
      (gfloat)(i / _omvp_num_vid_per_row) * (gfloat)_OMVP_TEX_SIZE);
    clutter_actor_set_size(player.scan_textures[i],
      _OMVP_TEX_SIZE, _OMVP_TEX_SIZE);
    clutter_actor_set_pivot_point(player.scan_textures[i], 0.5f, 0.5f);
    clutter_actor_set_scale(player.scan_textures[i],
      _OMVP_SCAN_VID_SCALE, _OMVP_SCAN_VID_SCALE);
  }

#if CLUTTER_GST_MAJOR_VERSION > 2
  player.texture = clutter_actor_new();
#else
  player.texture =
    g_object_new(CLUTTER_TYPE_TEXTURE, "disable-slicing", TRUE, NULL);
#endif
  player.o_texture.player = &player;
  player.o_texture.idx = -1;
  player.o_texture.texture = player.texture;
  clutter_actor_add_child(player.root_actor, player.texture);
  g_object_ref(player.o_texture.texture);
  constraint = clutter_bind_constraint_new(
    player.focus_actor, CLUTTER_BIND_ALL, 0.0f);
  clutter_actor_add_constraint(player.texture, constraint);
  clutter_actor_set_pivot_point(player.texture, 0.5f, 0.5f);
  clutter_actor_set_scale(player.texture,
    _OMVP_SCAN_VID_SCALE, _OMVP_SCAN_VID_SCALE);

  player.root_scan_texts = clutter_actor_new();
  clutter_actor_set_position(player.root_scan_texts, 0.0f, 0.0f);
  clutter_actor_set_size(player.root_scan_texts,
    _omvp_num_vid_per_row * _OMVP_TEX_SIZE, player.num_row * _OMVP_TEX_SIZE);
  clutter_actor_add_child(player.root_actor, player.root_scan_texts);

  for (i = 0; i < player.vids->num_vids; i++) {
    text = g_strdup_printf("%s\n%s",
      player.vids->ids[i], player.vids->names[i]);
    player.scan_texts[i] =
      clutter_text_new_full(_OMVP_SCAN_TEXT_FONT, text, &text_color);
    g_free(text);
    text_attrs = pango_attr_list_new();
    text_attr = pango_attr_background_new(0, 0, 0);
    pango_attr_list_insert(text_attrs, text_attr);
    clutter_text_set_attributes(
      CLUTTER_TEXT(player.scan_texts[i]), text_attrs);
    pango_attr_list_unref(text_attrs);
    clutter_actor_add_child(player.root_scan_texts, player.scan_texts[i]);
    constraint = clutter_bind_constraint_new(
      player.scan_textures[i], CLUTTER_BIND_ALL, 0.0f);
    clutter_actor_add_constraint(player.scan_texts[i], constraint);
    clutter_actor_set_pivot_point(player.scan_texts[i], 0.5f, 0.5f);
    clutter_actor_set_scale(player.scan_texts[i],
      _OMVP_SCAN_VID_SCALE, _OMVP_SCAN_VID_SCALE);
  }

  player.text = clutter_text_new_full(_OMVP_TEXT_FONT, "", &text_color);
  clutter_text_set_line_wrap(CLUTTER_TEXT(player.text), TRUE);
  text_attrs = pango_attr_list_new();
  text_attr = pango_attr_background_new(0, 0, 0);
  pango_attr_list_insert(text_attrs, text_attr);
  clutter_text_set_attributes(CLUTTER_TEXT(player.text), text_attrs);
  pango_attr_list_unref(text_attrs);
  clutter_actor_add_child(player.root_actor, player.text);
  constraint = clutter_bind_constraint_new(player.texture,
    CLUTTER_BIND_SIZE, 0.0f);
  clutter_actor_add_constraint(player.text, constraint);
  constraint = clutter_bind_constraint_new(player.texture,
    CLUTTER_BIND_X, 0.0f);
  clutter_actor_add_constraint(player.text, constraint);
  constraint = clutter_bind_constraint_new(player.texture,
    CLUTTER_BIND_Y, _OMVP_TEX_SIZE / 3);
  clutter_actor_add_constraint(player.text, constraint);
  clutter_actor_set_pivot_point(player.text, 0.5f, 0.5f);
  clutter_actor_set_scale(player.text,
    _OMVP_SCAN_VID_SCALE, _OMVP_SCAN_VID_SCALE);

  _omvp_refresh_text(&player);

  clutter_actor_set_easing_mode(player.root_actor, CLUTTER_EASE_OUT_CUBIC);
  clutter_actor_set_easing_duration(player.root_actor, _omvp_ani_duration);

  clutter_actor_show(stage);

  player.gst = omvp_gst_open(_omvp_proxy_uri,
    player.vids->uris[player.vid_idx], player.texture, FALSE, 0, 0,
    _omvp_texture_on_callback, &player.o_texture);
  player.o_texture.content = clutter_actor_get_content(player.texture);
  omvp_gst_set_volume(player.gst, player.volume);
  omvp_gst_set_mute(player.gst, player.mute);

  player.max_num_scan_vids = _omvp_scan_num_jobs;
  if (player.max_num_scan_vids > player.vids->num_vids) {
    player.max_num_scan_vids = player.vids->num_vids;
  }
  _omvp_scan_vid_start_all(&player);

  clutter_main();

  return 0;
}
