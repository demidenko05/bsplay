/* BSD 2-Clause License
Copyright (c) 2020, Beigesoft™
All rights reserved.
See the LICENSE.BSPLAY in the root source folder */
 
/**
 * <p>Beigesoft™ FFMPEG/GTK based external player interface, and some shared library.</p>
 * @author Yury Demidenko
 **/

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define BSS_LOCKING -11

typedef enum {
  STOPPED, PLAYING, PAUSED, PENDING
} PLAY_STATUS;

/* Play audio source, return 0 if OK */
int bsplay_play_audio(char *p_uri);

/* Play Internet stream, return 0 if OK */
int bsplay_play_http(char *p_uri);

/* Play video source with optional subtitles, return 0 if OK */
int bsplay_play_video(char *p_uri, char *p_uri_srt);

/* Stop, return 0 if OK */
int bsplay_stop();

/* Toggle pause */
void bsplay_pause_toggle();

/* Close A/V stream and all other player's resources.
 * It's invoked "on exit" event.
 */
void bsplay_destroy_av();

/* get duration in seconds, returns less than -1 if no play or Internet stream */
int bsplay_duration();

/* get position in seconds, returns less than -1 if no play or Internet stream */
int bsplay_position();

/* Set position (seek) in seconds, return 0 if OK */
int bsplay_set_position(int p_pos_sec);

/* returns play status */
PLAY_STATUS bsplay_play_status();

//Shared lib:
/* Check if file has audio extension, e.g. mp3 */
int bsplay_is_audio(char *p_file_nme);

/* Check if file has video extension, e.g. mp4 */
int bsplay_is_video(char *p_file_nme);

/* Check if it's audio extension, e.g. mp3 */
int bsplay_is_ext_audio(char *p_ext);

/* Check if it's video extension, e.g. mp4 */
int bsplay_is_ext_video(char *p_ext);

/* Check string#1 starts with p_len first characters of string#2 */
int bs_str_start(char *p_str1, char *p_str2, int p_len);
