#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mpd/client.h>
#include <grapheme.h>

#include "../util.h"

static char *scroll_text(const char *text, int idx, int num_chars, const char *loop_text, int on_text_fits);
static int utf8strlen(const char *text);

enum {
	NO_SCROLL,
	FULL_SPACE_SEPARATOR,
	FORCE_SCROLL
};

#ifndef MPD_TITLE_LENGTH
#define MPD_TITLE_LENGTH 20
#endif

#ifndef MPD_ON_TEXT_FITS
#define MPD_ON_TEXT_FITS NO_SCROLL
#endif

#ifndef MPD_LOOP_TEXT
#define MPD_LOOP_TEXT " ~ "
#endif

int
utf8strlen(const char *text)
{
	int utf8charlen, count;

	if (!text)
		return 0;

	for (count = 0; (utf8charlen = grapheme_next_character_break_utf8(text, SIZE_MAX)); count++) {
		text += utf8charlen;
	}

	return count;
}

/* This is a text scrolling function that uses the libgrapheme library to handle multi-byte UTF-8
 * characters.
 *
 * Parameters:
 *    input_text - the source text that we want to scroll
 *    idx - the nth character to start drawing on (if negative then this means reversed scrolling)
 *    num_chars - the number of whole UTF-8 characters to include in the output
 *    loop_text - the text to print after the input text before starting over again (optional)
 *    on_text_fits - controls what to do (scrolling wise) when the input text fits within the output
 *
 * Some notes: the calling function is responsible for incrementing the index (or decrementing it
 * if the reversd scrolling is used).
 *
 * When the text fits within the number of characters to be included in the output the on_text_fits
 * parameter determines how we handle that situation in relation to scrolling.
 *
 *    1) we can print the whole text (no scrolling) or
 *    2) we can let the whole text scroll out of view before it starts from the other side or
 *    3) we can use the given separator anyway
 *
 * This function returns a char array that need to be freed by the caller.
 */
char *
scroll_text(const char *input_text, int idx, int num_chars, const char *loop_text, int on_text_fits)
{
	int utf8charlen, i, s; // temporary variables
	int c = 0; // keeps track of how many UTF-8 characters have been copied so far
	int input_chars; // count of UTF-8 characters in the input text
	int loop_chars; //  count of UTF-8 characters in the loop text
	char *output_text = malloc((num_chars * 5 * sizeof(char)) + 1);
	char *dest_iter = output_text; // iterator for the output text
	const char *text_iter; // iterator for the input text
	char loop_spaces[num_chars + 1]; // used for full space separator

	/* Calculate the number of UTF-8 characters in the input text */
	input_chars = utf8strlen(input_text);

	/* Fall back to using full space separator */
	if (!loop_text || (input_chars <= num_chars && on_text_fits != FORCE_SCROLL)) {
		for (i = 0; i < num_chars; i++)
			loop_spaces[i] = ' ';
		loop_spaces[i] = '\0';
		loop_text = &loop_spaces[0];
	}

	if (input_chars <= num_chars && on_text_fits == NO_SCROLL) {
		idx = 0;
	}

	/* Calculate the number of UTF-8 characters in the loop text */
	loop_chars = utf8strlen(loop_text);

	/* Make the index wrap around when it gets too large */
	idx %= (input_chars + loop_chars);

	/* If the index is negative then that means that we are scrolling backwards */
	if (idx < 0) {
		idx += input_chars + loop_chars;
	}

	/* Determine whether we are starting with the loop text or the input text */
	if ((s = (idx >= input_chars))) {
		idx -= input_chars;
		text_iter = loop_text;
	} else {
		text_iter = input_text;
	}

	/* Skip ahead to the current character */
	for (i = 0; i < idx; i++) {
		text_iter += grapheme_next_character_break_utf8(text_iter, SIZE_MAX);
	}

	/* Start copying text */
	while (c < num_chars) {
		utf8charlen = grapheme_next_character_break_utf8(text_iter, SIZE_MAX);

		/* If this is the end of the text then swap the loop and input text strings */
		if (!utf8charlen) {
			text_iter = (s ? input_text : loop_text);
			s = !s;
			continue;
		}

		strncpy(dest_iter, text_iter, utf8charlen);
		text_iter += utf8charlen;
		dest_iter += utf8charlen;
		c++;
	}

	/* Ensure that we add a null terminator to the output string */
	*dest_iter = '\0';

	return output_text;
}

/* fmt consist of lowercase:
 *   "a" for artist,
 *   "t" for song title,
 *   "at" for song artist then title
 *   if not a or t, any character will be represented as separator.
 *   i.e: "a-t" gives "artist-title"
 *
 * By default the whole output will scroll, but a capital A or a capital T can be used to only
 * scroll the song title, for example.
 */
const char *
mpdonair(const char *fmt)
{
	static struct mpd_connection *conn; /* kept between calls */
	static int scroll_idx = 0, artist_idx = 0, title_idx = 0;
	static char prev_artist[255] = {0};
	static char prev_title[255] = {0};
	int scroll = 0, i;
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
		goto mpdreturn;
	} else if (state == MPD_STATE_UNKNOWN) {
		goto mpdout;
	}

	const char *title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
	const char *artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);

	/* Checks to make sure that the scroll index resets when a new song is played */
	if (title) {
		if (strcmp(title, prev_title))
			scroll_idx = 0;
		strncpy(prev_title, title, sizeof(prev_title));
	}

	if (artist) {
		if (strcmp(artist, prev_artist))
			scroll_idx = 0;
		strncpy(prev_artist, artist, sizeof(prev_artist));
	}

	char titlebuffer[256] = {0};

	for (i = 0; i < (int)strlen(fmt); i++) {
		char separator[2] = {fmt[i], '\0'};
		switch (fmt[i]) {
		case 'a':
			if (artist != NULL) {
				strlcat(titlebuffer, artist, sizeof(titlebuffer));
			}
			break;
		case 'A':
			if (artist != NULL) {
				char *scrolled_artist = scroll_text(artist, artist_idx, utf8strlen(artist), " ", FORCE_SCROLL);
				strlcat(titlebuffer, scrolled_artist, sizeof(titlebuffer));
				free(scrolled_artist);
				artist_idx += scroll;
				scroll = 0;
			}
			break;
		case 't':
			if (title != NULL) {
				strlcat(titlebuffer, title, sizeof(titlebuffer));
			}
			break;
		case 'T':
			if (title != NULL) {
				char *scrolled_title = scroll_text(title, title_idx, utf8strlen(title), " ", FORCE_SCROLL);
				strlcat(titlebuffer, scrolled_title, sizeof(titlebuffer));
				free(scrolled_title);
				title_idx += scroll;
				scroll = 0;
			}
			break;
		default:
			strlcat(titlebuffer, separator, sizeof(titlebuffer));
			break;
		}
	}

	char *scrolled_text = scroll_text(titlebuffer, scroll_idx, MPD_TITLE_LENGTH, MPD_LOOP_TEXT, MPD_ON_TEXT_FITS);
	strncat(buf, scrolled_text, sizeof(buf) - strlen(buf) - 1);
	free(scrolled_text);

	scroll_idx += scroll;

mpdreturn:
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
