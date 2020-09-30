/* BSD 2-Clause License
Copyright (c) 2020, Beigesoft™
All rights reserved.
See the LICENSE.BSPLAY in the root source folder */
 
/**
 * <p>Beigesoft™ FFMPEG/GTK based player.</p>
 * @author Yury Demidenko
 **/

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <bsslist.h>
#include <bsplay.h>

static int s_argc;
static char **s_argv;

static BSStPlDtLs s_bsstpldtls = { .rows = 0, .columns = 3, .buf_size = 50, .buf_inc = 10 };
static GtkWidget *s_window, * s_progress_bar, *s_gtklist;

static struct tm s_timeb = { .tm_year = 2001 - 1900 }; //TODO synchronization between threads!
//ID for progress thread:
static int s_timer = -1;
static GtkToolItem *s_btn_play, *s_btn_stop, *s_btn_previous, *s_btn_next;
static char *s_unknown_str = "?";
static char *s_unknown_dur = "99:99:99";
//Menu:
enum EMenuVals { ADD_FILE, SAVE_M3U, GOTO_POSITION, TOGGLE_RANDOM, TOGGLE_LOOP, DELETE_SELECTED, CLEAR_LIST };
static int s_menu_vals[] = { ADD_FILE, SAVE_M3U, GOTO_POSITION, TOGGLE_RANDOM, TOGGLE_LOOP, DELETE_SELECTED, CLEAR_LIST };
//TODO i18n:
static const gchar *s_win_title = "BS media player";
static const gchar *s_flchooser_title = "Select one or more files to open";
//buffers:
static char s_sbuffer[401];
static const char *s_ass_stl = ":force_style='SecondaryColour=0xFFFFFF00,BackColour=0x00000000,FontSize=20,Bold=300,Outline=1.3,Shadow=0.3,Spacing=1.5'";
//allocated here:
static char *s_srt_file = NULL;
static int *s_random_idxs = NULL; //random indexes
static int s_random_idxs_len = 0; //random indexes length

//state:
static int s_selected_row = -1; //-1 means not selected row
static int s_playing_row = -1; //-1 not playing
static int s_playing_row_need_dur = FALSE; //if playing row needs to update duration
static int s_last_played_row = -1; //-1 not
#define REFRESH_PROGRESS 250 // 0.25 sec
#define REFRESH_PLAYING 3 // 0.5 sec
#define PAUSE_TO_NEXT 0 // pause in sec to next item
static int s_need_refresh_playing = REFRESH_PLAYING; //for reducing refreshing list to REFRESH_PROGRESS * (REFRESH_PLAYING - 1) ms
static int s_random = FALSE; //flag play randomly
static int s_loop = FALSE; //flag play loop all
static int s_seek_pos_sec = 0; //seek position in seconds
//Sending sleep on/off vars:
#define SENT_SLEEP_NON 0
#define SENT_SLEEP_OFF 1
#define SENT_SLEEP_ON 2
static int s_sent_sleep_status = SENT_SLEEP_NON; //sent sleep (ScreenSaver on/off) state
static guint s_sent_sleep_cookie = SENT_SLEEP_NON; //sent sleep cookie
static const char *s_sleep_bus_name = "org.freedesktop.ScreenSaver";
static const char *s_sleep_srv_path = "/ScreenSaver";
static const char *s_sleep_srv_name = "org.freedesktop.ScreenSaver";

//Local lib:
  //Local sub-lib level#1:
/* play given row, p_idx must be >= 0 && < s_bsstpldtls.rows!!! */
static void su_fill_playing_row(char *p_row[]) {
  if (s_random || s_loop) {
    char * rl;
    if (s_random && s_loop) {
      rl = "RL";
    } else if (s_random) {
      rl = "R";
    } else {
      rl = "L";
    }
    sprintf(s_sbuffer, "<b>%s(%s) %s</b>", rl, p_row[1], p_row[0]);
  } else {
    sprintf(s_sbuffer, "<b>(%s) %s</b>", p_row[1], p_row[0]);
  }
}

/* get random previous value */ //TODO all rnd into shared lib
static int su_random_previous(int p_val) {
  for (int i = 0; i < s_bsstpldtls.rows; i++) {
    if (s_random_idxs[i] == p_val) {
      if (i == 0) {
        if (s_loop) {
          return s_random_idxs[s_bsstpldtls.rows - 1];
        } else {
          return -1;
        }
      }
      return s_random_idxs[i - 1];
    }
  }
  return -1;
}

/* get random next value */
static int su_random_next(int p_val) {
  for (int i = 0; i < s_bsstpldtls.rows; i++) {
    if (s_random_idxs[i] == p_val) {
      if (i == s_bsstpldtls.rows - 1) {
        if (s_loop) {
          return s_random_idxs[0];
        } else {
          return -1;
        }
      }
      return s_random_idxs[i + 1];
    }
  }
  return -1;
}

/* make random indexes */
static void su_make_random_idxs() {
  if (s_random_idxs_len < s_bsstpldtls.rows) {
    if (s_random_idxs) {
      s_random_idxs = realloc(s_random_idxs, s_bsstpldtls.buf_size * sizeof(int));
    } else {
      s_random_idxs = malloc(s_bsstpldtls.buf_size * sizeof(int));
    }
    s_random_idxs_len = s_bsstpldtls.buf_size;
  }
  //re/shuffle:
  for (int i = 0; i < s_bsstpldtls.rows; i++) {
    s_random_idxs[i] = -1;
  }
  for (int i = 0; i < s_bsstpldtls.rows; i++) {
    int rnd = rand();
    int pos = rnd % (s_bsstpldtls.rows - i);
    int is_ok = FALSE;
    for (int j = 0; j < s_bsstpldtls.rows; j++) {
      int k = j + pos;
      if (k > s_bsstpldtls.rows - 1) {
        k = k % (s_bsstpldtls.rows - 1);
      }
      if (s_random_idxs[k] == -1) {
        s_random_idxs[k] = i;
        is_ok = TRUE;
        break;
      }
    }
    if (!is_ok) {
      printf("Random algorithm problem!!!\n");
      for (int j = 0; j < s_bsstpldtls.rows; j++) {
        if (s_random_idxs[j] == -1) {
          s_random_idxs[j] = i;
          break;
        }
      }
    }
  }
/*for (int i = 0; i < s_bsstpldtls.rows; i++) {
printf("RND %d -> %d\n", i, s_random_idxs[i]);
}*/
}

/* makes string time from one in seconds, e.g. p_sec=227 will output "00:03:47", p_sec<0 (radio) will output "99:99:99" */
static char *su_time_str(int p_sec) { //TODO shared lib
  if (p_sec < 0) {
    return s_unknown_dur;
  } else {
    char *durs = malloc(10 * sizeof(char));
    s_timeb.tm_hour = p_sec / 3600;
    s_timeb.tm_min = (p_sec % 3600) / 60;
    s_timeb.tm_sec = p_sec % 60;
    strftime(durs, 10, "%T", &s_timeb);
    return durs;
  }
}

/* Confirmation dialog, returns 1 (TRUE) if confirmed */
static int su_dialog_confirm(char *p_quest) {
  GtkWidget *dialog;
  gint response;
  dialog = gtk_message_dialog_new (GTK_WINDOW(s_window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, p_quest);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  if (response == GTK_RESPONSE_OK) {
    return TRUE;
  }
  return FALSE;
}

static void
  s_escape_str (char *pStr)
{
  for ( int i = 0; ; i++ )
  {
    if ( pStr[i] == 0 )
                  { break; }
    if ( pStr[i] == '%' )
    {
      if ( pStr[i + 1] == 0 )
                  { break; }
      char chr = 0;
      int rd = sscanf (pStr + i + 1, "%2hhx",  &chr);
      if ( rd != 1 )
          { rd = sscanf (pStr + i + 1, "%2hhX",  &chr); }
      if (rd == 1)
      {
        pStr[i] = chr;
        for ( int j = i + 1; ; j++ )
        {
          pStr[j] = pStr[j + 2];
          if ( pStr[j] == 0 )
                  { break; }
        }
      }
    }
  }
}

int su_read_m3u(GFile *p_file) {
  FILE *file = fopen(g_file_get_path(p_file), "r");
  if (!file) {
    return 123;
  }
  float durf; char nmeb[251], pathb[301];
  int max_cant_read = 100;
  while (!feof(file) && !ferror(file)) {
    int crdd = fscanf(file, "#EXTINF:%f", &durf);
    if (crdd == 1) {
      int crdd = fscanf(file, ",%250[^\n]\n%300[^\n]", nmeb, pathb);
      if (crdd == 2) {
        //printf("SCANF m3u: %f %s %s \n", durf, nmeb, pathb);
        char *durs = s_unknown_str;
        if (durf < 0.0 || durf > 0.01) {
          durs = su_time_str((int) durf);
        }
        char *nme = malloc((strlen(nmeb) + 1) * sizeof(char));
        strcpy(nme, nmeb);
        char *pth = NULL;
        if (bs_str_start(pathb, "http", 4) || bs_str_start(pathb, "file", 4)
          || bs_str_start(pathb, "/", 1)) { //absolute path/uri
          pth = malloc((strlen(pathb) + 1) * sizeof(char));
          strcpy(pth, pathb);
        } else { // relative path
          GFile *dir = g_file_get_parent(p_file);
          strcpy(s_sbuffer, g_file_get_path(dir));
          strcat(s_sbuffer, "/");
          s_escape_str(pathb);
          strcat(s_sbuffer, pathb);
          pth = malloc((strlen(s_sbuffer) + 1) * sizeof(char));
          strcpy(pth, s_sbuffer);
//printf("fixed path: %s \n", pth);
        }
        char *fl[3] = { nme, durs, pth };
        bsslist_add(&s_bsstpldtls, fl);
      } else {
        printf("Can't read m3u entry's dur=%f name/path!\n", durf);
      }
    } else if (max_cant_read-- == 0) {
      printf("Can't read m3u!\n");
      break;
    }
    //skip trash:
    fscanf(file, "%*[^\n]"); 
    fscanf(file, "%*[\n]");
  }
  fclose(file);
  return 0;
}

static int su_play_row(int p_idx) {
  char** row = bsslist_get(&s_bsstpldtls, p_idx);
  if (row == NULL) { //excessive checking, never should be
    return 123;
  }
  if (s_playing_row >= 0) {
    s_last_played_row = s_playing_row;
  }
  s_playing_row = p_idx;
  if (bs_str_start(row[2], "http", 4)) {
    bsplay_play_http(row[2]);
  } else if (bsplay_is_video(row[2])) {
    char *subt = NULL;
    strcpy(s_sbuffer, row[2]);
    char *ext = strrchr(s_sbuffer, '.');
    ext[1] = 's'; ext[2] = 'r'; ext[3] = 't'; ext[4] = '\0';
    //printf("SRT-%s\n", s_sbuffer);
    FILE *sbtfl = fopen(s_sbuffer, "r");
    if (sbtfl != NULL) {
      fclose(sbtfl);
      strcpy(s_sbuffer, "subtitles=");
      strcat(s_sbuffer, row[2]);
      ext = strrchr(s_sbuffer, '.');
      ext[1] = 's'; ext[2] = 'r'; ext[3] = 't'; ext[4] = '\0';
      //strcat(s_sbuffer, ":force_style='FontName=Dejavu Serif,FontSize=22,Bold=250,Outline=1.8'");
      int is_sopt = FALSE;
      if (s_argc > 0) {
        for (int i = 0; i < s_argc; i++) {
          if (strncmp(s_argv[i], ":force_style", 12) == 0) {
            strcat(s_sbuffer, s_argv[i]);
            is_sopt = TRUE;
printf("fs=%s", s_argv[i]);
            break;
          }
        }
      }
      if (!is_sopt) {
        strcat(s_sbuffer, s_ass_stl);
      }
      if (s_srt_file) {
        free(s_srt_file);
      }
      s_srt_file = malloc((strlen(s_sbuffer) + 1) * sizeof(char));
      strcpy(s_srt_file, s_sbuffer);
      subt = s_srt_file;
    }
    bsplay_play_video(row[2], subt);
  } else {
    bsplay_play_audio(row[2]);
  }
  if (row[1] == s_unknown_str) {
    s_playing_row_need_dur = TRUE;
  }
  return 0;
}

/* Send sleep off through DBUS */
static void su_send_sleep_off() {
//printf("Sending sleep off...\n");
  GVariant *resp = NULL;
  GDBusConnection *con = NULL;
  GError *err = NULL;
  con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
  if (con && !g_dbus_connection_is_closed(con) && err == NULL) {
    resp = g_dbus_connection_call_sync (con,
      s_sleep_bus_name, s_sleep_srv_path, s_sleep_srv_name,
        "Inhibit", g_variant_new ("(ss)", "BSPLAY", "Playing something"),
          NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &err);
    if (resp) {
      s_sent_sleep_cookie = g_variant_get_uint32(g_variant_get_child_value(resp, 0));
      g_variant_unref (resp);
  //printf("Response rez=%d \n", s_sent_sleep_cookie);
    } else {
  //printf("No resp...\n");
      s_sent_sleep_cookie = SENT_SLEEP_NON;
    }
    if (con) {
      g_object_unref (con);
    }
  }
  //it may no work if XScreensaver isn't used...
  s_sent_sleep_status = SENT_SLEEP_OFF;
}

/* Send sleep on through DBUS */
static void su_send_sleep_on() {
  if (s_sent_sleep_cookie != SENT_SLEEP_NON) {
//printf("Sending sleep on...\n");
    GVariant *resp = NULL;
    GDBusConnection *con = NULL;
    GError *err = NULL;
    con = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (con && !g_dbus_connection_is_closed(con) && err == NULL) {
      resp = g_dbus_connection_call_sync (con,
        s_sleep_bus_name, s_sleep_srv_path, s_sleep_srv_name,
        "UnInhibit", g_variant_new ("(u)", s_sent_sleep_cookie),
          NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &err);
      if (resp) {
        g_variant_unref (resp);
      }
      if (con) {
        g_object_unref (con);
      }
    }
  }
  //it may no work if XScreensaver isn't used
  s_sent_sleep_status = SENT_SLEEP_ON;
}

  //Local lib base level:
/* Refresh GUI buttons, title, playing row on play/pause/next... event */
static void sf_refresh_playing() {
  PLAY_STATUS play_status = bsplay_play_status();
  if (s_playing_row >= 0) {
    if (play_status == PLAYING) {
      char **row = bsslist_get(&s_bsstpldtls, s_playing_row);
      su_fill_playing_row(row);
      gtk_window_set_title(GTK_WINDOW(s_window), row[0]);
      GList *glrow = g_list_nth(((GtkList *) s_gtklist)->children, s_playing_row);
      GtkItem gi = GTK_LIST_ITEM(glrow->data)->item;
      GList *glistlab = gtk_container_get_children(GTK_CONTAINER(&gi));
      gtk_label_set_text((GtkLabel *) glistlab->data, s_sbuffer);
      gtk_label_set_use_markup((GtkLabel *) glistlab->data, TRUE);
    } else if (play_status == STOPPED) {
      gtk_window_set_title(GTK_WINDOW(s_window), s_win_title);
      char **row = bsslist_get(&s_bsstpldtls, s_playing_row);
      sprintf(s_sbuffer, "(%s) %s", row[1], row[0]);
      GList *glrow = g_list_nth(((GtkList *) s_gtklist)->children, s_playing_row);
      GtkItem gi = GTK_LIST_ITEM(glrow->data)->item;
      GList *glistlab = gtk_container_get_children(GTK_CONTAINER(&gi));
      gtk_label_set_text((GtkLabel *) glistlab->data, s_sbuffer);
    }
  }
  if (s_last_played_row >= 0) {
    char **row = bsslist_get(&s_bsstpldtls, s_last_played_row);
    sprintf(s_sbuffer, "(%s) %s", row[1], row[0]);
    GList *glrow = g_list_nth(((GtkList *) s_gtklist)->children, s_last_played_row);
    GtkItem gi = GTK_LIST_ITEM(glrow->data)->item;
    GList *glistlab = gtk_container_get_children(GTK_CONTAINER(&gi));
    gtk_label_set_text((GtkLabel *) glistlab->data, s_sbuffer);
    s_last_played_row = -1;
  }
  if (play_status == STOPPED || play_status == PAUSED) {
    gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(s_btn_play), GTK_STOCK_MEDIA_PLAY);
  } else if (play_status == PLAYING) {
    gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(s_btn_play), GTK_STOCK_MEDIA_PAUSE);
  }
  if (play_status != STOPPED && s_sent_sleep_status != SENT_SLEEP_OFF) {
    su_send_sleep_off();
  } else if (play_status == STOPPED && s_sent_sleep_status == SENT_SLEEP_OFF) {
    su_send_sleep_on();
  }
}

/* Refresh GUI list. */
static void sf_refresh_list() {
  gtk_list_clear_items(((GtkList *) s_gtklist), 0, -1);
  for (int i = 0; i < s_bsstpldtls.rows; i++) {
    char **row = bsslist_get(&s_bsstpldtls, i);
    if (s_playing_row == i) {
      su_fill_playing_row(row);
    } else {
      sprintf(s_sbuffer, "(%s) %s", row[1], row[0]);
    }
    GtkWidget *list_item = gtk_list_item_new_with_label(s_sbuffer);
    gtk_container_add(GTK_CONTAINER(s_gtklist), list_item);
    if (s_playing_row == i) {
      GtkItem gi = GTK_LIST_ITEM(list_item)->item;
      GList *glistlab = gtk_container_get_children(GTK_CONTAINER(&gi));
      gtk_label_set_text((GtkLabel *) glistlab->data, s_sbuffer);
      gtk_label_set_use_markup((GtkLabel *) glistlab->data, TRUE);
    }
    gtk_widget_show(list_item);
  }
  s_selected_row = -1;
}

/* Save into m3u file dialog. */
static void sf_dialog_save() {
  GtkWidget *flchooser;
  flchooser = gtk_file_chooser_dialog_new(s_flchooser_title, (GtkWindow*) s_window,
    GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
      GTK_RESPONSE_ACCEPT, NULL);
  GtkFileFilter * flt = gtk_file_filter_new();
  gtk_file_filter_add_mime_type(flt, "audio/m3u");
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(flchooser), flt);
  gtk_dialog_run(GTK_DIALOG(flchooser));
  GFile *gfile = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(flchooser));
  if (gfile) {
    int is_do = TRUE;
    if (g_file_query_exists(gfile, NULL)) {
      is_do = su_dialog_confirm("Overwrite existing file?");
    }
    if (is_do) {
      FILE *file = fopen(g_file_get_path(gfile), "w+");
      if (file) {
        int hours, mins, secs;
        for (int i = 0; i < s_bsstpldtls.rows; i++) {
          char **row = bsslist_get(&s_bsstpldtls, i);
          int dur = 0;
          if (row[1] != s_unknown_str) {
            if (row[1] == s_unknown_dur) {
              dur = -1;
            } else {
              int crdd = sscanf(row[1], "%d:%d:%d", &hours, &mins, &secs);
              if (crdd == 3) {
                dur = hours * 3600 + mins * 60 + secs;
              }
            }
          }
          fprintf(file, "#EXTINF:%d,%s\n%s\n", dur, row[0], row[2]);
        }
        fclose(file);
      }
    }
  }
  gtk_widget_destroy(flchooser);
}

/* Choose file/s dialog. */
static void sf_dialog_choose() {
  GtkWidget *flchooser;
  flchooser = gtk_file_chooser_dialog_new(s_flchooser_title, (GtkWindow*) s_window,
    GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
      GTK_RESPONSE_ACCEPT, NULL);
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(flchooser), TRUE);
  GtkFileFilter * flt = gtk_file_filter_new();
  gtk_file_filter_add_mime_type(flt, "audio/m3u");
  gtk_file_filter_add_mime_type(flt, "audio/mpeg");
  gtk_file_filter_add_mime_type(flt, "audio/wma");
  gtk_file_filter_add_mime_type(flt, "video/mp4");
  gtk_file_filter_add_mime_type(flt, "video/3gpp");
  gtk_file_filter_add_mime_type(flt, "video/wmv");
  gtk_file_filter_add_mime_type(flt, "video/flv");
  gtk_file_filter_add_mime_type(flt, "video/mkv");
  gtk_file_filter_add_mime_type(flt, "video/vob");
  gtk_file_filter_add_mime_type(flt, "video/avi");
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(flchooser), flt);
  gtk_dialog_run(GTK_DIALOG(flchooser));
  GSList *files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(flchooser));
  if (files) {
    do {
      GFile *file = G_FILE(files->data);
      if (file) {
        gchar *bnm = g_file_get_basename(file);
        if (strstr(bnm, ".m3u")) {
          su_read_m3u(file);
        } else  {
          //it maybe folder on close, also filter wrong!
          char *ext = strrchr(bnm, '.');
          if (ext && (bsplay_is_ext_audio(ext) || bsplay_is_ext_video(ext))) {
            char *nme = malloc((strlen(bnm) + 1) * sizeof(char));
            strcpy(nme, bnm);
            char *pth = malloc((strlen(g_file_get_path(file)) + 1) * sizeof(char));
            strcpy(pth, g_file_get_path(file));
            char *fl[3] = { nme, s_unknown_str, pth };
            bsslist_add(&s_bsstpldtls, fl);
          }
        }
      }
      files = files->next;
    } while (files);
    if (s_random) {
      su_make_random_idxs();
    }
    sf_refresh_list();
  }
  gtk_widget_destroy(flchooser);
}

/* Set position in seconds (seek) dialog */
static void sf_dialog_seek() {
  GtkWidget *dialog, *hbox, *table;
  GtkWidget *entr_hours, *entr_mins, *entr_secs;
  GtkWidget *label;
  gint response;
  int hours = s_seek_pos_sec / 3600;
  int mins = (s_seek_pos_sec % 3600) / 60;
  int secs = s_seek_pos_sec % 60;
  char hourss[9];
  char minss[9];
  char secss[9];
  sprintf(hourss, "%d", hours);
  sprintf(minss, "%d", mins);
  sprintf(secss, "%d", secs);
  dialog = gtk_dialog_new_with_buttons("Go to position", GTK_WINDOW(s_window),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK,
			GTK_RESPONSE_OK,GTK_STOCK_CANCEL , GTK_RESPONSE_CANCEL,	NULL);
  hbox = gtk_hbox_new(FALSE, 8);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox, FALSE, FALSE, 0);
  table = gtk_table_new(6, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), 4);
  gtk_table_set_col_spacings(GTK_TABLE(table), 4);
  gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
  label = gtk_label_new("HH");
  gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
  label = gtk_label_new("MM");
  gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 0, 1);
  label = gtk_label_new("SS");
  gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, 0, 1);
  entr_hours = gtk_entry_new();
  gtk_entry_set_width_chars(GTK_ENTRY(entr_hours), 2);
  gtk_entry_set_text(GTK_ENTRY(entr_hours), hourss);
  gtk_table_attach_defaults(GTK_TABLE(table), entr_hours, 0, 1, 1, 2);
  entr_mins = gtk_entry_new();
  gtk_entry_set_width_chars(GTK_ENTRY(entr_mins), 2);
  gtk_entry_set_text(GTK_ENTRY(entr_mins), minss);
  gtk_table_attach_defaults(GTK_TABLE(table), entr_mins, 1, 2, 1, 2);
  entr_secs = gtk_entry_new();
  gtk_entry_set_width_chars(GTK_ENTRY(entr_secs), 2);
  gtk_entry_set_text(GTK_ENTRY(entr_secs), secss);
  gtk_table_attach_defaults(GTK_TABLE(table), entr_secs, 2, 3, 1, 2);
  gtk_widget_show_all(hbox);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  if (response == GTK_RESPONSE_OK) {
    int crdd = sscanf(gtk_entry_get_text(GTK_ENTRY(entr_hours)), "%d", &hours);
    if (crdd == 1) {
      crdd = sscanf(gtk_entry_get_text(GTK_ENTRY(entr_mins)), "%d", &mins);
    }
    if (crdd == 1) {
      crdd = sscanf(gtk_entry_get_text(GTK_ENTRY(entr_secs)), "%d", &secs);
    }
    if (crdd == 1) {
      s_seek_pos_sec = hours * 3600 + mins * 60 + secs;
      bsplay_set_position(s_seek_pos_sec);
    }
  }
  gtk_widget_destroy(dialog);
}

/* play previous row */
static int sf_play_previous() {
  if (s_bsstpldtls.rows == 0) {
    return 123;
  }
  int row = -1;
  int base_row = -1;
  if (s_playing_row >= 0) {
    base_row = s_playing_row;
  } else if (s_selected_row >= 0) {
    base_row = s_selected_row;
  }
  if (base_row >= 0) {
    if (!s_random) {
      row = base_row - 1;
      if (row < 0) {
        if (s_loop) {
          row = s_bsstpldtls.rows - 1;
        } else {
          row = -1;
        }
      }
    } else {
      row = su_random_previous(base_row);
    }
  } else {
    row = 0;
  }
  if (row >= 0 && row < s_bsstpldtls.rows) {
     su_play_row(row);
  } else {
    bsplay_stop();
  }
  return 0;
}

/* Free allocated here data, e.g. list's strings created here with malloc and aren't used in GTK widgets */
static void sf_free_here() {
  if (s_srt_file) {
    free(s_srt_file);
  }
  if (s_random_idxs) {
    free(s_random_idxs);
  }
  if (s_bsstpldtls.list) {
    for (int i = 0; i < s_bsstpldtls.buf_size; i++) {
      char *durs = *(s_bsstpldtls.list + i * s_bsstpldtls.columns + 1);
      if (durs && durs != s_unknown_str && durs != s_unknown_dur) {
        free(durs);
      }
      char *pth = *(s_bsstpldtls.list + i * s_bsstpldtls.columns + 2);
      if (pth) {
        free(pth);
      }
      //name will be freed by GTK during label in list destroying! otherwise: double free or corruption
    }
  }
}

/* play next row */
static int sf_play_next() {
  if (s_bsstpldtls.rows == 0) {
    return 123;
  }
  int row = -1;
  int base_row = -1;
  if (s_playing_row >= 0) {
    base_row = s_playing_row;
  } else if (s_selected_row >= 0) {
    base_row = s_selected_row;
  }
  if (base_row >= 0) {
    if (!s_random) {
      row = base_row + 1;
      if (row > s_bsstpldtls.rows - 1) {
        if (s_loop) {
          row = 0;
        } else {
          row = -1;
        }
      }
    } else {
      row = su_random_next(base_row);
    }
  } else {
    row = 0;
  }
  if (row >= 0 && row < s_bsstpldtls.rows) {
    su_play_row(row);
  } else {
    bsplay_stop();
  }
  return 0;
}

/* play selected row */
static void sf_play_selected() {
  if (s_selected_row >= 0 && s_selected_row < s_bsstpldtls.rows) {
    su_play_row(s_selected_row);
  }
}

  //Local lib event handlers level:
/* play next row */
static void sfe_play_next(gpointer p_data) {
  sf_play_next();
}

/* play previous row */
static void sfe_play_previous(gpointer p_data) {
  sf_play_previous();
}

/* move item up */
static void sfe_item_up(gpointer p_data) {
  if (s_selected_row >=0 && bsslist_up(&s_bsstpldtls, s_selected_row) == 0) {
    int sr = s_selected_row;
    sf_refresh_list();
    s_selected_row = sr - 1;
    gtk_list_select_item(GTK_LIST(s_gtklist), s_selected_row);
  }
}

/* move item up */
static void sfe_item_down(gpointer p_data) {
  if (s_selected_row >=0 && bsslist_down(&s_bsstpldtls, s_selected_row) == 0) {
    int sr = s_selected_row;
    sf_refresh_list();
    s_selected_row = sr + 1;
    gtk_list_select_item(GTK_LIST(s_gtklist), s_selected_row);
  }
}

/* On click event play/pause. */
static void sfe_play_pause(gpointer p_data) {
  PLAY_STATUS play_status = bsplay_play_status();
  if (play_status == PAUSED) {
    bsplay_pause_toggle();
  } else if (play_status == PLAYING) {
    bsplay_pause_toggle();
  } else if (s_selected_row == -1) { //excessive checking
    return;
  } else { // else start playing selected entry:
    sf_play_selected();
  }
}

/* On click event stop play. */
static void sfe_stop(gpointer p_data) {
  PLAY_STATUS play_status = bsplay_play_status();
  if (play_status != STOPPED) {
    bsplay_stop();
  }
}

/* Delete row. */
static void sf_delete_row() {
  if (s_selected_row >= 0) {
    bsslist_del(&s_bsstpldtls, s_selected_row);
    if (s_random) {
      su_make_random_idxs();
    }
    sf_refresh_list();
  }
}

/* On select row (double click) event handler. */
static void sfe_on_menu_item_click(int *p_menu_val) {
  PLAY_STATUS play_status = bsplay_play_status();
  switch (*p_menu_val) {
    case DELETE_SELECTED:
      sf_delete_row();
      break;
    case ADD_FILE:
      sf_dialog_choose();
      break;
    case TOGGLE_RANDOM:
      s_random = !s_random;
      if( s_random) {
        su_make_random_idxs();
      }
      break;
    case TOGGLE_LOOP:
      s_loop = !s_loop;
      break;
    case CLEAR_LIST:
      if (play_status == STOPPED) {
        bsslist_clear(&s_bsstpldtls);
        s_playing_row = -1;
        sf_refresh_list();
      }
      break;
    case GOTO_POSITION:
      if (play_status != STOPPED) {
        sf_dialog_seek();
      }
      break;
    case SAVE_M3U:
      if (s_bsstpldtls.rows > 0) {
        sf_dialog_save();
      }
      break;
  }
}

static void sfe_menu_open(GtkWidget *p_menu) {
  gtk_menu_popup(GTK_MENU(p_menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

/* On select row (double click) event handler. */
void sfe_on_row_select(GtkWidget *p_data1, gpointer p_data2) {
  if (((GtkList *) s_gtklist)->selection) { //type-safe checking GTK_LIST (gtklist)->selection is more slow
    s_selected_row = g_list_index(((GtkList *) s_gtklist)->children, ((GtkList *) s_gtklist)->selection->data);
//printf("Selected %d ", s_selected_row);
    //sf_play_selected();
  }
}

/* Refreshing thread */
static gboolean sfe_refresh_gui(gpointer p_data) {
  if (bsplay_play_status() == PENDING) {
    return TRUE;
  }
  if (--s_need_refresh_playing == 1) {
    sf_refresh_playing();
    s_need_refresh_playing = REFRESH_PLAYING; //reducing refreshing list frequency
  }
  if (bsplay_play_status() == STOPPED) {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s_progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(s_progress_bar), "");
  } else if (bsplay_play_status() == PLAYING) {
    int dur = bsplay_duration();
    if (dur >= 0) { // a file
      int pos = bsplay_position();
      if (pos > dur + PAUSE_TO_NEXT) {
        sf_play_next();
      } else if (pos != BSS_LOCKING) {
        if (pos < 0) {
          pos = 0;
        }
        if (s_playing_row_need_dur) {
          char** row = bsslist_get(&s_bsstpldtls, s_playing_row);
          char *durs = su_time_str(dur);
          row[1] = durs;
          s_playing_row_need_dur = FALSE;
          sf_refresh_list();
        }
        char sbuffer[20];
        s_timeb.tm_hour = pos / 3600;
        s_timeb.tm_min = (pos % 3600) / 60;
        s_timeb.tm_sec = pos % 60;
        strftime(sbuffer, 9, "%T", &s_timeb);
        sbuffer[8] = '/';
        s_timeb.tm_hour = dur / 3600;
        s_timeb.tm_min = (dur % 3600) / 60;
        s_timeb.tm_sec = dur % 60;
        strftime(sbuffer + 9, 10, "%T", &s_timeb);
        double frac = (double) pos / (double) dur;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s_progress_bar), frac);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(s_progress_bar), sbuffer);
      }
    } else if (dur != BSS_LOCKING) { // a radio
      gtk_progress_bar_set_text(GTK_PROGRESS_BAR(s_progress_bar), "");
      gtk_progress_bar_pulse(GTK_PROGRESS_BAR(s_progress_bar));
    }
  }
  return TRUE;
}

/* On exit event handler. */
static void sfe_on_exit() {
  if (s_timer != -1) {
    g_source_remove (s_timer);
  }
  sf_free_here();
  bsslist_free(&s_bsstpldtls);
  bsplay_destroy_av();
  gtk_main_quit();
}

//Shared lib:
/* Check if file has audio extension, e.g. mp3 */
int bsplay_is_audio(char *p_file_nme) {
  char *ext = strrchr(p_file_nme, '.');
  return bsplay_is_ext_audio(ext);
}

/* Check if file has video extension, e.g. mp4 */
int bsplay_is_video(char *p_file_nme) {
  char *ext = strrchr(p_file_nme, '.');
  return bsplay_is_ext_video(ext);
}

/* Check if it's audio extension, e.g. mp3 */
int bsplay_is_ext_audio(char *p_ext) {
  char *audio_exts = ".3gp.aa.aac.aax.act.aiff.alac.amr.ape.au.awb.dvf.flac.m4a.mp3.mpc.nmf.ogg.oga.mogg.opus.sln.tta.voc.vox.wav.wma";
  char *fnd = strstr(audio_exts, p_ext);
  return fnd != NULL;
}

/* Check if it's video extension, e.g. mp4 */
int bsplay_is_ext_video(char *p_ext) {
  char *video_exts = ".3gp.3g2.asf.amv.avi.m4v.mkv.mov.mp4.m4p.m4v.mpg.mp2.mpeg.mpe.mpv.flv.ogv.ogg.qt.vob.webm.wmv.asf";
  char *fnd = strstr(video_exts, p_ext);
  return fnd != NULL;
}

/* Check string#1 starts with p_len first characters of string#2 */
int bs_str_start(char *p_str1, char *p_str2, int p_len) {
  if (strlen(p_str1) < p_len) {
    return FALSE;
  }
  int rez = TRUE;
  for (int i = 0; i < p_len; i++) {
    if (p_str1[i] != p_str2[i]) {
      rez = FALSE;
      break;
    }
  }
  return rez;
}

int main(int argc, char *argv[]) {
  s_argc = argc;
  s_argv = argv;
  GtkWidget *vbox, *align;
  GtkWidget *scrolled_window;
  GtkToolItem *btn_down, *btn_up, *btn_menu;    
  GtkWidget *tool_bar;
  GtkWidget *menu;
  GtkWidget *menu_item;
  //init, main window:
  gtk_init(&argc, &argv);
  s_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request(GTK_WIDGET(s_window), 300, 150);
  gtk_window_set_title(GTK_WINDOW(s_window), s_win_title);
  g_signal_connect(G_OBJECT(s_window), "destroy", G_CALLBACK(sfe_on_exit), NULL);
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
  gtk_container_add(GTK_CONTAINER(s_window), vbox);
  //gtklist:
  scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
  s_gtklist = gtk_list_new();
  GTK_LIST(s_gtklist)->selection_mode = GTK_SELECTION_SINGLE;
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), s_gtklist);
  g_signal_connect(s_gtklist, "selection-changed", G_CALLBACK (sfe_on_row_select), NULL);
  //g_signal_connect(s_gtklist, "button-release-event", G_CALLBACK (sfe_gtklist_btn_release), NULL);
  //progress bar:
  s_progress_bar = gtk_progress_bar_new();
  align = gtk_alignment_new(0.5, 0.5, 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), align, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(align), s_progress_bar);
  s_timer = gdk_threads_add_timeout(REFRESH_PROGRESS, sfe_refresh_gui, NULL);
  //menu:
  menu = gtk_menu_new();
  sprintf(s_sbuffer, "Add file/list"); 
  menu_item = gtk_menu_item_new_with_label(s_sbuffer);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect_swapped(menu_item, "activate",
    G_CALLBACK(sfe_on_menu_item_click), s_menu_vals + ADD_FILE);
  gtk_widget_show (menu_item);
  sprintf(s_sbuffer, "Save play-list"); 
  menu_item = gtk_menu_item_new_with_label(s_sbuffer);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect_swapped(menu_item, "activate",
    G_CALLBACK(sfe_on_menu_item_click), s_menu_vals + SAVE_M3U);
  gtk_widget_show (menu_item);
  sprintf(s_sbuffer, "Go to position"); 
  menu_item = gtk_menu_item_new_with_label(s_sbuffer);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect_swapped(menu_item, "activate",
    G_CALLBACK(sfe_on_menu_item_click), s_menu_vals + GOTO_POSITION);
  gtk_widget_show (menu_item);
  sprintf(s_sbuffer, "Toggle random"); 
  menu_item = gtk_menu_item_new_with_label(s_sbuffer);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect_swapped(menu_item, "activate",
    G_CALLBACK(sfe_on_menu_item_click), s_menu_vals + TOGGLE_RANDOM);
  gtk_widget_show (menu_item);
  sprintf(s_sbuffer, "Toggle loop all"); 
  menu_item = gtk_menu_item_new_with_label(s_sbuffer);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect_swapped(menu_item, "activate",
    G_CALLBACK(sfe_on_menu_item_click), s_menu_vals + TOGGLE_LOOP);
  gtk_widget_show (menu_item);
  sprintf(s_sbuffer, "Delete selected"); 
  menu_item = gtk_menu_item_new_with_label(s_sbuffer);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect_swapped(menu_item, "activate",
    G_CALLBACK(sfe_on_menu_item_click), s_menu_vals + DELETE_SELECTED);
  gtk_widget_show (menu_item);
  sprintf(s_sbuffer, "Clear list"); 
  menu_item = gtk_menu_item_new_with_label(s_sbuffer);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect_swapped(menu_item, "activate",
    G_CALLBACK(sfe_on_menu_item_click), s_menu_vals + CLEAR_LIST);
  gtk_widget_show (menu_item);
  //tool bar:
  tool_bar = gtk_toolbar_new();
  gtk_box_pack_start(GTK_BOX(vbox), tool_bar, FALSE, FALSE, 0);
  //tool bar buttons:
  s_btn_play = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
  gtk_tool_item_set_tooltip_text(s_btn_play, "Play/pause");
  gtk_toolbar_insert(GTK_TOOLBAR(tool_bar), s_btn_play, -1);
  s_btn_previous = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS);
  gtk_tool_item_set_tooltip_text(s_btn_previous, "Previous");
  gtk_toolbar_insert(GTK_TOOLBAR(tool_bar), s_btn_previous, -1);
  s_btn_stop = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
  gtk_tool_item_set_tooltip_text(s_btn_stop, "Stop");
  gtk_toolbar_insert(GTK_TOOLBAR(tool_bar), s_btn_stop, -1);
  s_btn_next = gtk_tool_button_new_from_stock(GTK_STOCK_MEDIA_NEXT);
  gtk_tool_item_set_tooltip_text(s_btn_next, "Next");
  gtk_toolbar_insert(GTK_TOOLBAR(tool_bar), s_btn_next, -1);
  btn_up = gtk_tool_button_new_from_stock(GTK_STOCK_GO_UP);
  gtk_tool_item_set_tooltip_text(btn_up, "Move up");
  gtk_toolbar_insert(GTK_TOOLBAR(tool_bar), btn_up, -1);
  btn_down = gtk_tool_button_new_from_stock(GTK_STOCK_GO_DOWN);
  gtk_tool_item_set_tooltip_text(btn_down, "Move down");
  gtk_toolbar_insert(GTK_TOOLBAR(tool_bar), btn_down, -1);
  btn_menu = gtk_tool_button_new_from_stock(GTK_STOCK_INDEX);
  gtk_tool_item_set_tooltip_text(btn_menu, "Menu");
  gtk_toolbar_insert(GTK_TOOLBAR(tool_bar), btn_menu, -1);
  g_signal_connect_swapped(s_btn_play, "clicked", G_CALLBACK(sfe_play_pause), NULL);
  g_signal_connect_swapped(s_btn_stop, "clicked", G_CALLBACK(sfe_stop), NULL);
  g_signal_connect_swapped(s_btn_next, "clicked", G_CALLBACK(sfe_play_next), NULL);
  g_signal_connect_swapped(s_btn_previous, "clicked", G_CALLBACK(sfe_play_previous), NULL);
  g_signal_connect_swapped(btn_up, "clicked", G_CALLBACK(sfe_item_up), NULL);
  g_signal_connect_swapped(btn_down, "clicked", G_CALLBACK(sfe_item_down), NULL);
  //g_signal_connect_swapped(btn_menu, "event", G_CALLBACK(sfe_menu_open1), menu);
  g_signal_connect_swapped(btn_menu, "clicked", G_CALLBACK(sfe_menu_open), menu);
  gtk_window_set_icon_name(GTK_WINDOW(s_window), "bssoft");
  //launch all:
  gtk_widget_show_all(s_window);
  gtk_main();
  return 0;
}
