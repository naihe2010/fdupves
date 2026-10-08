/*
 * This file is part of the fdupves package
 * Copyright (C) <2010>  <Alf>
 *
 * Contact: Alf <naihe2010@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
/* @CFILE gui.c
 *
 *  Author: Alf <naihe2010@126.com>
 */
/* @date Created: 2013/01/16 13:36:10 Alf*/

#include "gui.h"
#include "audio.h"
#include "cache.h"
#include "find.h"
#include "image.h"
#include "ini.h"
#include "util.h"
#include "video.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <shellapi.h>
#endif

typedef struct
{
  same_type type;
  GSList *files;
  GtkTreeRowReference *treerowref;
  gboolean show;
} same_node;

void same_node_free (same_node *);

void same_list_free (GSList *);

struct file_node_s
{
  /* same node */
  same_node *node;

  gchar *path;
  gchar *name;
  gchar *dir;

  /*
    0, FD_IMAGE
    1, FD_VIDEO
    2, FD_AUDIO */
  gint type;

  /* format desc */
  gchar *format;

  /* file size */
  gint size;

  /* image/screenshot size */
  gint width, height;

  /* video length */
  gdouble length;

  /* bool select */
  gboolean selected;
};

static file_node *file_node_new (same_node *, const gchar *, gint);

static void file_node_free (file_node *);

/* free the file node and same_node if necesstity */
static void file_node_free_full (file_node *);

static void file_node_to_tree_iter (file_node *, GtkTreeStore *,
                                    GtkTreeIter *);

typedef struct
{
  GtkWidget *dialog;

  GtkWidget *content;
  GtkWidget *container;

  GtkWidget *butprev, *butnext;

  const file_node *afn, *bfn;

  gboolean from_tail;

  gint index;

  gint alen;
  gint blen;

  gui_t *gui;
} diff_dialog;

static void diff_dialog_new (gui_t *, const file_node *, const file_node *);

static void diffdia_onclose (GtkWidget *, diff_dialog *);

static void diffdia_onresponse (GtkWidget *, gint, diff_dialog *);

static void diffdia_onseekchanged (GtkEntry *, diff_dialog *);

static void diffdia_onheadtail (GtkToggleButton *, diff_dialog *);

static void diffdia_onnext (GtkWidget *, diff_dialog *);

static void diffdia_onprev (GtkWidget *, diff_dialog *);

static void toolbar_new (gui_t *);

static void mainframe_new (gui_t *);

static GtkWidget *dir_list_new (gui_t *);

static GtkWidget *find_attr_new (gui_t *);

static GtkWidget *res_tree_new (gui_t *);

static GtkWidget *log_view_new (gui_t *);

static void progressbar_new (gui_t *);

static void gui_add_cb (GtkWidget *, gui_t *);

static void gui_find_cb (GtkWidget *, gui_t *);

static void gui_delete_seleted_cb (GtkWidget *, gui_t *);

static void gui_pref_cb (GtkWidget *, gui_t *);

static void gui_cleanup_cb (GtkWidget *, gui_t *);

static void gui_help_cb (GtkWidget *, gui_t *);

static void gui_find_step_cb (const find_step *, gui_t *);

static GSList *gui_append_same_slist (gui_t *, GSList *, const gchar *,
                                      const gchar *, same_type);

static gui_t gui[1];

static void gui_destroy (gui_t *);

static void gui_destroy_cb (GtkWidget *, GdkEvent *, gui_t *);

static void gui_add_dir (gui_t *, const gchar *);

static gboolean gui_find_dispatch (gui_t *);

static void gui_save_directories (gui_t *);

static void gui_load_directories (gui_t *);

static gboolean dir_find_item (GtkTreeModel *, GtkTreePath *, GtkTreeIter *,
                               gui_t *);

static void gui_list_dir (gui_t *, const gchar *);

static void gui_list_file (gui_t *, const gchar *);

static void gui_list_link (gui_t *, const gchar *);

static gboolean gui_queue_timer_callback (gui_t *gui);

static void gui_process_step (gui_t *gui, const find_step *step);

static void gui_process_log (gui_t *gui, gchar *log);

static void result_select_small_file (same_node *node, gui_t *);

static void result_select_big_file (same_node *node, gui_t *);

static void result_select_small_image (same_node *node, gui_t *);

static void result_select_big_image (same_node *node, gui_t *);

static void result_select_short_video (same_node *node, gui_t *);

static void result_select_long_video (same_node *node, gui_t *);

static void result_select_others (same_node *node, gui_t *);

static void result_filter_changed (GtkEntry *, gui_t *);

static void result_selcombo_changed (GtkComboBox *, gui_t *);

static void dirlist_onactivated (GtkTreeView *, GtkTreePath *,
                                 GtkTreeViewColumn *, gui_t *);

static gboolean result_onbutpress (GtkWidget *, GdkEventButton *, gui_t *);

static void result_onactivated (GtkTreeView *, GtkTreePath *,
                                GtkTreeViewColumn *, gui_t *);

static void resultsel_onchanged (GtkTreeSelection *, gui_t *);

static GtkWidget *result_open_menuitem (gui_t *);

static GtkWidget *result_opendir_menuitem (gui_t *);

static GtkWidget *result_delete_menuitem (gui_t *);

static GtkWidget *result_diff_menuitem (gui_t *);

static void result_open (GtkMenuItem *, gui_t *);

static void result_opendir (GtkMenuItem *, gui_t *);

static void result_delete (GtkMenuItem *, gui_t *);

static void result_diff (GtkMenuItem *, gui_t *);

#ifndef FDUPVES_THREAD_STACK_SIZE
#define FDUPVES_THREAD_STACK_SIZE (1024 * 1024 * 10)
#endif

#ifndef FDUPVES_MAXLOG
#define FDUPVES_MAXLOG 1000
#endif

gboolean
gui_init (int argc, char *argv[])
{
  int i;
  gchar *insdir, *iconfile;

  if (ini_new_with_file (FD_USR_CONF_FILE) == FALSE)
    {
      ini_new ();
    }

  gui->quit = FALSE;

  gui->widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (gui->widget), PACKAGE_STRING);
  gtk_window_set_default_size (GTK_WINDOW (gui->widget), 800, 600);

  insdir = fd_install_path ();
  if (insdir)
    {
      iconfile
          = g_build_filename (insdir, FD_SYS_ICON_DIR, PROJECT_ICON, NULL);
      gtk_window_set_icon_from_file (GTK_WINDOW (gui->widget), iconfile, NULL);
      g_free (iconfile);
      g_free (insdir);
    }

  g_signal_connect (G_OBJECT (gui->widget), "destroy-event",
                    G_CALLBACK (gui_destroy_cb), gui);
  g_signal_connect (G_OBJECT (gui->widget), "delete-event",
                    G_CALLBACK (gui_destroy_cb), gui);

  gui->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_container_add (GTK_CONTAINER (gui->widget), gui->main_vbox);

  toolbar_new (gui);
  mainframe_new (gui);
  progressbar_new (gui);

  gui->step_queue = g_async_queue_new_full (g_free);
  gui->log_queue = g_async_queue_new_full (g_free);
  gui->queue_timer
      = g_timeout_add (500, G_SOURCE_FUNC (gui_queue_timer_callback), gui);

  gtk_widget_show_all (gui->widget);

  if (argc > 1)
    {
      for (i = 1; i < argc; ++i)
        {
          gui_add_dir (gui, argv[i]);
        }
    }
  else
    {
      gui_load_directories (gui);
    }

  return TRUE;
}

static GtkWidget *
fd_toolbar_icon_new (const gchar *name)
{
  gchar *prgdir, *icondir, *iconfile;
  GtkWidget *img;

  img = NULL;

  prgdir = fd_install_path ();
  if (prgdir)
    {
      icondir = g_build_filename (prgdir, FD_SYS_ICON_DIR, NULL);
      if (icondir)
        {
          iconfile = g_build_filename (icondir, name, NULL);
          if (iconfile)
            {
              img = gtk_image_new_from_file (iconfile);
              g_free (iconfile);
            }

          g_free (icondir);
        }

      g_free (prgdir);
    }

  return img;
}

static void
toolbar_new (gui_t *gui)
{
  GtkWidget *toolbar;
  GtkWidget *img;
  GtkToolItem *but;

  toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH);
  gtk_box_pack_start (GTK_BOX (gui->main_vbox), toolbar, FALSE, FALSE, 2);

  img = fd_toolbar_icon_new ("add.png");
  gui->but_add = gtk_tool_button_new (img, _ ("Add"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (gui->but_add),
                               _ ("Add Source Files"));
  g_signal_connect (G_OBJECT (gui->but_add), "clicked",
                    G_CALLBACK (gui_add_cb), gui);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), gui->but_add, -1);

  img = fd_toolbar_icon_new ("find.png");
  gui->but_find = gtk_tool_button_new (img, _ ("Find"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (gui->but_find),
                               _ ("Find Duplicate Files"));
  g_signal_connect (G_OBJECT (gui->but_find), "clicked",
                    G_CALLBACK (gui_find_cb), gui);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), gui->but_find, -1);

  img = fd_toolbar_icon_new ("del.png");
  gui->but_del = gtk_tool_button_new (img, _ ("Delete Selected"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (gui->but_del),
                               _ ("Delete the selected files"));
  g_signal_connect (G_OBJECT (gui->but_del), "clicked",
                    G_CALLBACK (gui_delete_seleted_cb), gui);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), gui->but_del, -1);

  img = fd_toolbar_icon_new ("pref.png");
  but = gtk_tool_button_new (img, _ ("Preference"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (but), _ ("Preference"));
  g_signal_connect (G_OBJECT (but), "clicked", G_CALLBACK (gui_pref_cb), gui);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), but, -1);

  img = fd_toolbar_icon_new ("cleanup.png");
  but = gtk_tool_button_new (img, _ ("Cleanup"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (but),
                               _ ("Cleanup not used cache data"));
  g_signal_connect (G_OBJECT (but), "clicked", G_CALLBACK (gui_cleanup_cb),
                    gui);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), but, -1);

  img = fd_toolbar_icon_new ("help.png");
  but = gtk_tool_button_new (img, _ ("Help"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (but), _ ("Help infomations"));
  g_signal_connect (G_OBJECT (but), "clicked", G_CALLBACK (gui_help_cb), gui);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), but, -1);
}

static gchar *
gui_log_format (const gchar *message)
{
  GDateTime *datetime;
  gchar *dtstr;
  gchar *log;

  datetime = g_date_time_new_now_local ();
  if (datetime == NULL)
    return g_strdup (message);

  dtstr = g_date_time_format (datetime, "%Y-%m-%dT%H:%M:%S.%f");
  g_date_time_unref (datetime);
  if (dtstr == NULL)
    return g_strdup (message);

  log = g_strdup_printf ("%s %s", dtstr, message);
  g_free (dtstr);

  return log;
}

static void
gui_log (const gchar *log_domain, GLogLevelFlags log_level,
         const gchar *message, gpointer user_data)
{
  gui_t *gui;
  gchar *log;

  gui = (gui_t *)user_data;
  log = gui_log_format (message);
  g_async_queue_push (gui->log_queue, log);
}

static void
gui_process_log (gui_t *gui, gchar *log)
{
  GtkTreeIter itr[1];
  GtkTreePath *path;
  gboolean is_end;

  GtkAdjustment *vadjustment
      = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (gui->log_tree));
  gdouble value = gtk_adjustment_get_value (vadjustment);
  gdouble upper = gtk_adjustment_get_upper (vadjustment);
  gdouble page_size = gtk_adjustment_get_page_size (vadjustment);
  is_end = value + page_size + 1.0 >= upper;

  gtk_list_store_append (gui->log_store, itr);
  gtk_list_store_set (gui->log_store, itr, 0, log, -1);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (gui->log_store), itr);
  if (is_end)
    {
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (gui->log_tree), path, NULL,
                                    FALSE, 0.0, 0.0);
    }
  gtk_tree_path_free (path);

  if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (gui->log_store), NULL)
      >= FDUPVES_MAXLOG)
    {
      gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gui->log_store), itr);
      gtk_list_store_remove (gui->log_store, itr);
    }
}

static void
gui_ibutcb (GtkWidget *but, gui_t *gui)
{
  g_ini->proc_image = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (but));
}

static void
gui_vbutcb (GtkWidget *but, gui_t *gui)
{
  g_ini->proc_video = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (but));
}

static void
gui_abutcb (GtkWidget *but, gui_t *gui)
{
  g_ini->proc_audio = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (but));
}

static void
gui_ebutcb (GtkWidget *but, gui_t *gui)
{
  g_ini->proc_ebook = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (but));
}

static void
gui_compareareacb (GtkWidget *combo, gui_t *gui)
{
  g_ini->compare_area = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
}

static void
gui_filtertimeratecb (GtkWidget *combo, gui_t *gui)
{
  g_ini->filter_time_rate = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
}

static void
mainframe_new (gui_t *gui)
{
  GtkWidget *hpaned, *vpaned, *vbox, *win;

  vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_pack_start (GTK_BOX (gui->main_vbox), vpaned, TRUE, TRUE, 2);

  hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_add1 (GTK_PANED (vpaned), hpaned);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_paned_add1 (GTK_PANED (hpaned), vbox);

  win = dir_list_new (gui);
  gtk_box_pack_start (GTK_BOX (vbox), win, TRUE, TRUE, 2);

  win = find_attr_new (gui);
  gtk_box_pack_end (GTK_BOX (vbox), win, FALSE, FALSE, 2);

  win = res_tree_new (gui);
  gtk_paned_add2 (GTK_PANED (hpaned), win);

  win = log_view_new (gui);
  gtk_paned_add2 (GTK_PANED (vpaned), win);
}

static GtkWidget *
dir_list_new (gui_t *gui)
{
  GtkWidget *win, *dirview;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  /* dir list win */
  win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (win),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (win),
                                       GTK_SHADOW_IN);
  gui->dir_store = gtk_list_store_new (1, G_TYPE_STRING);
  dirview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (gui->dir_store));
  gtk_container_add (GTK_CONTAINER (win), dirview);
  gtk_widget_set_size_request (dirview, 300, 200);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (
      _ ("Source Dir"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dirview), column);

  g_signal_connect (G_OBJECT (dirview), "row-activated",
                    G_CALLBACK (dirlist_onactivated), gui);

  return win;
}

static GtkWidget *
find_attr_new (gui_t *gui)
{
  GtkWidget *typebox, *vbox;
  GtkWidget *comparearea, *filtertimerate;
  GtkWidget *typeibut, *typevbut, *typeabut, *typeebut;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  typebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start (GTK_BOX (vbox), typebox, FALSE, FALSE, 2);

  typeibut = gtk_check_button_new_with_label (_ ("Image"));
  gtk_box_pack_start (GTK_BOX (typebox), typeibut, FALSE, FALSE, 2);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (typeibut),
                                g_ini->proc_image);
  g_signal_connect (G_OBJECT (typeibut), "toggled", G_CALLBACK (gui_ibutcb),
                    gui);

  typevbut = gtk_check_button_new_with_label (_ ("Video"));
  gtk_box_pack_start (GTK_BOX (typebox), typevbut, FALSE, FALSE, 2);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (typevbut),
                                g_ini->proc_video);
  g_signal_connect (G_OBJECT (typevbut), "toggled", G_CALLBACK (gui_vbutcb),
                    gui);

  typeabut = gtk_check_button_new_with_label (_ ("Audio"));
  gtk_box_pack_start (GTK_BOX (typebox), typeabut, FALSE, FALSE, 2);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (typeabut),
                                g_ini->proc_audio);
  g_signal_connect (G_OBJECT (typeabut), "toggled", G_CALLBACK (gui_abutcb),
                    gui);

  typeebut = gtk_check_button_new_with_label (_ ("E-Book"));
  gtk_box_pack_start (GTK_BOX (typebox), typeebut, FALSE, FALSE, 2);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (typeebut),
                                g_ini->proc_ebook);
  g_signal_connect (G_OBJECT (typeebut), "toggled", G_CALLBACK (gui_ebutcb),
                    gui);

  typebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start (GTK_BOX (vbox), typebox, FALSE, FALSE, 2);

  comparearea = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (typebox), comparearea, FALSE, FALSE, 2);
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (comparearea),
                                  FD_COMPARE_ALL, _ ("Compare all"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (comparearea),
                                  FD_COMPARE_TOP, _ ("Compare top"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (comparearea),
                                  FD_COMPARE_BOTTOM, _ ("Compare bottom"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (comparearea),
                                  FD_COMPARE_LEFT, _ ("Compare left"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (comparearea),
                                  FD_COMPARE_RIGHT, _ ("Compare right"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (comparearea),
                                  FD_COMPARE_AUDIO_IN_VIDEO,
                                  _ ("compare audio in video"));
  g_signal_connect (G_OBJECT (comparearea), "changed",
                    G_CALLBACK (gui_compareareacb), gui);
  gtk_combo_box_set_active (GTK_COMBO_BOX (comparearea), g_ini->compare_area);

  filtertimerate = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (vbox), filtertimerate, FALSE, FALSE, 2);
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (filtertimerate),
                                  FD_FILTER_TIME_RATE_0, _ ("no time limit"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (filtertimerate),
                                  FD_FILTER_TIME_RATE_1,
                                  _ ("time limit in 1"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (filtertimerate),
                                  FD_FILTER_TIME_RATE_2,
                                  _ ("time limit in 1/2"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (filtertimerate),
                                  FD_FILTER_TIME_RATE_10,
                                  _ ("time limit int 1/10"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (filtertimerate),
                                  FD_FILTER_TIME_RATE_20,
                                  _ ("time limit in 1/20"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (filtertimerate),
                                  FD_FILTER_TIME_RATE_100,
                                  _ ("time limit in 1/100"));
  g_signal_connect (G_OBJECT (filtertimerate), "changed",
                    G_CALLBACK (gui_filtertimeratecb), gui);
  gtk_combo_box_set_active (GTK_COMBO_BOX (filtertimerate),
                            g_ini->filter_time_rate);

  return vbox;
}

static GtkWidget *
res_tree_new (gui_t *gui)
{
  GtkWidget *win, *vbox, *hbox, *scrwin;
  GtkWidget *combo, *entry;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  win = gtk_frame_new (_ ("Result"));

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (win), vbox);

  /* result filter */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 2);
  gui->result_filter = entry;
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry),
                                  _ ("filename filter condition"));
  g_signal_connect (G_OBJECT (entry), "changed",
                    G_CALLBACK (result_filter_changed), gui);

  combo = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 2);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("Select manual"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("select small path"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("select big path"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("select small image"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("select big image"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("select short video"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("select long video"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                  _ ("select others"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
  g_signal_connect (G_OBJECT (combo), "changed",
                    G_CALLBACK (result_selcombo_changed), gui);

  /* result tree */
  scrwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scrwin, TRUE, TRUE, 2);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrwin),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrwin),
                                       GTK_SHADOW_IN);
  gui->result_store
      = gtk_tree_store_new (6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  gui->result_tree
      = gtk_tree_view_new_with_model (GTK_TREE_MODEL (gui->result_store));
  gtk_container_add (GTK_CONTAINER (scrwin), gui->result_tree);

  /* path */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_ ("File Path"), renderer,
                                                     "text", 0, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (gui->result_tree), column);
  /* image size */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (
      _ ("Image Size"), renderer, "text", 1, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (gui->result_tree), column);
  /* file size */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_ ("File Size"), renderer,
                                                     "text", 2, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (gui->result_tree), column);
  /* video length */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (
      _ ("Video Length"), renderer, "text", 3, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (gui->result_tree), column);
  /* format */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_ ("Format"), renderer,
                                                     "text", 4, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (gui->result_tree), column);

  gtk_widget_add_events (GTK_WIDGET (gui->result_tree), GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (gui->result_tree), "button-press-event",
                    G_CALLBACK (result_onbutpress), gui);
  g_signal_connect (G_OBJECT (gui->result_tree), "row-activated",
                    G_CALLBACK (result_onactivated), gui);
  gui->result_select
      = gtk_tree_view_get_selection (GTK_TREE_VIEW (gui->result_tree));
  gtk_tree_selection_set_mode (gui->result_select, GTK_SELECTION_MULTIPLE);
  g_signal_connect (G_OBJECT (gui->result_select), "changed",
                    G_CALLBACK (resultsel_onchanged), gui);

  return win;
}

static GtkWidget *
log_view_new (gui_t *gui)
{
  GtkWidget *win;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (win),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (win),
                                       GTK_SHADOW_IN);
  gui->log_store = gtk_list_store_new (1, G_TYPE_STRING);
  gui->log_tree
      = gtk_tree_view_new_with_model (GTK_TREE_MODEL (gui->log_store));
  gtk_container_add (GTK_CONTAINER (win), gui->log_tree);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_ ("Message"), renderer,
                                                     "text", 0, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (gui->log_tree), column);

  g_log_set_handler (NULL, G_LOG_LEVEL_MASK, gui_log, gui);

  return win;
}

static void
progressbar_new (gui_t *gui)
{
  gui->progress = gtk_progress_bar_new ();
  gtk_box_pack_end (GTK_BOX (gui->main_vbox), gui->progress, FALSE, FALSE, 2);
}

static void
gui_add_dir (gui_t *gui, const char *path)
{
  GtkTreeIter itr[1];

  gtk_list_store_append (gui->dir_store, itr);
  gtk_list_store_set (gui->dir_store, itr, 0, path, -1);
}

static void
gui_add_cb (GtkWidget *wid, gui_t *gui)
{
  GtkWidget *dia;

  dia = gtk_file_chooser_dialog_new (
      _ ("Choose Folder"), GTK_WINDOW (gui->widget),
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _ ("OK"), GTK_RESPONSE_OK,
      _ ("Cancel"), GTK_RESPONSE_CANCEL, NULL);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dia), TRUE);

  if (gtk_dialog_run (GTK_DIALOG (dia)) == GTK_RESPONSE_OK)
    {
      GSList *list, *cur;

      list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (dia));
      for (cur = list; cur; cur = g_slist_next (cur))
        {
          gui_add_dir (gui, cur->data);
          g_free (cur->data);
        }
      g_slist_free (list);
    }
  gtk_widget_destroy (dia);
}

static void
gui_signal_dispatch (gui_t *gui, int state)
{
  g_atomic_int_set (&gui->state, state);
  g_timeout_add (100, (GSourceFunc)gui_find_dispatch, gui);
}

static void
gui_find_cb (GtkWidget *wid, gui_t *gui)
{
  gui_signal_dispatch (gui, FDUPVES_FIND_STARTED);
}

static void
gui_save_directories (gui_t *gui)
{
  gint count, i;
  GtkTreeIter itr;
  gchar *path;

  if (g_ini->directories)
    {
      g_strfreev (g_ini->directories);
      g_ini->directories = NULL;
      g_ini->directory_count = 0;
    }

  count
      = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (gui->dir_store), NULL);
  if (count <= 0)
    {
      return;
    }

  g_ini->directory_count = count;
  g_ini->directories = (gchar **)g_new0 (gchar *, count + 1);

  for (i = 0; i < count; ++i)
    {
      if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (gui->dir_store), &itr,
                                         NULL, i))
        {
          gtk_tree_model_get (GTK_TREE_MODEL (gui->dir_store), &itr, 0, &path,
                              -1);
          g_ini->directories[i] = path;
        }
    }
}

static void
gui_load_directories (gui_t *gui)
{
  int i;
  for (i = 0; i < g_ini->directory_count; ++i)
    {
      gui_add_dir (gui, g_ini->directories[i]);
    }
}

static guint
gui_wait_same_count (gui_t *gui)
{
  // because the find_step struct using const char *area
  while (g_async_queue_length (gui->step_queue) > 0)
    {
      g_usleep (100 * 1000);
    }

  if (gui->same_list == NULL)
    return 0;

  return g_slist_length (gui->same_list);
}

static int
gui_find_thread_func (gui_t *gui)
{
  guint fimage = 0;
  guint fvideo = 0;
  guint faudio = 0;
  guint febook = 0;

  if (g_ini->proc_image && gui->images->len > 0)
    {
      g_message (_ ("find %u images to process"), gui->images->len);
      find_images (gui->images, (find_step_cb)gui_find_step_cb, gui);
      fimage = gui_wait_same_count (gui);
      g_message (_ ("found %u groups of same images"), fimage);
    }

  if (g_ini->proc_video && gui->videos->len > 0)
    {
      g_message (_ ("find %u videos to process"), gui->videos->len);
      if (g_ini->compare_area == FD_COMPARE_AUDIO_IN_VIDEO)
        {
          find_audios (gui->videos, (find_step_cb)gui_find_step_cb, gui);
        }
      else
        {
          find_videos (gui->videos, (find_step_cb)gui_find_step_cb, gui);
        }
      fvideo = gui_wait_same_count (gui);
      fvideo -= fimage;
      g_message (_ ("found %u groups of same videos"), fvideo);
    }

  if (g_ini->proc_audio && gui->audios->len > 0)
    {
      g_message (_ ("find %u audios to process"), gui->audios->len);
      find_audios (gui->audios, (find_step_cb)gui_find_step_cb, gui);
      faudio = gui_wait_same_count (gui);
      faudio -= fimage;
      faudio -= fvideo;
      g_message (_ ("found %u groups of same audios"), faudio);
    }

  if (g_ini->proc_ebook && gui->ebooks->len > 0)
    {
      g_message (_ ("find %u ebooks to process"), gui->ebooks->len);
      find_ebooks (gui->ebooks, (find_step_cb)gui_find_step_cb, gui);
      febook = gui_wait_same_count (gui);
      febook -= fimage;
      febook -= fvideo;
      febook -= faudio;
      g_message (_ ("found %u groups of same ebooks"), febook);
    }

  gui_signal_dispatch (gui, FDUPVES_FIND_THREAD_FINISHED);
  return 0;
}

static int
gui_find_thread_start (gui_t *gui)
{
  GThread *thread;
  thread = g_thread_new ("find", (GThreadFunc)gui_find_thread_func, gui);
  g_thread_unref (thread);
  return 0;
}

static void
gui_find_started (gui_t *gui)
{
  gtk_widget_set_sensitive (GTK_WIDGET (gui->but_add), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (gui->but_find), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (gui->but_del), FALSE);

  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (gui->progress), "");
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (gui->progress), 0);

  gtk_tree_store_clear (gui->result_store);
  if (gui->same_list)
    {
      same_list_free (gui->same_list);
      gui->same_list = NULL;
    }

  gui->images = g_ptr_array_new_with_free_func (g_free);
  gui->videos = g_ptr_array_new_with_free_func (g_free);
  gui->audios = g_ptr_array_new_with_free_func (g_free);
  gui->ebooks = g_ptr_array_new_with_free_func (g_free);

  gui_signal_dispatch (gui, FDUPVES_FIND_SCAN_FILE);
}

static void
gui_find_scan_file (gui_t *gui)
{
  gtk_tree_model_foreach (GTK_TREE_MODEL (gui->dir_store),
                          (GtkTreeModelForeachFunc)dir_find_item, gui);
  gui_signal_dispatch (gui, FDUPVES_FIND_THREAD_START);
}

static void
gui_find_finished (gui_t *gui)
{
  g_ptr_array_free (gui->images, TRUE);
  g_ptr_array_free (gui->videos, TRUE);
  g_ptr_array_free (gui->audios, TRUE);
  g_ptr_array_free (gui->ebooks, TRUE);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (gui->result_tree));

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (gui->progress), 0);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (gui->progress), "");

  /* enable the add/find tool */
  gtk_widget_set_sensitive (GTK_WIDGET (gui->but_add), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (gui->but_find), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (gui->but_del), TRUE);

  g_atomic_int_set (&gui->state, FDUPVES_FIND_INIT);
}

static gboolean
gui_find_dispatch (gui_t *gui)
{
  switch (g_atomic_int_get (&gui->state))
    {
    case FDUPVES_FIND_STARTED:
      gui_find_started (gui);
      break;

    case FDUPVES_FIND_SCAN_FILE:
      gui_find_scan_file (gui);
      break;

    case FDUPVES_FIND_THREAD_START:
      gui_find_thread_start (gui);
      break;

    case FDUPVES_FIND_INIT:
    case FDUPVES_FIND_THREAD_FINISHED:
    case FDUPVES_FIND_ERROR:
    default:
      gui_find_finished (gui);
    }
  return FALSE;
}

static void
gui_pref_cb (GtkWidget *wid, gui_t *gui)
{
  GtkWidget *dialog, *vbox, *frame, *label, *hbox;
  GtkWidget *i_exts, *v_exts, *a_exts;
  GtkWidget *i_level, *v_level, *a_level;
  GtkWidget *thd_count;
  GtkWidget *ebook_viewer;
  gchar *tmpstr;
  const gchar *entrystr;
  gint result, ivalue;

  dialog = gtk_dialog_new_with_buttons (
      _ ("Preference"), NULL,
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, _ ("Accept"),
      GTK_RESPONSE_ACCEPT, _ ("Reject"), GTK_RESPONSE_REJECT, NULL);

  frame = gtk_frame_new ("");
  gtk_box_pack_start (
      GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), frame, TRUE,
      TRUE, 20);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, TRUE);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  /* i_exts */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("Image Type"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  i_exts = gtk_entry_new ();
  tmpstr = g_strjoinv (",", g_ini->image_suffix);
  gtk_entry_set_text (GTK_ENTRY (i_exts), tmpstr);
  g_free (tmpstr);
  gtk_box_pack_end (GTK_BOX (hbox), i_exts, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  /* v_exts */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("Video Type"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  v_exts = gtk_entry_new ();
  tmpstr = g_strjoinv (",", g_ini->video_suffix);
  gtk_entry_set_text (GTK_ENTRY (v_exts), tmpstr);
  g_free (tmpstr);
  gtk_box_pack_end (GTK_BOX (hbox), v_exts, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  /* a_exts */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("Audio Type"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  a_exts = gtk_entry_new ();
  tmpstr = g_strjoinv (",", g_ini->audio_suffix);
  gtk_entry_set_text (GTK_ENTRY (a_exts), tmpstr);
  g_free (tmpstr);
  gtk_box_pack_end (GTK_BOX (hbox), a_exts, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  /* i_level */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("Image Same Rate"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  i_level = gtk_spin_button_new_with_range (1, 10, 1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (i_level), TRUE);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (i_level),
                             10 - g_ini->same_image_distance);
  gtk_box_pack_end (GTK_BOX (hbox), i_level, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  /* v_level */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("Video Same Rate"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  v_level = gtk_spin_button_new_with_range (1, 10, 1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (v_level), TRUE);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (v_level),
                             10 - g_ini->same_video_distance);
  gtk_box_pack_end (GTK_BOX (hbox), v_level, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  /* a_level */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("Audio Same Rate"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  a_level = gtk_spin_button_new_with_range (1, 10, 1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (a_level), TRUE);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (a_level),
                             10 - g_ini->same_audio_distance);
  gtk_box_pack_end (GTK_BOX (hbox), a_level, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  /* threads_count */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("Threads Count"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  thd_count = gtk_spin_button_new_with_range (1, 32, 1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (thd_count), TRUE);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (thd_count),
                             g_ini->threads_count);
  gtk_box_pack_end (GTK_BOX (hbox), thd_count, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  /* ebook viewer */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  label = gtk_label_new (_ ("EBook Viewer"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
  ebook_viewer = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (ebook_viewer), g_ini->ebook_viewer);
  gtk_box_pack_end (GTK_BOX (hbox), ebook_viewer, TRUE, TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));

  result = gtk_dialog_run (GTK_DIALOG (dialog));
  if (result != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (dialog);
      return;
    }

  entrystr = gtk_entry_get_text (GTK_ENTRY (i_exts));
  if (entrystr != NULL)
    {
      g_strfreev (g_ini->image_suffix);
      g_ini->image_suffix = g_strsplit (entrystr, ",", 0);
    }

  entrystr = gtk_entry_get_text (GTK_ENTRY (v_exts));
  if (entrystr != NULL)
    {
      g_strfreev (g_ini->video_suffix);
      g_ini->video_suffix = g_strsplit (entrystr, ",", 0);
    }

  entrystr = gtk_entry_get_text (GTK_ENTRY (a_exts));
  if (entrystr != NULL)
    {
      g_strfreev (g_ini->audio_suffix);
      g_ini->audio_suffix = g_strsplit (entrystr, ",", 0);
    }

  ivalue = (int)gtk_spin_button_get_value (GTK_SPIN_BUTTON (i_level));
  g_ini->same_image_distance = 10 - ivalue;
  ivalue = (int)gtk_spin_button_get_value (GTK_SPIN_BUTTON (v_level));
  g_ini->same_video_distance = 10 - ivalue;
  ivalue = (int)gtk_spin_button_get_value (GTK_SPIN_BUTTON (a_level));
  g_ini->same_audio_distance = 10 - ivalue;

  ivalue = (int)gtk_spin_button_get_value (GTK_SPIN_BUTTON (thd_count));
  g_ini->threads_count = ivalue;

  entrystr = gtk_entry_get_text (GTK_ENTRY (ebook_viewer));
  if (entrystr != NULL)
    {
      g_free (g_ini->ebook_viewer);
      g_ini->ebook_viewer = g_strdup (entrystr);
    }

  ini_save (g_ini, FD_USR_CONF_FILE);

  gtk_widget_destroy (dialog);
}

static void
gui_cleanup_cb (GtkWidget *wid, gui_t *gui)
{
  cache_cleanup (g_cache);
}

static void
gui_help_cb (GtkWidget *wid, gui_t *gui)
{
  const gchar *uri
      = "https://github.com/naihe2010/fdupves/blob/master/README.md";

#ifndef WIN32
  gtk_show_uri_on_window (NULL, uri, GDK_CURRENT_TIME, NULL);
#else
  ShellExecute (NULL, "open", uri, NULL, NULL, SW_SHOW);
#endif
}

static gboolean
dir_find_item (GtkTreeModel *model, GtkTreePath *tpath, GtkTreeIter *itr,
               gui_t *gui)
{
  gchar *path;

  gtk_tree_model_get (model, itr, 0, &path, -1);
  if (path)
    {
      if (g_file_test (path, G_FILE_TEST_IS_DIR))
        {
          gui_list_dir (gui, path);
        }
      else if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        {
          gui_list_file (gui, path);
        }
      else if (g_file_test (path, G_FILE_TEST_IS_SYMLINK))
        {
          gui_list_link (gui, path);
        }

      g_free (path);
    }

  return FALSE;
}

static void
gui_list_dir (gui_t *gui, const gchar *path)
{
  GQueue stack[1];
  GDir *gdir;
  GError *err;
  gchar *dir, *curpath;
  const gchar *cur;

  g_queue_init (stack);
  dir = g_strdup (path);
  g_queue_push_tail (stack, dir);

  while ((dir = g_queue_pop_tail (stack)) != NULL)
    {
      err = NULL;
      gdir = g_dir_open (dir, 0, &err);
      if (err)
        {
          g_warning ("Can't open dir: %s: %s", dir, err->message);
          g_error_free (err);
        }

      while ((cur = g_dir_read_name (gdir)) != NULL)
        {
          if (strcmp (cur, ".") == 0 || strcmp (cur, "..") == 0)
            {
              continue;
            }

          curpath = g_build_filename (dir, cur, NULL);
          if (g_file_test (curpath, G_FILE_TEST_IS_DIR))
            {
              g_queue_push_tail (stack, curpath);
            }
          else if (g_file_test (curpath, G_FILE_TEST_IS_REGULAR))
            {
              gui_list_file (gui, curpath);
              g_free (curpath);
            }
          else if (g_file_test (curpath, G_FILE_TEST_IS_SYMLINK))
            {
              gui_list_link (gui, curpath);
              g_free (curpath);
            }
        }

      g_dir_close (gdir);
      g_free (dir);
    }
}

static void
gui_list_file (gui_t *gui, const gchar *path)
{
  gchar *p;
  int found;

  found = 0;
  if (g_ini->proc_image && is_image (path))
    {
      p = g_strdup (path);
      g_ptr_array_add (gui->images, p);
      found = 1;
    }

  if (g_ini->proc_video && is_video (path))
    {
      p = g_strdup (path);
      g_ptr_array_add (gui->videos, p);
      found = 1;
    }

  if (g_ini->proc_audio && is_audio (path))
    {
      p = g_strdup (path);
      g_ptr_array_add (gui->audios, p);
      found = 1;
    }

  if (g_ini->proc_ebook && is_ebook (path))
    {
      p = g_strdup (path);
      g_ptr_array_add (gui->ebooks, p);
      found = 1;
    }

  if (g_ini->compare_area == 5 && is_video (path))
    {
      p = g_strdup (path);
      g_ptr_array_add (gui->audios, p);
      found = 1;
    }

  if (found == 0)
    {
      g_debug ("%s is not image/video/audio/e-book, skipped", path);
      if (g_ini->proc_other)
        {
        }
    }
}

static void
gui_list_link (gui_t *gui, const gchar *path)
{
}

static gboolean
gui_queue_timer_callback (gui_t *gui)
{
  find_step *step;
  gchar *log;

  while (!gui->quit)
    {
      log = g_async_queue_try_pop (gui->log_queue);
      if (log != NULL)
        {
          gui_process_log (gui, log);
          g_free (log);
        }

      step = g_async_queue_try_pop (gui->step_queue);
      if (step != NULL)
        {
          gui_process_step (gui, step);
          g_free (step);
        }

      if (log == NULL && step == NULL)
        {
          break;
        }
    }

  return !gui->quit;
}

static void
gui_destroy_cb (GtkWidget *but, GdkEvent *ev, gui_t *gui)
{
  GtkWidget *dia;
  gint ret;

  dia = gtk_message_dialog_new (GTK_WINDOW (gui->widget),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO,
                                _ ("Are you sure you want to quit fdupves?"));

  ret = gtk_dialog_run (GTK_DIALOG (dia));
  gtk_widget_destroy (dia);

  if (ret == GTK_RESPONSE_YES)
    {
      gui_destroy (gui);
    }
}

static void
gui_destroy (gui_t *gui)
{
  gui->quit = TRUE;

  gui_save_directories (gui);
  ini_save (g_ini, FD_USR_CONF_FILE);

  gtk_widget_destroy (gui->widget);
  gtk_main_quit ();
}

static void
dirlist_onactivated (GtkTreeView *tree, GtkTreePath *path,
                     GtkTreeViewColumn *column, gui_t *gui)
{
  GtkTreeIter itr[1];

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (gui->dir_store), itr, path))
    {
      gtk_list_store_remove (gui->dir_store, itr);
    }
}

static void
result_onactivated (GtkTreeView *tree, GtkTreePath *path,
                    GtkTreeViewColumn *column, gui_t *gui)
{
  result_open (NULL, gui);
}

static gboolean
result_onbutpress (GtkWidget *wid, GdkEventButton *event, gui_t *gui)
{
  GtkWidget *menu, *item;
  int selcnt;

  if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
    {
      return FALSE;
    }

  selcnt = gtk_tree_selection_count_selected_rows (gui->result_select);
  if (selcnt == 0)
    {
      return FALSE;
    }

  menu = gtk_menu_new ();
  switch (selcnt)
    {
    case 1:
      item = result_open_menuitem (gui);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      item = result_opendir_menuitem (gui);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      item = result_delete_menuitem (gui);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      break;

    case 2:
      item = result_diff_menuitem (gui);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      item = result_delete_menuitem (gui);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      break;

    default: /* >= 3 */
      item = result_delete_menuitem (gui);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      break;
    }

  // TODO
  // gtk_menu_set_title (GTK_MENU (menu), _ ("process file"));
  gtk_widget_show_all (menu);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), (const GdkEvent *)event);

  return TRUE;
}

static GtkWidget *
result_open_menuitem (gui_t *gui)
{
  GtkWidget *item;

  item = gtk_menu_item_new_with_label (_ ("open"));
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (result_open),
                    gui);

  return item;
}

static GtkWidget *
result_opendir_menuitem (gui_t *gui)
{
  GtkWidget *item;

  item = gtk_menu_item_new_with_label (_ ("open dir"));
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (result_opendir),
                    gui);

  return item;
}

static GtkWidget *
result_delete_menuitem (gui_t *gui)
{
  GtkWidget *item;

  item = gtk_menu_item_new_with_label (_ ("delete"));
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (result_delete),
                    gui);

  return item;
}

static GtkWidget *
result_diff_menuitem (gui_t *gui)
{
  GtkWidget *item;

  item = gtk_menu_item_new_with_label (_ ("diff"));
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (result_diff),
                    gui);

  return item;
}

static void
resultsel_onchanged (GtkTreeSelection *sel, gui_t *gui)
{
  GList *list, *cur;
  gsize i, cnt;
  GtkTreeIter *itr;

  if (gui->result_file_nodes)
    {
      g_free (gui->result_file_nodes);
      gui->result_file_nodes = NULL;
    }
  if (gui->result_select_iters)
    {
      g_free (gui->result_select_iters);
      gui->result_select_iters = NULL;
    }

  list = gtk_tree_selection_get_selected_rows (sel, NULL);
  cnt = g_list_length (list);
  if (cnt < 1)
    {
      return;
    }

  gui->result_file_nodes = g_new0 (file_node *, cnt);
  gui->result_select_iters = g_new0 (GtkTreeIter, cnt);

  for (i = 0, cur = list; cur; ++i, cur = g_list_next (cur))
    {
      itr = gui->result_select_iters + i;
      gtk_tree_model_get_iter (GTK_TREE_MODEL (gui->result_store), itr,
                               cur->data);
      gtk_tree_model_get (GTK_TREE_MODEL (gui->result_store), itr, 5,
                          gui->result_file_nodes + i, -1);
      gtk_tree_path_free (cur->data);
    }
  g_list_free (list);
}

static void
result_open (GtkMenuItem *item, gui_t *gui)
{
#ifndef WIN32
  gchar *uri;
  GError *err;

  uri = g_filename_to_uri (gui->result_file_nodes[0]->path, NULL, NULL);
  err = NULL;
  gtk_show_uri_on_window (NULL, uri, GDK_CURRENT_TIME, &err);
  if (err)
    {
      g_debug ("Open uri: %s error: %s", uri, err->message);
      g_error_free (err);
    }
  g_free (uri);

#else
  gchar *filename;

  filename
      = g_win32_locale_filename_from_utf8 (gui->result_file_nodes[0]->path);
  if (filename)
    {
      ShellExecute (NULL, "open", filename, NULL, NULL, SW_SHOW);
      g_free (filename);
    }
  else
    {
      ShellExecute (NULL, "open", gui->result_file_nodes[0]->path, NULL, NULL,
                    SW_SHOW);
    }
#endif
}

static void
result_opendir (GtkMenuItem *item, gui_t *gui)
{
  gchar *dir;
#ifndef WIN32
  gchar *uri;
  GError *err;
#else
  gchar *dirname;
#endif

  dir = g_path_get_dirname (gui->result_file_nodes[0]->path);
  if (dir == NULL)
    {
      g_warning ("get file: %s dirname failed",
                 gui->result_file_nodes[0]->path);
      return;
    }

#ifndef WIN32
  uri = g_filename_to_uri (dir, NULL, NULL);

  err = NULL;
  gtk_show_uri_on_window (NULL, uri, GDK_CURRENT_TIME, &err);
  if (err)
    {
      g_debug ("Open uri: %s error: %s", uri, err->message);
      g_error_free (err);
    }
  g_free (uri);

#else

  dirname = g_win32_locale_filename_from_utf8 (dir);
  if (dirname)
    {
      ShellExecute (NULL, "open", dirname, NULL, NULL, SW_SHOW);
      g_free (dirname);
    }
  else
    {
      ShellExecute (NULL, "open", dir, NULL, NULL, SW_SHOW);
    }
#endif

  g_free (dir);
}

static void
gui_tree_store_get_iter (GtkTreeStore *store, GtkTreeIter *itrc,
                         GtkTreeIter *itr, int index)
{
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), itrc, itr,
                                      index))
    {
      gtk_tree_store_append (gui->result_store, itrc, itr);
    }
}

static void
gui_tree_store_iter_set_count (GtkTreeStore *store, GtkTreeIter *itr,
                               int count)
{
  GtkTreeIter itrc[1];
  int index_cnt;

  index_cnt = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), itr);
  while (index_cnt > count)
    {
      gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), itrc, itr,
                                     index_cnt - 1);
      gtk_tree_store_remove (store, itrc);
      index_cnt = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), itr);
    }
}

static gboolean
same_node_matches_filter (same_node *node, const gchar *filter)
{
  GSList *files;

  for (files = node->files; files; files = files->next)
    {
      file_node *fn = files->data;
      if (strstr (fn->path, filter))
        {
          return TRUE;
        }
    }
  return FALSE;
}

static void
same_node_to_tree_rows (GtkTreeStore *store, same_node *node,
                        GtkTreeIter *itr)
{
  GtkTreeIter child[1];
  GSList *files;
  gint index;

  file_node_to_tree_iter (node->files->data, store, itr);
  for (index = 0, files = node->files->next; files;
       files = files->next, ++index)
    {
      gui_tree_store_get_iter (store, child, itr, index);
      file_node_to_tree_iter (files->data, store, child);
    }
  gui_tree_store_iter_set_count (store, itr, index);
}

static void
gui_filter_result (gui_t *gui, const gchar *filter)
{
  GtkTreeIter itr[1];
  gint index;
  GtkTreePath *path;
  GSList *nodelist;
  same_node *node;

  for (index = 0, nodelist = gui->same_list; nodelist != NULL;
       nodelist = g_slist_next (nodelist))
    {
      node = (same_node *)nodelist->data;

      if (node->treerowref)
        {
          gtk_tree_row_reference_free (node->treerowref);
          node->treerowref = NULL;
        }
      node->show = FALSE;

      if (node->files == NULL)
        {
          continue;
        }

      if (same_node_matches_filter (node, filter))
        {
          gui_tree_store_get_iter (gui->result_store, itr, NULL, index++);
          same_node_to_tree_rows (gui->result_store, node, itr);

          path = gtk_tree_model_get_path (GTK_TREE_MODEL (gui->result_store),
                                          itr);
          node->treerowref = gtk_tree_row_reference_new (
              GTK_TREE_MODEL (gui->result_store), path);
          gtk_tree_path_free (path);

          node->show = TRUE;
        }
    }

  gui_tree_store_iter_set_count (gui->result_store, NULL, index);
  resultsel_onchanged (gui->result_select, gui);
}

#ifdef WIN32
static int
win32_remove (gui_t *gui, const gchar *filename, gboolean totrash)
{
  int ret;
  gunichar2 *destfile;
  glong length, i;
  SHFILEOPSTRUCTW FileOp;

  destfile = g_utf8_to_utf16 (filename, -1, NULL, &length, NULL);
  if (!destfile)
    {
      return ERROR_NO_UNICODE_TRANSLATION;
    }

  destfile = g_renew (gunichar2, destfile, length + 2);
  destfile[length + 1] = 0;
  for (i = 0; i < length; ++i)
    {
      if (destfile[i] == '/')
        {
          destfile[i] = '\\';
        }
    }

  memset (&FileOp, 0, sizeof FileOp);
  FileOp.hwnd = HWND_DESKTOP;
  FileOp.wFunc = FO_DELETE;
  FileOp.fFlags = FOF_NOCONFIRMATION;
  FileOp.pFrom = (LPCWSTR)destfile;
  FileOp.lpszProgressTitle = L"Delete file";

  if (totrash)
    {
      FileOp.fFlags |= FOF_ALLOWUNDO;
    }

  ret = SHFileOperationW (&FileOp);
  g_free (destfile);
  if (ret == 0 && FileOp.fAnyOperationsAborted)
    {
      ret = ERROR_CANCELLED;
    }

  return ret;
}
#endif

#define FDUPVES_DEL_ALL 1
#define FDUPVES_DEL_TOTRASH (1 << 1)

static void
gui_delete_all_cb (GtkToggleButton *but, gint *pflags)
{
  g_assert (pflags);

  if (gtk_toggle_button_get_active (but))
    {
      *pflags |= FDUPVES_DEL_ALL;
    }
  else
    {
      *pflags &= (~FDUPVES_DEL_ALL);
    }
}

#ifdef WIN32
static void
gui_delete_totrash_cb (GtkToggleButton *but, gint *pflags)
{
  g_assert (pflags);

  if (gtk_toggle_button_get_active (but))
    {
      *pflags |= FDUPVES_DEL_TOTRASH;
    }
  else
    {
      *pflags &= (~FDUPVES_DEL_TOTRASH);
    }
}
#endif

static gint
gui_delete_dialog_ask (gui_t *gui, const gchar *filename, gint *pflags)
{
  GtkWidget *dia, *all;
  gint res;
#ifdef WIN32
  GtkWidget *totrash;
#endif

  dia = gtk_message_dialog_new (
      GTK_WINDOW (gui->widget), GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO,
      _ ("Are you sure you want to delete %s?"), filename);
  all = gtk_check_button_new_with_label (
      _ ("Use this action for all the other files"));
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dia))),
                      all, TRUE, TRUE, 2);
  g_signal_connect (G_OBJECT (all), "toggled", G_CALLBACK (gui_delete_all_cb),
                    pflags);
#ifdef WIN32
  totrash = gtk_check_button_new_with_label (
      _ ("move file to trash, but not really delete it"));
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dia))),
                      totrash, TRUE, TRUE, 2);
  g_signal_connect (G_OBJECT (totrash), "toggled",
                    G_CALLBACK (gui_delete_totrash_cb), pflags);
#endif
  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dia)));

  res = gtk_dialog_run (GTK_DIALOG (dia));
  gtk_widget_destroy (dia);

  return res;
}

static void
result_refresh (gui_t *gui, GSList *nodes)
{
  GSList *cur;
  same_node *node;
  GtkTreePath *path;
  GtkTreeIter itr[1];
  const gchar *filter;

  filter = gtk_entry_get_text (GTK_ENTRY (gui->result_filter));
  for (cur = nodes; cur; cur = cur->next)
    {
      node = cur->data;
      if (!node->show)
        {
          continue;
        }

      path = gtk_tree_row_reference_get_path (node->treerowref);
      if (path == NULL)
        {
          gtk_tree_row_reference_free (node->treerowref);
          node->treerowref = NULL;
          node->show = FALSE;
          continue;
        }
      gtk_tree_model_get_iter (GTK_TREE_MODEL (gui->result_store), itr, path);
      gtk_tree_path_free (path);

      if (node->files == NULL || !same_node_matches_filter (node, filter))
        {
          gtk_tree_store_remove (gui->result_store, itr);
          gtk_tree_row_reference_free (node->treerowref);
          node->treerowref = NULL;
          node->show = FALSE;
          continue;
        }

      same_node_to_tree_rows (gui->result_store, node, itr);
    }
}

static void
result_delete (GtkMenuItem *item, gui_t *gui)
{
  gint last, i, res, ret, flags;
  file_node **files;
  file_node *fn;
  GSList *deleted = NULL, *nodes = NULL, *cur, *n;
  GList *list, *row;
  GtkTreeIter itr[1];
  GtkWidget *dia;
  same_node *node;
  gboolean dup;

  list = gtk_tree_selection_get_selected_rows (gui->result_select, NULL);
  last = g_list_length (list);
  if (last == 0)
    {
      return;
    }

  files = g_new (file_node *, last);
  for (i = 0, row = list; row; ++i, row = row->next)
    {
      gtk_tree_model_get_iter (GTK_TREE_MODEL (gui->result_store), itr,
                               row->data);
      gtk_tree_model_get (GTK_TREE_MODEL (gui->result_store), itr, 5,
                          files + i, -1);
      gtk_tree_path_free (row->data);
    }
  g_list_free (list);

  res = 0;
  flags = 0;
  for (i = last - 1; i >= 0; --i)
    {
      if (res == 0)
        {
          flags = 0;
          res = gui_delete_dialog_ask (gui, files[i]->path, &flags);
        }

      if (res != GTK_RESPONSE_YES && res != GTK_RESPONSE_NO)
        {
          break;
        }

      if (res == GTK_RESPONSE_YES)
        {
          dup = FALSE;
          for (cur = deleted; cur; cur = cur->next)
            {
              fn = cur->data;
              if (strcmp (fn->path, files[i]->path) == 0)
                {
                  dup = TRUE;
                  break;
                }
            }

          if (dup)
            {
              ret = 0;
            }
          else
            {
#ifdef WIN32
              ret = win32_remove (gui, files[i]->path,
                                  (flags & FDUPVES_DEL_TOTRASH) != 0);
#else
              ret = g_remove (files[i]->path);
              if (ret != 0)
                {
                  ret = errno;
                }
#endif
            }

          if (ret == 0)
            {
              deleted = g_slist_prepend (deleted, files[i]);
            }
          else
            {
              dia = gtk_message_dialog_new (
                  GTK_WINDOW (gui->widget), GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                  _ ("Could not delete %s"), files[i]->path);
#ifdef WIN32
              gtk_message_dialog_format_secondary_text (
                  GTK_MESSAGE_DIALOG (dia), _ ("Error code: %d"), ret);
#else
              gtk_message_dialog_format_secondary_text (
                  GTK_MESSAGE_DIALOG (dia), "%s", g_strerror (ret));
#endif
              gtk_dialog_run (GTK_DIALOG (dia));
              gtk_widget_destroy (dia);
            }
        }

      if (!(flags & FDUPVES_DEL_ALL))
        {
          res = 0;
        }
    }

  g_free (files);
  if (deleted == NULL)
    {
      return;
    }

  gtk_tree_selection_unselect_all (gui->result_select);
  for (cur = gui->same_list; cur; cur = cur->next)
    {
      node = cur->data;
      for (n = node->files; n; n = n->next)
        {
          fn = n->data;
          fn->selected = FALSE;
        }
    }

  for (cur = deleted; cur; cur = cur->next)
    {
      fn = cur->data;
      if (!g_slist_find (nodes, fn->node))
        {
          nodes = g_slist_prepend (nodes, fn->node);
        }
    }

  for (cur = deleted; cur; cur = cur->next)
    {
      for (n = nodes; n; n = n->next)
        {
          node = n->data;
          if (g_slist_find (node->files, cur->data))
            {
              file_node_free_full (cur->data);
              break;
            }
        }
    }

  result_refresh (gui, nodes);
  g_slist_free (nodes);
  g_slist_free (deleted);
}

static GtkWidget *
image2widget (const file_node *fn)
{
  gchar *desc;
  GtkWidget *vbox, *label, *image;
  GdkPixbuf *pixbuf;
  GError *err;

  desc
      = g_strdup_printf ("Name: %s\n"
                         "Dir: %s\n"
                         "size: %d:%d\n"
                         "format: %s",
                         fn->name, fn->dir, fn->width, fn->height, fn->format);

  label = gtk_label_new (desc);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
  g_free (desc);

  err = NULL;
  pixbuf = fdupves_gdkpixbuf_load_file_at_size (fn->path, g_ini->thumb_size[0],
                                                g_ini->thumb_size[1], &err);
  if (err)
    {
      g_warning ("load image: %s error: %s", fn->path, err->message);
      g_error_free (err);
      g_free (desc);
      return NULL;
    }
  image = gtk_image_new_from_pixbuf (pixbuf);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 2);
  gtk_box_pack_end (GTK_BOX (vbox), image, TRUE, FALSE, 2);

  return vbox;
}

static void
diff_add_image (diff_dialog *dia, const file_node *afn, const file_node *bfn)
{
  GtkWidget *aimage, *bimage;

  dia->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start (GTK_BOX (dia->content), dia->container, TRUE, TRUE, 0);

  aimage = image2widget (afn);
  gtk_box_pack_start (GTK_BOX (dia->container), aimage, TRUE, TRUE, 0);

  bimage = image2widget (bfn);
  gtk_box_pack_end (GTK_BOX (dia->container), bimage, TRUE, TRUE, 0);
}

static GtkWidget *
video2widget (const file_node *fn, int seek)
{
  gchar *desc, *tmpfile, tmpname[0x10];
  GtkWidget *label, *image, *vbox;

  desc = g_strdup_printf ("Name: %s\n"
                          "Dir: %s\n"
                          "Format %s\n"
                          "Length: %f\n"
                          "Size: %d-%d",
                          fn->name, fn->dir, fn->format, fn->length, fn->width,
                          fn->height);
  label = gtk_label_new (desc);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
  g_free (desc);

  g_snprintf (tmpname, sizeof tmpname, "%d.jpg", rand ());
  tmpfile = g_build_filename (g_get_tmp_dir (), tmpname, NULL);
  video_time_screenshot_file (fn->path, seek, g_ini->thumb_size[0],
                              g_ini->thumb_size[1], tmpfile);
  image = gtk_image_new_from_file (tmpfile);
  g_free (tmpfile);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 2);
  gtk_box_pack_end (GTK_BOX (vbox), image, TRUE, FALSE, 2);

  return vbox;
}

static void
diffdia_refresh_video_pic (diff_dialog *dia)
{
  GList *list, *cur;
  GtkWidget *avideo, *bvideo;
  int seek, rate;

  list = gtk_container_get_children (GTK_CONTAINER (dia->container));
  for (cur = list; cur; cur = g_list_next (cur))
    {
      gtk_container_remove (GTK_CONTAINER (dia->container), cur->data);
    }

  rate = (int)((dia->afn->length < dia->bfn->length ? dia->afn->length
                                                    : dia->bfn->length)
               / (g_ini->compare_count + 1));

  seek = dia->from_tail ? (int)(dia->afn->length - rate * dia->index)
                        : rate * dia->index;
  avideo = video2widget (dia->afn, seek);
  gtk_box_pack_start (GTK_BOX (dia->container), avideo, TRUE, TRUE, 0);

  seek = dia->from_tail ? (int)(dia->bfn->length - rate * dia->index)
                        : rate * dia->index;
  bvideo = video2widget (dia->bfn, seek);
  gtk_box_pack_end (GTK_BOX (dia->container), bvideo, TRUE, TRUE, 0);

  if (dia->index <= 1)
    {
      gtk_widget_set_sensitive (dia->butprev, FALSE);
      gtk_widget_set_sensitive (dia->butnext, TRUE);
    }
  else if (dia->index >= g_ini->compare_count)
    {
      gtk_widget_set_sensitive (dia->butprev, TRUE);
      gtk_widget_set_sensitive (dia->butnext, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (dia->butprev, TRUE);
      gtk_widget_set_sensitive (dia->butnext, TRUE);
    }

  gtk_widget_show_all (dia->content);
}

static void
diff_add_video (diff_dialog *dia, const file_node *afn, const file_node *bfn)
{
  GtkWidget *hbox, *hbox2;
  GtkWidget *entry, *label, *headortail;
  gchar count[10];

  dia->afn = afn;
  dia->bfn = bfn;

  /* head or tail */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start (GTK_BOX (dia->content), hbox, FALSE, FALSE, 5);

  /* screenshot */
  dia->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_end (GTK_BOX (dia->content), dia->container, TRUE, TRUE, 0);

  headortail = gtk_toggle_button_new_with_label (_ ("From Tail"));
  gtk_box_pack_start (GTK_BOX (hbox), headortail, FALSE, FALSE, 2);
  g_signal_connect (G_OBJECT (headortail), "toggled",
                    G_CALLBACK (diffdia_onheadtail), dia);

  hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 2);
  label = gtk_label_new (_ ("Compare Count"));
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 2);
  entry = gtk_entry_new ();
  gtk_box_pack_end (GTK_BOX (hbox2), entry, FALSE, FALSE, 2);
  g_signal_connect (G_OBJECT (entry), "changed",
                    G_CALLBACK (diffdia_onseekchanged), dia);

  dia->butprev = gtk_button_new_with_label (_ ("Previous"));
  gtk_box_pack_start (GTK_BOX (hbox), dia->butprev, FALSE, FALSE, 2);
  g_signal_connect (G_OBJECT (dia->butprev), "clicked",
                    G_CALLBACK (diffdia_onprev), dia);
  dia->butnext = gtk_button_new_with_label (_ ("Next"));
  gtk_box_pack_start (GTK_BOX (hbox), dia->butnext, FALSE, FALSE, 2);
  g_signal_connect (G_OBJECT (dia->butnext), "clicked",
                    G_CALLBACK (diffdia_onnext), dia);

  g_snprintf (count, sizeof count, "%d", g_ini->compare_count);
  gtk_entry_set_text (GTK_ENTRY (entry), count);
  dia->index = 1;
  diffdia_refresh_video_pic (dia);
}

static void
result_diff (GtkMenuItem *item, gui_t *gui)
{
  diff_dialog_new (gui, gui->result_file_nodes[0], gui->result_file_nodes[1]);
}

static void
diff_dialog_new (gui_t *gui, const file_node *afn, const file_node *bfn)
{
  diff_dialog *diffdia;

  if (afn->type == FD_EBOOK && bfn->type == FD_EBOOK)
    {
      gchar *cmd1 = g_strdup_printf ("apvlv \"%s\"", afn->path);
      gchar *cmd2 = g_strdup_printf ("apvlv \"%s\"", bfn->path);
      g_spawn_command_line_async (cmd1, NULL);
      g_spawn_command_line_async (cmd2, NULL);
      g_free (cmd1);
      g_free (cmd2);
      return;
    }

  diffdia = g_malloc0 (sizeof (diff_dialog));

  diffdia->gui = gui;

  diffdia->dialog = gtk_dialog_new_with_buttons (
      "fdupves diff dialog", GTK_WINDOW (gui->widget),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, _ ("Close"),
      GTK_RESPONSE_CLOSE, NULL);

  diffdia->content
      = gtk_dialog_get_content_area (GTK_DIALOG (diffdia->dialog));

  g_signal_connect (G_OBJECT (diffdia->dialog), "response",
                    G_CALLBACK (diffdia_onresponse), diffdia);
  g_signal_connect (G_OBJECT (diffdia->dialog), "close",
                    G_CALLBACK (diffdia_onclose), diffdia);

  if (afn->type == FD_IMAGE && bfn->type == FD_IMAGE)
    {
      diff_add_image (diffdia, afn, bfn);
    }
  else if (afn->type == FD_VIDEO && bfn->type == FD_VIDEO)
    {
      diff_add_video (diffdia, afn, bfn);
    }
  else if (afn->type == FD_AUDIO && bfn->type == FD_AUDIO)
    {
      diff_add_video (diffdia, afn, bfn);
    }
  else
    {
      gchar *desc;
      GtkWidget *label;

      desc = g_strdup_printf ("file type are not same, can't show diff\n"
                              "file 1: %s\n"
                              "file 2: %s\n",
                              afn->path, bfn->path);
      label = gtk_label_new (desc);
      g_free (desc);
      gtk_box_pack_start (GTK_BOX (diffdia->content), label, TRUE, FALSE, 2);
    }
  gtk_widget_show_all (diffdia->content);

  gtk_dialog_run (GTK_DIALOG (diffdia->dialog));
}

static void
diffdia_onnext (GtkWidget *but, diff_dialog *dialog)
{
  dialog->index += 1;
  diffdia_refresh_video_pic (dialog);
}

static void
diffdia_onprev (GtkWidget *but, diff_dialog *dialog)
{
  dialog->index -= 1;
  diffdia_refresh_video_pic (dialog);
}

static void
diffdia_onclose (GtkWidget *dia, diff_dialog *dialog)
{
  gtk_widget_destroy (dialog->dialog);
  g_free (dialog);
}

static void
diffdia_onresponse (GtkWidget *dia, gint res, diff_dialog *dialog)
{
  gtk_widget_destroy (dialog->dialog);
  g_free (dialog);
}

static void
diffdia_onheadtail (GtkToggleButton *but, diff_dialog *dia)
{
  dia->from_tail = gtk_toggle_button_get_active (but);
  diffdia_refresh_video_pic (dia);
}

static void
diffdia_onseekchanged (GtkEntry *entry, diff_dialog *dia)
{
  const gchar *text;
  int cnt;

  text = gtk_entry_get_text (entry);
  cnt = atoi (text);
  if (cnt < 1 || cnt == g_ini->compare_count)
    {
      return;
    }

  g_ini->compare_count = cnt;

  dia->index = 1;
  diffdia_refresh_video_pic (dia);
}

static void
gui_find_step_cb (const find_step *step, gui_t *gui)
{
  find_step *step2;
  step2 = g_new0 (find_step, 1);
  memcpy (step2, step, sizeof (find_step));
  g_async_queue_push (gui->step_queue, step2);
}

static void
gui_process_step (gui_t *gui, const find_step *step)
{
  if (step->doing)
    {
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (gui->progress),
                                 step->doing);
    }

  if (step->total > 0 && step->now < step->total)
    {
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (gui->progress),
                                     (gdouble)step->now
                                         / (gdouble)step->total);
    }

  if (step->found)
    {
      gui->same_list = gui_append_same_slist (gui, gui->same_list, step->afile,
                                              step->bfile, step->type);
    }
}

static GSList *
gui_append_same_slist (gui_t *gui, GSList *slist, const gchar *afile,
                       const gchar *bfile, same_type type)
{
  GSList *cur, *fslist;
  same_node *node;
  file_node *fn, *fn2;
  gboolean afind, bfind;
  GtkTreeIter itr[1], itrc[1];
  GtkTreePath *path;
  int filetype;

  filetype = FD_IMAGE;
  if (type == FD_SAME_VIDEO_HEAD || type == FD_SAME_VIDEO_TAIL)
    {
      filetype = FD_VIDEO;
    }
  else if (type == FD_SAME_AUDIO_HEAD || type == FD_SAME_AUDIO_TAIL)
    {
      filetype = FD_AUDIO;
    }
  else if (type == FD_SAME_EBOOK)
    {
      filetype = FD_EBOOK;
    }

  for (cur = slist; cur; cur = g_slist_next (cur))
    {
      node = cur->data;

      if (node->type != filetype)
        {
          continue;
        }

      afind = bfind = FALSE;
      for (fslist = node->files; fslist; fslist = g_slist_next (fslist))
        {
          fn = fslist->data;

          if (strcmp (fn->path, afile) == 0)
            {
              afind = TRUE;
            }
          else if (strcmp (fn->path, bfile) == 0)
            {
              bfind = TRUE;
            }
        }

      if (afind && bfind)
        {
          return slist;
        }
      else if (afind)
        {
          fn = file_node_new (node, bfile, filetype);

          if (node->show)
            {
              path = gtk_tree_row_reference_get_path (node->treerowref);
              gtk_tree_model_get_iter (GTK_TREE_MODEL (gui->result_store), itr,
                                       path);
              gtk_tree_store_append (gui->result_store, itrc, itr);
              file_node_to_tree_iter (fn, gui->result_store, itrc);
              gtk_tree_path_free (path);
            }

          return slist;
        }
      else if (bfind)
        {
          fn = file_node_new (node, afile, filetype);

          if (node->show)
            {
              path = gtk_tree_row_reference_get_path (node->treerowref);
              gtk_tree_model_get_iter (GTK_TREE_MODEL (gui->result_store), itr,
                                       path);
              gtk_tree_store_append (gui->result_store, itrc, itr);
              file_node_to_tree_iter (fn, gui->result_store, itrc);
              gtk_tree_path_free (path);
            }

          return slist;
        }
    }

  node = g_malloc0 (sizeof (same_node));
  g_return_val_if_fail (node, slist);

  node->type = filetype;
  fn = file_node_new (node, afile, filetype);
  fn2 = file_node_new (node, bfile, filetype);

  gtk_tree_store_append (gui->result_store, itr, NULL);
  file_node_to_tree_iter (fn, gui->result_store, itr);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (gui->result_store), itr);
  node->treerowref
      = gtk_tree_row_reference_new (GTK_TREE_MODEL (gui->result_store), path);
  gtk_tree_path_free (path);
  gtk_tree_store_append (gui->result_store, itrc, itr);
  file_node_to_tree_iter (fn2, gui->result_store, itrc);
  node->show = TRUE;

  return g_slist_append (slist, node);
}

void
same_node_free (same_node *node)
{
  if (node->treerowref)
    {
      gtk_tree_row_reference_free (node->treerowref);
      node->treerowref = NULL;
    }
  g_slist_foreach (node->files, (GFunc)file_node_free, NULL);
  g_slist_free (node->files);
  g_free (node);
}

void
same_list_free (GSList *list)
{
  g_slist_foreach (list, (GFunc)same_node_free, NULL);
  g_slist_free (list);
}

static void
result_filter_changed (GtkEntry *entry, gui_t *gui)
{
  gui_filter_result (gui, gtk_entry_get_text (entry));
}

static void
result_selcombo_changed (GtkComboBox *comtext, gui_t *gui)
{
  gint id;

  id = gtk_combo_box_get_active (comtext);
  switch (id)
    {
    case 0:
      break;

    case 1:
      g_slist_foreach (gui->same_list, (GFunc)result_select_small_file, gui);
      break;
    case 2:
      g_slist_foreach (gui->same_list, (GFunc)result_select_big_file, gui);
      break;
    case 3:
      g_slist_foreach (gui->same_list, (GFunc)result_select_small_image, gui);
      break;
    case 4:
      g_slist_foreach (gui->same_list, (GFunc)result_select_big_image, gui);
      break;
    case 5:
      g_slist_foreach (gui->same_list, (GFunc)result_select_short_video, gui);
      break;
    case 6:
      g_slist_foreach (gui->same_list, (GFunc)result_select_long_video, gui);
      break;
    case 7:
      g_slist_foreach (gui->same_list, (GFunc)result_select_others, gui);
      break;

    default:
      break;
    }
}

static void
gui_delete_seleted_cb (GtkWidget *but, gui_t *gui)
{
  result_delete (NULL, gui);
}

static file_node *
file_node_new (same_node *node, const gchar *path, gint type)
{
  file_node *fn;
#ifdef WIN32
  GStatBuf buf[1];
#else
  struct stat buf[1];
#endif

  fn = g_malloc0 (sizeof (file_node));
  g_return_val_if_fail (fn, NULL);

  fn->path = g_strdup (path);
  fn->name = g_path_get_basename (path);
  fn->dir = g_path_get_dirname (path);

  if (g_stat (path, buf) == 0)
    {
      fn->size = buf->st_size;
    }

  fn->type = type;

  if (type == FD_IMAGE)
    {
      GdkPixbufFormat *format;

      format = gdk_pixbuf_get_file_info (path, &fn->width, &fn->height);
      if (format)
        {
          fn->format = gdk_pixbuf_format_get_name (format);
        }
    }
  else if (type == FD_VIDEO)
    {
      video_info *info;

      info = video_get_info (path);
      if (info)
        {
          fn->width = info->size[0];
          fn->height = info->size[1];
          fn->length = info->length;
          fn->format = g_strdup (info->format);
          video_info_free (info);
        }
    }
  else if (type == FD_AUDIO)
    {
      audio_info *info;

      info = audio_get_info (path);
      if (info)
        {
          fn->length = info->length;
          fn->format = g_strdup (info->format);
          audio_info_free (info);
        }
    }

  fn->node = node;
  node->files = g_slist_append (node->files, fn);

  return fn;
}

static void
file_node_free (file_node *fn)
{
  g_free (fn->path);
  g_free (fn->name);
  g_free (fn->dir);
  g_free (fn->format);
  g_free (fn);
}

static void
file_node_free_full (file_node *fn)
{
  same_node *node;

  node = fn->node;

  if (g_cache)
    {
      cache_remove (g_cache, fn->path);
    }

  node->files = g_slist_remove (node->files, fn);
  file_node_free (fn);

  if (node->files && !node->files->next)
    {
      file_node_free_full (node->files->data);
    }
}

static void
file_node_to_tree_iter (file_node *fn, GtkTreeStore *store, GtkTreeIter *itr)
{
  gchar isizestr[0x100], vlenstr[0x100], *fsizestr;

  g_snprintf (isizestr, sizeof isizestr, "%u:%u", fn->width, fn->height);
  g_snprintf (vlenstr, sizeof vlenstr, "%f", fn->length);
#if GLIB_CHECK_VERSION(2, 30, 0)
  fsizestr = g_format_size (fn->size);
#else
  fsizestr = g_strdup_printf ("%d", fn->size);
#endif

  gtk_tree_store_set (store, itr, 0, fn->path, 1, isizestr, 2, fsizestr, 3,
                      vlenstr, 4, fn->format, 5, fn, -1);
  g_free (fsizestr);
}

static void
result_select_rowref_id (gui_t *gui, GtkTreeRowReference *ref, gint id)
{
  GtkTreePath *path;

  path = gtk_tree_row_reference_get_path (ref);
  if (id > 0)
    {
      gtk_tree_path_down (path);
      while (--id)
        {
          gtk_tree_path_next (path);
        }
    }
  gtk_tree_selection_select_path (gui->result_select, path);
}

static void
result_unselect_rowref_id (gui_t *gui, GtkTreeRowReference *ref, gint id)
{
  GtkTreePath *path;

  path = gtk_tree_row_reference_get_path (ref);
  if (id > 0)
    {
      gtk_tree_path_down (path);
      while (--id)
        {
          gtk_tree_path_next (path);
        }
    }
  gtk_tree_selection_unselect_path (gui->result_select, path);
}

static void
result_select_small_file (same_node *node, gui_t *gui)
{
  GSList *slist;
  file_node *fn, *sfn;

  if (!node->show)
    {
      return;
    }

  for (sfn = node->files->data, slist = node->files; slist != NULL;
       slist = g_slist_next (slist))
    {
      fn = slist->data;

      fn->selected = FALSE;
      result_unselect_rowref_id (gui, node->treerowref,
                                 g_slist_index (node->files, fn));
      if (fn->size < sfn->size)
        {
          sfn = fn;
        }
    }

  sfn->selected = TRUE;
  result_select_rowref_id (gui, node->treerowref,
                           g_slist_index (node->files, sfn));
}

static void
result_select_big_file (same_node *node, gui_t *gui)
{
  GSList *slist;
  file_node *fn, *sfn;

  if (!node->show)
    {
      return;
    }

  for (sfn = node->files->data, slist = node->files; slist != NULL;
       slist = g_slist_next (slist))
    {
      fn = slist->data;

      fn->selected = FALSE;
      result_unselect_rowref_id (gui, node->treerowref,
                                 g_slist_index (node->files, fn));
      if (fn->size > sfn->size)
        {
          sfn = fn;
        }
    }

  sfn->selected = TRUE;
  result_select_rowref_id (gui, node->treerowref,
                           g_slist_index (node->files, sfn));
}

static void
result_select_small_image (same_node *node, gui_t *gui)
{
  GSList *slist;
  file_node *fn, *sfn;

  if (!node->show)
    {
      return;
    }

  for (sfn = node->files->data, slist = node->files; slist != NULL;
       slist = g_slist_next (slist))
    {
      fn = slist->data;

      fn->selected = FALSE;
      result_unselect_rowref_id (gui, node->treerowref,
                                 g_slist_index (node->files, fn));
      if (fn->width * fn->height < sfn->width * sfn->height)
        {
          sfn = fn;
        }
    }

  sfn->selected = TRUE;
  result_select_rowref_id (gui, node->treerowref,
                           g_slist_index (node->files, sfn));
}

static void
result_select_big_image (same_node *node, gui_t *gui)
{
  GSList *slist;
  file_node *fn, *sfn;

  if (!node->show)
    {
      return;
    }

  for (sfn = node->files->data, slist = node->files; slist != NULL;
       slist = g_slist_next (slist))
    {
      fn = slist->data;

      fn->selected = FALSE;
      result_unselect_rowref_id (gui, node->treerowref,
                                 g_slist_index (node->files, fn));
      if (fn->width * fn->height > sfn->width * sfn->height)
        {
          sfn = fn;
        }
    }

  sfn->selected = TRUE;
  result_select_rowref_id (gui, node->treerowref,
                           g_slist_index (node->files, sfn));
}

static void
result_select_short_video (same_node *node, gui_t *gui)
{
  GSList *slist;
  file_node *fn, *sfn;

  if (!node->show)
    {
      return;
    }

  for (sfn = node->files->data, slist = node->files; slist != NULL;
       slist = g_slist_next (slist))
    {
      fn = slist->data;

      fn->selected = FALSE;
      result_unselect_rowref_id (gui, node->treerowref,
                                 g_slist_index (node->files, fn));
      if (fn->length < sfn->length)
        {
          sfn = fn;
        }
    }

  sfn->selected = TRUE;
  result_select_rowref_id (gui, node->treerowref,
                           g_slist_index (node->files, sfn));
}

static void
result_select_long_video (same_node *node, gui_t *gui)
{
  GSList *slist;
  file_node *fn, *sfn;

  if (!node->show)
    {
      return;
    }

  for (sfn = node->files->data, slist = node->files; slist != NULL;
       slist = g_slist_next (slist))
    {
      fn = slist->data;

      fn->selected = FALSE;
      result_unselect_rowref_id (gui, node->treerowref,
                                 g_slist_index (node->files, fn));
      if (fn->length > sfn->length)
        {
          sfn = fn;
        }
    }

  sfn->selected = TRUE;
  result_select_rowref_id (gui, node->treerowref,
                           g_slist_index (node->files, sfn));
}

static void
result_select_others (same_node *node, gui_t *gui)
{
  GSList *slist;
  file_node *fn;

  if (!node->show)
    {
      return;
    }

  for (slist = node->files; slist != NULL; slist = g_slist_next (slist))
    {
      fn = slist->data;

      if (fn->selected)
        {
          fn->selected = FALSE;
          result_unselect_rowref_id (gui, node->treerowref,
                                     g_slist_index (node->files, fn));
        }
      else
        {
          fn->selected = TRUE;
          result_select_rowref_id (gui, node->treerowref,
                                   g_slist_index (node->files, fn));
        }
    }
}
