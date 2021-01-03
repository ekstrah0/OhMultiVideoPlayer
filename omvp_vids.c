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

#include <stdio.h>
#include <string.h>
#include "omvp_vids.h"

static const gchar *_omvp_vids_get_filename_ext(const gchar *filename);

static const gchar *_omvp_vids_get_filename_ext(const gchar *filename) {
  const gchar *ext;

  ext = strrchr(filename, '.');

  if (ext && *(ext + 1)) {
    return (ext + 1);
  }

  return NULL;
}

OMVPVids *omvp_vids_open(const gchar *filename) {

#define XSTR(A) STR(A)
#define STR(A) #A
#define _OMVP_MAX_STRLEN 1023

  FILE *fp;
  int ret;
  gchar id[_OMVP_MAX_STRLEN + 1];
  gchar name[_OMVP_MAX_STRLEN + 1];
  gchar uri[_OMVP_MAX_STRLEN + 1];
  const gchar *ext;
  gint num_vids;
  gint i;
  OMVPVids *vids;

  fp = fopen(filename, "r");
  if (fp == NULL) {
    return NULL;
  }
  ext = _omvp_vids_get_filename_ext(filename);
  if (ext && g_ascii_strcasecmp(ext, "m3u") == 0) {
#define _OMVP_EXT_HDR "#EXTINF:"
    gchar *str;
    gint ext_hdr_len;
    num_vids = 0;
    for (;;) {
      str = fgets(uri, _OMVP_MAX_STRLEN + 1, fp);
      if (!str) {
        break;
      }
      if (*str != '#' && !g_ascii_isspace(*str) &&
        strncmp("\xef\xbb\xbf", str, 3) != 0) {
        num_vids++;
      }
    }
    rewind(fp);
    if (num_vids == 0) {
      fclose(fp);
      return NULL;
    }
    ext_hdr_len = strlen(_OMVP_EXT_HDR);
    vids = g_malloc(sizeof(OMVPVids));
    vids->num_vids = num_vids;
    vids->ids = g_malloc(sizeof(gchar *) * num_vids);
    vids->names = g_malloc(sizeof(gchar *) * num_vids);
    vids->uris = g_malloc(sizeof(gchar *) * num_vids);
    id[0] = '\0';
    name[0] = '\0';
    for (i = 0; i < num_vids; i++) {
      for (;;) {
        str = fgets(uri, _OMVP_MAX_STRLEN + 1, fp);
        if (!str) {
          break;
        }
        if (*str == '#') {
          if (strncmp(str, _OMVP_EXT_HDR, ext_hdr_len) == 0) {
            str += ext_hdr_len;
            ret = sscanf(str,
              "%" XSTR(_OMVP_MAX_STRLEN) "[^,],%" XSTR(_OMVP_MAX_STRLEN)
              "[^\n]", id, name);
          }
        } else if (strncmp("\xef\xbb\xbf", str, 3) == 0) {
        } else if (!g_ascii_isspace(*str)) {
          uri[strcspn(uri, "\r\n")] = '\0';
          vids->ids[i] = g_strdup(id);
          vids->names[i] = g_strdup(name);
          vids->uris[i] = g_strdup(uri);
          id[0] = '\0';
          name[0] = '\0';
          break;
        }
      }
    }
  } else {
    num_vids = 0;
    for (;;) {
      ret = fscanf(fp,
        "%" XSTR(_OMVP_MAX_STRLEN) "[^,],%" XSTR(_OMVP_MAX_STRLEN)
        "[^,],%" XSTR(_OMVP_MAX_STRLEN) "s\n", id, name, uri);
      if (ret != 3 || ret == EOF) {
        break;
      }
      num_vids++;
    }
    rewind(fp);
    if (num_vids == 0) {
      fclose(fp);
      return NULL;
    }
    vids = g_malloc(sizeof(OMVPVids));
    vids->num_vids = num_vids;
    vids->ids = g_malloc(sizeof(gchar *) * num_vids);
    vids->names = g_malloc(sizeof(gchar *) * num_vids);
    vids->uris = g_malloc(sizeof(gchar *) * num_vids);
    for (i = 0; i < num_vids; i++) {
      ret = fscanf(fp,
        "%" XSTR(_OMVP_MAX_STRLEN) "[^,],%" XSTR(_OMVP_MAX_STRLEN)
        "[^,],%" XSTR(_OMVP_MAX_STRLEN) "s\n", id, name, uri);
      if (ret != 3 || ret == EOF) {
        break;
      }
      vids->ids[i] = g_strdup(id);
      vids->names[i] = g_strdup(name);
      vids->uris[i] = g_strdup(uri);
    }
  }
  fclose(fp);
  return vids;
}

gint omvp_vids_close(OMVPVids *vids) {
  gint i, n;

  n = vids->num_vids;
  for (i = 0; i < n; i++) {
    g_free(vids->ids[i]);
    g_free(vids->names[i]);
    g_free(vids->uris[i]);
  }
  g_free(vids->ids);
  g_free(vids->names);
  g_free(vids->uris);
  g_free(vids);

  return 0;
}
