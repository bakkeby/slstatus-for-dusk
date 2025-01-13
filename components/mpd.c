#include <err.h>
#include <stdio.h>
#include <string.h>

#include <mpd/client.h>

#include "../util.h"

/* fmt consist of lowercase :
 * "a" for artist,
 * "t" for song title,
 * "at" for song artist then title
 * if not a or t, any character will be represented as separator.
 * i.e: "a-t" gives "artist-title"
*/

#define TITLE_LENGTH 24

void scroll_title(const char *title, enum mpd_state state) {
  char scroll[TITLE_LENGTH + 1] = {'\0'};
  static char prev[256] = {'\0'};
  static int idx = 0;

  if (title && strcmp(title, prev) != 0) {
    idx = 0;
    strncpy(prev, title, sizeof(prev) - 1);
    prev[sizeof(prev) - 1] = '\0';
  }

  if (state == MPD_STATE_PLAY || state == MPD_STATE_PAUSE) {
    if (strlen(title) > TITLE_LENGTH) {
      snprintf(scroll, TITLE_LENGTH + 1, "%s", title + idx);
      strncat(buf, scroll, sizeof(buf) - strlen(buf) - 1);
      if (state == MPD_STATE_PLAY) {
        idx++;
        if ((size_t)(idx) > strlen(title) - TITLE_LENGTH) {
          idx = 0;
        }
      }
    } else {
      strncat(buf, title, sizeof(buf) - strlen(buf) - 1);
    }

  } else if (state == MPD_STATE_STOP) {
    idx = 0;
    if (strlen(title) > TITLE_LENGTH) {
      snprintf(scroll, TITLE_LENGTH + 1, "%s", title);
      strncat(buf, scroll, sizeof(buf) - strlen(buf) - 1);
    } else {
      strncat(buf, title, sizeof(buf) - strlen(buf) - 1);
    }
  }
}

const char *
mpdonair(const char *fmt) {
  static struct mpd_connection *conn; /* kept between calls */
  struct mpd_song *song = NULL;
  struct mpd_status *status = NULL;

  if (conn == NULL) {
    conn = mpd_connection_new(NULL, 0, 5000);
    if (conn == NULL) {
      warn("MPD error: %s", mpd_connection_get_error_message(conn));
      goto mpdout;
    }
  }

  mpd_send_status(conn);
  status = mpd_recv_status(conn);
  if (status == NULL) {
    goto mpdout;
  }

  mpd_send_current_song(conn);
  song = mpd_recv_song(conn);
  if (song == NULL) {
    goto mpdout;
  } else {
    buf[0] = '\0';
    enum mpd_state state = mpd_status_get_state(status);
    if (state == MPD_STATE_PLAY) {
      strncat(buf, "  ", sizeof(buf) - strlen(buf) -1);
    } else if (state == MPD_STATE_PAUSE) {
      strncat(buf, "  ", sizeof(buf) - strlen(buf) -1);
    } else if (state == MPD_STATE_STOP) {
      strncat(buf, "  ", sizeof(buf) - strlen(buf) -1);
    } else if (state == MPD_STATE_UNKNOWN) {
      goto mpdout;
    }

    const char *title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
    const char *artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);

    for (int i = 0; i < (int)strlen(fmt); i++) {
      char separator[2] = {fmt[i], '\0'};
      switch (fmt[i]) {
        case 'a':
          if (artist != NULL) {
            strncat(buf, artist, sizeof(buf) - strlen(buf) - 1);
          }
          break;
        case 't':
          if (title != NULL) {
            scroll_title(title, state);
          }
          break;
        default:
          strncat(buf, separator, sizeof(buf) - strlen(buf) - 1);
          break;
      }
    }
    mpd_status_free(status);
    mpd_song_free(song);
    mpd_response_finish(conn);
    return(buf);
  }

mpdout:
  mpd_response_finish(conn);
  mpd_connection_free(conn);
  conn = NULL;
  return "";
}
