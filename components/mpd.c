#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mpd/client.h>
#include <grapheme.h>

#include "../util.h"

static char *scroll_text(const char *text, int idx, int num_chars);

/* fmt consist of lowercase :
 * "a" for artist,
 * "t" for song title,
 * "at" for song artist then title
 * if not a or t, any character will be represented as separator.
 * i.e: "a-t" gives "artist-title"
*/

#define TITLE_LENGTH 24

/* Returns a char array that need to be freed by the caller */
char *
scroll_text(const char *input_text, int idx, int num_chars)
{
	int utf8charlen, i, c = 0, total_chars;
	const char *text;
	int num_leading_spaces;
	int output_length = (num_chars * 5 * sizeof(char));
	char *output_text = malloc(output_length + 1);
	char *dest = output_text;

	/* Work out the total number of characters in the input text */
	text = input_text;
	for (total_chars = 0, utf8charlen = 1; utf8charlen; total_chars++) {
		utf8charlen = grapheme_next_character_break_utf8(text, SIZE_MAX);
		text += utf8charlen;
	}

	/* Reset text pointer back to the start of the string */
	text = input_text;

	/* Make the index wrap around when it gets too large */
	idx %= (total_chars + num_chars);

	/* If the index is negative then that means that we are scrolling backwards */
	if (idx < 0) {
		idx += total_chars + num_chars;
	}

	/* Add up to num_chars leading spaces when the text wraps around */
	if (idx >= total_chars) {
		num_leading_spaces = (num_chars + total_chars - idx);
		for (i = 0; i < num_leading_spaces; i++) {
			strncpy(dest, " ", 1);
			dest++;
			c++;
		}
		idx = 0;
	}

	/* Skip ahead to the current character */
	for (i = 0; i < idx; i++) {
		text += grapheme_next_character_break_utf8(text, SIZE_MAX);
	}

	/* Start copying text */
	for (; c < num_chars; c++) {
		utf8charlen = grapheme_next_character_break_utf8(text, SIZE_MAX);
		if (!utf8charlen) {
			/* If this is the end of the text, then add up to num_chars trailing spaces */
			for (; c < num_chars; c++) {
				strncpy(dest, " ", 1);
				dest++;
			}
			break;
		}
		strncpy(dest, text, utf8charlen);
		text += utf8charlen;
		dest += utf8charlen;
	}

	strncpy(dest, "\0", 1);
	return output_text;
}

const char *
mpdonair(const char *fmt) {
	static struct mpd_connection *conn; /* kept between calls */
	static int scroll_idx = 0;
	int scroll = 0;
	struct mpd_song *song = NULL;
	struct mpd_status *status = NULL;

	if (conn == NULL) {
		conn = mpd_connection_new(NULL, 0, 5000);
		if (conn == NULL) {
			warn("MPD error: %s", mpd_connection_get_error_message(conn));
			return "";
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
	}

	buf[0] = '\0';
	enum mpd_state state = mpd_status_get_state(status);
	if (state == MPD_STATE_PLAY) {
		strncat(buf, "  ", sizeof(buf) - strlen(buf) -1);
		scroll = 1;
	} else if (state == MPD_STATE_PAUSE) {
		strncat(buf, "  ", sizeof(buf) - strlen(buf) -1);
		scroll = 0;
	} else if (state == MPD_STATE_STOP) {
		strncat(buf, "  ", sizeof(buf) - strlen(buf) -1);
		scroll = 0;
		scroll_idx = 0;
	} else if (state == MPD_STATE_UNKNOWN) {
		goto mpdout;
	}

	const char *title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
	const char *artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);

	char titlebuffer[256] = {0};

	for (int i = 0; i < (int)strlen(fmt); i++) {
		char separator[2] = {fmt[i], '\0'};
		switch (fmt[i]) {
			case 'a':
				if (artist != NULL) {
					strncat(titlebuffer, artist, sizeof(titlebuffer) - strlen(titlebuffer) - 1);
				}
				break;
			case 't':
				if (title != NULL) {
					strncat(titlebuffer, title, sizeof(titlebuffer) - strlen(titlebuffer) - 1);
				}
				break;
			default:
				strncat(titlebuffer, separator, sizeof(titlebuffer) - strlen(titlebuffer) - 1);
				break;
		}
	}

	char *scrolled_text = scroll_text(titlebuffer, scroll_idx, TITLE_LENGTH);
	// fprintf(stderr, "scroll: '%s'\n", scrolled_text);
	strncat(buf, scrolled_text, sizeof(buf) - strlen(buf) - 1);
	free(scrolled_text);

	scroll_idx += scroll;

	mpd_status_free(status);
	mpd_song_free(song);
	mpd_response_finish(conn);
	return(buf);

mpdout:
	mpd_response_finish(conn);
	mpd_connection_free(conn);
	conn = NULL;
	return "";
}
