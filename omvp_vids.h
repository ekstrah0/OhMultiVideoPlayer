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

#ifndef _OMVP_VIDS_H_
#define _OMVP_VIDS_H_

#include <glib.h>

typedef struct _OMVPVids {
  gint num_vids;
  gchar **ids;
  gchar **names;
  gchar **uris;
} OMVPVids;

extern OMVPVids *omvp_vids_open(const gchar *filename);
extern gint omvp_vids_close(OMVPVids *vids);

#endif /* _OMVP_VIDS_H_ */
