/*
* This file is part of DisOrder
* Copyright © 2011 Richard Kettlewell
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file lib/ddb-track.c
* @brief DisOrder track database - track support
*/
#include "common.h"
#include "ddb.h"
#include "ddb-db.h"
#include "log.h"
#include "vector.h"

#include <time.h>

static int ddb_do_track_get(const char *track,
			    struct ddb_track_data *trackdata) {
  static const char context[] = "retrieving track";
  int rc;
  void *stmt = NULL;

  if((rc = ddb_create_bind(context, &stmt, ddb_track_get_sql,
			   P_STRING, track,
			   P_END)))
    goto error;
  switch(rc = ddb_unpick_row(context, stmt,
			     P_STRING, &trackdata->track,
			     P_STRING, &trackdata->artist,
			     P_STRING, &trackdata->album,
			     P_INT, &trackdata->sequence,
			     P_STRING, &trackdata->title,
			     P_STRING, &trackdata->tags,
			     P_INT, &trackdata->weight,
			     P_INT, &trackdata->pick_at_random,
			     P_INT, &trackdata->available,
			     P_TIME, &trackdata->noticed,
			     P_INT, &trackdata->length,
			     P_TIME, &trackdata->played_time,
			     P_INT, &trackdata->played,
			     P_INT, &trackdata->scratched,
			     P_INT, &trackdata->completed,
			     P_INT, &trackdata->requested,
			     P_END)) {
  case DDB_OK:
    return ddb_destroy_statement(context, &stmt);
  case DDB_NO_ROW:
    rc = DDB_NO_SUCH_TRACK;
    goto error;
  default:
    goto error;
  }
 error:
  ddb_destroy_statement(context, &stmt);
  return rc;
}

int ddb_track_get(const char *track,
                  struct ddb_track_data *trackdata) {
  TRANSACTION_WRAP("retrieving track",
		   ddb_do_track_get(track, trackdata));
}

static int ddb_do_track_notice(const char *track,
			       const char *artist,
			       const char *album,
			       int sequence,
			       const char *title) {
  static const char context[] = "dropping track";
  struct ddb_track_data trackdata;
  int rc;
  switch(rc = ddb_do_track_get(track, &trackdata)) {
  case DDB_OK:
    /* We know about this track already, so just mark it as available
     * if it isn't already.  Note that the 'noticed' time is NOT
     * updated - it's not really a new track; rather it probably lives
     * on a removable device which has just been reattached. */
    if(!trackdata.available)
      return ddb_bind_and_execute(context,
				  ddb_track_update_availability_sql,
				  P_INT, 1,
				  P_STRING, track,
				  P_END);
    else
      return DDB_OK;
  case DDB_NO_SUCH_TRACK:
    /* This one's new to us */
    return ddb_bind_and_execute(context,
				ddb_track_new_sql,
				P_STRING, track,
				P_STRING, artist,
				P_STRING, album,
				P_INT, sequence,
				P_STRING, title,
				P_STRING, "",        /* tags */
				P_INT, DEFAULT_WEIGHT,
				P_INT, 1,            /* pick_at_random */
				P_INT64, time(NULL), /* noticed */
				P_INT, 0,	     /* played */
				P_INT, 0,	     /* scratched */
				P_INT, 0,	     /* completed */
				P_INT, 0,	     /* requested */
				P_END);
  default:
    return rc;
  }
}

int ddb_track_notice(const char *track,
                     const char *artist,
                     const char *album,
                     int sequence,
                     const char *title) {
  TRANSACTION_WRAP("noticing track",
		   ddb_do_track_notice(track, artist, album, sequence, title));
}

static int ddb_do_track_drop(const char *track) {
  static const char context[] = "dropping track";
  struct ddb_track_data trackdata;
  int rc;
  /* Check the track is known */
  if((rc = ddb_do_track_get(track, &trackdata)) != DDB_OK)
    return rc;
  /* Mark it as unavailable if it isn't already */
  if(trackdata.available)
    return ddb_bind_and_execute(context,
				ddb_track_update_availability_sql,
				P_INT, 0,
				P_STRING, track,
				P_END);
  else
    return DDB_OK;
}

int ddb_track_drop(const char *track) {
  TRANSACTION_WRAP("dropping track",
		   ddb_do_track_drop(track));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
