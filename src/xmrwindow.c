/**
 * xmrwindow.c
 * This file is part of xmradio
 *
 * Copyright (C) 2012  Weitian Leung (weitianleung@gmail.com)

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <glib/gi18n.h>
#include <stdlib.h>
#include <libpeas-gtk/peas-gtk-plugin-manager.h>
#include <libpeas/peas-extension-set.h>
#include <libpeas/peas-activatable.h>

#include "xmrwindow.h"
#include "xmrplayer.h"
#include "xmrbutton.h"
#include "xmrskin.h"
#include "xmrdebug.h"
#include "xmrplayer.h"
#include "lib/xmrservice.h"
#include "config.h"
#include "xmrutil.h"
#include "xmrsettings.h"
#include "xmrradiochooser.h"
#include "xmrdb.h"
#include "xmrmarshal.h"
#include "xmrapp.h"
#include "xmrpluginengine.h"

G_DEFINE_TYPE(XmrWindow, xmr_window, GTK_TYPE_WINDOW);

#define DEFAULT_RADIO_URL	"http://www.xiami.com/kuang/xml/type/6/id/0"
#define COVER_WIDTH	100
#define COVER_HEIGHT 100

enum
{
	BUTTON_CLOSE = 0,	// X button
	BUTTON_MINIMIZE,	// - button
	BUTTON_PLAY,		// play
	BUTTON_PAUSE,
	BUTTON_NEXT,
	BUTTON_LIKE,
	BUTTON_DISLIKE,
	BUTTON_VOLUME,
	BUTTON_LYRIC,
	BUTTON_DOWNLOAD,
	BUTTON_SHARE,
	BUTTON_SIREN,
	BUTTON_FENGGE,
	BUTTON_XINGZUO,
	BUTTON_NIANDAI,
	LAST_BUTTON
};

enum
{
	LABEL_RADIO = 0,
	LABEL_SONG_NAME,
	LABEL_ARTIST,
	LABEL_TIME,
	LAST_LABEL
};

struct _XmrWindowPrivate
{
	gchar		*skin;
	gboolean	gtk_theme;	/* TRUE to use gtk theme rather than skin */

	GtkWidget	*buttons[LAST_BUTTON];	/* ui buttons */
	GtkWidget	*labels[LAST_LABEL];
	GtkWidget	*image;	/* album cover image */

	cairo_surface_t	*cs_bkgnd;	/* backgroud image surface */

	GtkWidget	*fixed;			/* #GtkFixed */
	GtkWidget	*popup_menu;	/* #GtkMenu */
	GtkWidget	*skin_menu;

	/**
	 * #XmrRadioChooser
	 * 0 - FengGe
	 * 1 - XingZuo
	 * 2 - NianDai
	 */
	GtkWidget	*chooser[3];

	XmrPlayer *player;
	XmrService *service;

	GList *playlist;
	gchar *playlist_url;	/* set NULL to get private list */

	XmrSettings *settings;
	GList *skin_list;

	GSList *skin_item_group; /* skin menu item list */

	GdkPixbuf *pb_cover;	/* default album cover image pixbuf */

	GtkBuilder *ui_pref;
	GtkBuilder *ui_login;

	gchar *usr;
	gchar *pwd;

	/**
	 * flag to state when login success whether 
	 * switch to siren radio or not
	 */
	gboolean switch_radio;

	XmrPluginEngine		*plugin_engine;
	PeasExtensionSet	*extensions;
};

enum
{
	PROP_0,
	PROP_PLAYER,
	PROP_SERVICE,
	PROP_SKIN,
	PROP_GTK_THEME,
	PROP_MENU_POPUP,
	PROP_LAYOUT,
	PROP_PLAYLIST
};

enum
{
	THEME_CHANGED,
	TRACK_CHANGED,
	RADIO_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static const gchar *radio_style[]=
{
	"风格电台", "星座电台", "年代电台"
};

static void
xmr_window_dispose(GObject *obj);

static void
xmr_window_finalize(GObject *obj);

static void
xmr_window_get_property(GObject *object,
		       guint prop_id,
		       GValue *value,
		       GParamSpec *pspec);

static void
xmr_window_set_property(GObject *object,
		       guint prop_id,
		       const GValue *value,
		       GParamSpec *pspec);

static gboolean
on_draw(XmrWindow *window, cairo_t *cr, gpointer data);

static gboolean
on_button_press(XmrWindow *window, GdkEventButton *event, gpointer data);

static void
on_xmr_button_clicked(GtkWidget *widget, gpointer data);

static void
radio_selected(XmrRadioChooser *chooser,
			XmrRadio *radio,
			XmrWindow *window);

static void
set_window_image(XmrWindow *window, GdkPixbuf *pixbuf);

static void
set_cover_image(XmrWindow *window, GdkPixbuf *pixbuf);

static void
set_gtk_theme(XmrWindow *window);

static void
set_skin(XmrWindow *window, const gchar *skin);

static void
hide_children(XmrWindow *window);

/**
 * player signals
 */
static void
player_eos(XmrPlayer *player,
			gboolean early,
			XmrWindow *window);

static void
player_error(XmrPlayer *player,
			GError *error,
			XmrWindow *window);

static void
player_tick(XmrPlayer *player,
			gint64 elapsed,
			gint64 duration,
			XmrWindow *window);

static void
player_buffering(XmrPlayer *player,
			guint progress,
			XmrWindow *window);

static void
player_state_changed(XmrPlayer *player,
			gint old_state,
			gint new_state,
			XmrWindow *window);

/**
 * threads
 */
static gboolean
thread_finish(GThread *thread);

static gpointer
thread_get_playlist(XmrWindow *window);

static gpointer
thread_get_cover_image(XmrWindow *window);

static gpointer
thread_login(XmrWindow *window);

static gpointer
thread_update_radio_list(XmrWindow *window);

static void
xmr_window_get_playlist(XmrWindow *window);

static void
xmr_window_get_cover_image(XmrWindow *window);

static void
xmr_window_login(XmrWindow *window);

static void
xmr_window_set_track_info(XmrWindow *window);

static void
update_radio_list(XmrWindow *window);

static void
create_popup_menu(XmrWindow *window);

/**
 * popup menu message
 */
static void
on_menu_item_activate(GtkMenuItem *item, XmrWindow *window);

/**
 * for skin menu item only
 */
static void
on_skin_menu_item_activate(GtkMenuItem *item, SkinInfo *skin);

static void
load_settings(XmrWindow *window);

static void
load_skin(XmrWindow *window);

static void
load_radio(XmrWindow *window);

/**
 * read @skin information
 * if OK, then append to skin_list
 * @data, skin_list pointer pointer
 */
static void
append_skin(const gchar *skin,
			gpointer data);

static GtkWidget *
append_skin_to_menu(XmrWindow *window, SkinInfo *info);

/**
 * set @widget fg color by parse @color_str
 */
static void
set_widget_fg_color(GtkWidget *widget,
			const gchar *color_str);

/**
 * set @widget font
 */
static void
set_widget_font(GtkWidget *widget,
			const gchar *font_desc);

static GtkBuilder *
create_builder_with_file(const gchar *file);

static void
init_pref_window(XmrWindow *window,
			GtkWidget *pref);

/**
 * login to server
 */
static void
do_login(XmrWindow *window);


/**
 * set @url to NULL to change to siren radio
 */
static void
change_radio(XmrWindow *window,
			const gchar *name,
			const gchar *url);

/**
 * login dialog >login button clicked
 */
static gboolean
on_login_dialog_button_login_clicked(GtkButton *button,
			GdkEvent  *event,
			XmrWindow *window);

/**
 * test if @text is empty
 * or just contains blank chars
 */
static gboolean
is_text_empty(const gchar *text);

static gchar *
radio_logo_url_to_uri(RadioInfo *radio);

static gboolean
on_delete_event(XmrWindow *window,
			GdkEvent *event,
			gpointer data);

static void
on_extension_added(PeasExtensionSet *set,
		    PeasPluginInfo   *info,
		    PeasActivatable  *activatable,
		    gpointer data);

static void
on_extension_removed(PeasExtensionSet *set,
		      PeasPluginInfo   *info,
			  PeasActivatable  *activatable,
			  gpointer data);
static void
install_properties(GObjectClass *object_class)
{
	g_object_class_install_property(object_class,
				PROP_PLAYER,
				g_param_spec_object("player",
					"Player",
					"Player object",
					G_TYPE_OBJECT,
					G_PARAM_READABLE));

	g_object_class_install_property(object_class,
				PROP_SERVICE,
				g_param_spec_object("service",
					"Service",
					"XmrService object",
					G_TYPE_OBJECT,
					G_PARAM_READABLE));

	g_object_class_install_property(object_class,
				PROP_SKIN,
				g_param_spec_string("skin",
					"Skin",
					"Skin",
					NULL,
					G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
				PROP_GTK_THEME,
				g_param_spec_boolean("gtk-theme",
					"Gtk theme",
					"Use gtk theme or not",
					FALSE,
					G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
				PROP_GTK_THEME,
				g_param_spec_object("layout",
					"Layout",
					"Window layout",
					G_TYPE_OBJECT,
					G_PARAM_READABLE));

	g_object_class_install_property(object_class,
				PROP_MENU_POPUP,
				g_param_spec_object("menu-popup",
					"Popup menu",
					"Popup menu",
					G_TYPE_OBJECT,
					G_PARAM_READABLE));

	g_object_class_install_property(object_class,
				PROP_MENU_POPUP,
				g_param_spec_pointer("playlist",
					"playlist",
					"Current playlist",
					G_PARAM_READABLE));
}

static void
create_signals(XmrWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	/**
	 * emit when user changed skin
	 * when @new_theme is NULL, it means
	 * UI use GTK theme
	 */
	signals[THEME_CHANGED] =
		g_signal_new("theme-changed",
					G_OBJECT_CLASS_TYPE(object_class),
					G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
					G_STRUCT_OFFSET(XmrWindowClass, theme_changed),
					NULL, NULL,
					g_cclosure_marshal_VOID__STRING,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING);

	/**
	 * emit when current track changed
	 */
	signals[TRACK_CHANGED] =
		g_signal_new("track-changed",
					G_OBJECT_CLASS_TYPE(object_class),
					G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
					G_STRUCT_OFFSET(XmrWindowClass, track_changed),
					NULL, NULL,
					g_cclosure_marshal_VOID__POINTER,
					G_TYPE_NONE,
					1,
					G_TYPE_POINTER);

	/**
	 * emit when current radio changed
	 */
	signals[RADIO_CHANGED] =
		g_signal_new("radio-changed",
					G_OBJECT_CLASS_TYPE(object_class),
					G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
					G_STRUCT_OFFSET(XmrWindowClass, radio_changed),
					NULL, NULL,
					xmr_marshal_VOID__STRING_STRING,
					G_TYPE_NONE,
					2,
					G_TYPE_STRING, G_TYPE_STRING);
}

static void 
xmr_window_class_init(XmrWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->set_property = xmr_window_set_property;
	object_class->get_property = xmr_window_get_property;

	object_class->dispose = xmr_window_dispose;
	object_class->finalize = xmr_window_finalize;

	install_properties(object_class);
	create_signals(klass);

	g_type_class_add_private(object_class, sizeof(XmrWindowPrivate));
}

static void 
xmr_window_init(XmrWindow *window)
{
	XmrWindowPrivate *priv;
	gint i;
	gchar *path = NULL;

	window->priv = G_TYPE_INSTANCE_GET_PRIVATE(window, XMR_TYPE_WINDOW, XmrWindowPrivate);
	priv = window->priv;

	priv->player = xmr_player_new();
	priv->service = xmr_service_new();
	priv->skin = NULL;
	priv->gtk_theme = FALSE;
	priv->cs_bkgnd = NULL;
	priv->playlist = NULL;
	priv->playlist_url = NULL;
	priv->settings = xmr_settings_new();
	priv->skin_list = NULL;
	priv->skin_item_group = NULL;
	priv->ui_pref = create_builder_with_file(UIDIR"/pref.ui");
	priv->ui_login = NULL;
	priv->usr = NULL;
	priv->pwd = NULL;
	priv->switch_radio = FALSE;
	priv->pb_cover = NULL;
	priv->plugin_engine = xmr_plugin_engine_new();
	priv->extensions = peas_extension_set_new(PEAS_ENGINE(priv->plugin_engine),
						   PEAS_TYPE_ACTIVATABLE,
						   "object", window,
						   NULL);
	peas_extension_set_foreach(priv->extensions,
                              (PeasExtensionSetForeachFunc)on_extension_added,
                              NULL);

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);
	gtk_widget_add_events(GTK_WIDGET(window), GDK_BUTTON_PRESS_MASK);

	gtk_window_set_default_icon_name("xmradio");

	priv->fixed = gtk_fixed_new();
	create_popup_menu(window);

	for(i=0; i<LAST_BUTTON; ++i)
	{
		priv->buttons[i] = xmr_button_new(XMR_BUTTON_SKIN);
		gtk_fixed_put(GTK_FIXED(priv->fixed), priv->buttons[i], 0, 0);

		g_signal_connect(priv->buttons[i], "clicked",
					G_CALLBACK(on_xmr_button_clicked), (gpointer)(glong)i);
	}

	for(i=0; i<LAST_LABEL; ++i)
	{
		priv->labels[i] = gtk_label_new("");
		gtk_fixed_put(GTK_FIXED(priv->fixed), priv->labels[i], 0, 0);
	}

	priv->image = gtk_image_new();
	gtk_fixed_put(GTK_FIXED(priv->fixed), priv->image, 0, 0);

	gtk_container_add(GTK_CONTAINER(window), priv->fixed);

	gtk_widget_show(priv->fixed);

	priv->chooser[0] = xmr_radio_chooser_new(_("风格电台"));
	priv->chooser[1] = xmr_radio_chooser_new(_("星座电台"));
	priv->chooser[2] = xmr_radio_chooser_new(_("年代电台"));

	g_signal_connect(window, "draw", G_CALLBACK(on_draw), NULL);
	g_signal_connect(window, "button-press-event", G_CALLBACK(on_button_press), NULL);

	g_signal_connect(priv->player, "eos", G_CALLBACK(player_eos), window);
	g_signal_connect(priv->player, "error", G_CALLBACK(player_error), window);
	g_signal_connect(priv->player, "tick", G_CALLBACK(player_tick), window);
	g_signal_connect(priv->player, "buffering", G_CALLBACK(player_buffering), window);
	g_signal_connect(priv->player, "state-changed", G_CALLBACK(player_state_changed), window);

	for(i=0; i<3; ++i)
	  g_signal_connect(priv->chooser[i], "radio-selected",
				  G_CALLBACK(radio_selected), window);

	g_signal_connect_after(window, "delete-event", G_CALLBACK(on_delete_event), NULL);

	g_signal_connect(priv->extensions, "extension-added",
			  G_CALLBACK(on_extension_added), NULL);

	g_signal_connect(priv->extensions, "extension-removed",
			  G_CALLBACK(on_extension_removed), NULL);

	gtk_widget_hide(priv->buttons[BUTTON_PAUSE]);

	load_settings(window);
}

GtkWidget* xmr_window_new()
{
	 return g_object_new(XMR_TYPE_WINDOW,
				"type", GTK_WINDOW_TOPLEVEL,
				"title", _("XMRadio"),
				"resizable", FALSE,
				NULL);
}

static void
xmr_window_dispose(GObject *obj)
{
	XmrWindow *window = XMR_WINDOW(obj);
	XmrWindowPrivate *priv = window->priv;

	if (priv->player != NULL)
	{
		g_object_unref(priv->player);
		priv->player = NULL;
	}
	if (priv->service != NULL)
	{
		g_object_unref(priv->service);
		priv->service = NULL;
	}

	if (priv->ui_pref)
	{
		g_object_unref(priv->ui_pref);
		priv->ui_pref = NULL;
	}

	if (priv->ui_login)
	{
		g_object_unref(priv->ui_login);
		priv->ui_login = NULL;
	}

	if (priv->pb_cover)
	{
		g_object_unref(priv->pb_cover);
		priv->pb_cover = NULL;
	}

	if (priv->extensions != NULL)
	{
		g_object_unref (priv->extensions);
		priv->extensions = NULL;
	}

	peas_engine_garbage_collect(PEAS_ENGINE(priv->plugin_engine));

	G_OBJECT_CLASS(xmr_window_parent_class)->dispose(obj);
}

static void
xmr_window_finalize(GObject *obj)
{
	XmrWindow *window = XMR_WINDOW(obj);
	XmrWindowPrivate *priv = window->priv;

	if (priv->skin)
		g_free(priv->skin);

	if (priv->cs_bkgnd)
		cairo_surface_destroy(priv->cs_bkgnd);

	if (priv->playlist_url)
		g_free(priv->playlist_url);

	if (priv->playlist)
		g_list_free_full(priv->playlist, (GDestroyNotify)song_info_free);

	if (priv->skin_list)
		g_list_free_full(priv->skin_list, (GDestroyNotify)xmr_skin_info_free);

	if (priv->usr)
		g_free(priv->usr);
	if (priv->pwd)
		g_free(priv->pwd);

	G_OBJECT_CLASS(xmr_window_parent_class)->finalize(obj);
}

static void
xmr_window_get_property(GObject *object,
		       guint prop_id,
		       GValue *value,
		       GParamSpec *pspec)
{
	XmrWindow *window = XMR_WINDOW(object);
	XmrWindowPrivate *priv = window->priv;

	switch(prop_id)
	{
	case PROP_PLAYER:
		g_value_set_object(value, priv->player);
		break;

	case PROP_SERVICE:
		g_value_set_object(value, priv->service);
		break;

	case PROP_SKIN:
		g_value_set_string(value, priv->skin);
		break;

	case PROP_GTK_THEME:
		g_value_set_boolean(value, priv->gtk_theme);
		break;

	case PROP_LAYOUT:
		g_value_set_object(value, priv->fixed);
		break;

	case PROP_MENU_POPUP:
		g_value_set_object(value, priv->popup_menu);
		break;

	case PROP_PLAYLIST:
		g_value_set_pointer(value, priv->playlist);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
xmr_window_set_property(GObject *object,
		       guint prop_id,
		       const GValue *value,
		       GParamSpec *pspec)
{
	XmrWindow *window = XMR_WINDOW(object);
	XmrWindowPrivate *priv = window->priv;

	switch(prop_id)
	{
	case PROP_SKIN:
		g_free(priv->skin);
		priv->skin = g_value_dup_string(value);
		break;

	case PROP_GTK_THEME:
		priv->gtk_theme = g_value_get_boolean(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean
on_draw(XmrWindow *window, cairo_t *cr, gpointer data)
{
	XmrWindowPrivate *priv = window->priv;

	if (priv->gtk_theme || priv->cs_bkgnd == NULL)
		return FALSE;

	cairo_set_source_surface(cr, priv->cs_bkgnd, 0, 0);
	cairo_paint(cr);
	
	return FALSE;
}

static gboolean
on_button_press(XmrWindow *window, GdkEventButton *event, gpointer data)
{
	if (event->button == 1)  
    {
		if (window->priv->gtk_theme) 	// ignore if using gtk theme
			return FALSE;
		gtk_window_begin_move_drag(GTK_WINDOW(window),
					event->button,
					event->x_root, event->y_root,
					event->time);
    }
	else if (event->button == 3)
	{
		GtkMenu *menu = GTK_MENU(window->priv->popup_menu);
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
					event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
on_xmr_button_clicked(GtkWidget *widget, gpointer data)
{
	XmrButton *button = XMR_BUTTON(widget);
	XmrWindow *window = XMR_WINDOW(gtk_widget_get_toplevel(widget));
	XmrWindowPrivate *priv = window->priv;
	glong id = (glong)data;

	switch(id)
	{
	case BUTTON_CLOSE:
		xmr_window_quit(window);
		break;

	case BUTTON_MINIMIZE:
		gtk_window_iconify(GTK_WINDOW(window));
		break;

	case BUTTON_PLAY:
		xmr_window_play(window);
		break;

	case BUTTON_PAUSE:
		xmr_player_pause(priv->player);
		break;

	case BUTTON_NEXT:
		xmr_window_play_next(window);
		break;

	case BUTTON_LIKE:
	case BUTTON_DISLIKE:
	{
		SongInfo *song;
		if (!xmr_service_is_logged_in(priv->service))
		{
			g_warning("You should login first\n");
			break;
		}

		if (priv->playlist == NULL || priv->playlist->data == NULL)
			break;

		song = (SongInfo *)priv->playlist->data;
		xmr_service_like_song(priv->service, song->song_id, id == BUTTON_LIKE);
		if (id == BUTTON_DISLIKE)
			xmr_window_play_next(window);
	}
		break;

	case BUTTON_LYRIC:
	case BUTTON_DOWNLOAD:
	{
		gint ret;
		SongInfo *song = xmr_window_get_current_song(window);
		gchar *command = NULL;
		if (song == NULL)
		{
			xmr_debug("Playlist empty");
			break;
		}
		if (id == BUTTON_LYRIC){
			command = g_strdup_printf("xdg-open http://www.xiami.com/radio/lyric/sid/%s", song->song_id);
		}else{
			command = g_strdup_printf("xdg-open http://www.xiami.com/song/%s", song->song_id);
		}
		if (command == NULL)
			g_error("No more memory\n");

		ret = system(command);

		g_free(command);
	}
		break;

	case BUTTON_SHARE:
		xmr_debug("Not implemented yet");
		break;
	
	case BUTTON_SIREN:
		if (!xmr_service_is_logged_in(priv->service))
		{
			priv->switch_radio = TRUE;
			do_login(window);
		}
		else
		{
			// to avoid change radio from siren to siren
			if (priv->playlist_url != NULL)
				change_radio(window, _("私人电台"), NULL);
		}
		break;

	case BUTTON_FENGGE:
		if (gtk_widget_get_visible(priv->chooser[0]))
		{
			gtk_widget_hide(priv->chooser[0]);
		}
		else
		{
			gtk_widget_show_all(priv->chooser[0]);
			gtk_widget_hide(priv->chooser[1]);
			gtk_widget_hide(priv->chooser[2]);
		}
		break;

	case BUTTON_XINGZUO:
		if (gtk_widget_get_visible(priv->chooser[1]))
		{
			gtk_widget_hide(priv->chooser[1]);
		}
		else
		{
			gtk_widget_show_all(priv->chooser[1]);
			gtk_widget_hide(priv->chooser[0]);
			gtk_widget_hide(priv->chooser[2]);
		}
		break;

	case BUTTON_NIANDAI:
		if (gtk_widget_get_visible(priv->chooser[2]))
		{
			gtk_widget_hide(priv->chooser[2]);
		}
		else
		{
			gtk_widget_show_all(priv->chooser[2]);
			gtk_widget_hide(priv->chooser[1]);
			gtk_widget_hide(priv->chooser[0]);
		}
		break;
	}
}

static void
radio_selected(XmrRadioChooser *chooser,
			XmrRadio *radio,
			XmrWindow *window)
{
	const gchar *name = xmr_radio_get_name(radio);
	const gchar *url = xmr_radio_get_url(radio);

	change_radio(window, name, url);
}

static void
set_window_image(XmrWindow *window, GdkPixbuf *pixbuf)
{
	cairo_surface_t *image;
	cairo_t *cr;
	cairo_region_t *mask;
	gint image_width, image_height;
	XmrWindowPrivate *priv;
	
	g_return_if_fail(window != NULL && pixbuf != NULL);
	priv = window->priv;

	image_width = gdk_pixbuf_get_width(pixbuf);
	image_height = gdk_pixbuf_get_height(pixbuf);

	image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, image_width, image_height);
	if (image == NULL)
		return ;

	cr = cairo_create(image);

	gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
	cairo_paint(cr);

	cairo_destroy(cr);

	if (priv->cs_bkgnd){
		cairo_surface_destroy(priv->cs_bkgnd);
	}

	priv->cs_bkgnd = image;
	mask = gdk_cairo_region_create_from_surface(priv->cs_bkgnd);
	if (mask == NULL)
		return ;
	gtk_widget_shape_combine_region(GTK_WIDGET(window), mask);

	cairo_region_destroy(mask);

	gtk_widget_set_size_request(GTK_WIDGET(window), image_width, image_height);

	gtk_widget_queue_draw(GTK_WIDGET(window));
}

static void
set_cover_image(XmrWindow *window, GdkPixbuf *pixbuf)
{
	XmrWindowPrivate *priv = window->priv;

	gint i_w, i_h;

	i_w = gdk_pixbuf_get_width(pixbuf);
	i_h = gdk_pixbuf_get_height(pixbuf);

	if (i_w != COVER_WIDTH || i_h != COVER_HEIGHT) //scale
	{
		GdkPixbuf *pb = gdk_pixbuf_scale_simple(pixbuf, COVER_WIDTH, COVER_HEIGHT, GDK_INTERP_BILINEAR);
		if (pb)
		{
			gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image), pb);
			g_object_unref(pb);
		}
	}
	else
	{
		gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image), pixbuf);
	}
}

static void
set_gtk_theme(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	struct Pos { gint x, y;	};
	gint i;

	static struct Pos button_pos[LAST_BUTTON] =
	{
		{0, 0}, {0, 0},
		{360, 100}, {360, 100}, {430, 100},
		{170, 160}, {210, 160}, {250, 160},
		{330, 170}, {385, 170}, {440, 170},
		{110, 250}, {195, 250}, {280, 250}, {365, 250}
	};

	static struct Pos label_pos[LAST_LABEL] =
	{
		{60, 45}, {175, 90},
		{175, 110}, {175, 140}
	};

	// for testing only
	static gchar *stock_id[] =
	{
		"", "",
		GTK_STOCK_MEDIA_PLAY, GTK_STOCK_MEDIA_PAUSE, GTK_STOCK_MEDIA_NEXT,
		GTK_STOCK_ABOUT, GTK_STOCK_CANCEL, NULL,
		NULL, NULL, NULL,
		NULL, NULL, NULL, NULL
		
	};

	priv->gtk_theme = TRUE;

	gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
	gtk_widget_set_size_request(GTK_WIDGET(window), 540, 250);
	gtk_window_set_default_size(GTK_WINDOW(window), 540, 250);

	hide_children(window);

	for(i=BUTTON_PLAY; i<LAST_BUTTON; ++i)
	{
		gtk_fixed_move(GTK_FIXED(priv->fixed),
					priv->buttons[i],
					button_pos[i].x, button_pos[i].y);

		xmr_button_set_type(XMR_BUTTON(priv->buttons[i]),
						XMR_BUTTON_NORMAL);

		xmr_button_set_image_from_stock(XMR_BUTTON(priv->buttons[i]),
					stock_id[i]);

		gtk_widget_show(priv->buttons[i]);
	}

	for(i=0; i<LAST_LABEL; ++i)
	{
		gtk_fixed_move(GTK_FIXED(priv->fixed),
					priv->labels[i],
					label_pos[i].x, label_pos[i].y);

		gtk_widget_show(priv->labels[i]);
	}

	gtk_fixed_move(GTK_FIXED(priv->fixed), priv->image, 60, 85);
	gtk_widget_show(priv->image);

	gtk_button_set_label(GTK_BUTTON(priv->buttons[BUTTON_LYRIC]), _("歌词"));
	gtk_button_set_label(GTK_BUTTON(priv->buttons[BUTTON_DOWNLOAD]), _("下载"));
	gtk_button_set_label(GTK_BUTTON(priv->buttons[BUTTON_SHARE]), _("分享"));

	gtk_button_set_label(GTK_BUTTON(priv->buttons[BUTTON_SIREN]), _("私人电台"));
	gtk_button_set_label(GTK_BUTTON(priv->buttons[BUTTON_FENGGE]), _("风格电台"));
	gtk_button_set_label(GTK_BUTTON(priv->buttons[BUTTON_XINGZUO]), _("星座电台"));
	gtk_button_set_label(GTK_BUTTON(priv->buttons[BUTTON_NIANDAI]), _("年代电台"));

	xmr_settings_set_theme(priv->settings, "");

	gtk_widget_queue_draw(GTK_WIDGET(window));

	if (xmr_player_playing(priv->player))
		gtk_widget_hide(priv->buttons[BUTTON_PLAY]);
	else
		gtk_widget_hide(priv->buttons[BUTTON_PAUSE]);
}

static void
set_skin(XmrWindow *window, const gchar *skin)
{
	XmrWindowPrivate *priv = window->priv;
	gint x, y;
	GdkPixbuf *pixbuf;
	gint i;
	XmrSkin *xmr_skin;
	static const gchar *ui_main_buttons[] =
	{
		"close", "minimize", "play", "pause",
		"next", "like", "dislike", "volume",
		"lyric", "download", "share", "siren",
		"fengge", "xingzuo", "niandai"
	};

	static const gchar *ui_main_labels[] =
	{
		"radio_name", "song_name",
		"artist", "progress"
	};

	xmr_skin = xmr_skin_new();

	do
	{
		if (!xmr_skin_load(xmr_skin, skin))
		{
			xmr_debug("failed to load skin: %s", skin);
			break;
		}

		pixbuf = xmr_skin_get_image(xmr_skin, UI_MAIN, NULL);
		if (pixbuf == NULL)
		{
			xmr_debug("Missing background image ?");
			break;
		}

		x = gdk_pixbuf_get_width(pixbuf);
		y = gdk_pixbuf_get_height(pixbuf);

		priv->gtk_theme = FALSE;

		gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
		set_window_image(window, pixbuf);
		g_object_unref(pixbuf);

		hide_children(window);

		for(i=0; i<LAST_BUTTON; ++i)
		{
			pixbuf = xmr_skin_get_image(xmr_skin, UI_MAIN, ui_main_buttons[i]);
			if (pixbuf == NULL)
				continue ;

			xmr_button_set_image_from_pixbuf(XMR_BUTTON(priv->buttons[i]), pixbuf);
			g_object_unref(pixbuf);
			xmr_button_set_type(XMR_BUTTON(priv->buttons[i]), XMR_BUTTON_SKIN);

			if (xmr_skin_get_position(xmr_skin, UI_MAIN, ui_main_buttons[i], &x, &y))
			{
				gtk_fixed_move(GTK_FIXED(priv->fixed), priv->buttons[i], x, y);
				gtk_widget_show(priv->buttons[i]);
			}
		}

		for(i=0; i<LAST_LABEL; ++i)
		{
			gchar *value = NULL;
			if (xmr_skin_get_position(xmr_skin, UI_MAIN, ui_main_labels[i], &x, &y))
			{
				gtk_fixed_move(GTK_FIXED(priv->fixed), priv->labels[i], x, y);
				gtk_widget_show(priv->labels[i]);
			}

			xmr_skin_get_color(xmr_skin, UI_MAIN, ui_main_labels[i], &value);
			set_widget_fg_color(priv->labels[i], value);
			g_free(value);

			value = NULL;
			xmr_skin_get_font(xmr_skin, UI_MAIN, ui_main_labels[i], &value);
			set_widget_font(priv->labels[i], value);
			g_free(value);
		}

		if (xmr_skin_get_position(xmr_skin, UI_MAIN, "cover", &x, &y))
		{
			gtk_fixed_move(GTK_FIXED(priv->fixed), priv->image, x, y);
			gtk_widget_show(priv->image);
		}

		if (priv->pb_cover)
			g_object_unref(priv->pb_cover);

		priv->pb_cover = xmr_skin_get_image(xmr_skin, UI_MAIN, "cover");
		if (priv->pb_cover && gtk_image_get_pixbuf(GTK_IMAGE(priv->image)) == NULL)
			set_cover_image(window, priv->pb_cover);

		// save to settings
		{
			SkinInfo *info = xmr_skin_info_new();
			if (info == NULL)
				g_error("No more memory !");
			xmr_skin_get_info(xmr_skin, info);
			xmr_settings_set_theme(priv->settings, info->name);

			if (priv->skin)
				g_free(priv->skin);

			priv->skin = g_strdup(info->name);
		}

		gtk_widget_queue_draw(GTK_WIDGET(window));
		g_signal_emit(window, signals[THEME_CHANGED], 0, skin);

		if (xmr_player_playing(priv->player))
			gtk_widget_hide(priv->buttons[BUTTON_PLAY]);
		else
			gtk_widget_hide(priv->buttons[BUTTON_PAUSE]);
	}
	while(0);

	g_object_unref(xmr_skin);
}

static void
hide_children(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	gint i;

	for(i=0; i<LAST_BUTTON; ++i)
	  gtk_widget_hide(priv->buttons[i]);

	for(i=0; i<LAST_LABEL; ++i)
	  gtk_widget_hide(priv->labels[i]);

	gtk_widget_hide(priv->image);
}

static void
player_eos(XmrPlayer *player,
			gboolean early,
			XmrWindow *window)
{
	xmr_window_play_next(window);
}

static void
player_error(XmrPlayer *player,
			GError *error,
			XmrWindow *window)
{
	g_warning("Player error: %s\n", error->message);
}

static void
player_tick(XmrPlayer *player,
			gint64 elapsed,
			gint64 duration,
			XmrWindow *window)
{
	glong mins_elapsed;
	glong secs_elapsed;

	glong mins_duration;
	glong secs_duration;

	gint64 secs;
	gchar *time;

	secs = elapsed / G_USEC_PER_SEC / 1000;
	mins_elapsed = secs / 60;
	secs_elapsed = secs % 60;

	secs = duration / G_USEC_PER_SEC / 1000;
	mins_duration = secs / 60;
	secs_duration = secs % 60;

	time = g_strdup_printf("%02ld:%02ld/%02ld:%02ld",
				mins_elapsed, secs_elapsed,
				mins_duration, secs_duration
				);

	if (time == NULL){
		g_error("Failed to alloc memory\n");
	}

	gtk_label_set_text(GTK_LABEL(window->priv->labels[LABEL_TIME]),
				time);

	g_free(time);
}

static void
player_buffering(XmrPlayer *player,
			guint progress,
			XmrWindow *window)
{
	xmr_debug("Buffering: %d\n", progress);
}

static void
player_state_changed(XmrPlayer *player,
			gint old_state,
			gint new_state,
			XmrWindow *window)
{
	if (new_state == GST_STATE_PLAYING)
	{
		gtk_widget_hide(window->priv->buttons[BUTTON_PLAY]);
		gtk_widget_show(window->priv->buttons[BUTTON_PAUSE]);
	}
	else if(new_state == GST_STATE_PAUSED)
	{
		gtk_widget_hide(window->priv->buttons[BUTTON_PAUSE]);
		gtk_widget_show(window->priv->buttons[BUTTON_PLAY]);
	}
}

static gboolean
thread_finish(GThread *thread)
{
#if GLIB_CHECK_VERSION(2, 32, 0)
	g_thread_unref(thread);
#endif

	return FALSE;
}

static gpointer
thread_get_playlist(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	gint result = 1;
	gboolean auto_play = g_list_length(priv->playlist) == 0;

	if (priv->playlist_url == NULL){
		result = xmr_service_get_track_list(priv->service, &priv->playlist);
	}else{
		result = xmr_service_get_track_list_by_style(priv->service, &priv->playlist, priv->playlist_url);
	}

	if (result != 0)
	{
		xmr_debug("failed to get track list: %d", result);
	}
	else if (auto_play)
	{
		if (g_list_length(priv->playlist) > 0)
		{
			SongInfo *song = (SongInfo *)priv->playlist->data;
			xmr_player_open(priv->player, song->location, NULL);
			xmr_player_play(priv->player);

			gdk_threads_enter();
			xmr_window_set_track_info(window);
			gdk_threads_leave();

			g_signal_emit(window, signals[TRACK_CHANGED], 0, song);
		}
	}

	g_idle_add((GSourceFunc)thread_finish, g_thread_self());

	return NULL;
}

static gpointer
thread_get_cover_image(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	GString *data = NULL;
	GdkPixbuf *pixbuf = NULL;

	do
	{
		SongInfo *track;
		gint result;

		if (priv->playlist == NULL)
			break;

		data = g_string_new("");
		if (data == NULL)
			break;

		// always get first song info
		track = (SongInfo *)priv->playlist->data;
		result = xmr_service_get_url_data(priv->service, track->album_cover, data);
		if (result != 0)
		{
			xmr_debug("xmr_service_get_url_data failed: %d", result);
			break;
		}

		pixbuf = gdk_pixbuf_from_memory(data->str, data->len);
		if (pixbuf == NULL)
		{
			xmr_debug("gdk_pixbuf_from_memory failed");
			break;
		}

		gdk_threads_enter();
		set_cover_image(window, pixbuf);
		gdk_threads_leave();
	}
	while(0);

	if (data) {
		g_string_free(data, TRUE);
	}
	if (pixbuf) {
		g_object_unref(pixbuf);
	}
	g_idle_add((GSourceFunc)thread_finish, g_thread_self());

	return NULL;
}

static gpointer
thread_login(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	gint result;
	gchar *login_message = NULL;

	result = xmr_service_login(priv->service, priv->usr, priv->pwd, &login_message);

	if (result != 0)
	{
		gchar *message;

		message = g_strdup_printf(_("Login failed:\n%s"), login_message);

		if (message == NULL){
			g_error("No more memory??\n");
		}

		gdk_threads_enter();
		xmr_message(GTK_WIDGET(window), message, _("Login Status"));
		gdk_threads_leave();
		g_free(message);

		change_radio(window, "", priv->playlist_url);
	}
	else
	{
		xmr_settings_set_usr_info(priv->settings, priv->usr, priv->pwd);
		// switch to siren radio
		if (priv->switch_radio || priv->playlist_url == NULL)
		{
			gdk_threads_enter();
			change_radio(window, _("私人电台"), NULL);
			gdk_threads_leave();
		}
	}
	xmr_debug("login status: %s", login_message);
	g_free(login_message);

	g_idle_add((GSourceFunc)thread_finish, g_thread_self());

	return NULL;
}

static gpointer
thread_update_radio_list(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	gint style[3] = { Style_FengGe, Style_XingZuo, Style_NianDai };
	gint i;
	XmrService *service;
	XmrDb *db = xmr_db_new();

	// use a new service rather than priv->service
	// it will block other data receiving
	service = xmr_service_new();

	// use BEGIN.. COMMIT to speed up writing
	xmr_db_begin(db);

	for(i=0; i<3; ++i)
	{
		GList *list = NULL;
		GList *p;
		gint result;

		result = xmr_service_get_radio_list(service, &list, style[i]);
		if (result != 0)
		{
			g_print("xmr_service_get_radio_list: %d", result);
			continue ;
		}

		p = list;

		while(p)
		{
			RadioInfo *radio_info = (RadioInfo *)p->data;
			GString *data = g_string_new("");

			result = xmr_service_get_url_data(service, radio_info->logo, data);
			if (result != 0)
			{
				g_print("xmr_service_get_url_data: %d", result);
			}
			else
			{
				gchar *uri = radio_logo_url_to_uri(radio_info);
				XmrRadio *xmr_radio;

				g_free(radio_info->logo);
				radio_info->logo = uri;

				// save cover to local file
				write_memory_to_file(uri, data->str, data->len);

				// save to database
				xmr_db_add_radio(db, radio_info, radio_style[i]);

				// append to chooser
				xmr_radio = xmr_radio_new_with_info(uri,
							radio_info->name, radio_info->url);

				xmr_radio_chooser_append(XMR_RADIO_CHOOSER(priv->chooser[i]),
							xmr_radio);
			}

			g_string_free(data, TRUE);
			p = p->next;
		} // end of while(p)

		g_list_free_full(list, (GDestroyNotify)radio_info_free);
	}

	xmr_db_commit(db);

	g_object_unref(service);
	g_object_unref(db);

	g_idle_add((GSourceFunc)thread_finish, g_thread_self());

	return NULL;
}

static void
xmr_window_get_playlist(XmrWindow *window)
{
#if GLIB_CHECK_VERSION(2, 32, 0)
	g_thread_new("playlist", (GThreadFunc)thread_get_playlist, window);
#else
	g_thread_create((GThreadFunc)thread_get_playlist, window, FALSE, NULL);
#endif
}

static void
xmr_window_get_cover_image(XmrWindow *window)
{
#if GLIB_CHECK_VERSION(2, 32, 0)
	g_thread_new("gcimage", (GThreadFunc)thread_get_cover_image, window);
#else
	g_thread_create((GThreadFunc)thread_get_cover_image, window, FALSE, NULL);
#endif
}

static void
xmr_window_login(XmrWindow *window)
{
#if GLIB_CHECK_VERSION(2, 32, 0)
	g_thread_new("login", (GThreadFunc)thread_login, window);
#else
	g_thread_create((GThreadFunc)thread_login, window, FALSE, NULL);
#endif
}

static void
xmr_window_set_track_info(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	SongInfo *song = (SongInfo *)priv->playlist->data;

	xmr_window_get_cover_image(window);

	gtk_label_set_text(GTK_LABEL(priv->labels[LABEL_SONG_NAME]), song->song_name);
	gtk_label_set_text(GTK_LABEL(priv->labels[LABEL_ARTIST]), song->artist_name);

	gtk_widget_set_tooltip_text(priv->image, song->album_name);
}

static void
update_radio_list(XmrWindow *window)
{
#if GLIB_CHECK_VERSION(2, 32, 0)
	g_thread_new("radio", (GThreadFunc)thread_update_radio_list, window);
#else
	g_thread_create((GThreadFunc)thread_update_radio_list, window, FALSE, NULL);
#endif
}

void
xmr_window_play(XmrWindow *window)
{
	XmrWindowPrivate *priv;

	g_return_if_fail(window != NULL);
	priv = window->priv;

	if (!xmr_player_playing(priv->player))
	{
		// playlist empty
		if (g_list_length(priv->playlist) == 0){
			xmr_window_get_playlist(window);
		}else{
			xmr_player_resume(priv->player);
		}
	}
}

void
xmr_window_play_next(XmrWindow *window)
{
	XmrWindowPrivate *priv;
	gpointer data;
	SongInfo *song;

	g_return_if_fail( window != NULL );
	priv = window->priv;

	// change to default cover image first
	if (priv->pb_cover)
		set_cover_image(window, priv->pb_cover);

	if (g_list_length(priv->playlist) == 0){
		goto no_more_track;
	}

	data = priv->playlist->data;

	// remove current song
	priv->playlist = g_list_remove(priv->playlist, data);
	song_info_free(data);

	if (g_list_length(priv->playlist) == 0){
		goto no_more_track;
	}

	song = (SongInfo *)priv->playlist->data;
	xmr_player_open(priv->player, song->location, NULL);
	xmr_player_play(priv->player);

	xmr_window_set_track_info(window);

	g_signal_emit(window, signals[TRACK_CHANGED], 0, song);

	// get new playlist if only one song in playlist
	if (g_list_length(priv->playlist) > 1)
	  return ;

no_more_track:
	xmr_window_get_playlist(window);
}

void
xmr_window_pause(XmrWindow *window)
{
	XmrWindowPrivate *priv;

	g_return_if_fail( window != NULL );
	priv = window->priv;

	xmr_player_pause(priv->player);
}

SongInfo *
xmr_window_get_current_song(XmrWindow *window)
{
	g_return_val_if_fail(window != NULL, NULL);

	if (g_list_length(window->priv->playlist) == 0)
		return NULL;

	return (SongInfo *)window->priv->playlist->data;
}

static void
create_popup_menu(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	GtkWidget *item = NULL;
	GtkAccelGroup* accel_group;

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

	priv->popup_menu = gtk_menu_new();
	priv->skin_menu = gtk_menu_new();

	item = gtk_radio_menu_item_new_with_mnemonic(priv->skin_item_group, _("_Gtk Theme"));
	priv->skin_item_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM(item));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->skin_menu), item);

	g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_activate), window);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Skin"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), priv->skin_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, accel_group);
	gtk_image_menu_item_set_accel_group(GTK_IMAGE_MENU_ITEM(item), accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_activate), window);

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_activate), window);

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_activate), window);

	gtk_widget_show_all(priv->popup_menu);
}

static void
on_menu_item_activate(GtkMenuItem *item, XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	const gchar *menu = gtk_menu_item_get_label(item);

	if (g_strcmp0(menu, _("_Gtk Theme")) == 0)
	{
		//set_gtk_theme(window);
	}
	else if(g_strcmp0(menu, GTK_STOCK_PREFERENCES) == 0)
	{
		static GtkWidget *pref_window = NULL;

		if (pref_window == NULL)
		{
			pref_window = (GtkWidget *)gtk_builder_get_object(priv->ui_pref, "window_pref");
			init_pref_window(window, pref_window);
		}

		gtk_widget_show_all(pref_window);
	}
	else if(g_strcmp0(menu, GTK_STOCK_ABOUT) == 0)
	{
		GtkBuilder *builder = NULL;
		static GtkWidget *dialog_about;

		if (dialog_about == NULL)
		{
			builder = create_builder_with_file(UIDIR"/about.ui");
			if (builder == NULL)
			{
				g_warning("Missing about.ui file!\n");
				return ;
			}

			dialog_about = GTK_WIDGET(gtk_builder_get_object(builder, "dialog_about"));

			gtk_window_set_transient_for(GTK_WINDOW(dialog_about), GTK_WINDOW(window));
			gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog_about), VERSION);

			g_object_unref(builder);
		}

		gtk_dialog_run(GTK_DIALOG(dialog_about));
		gtk_widget_hide(dialog_about);
	}
	else if(g_strcmp0(menu, GTK_STOCK_QUIT) == 0)
	{
		xmr_window_quit(window);
	}
}

static void
on_skin_menu_item_activate(GtkMenuItem *item, SkinInfo *skin)
{
	if (skin == NULL)
		return ;

	set_skin(skin->data, skin->file);
}

static void
load_settings(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	gchar *radio_name = NULL;
	gchar *radio_url = NULL;
	gint x = -1, y = -1;

	// call before load_skin
	priv->skin = xmr_settings_get_theme(priv->settings);

	load_skin(window);

	xmr_settings_get_radio(priv->settings, &radio_name, &radio_url);

	xmr_settings_get_window_pos(priv->settings, &x, &y);
	if (x !=- 1 && y != -1)
		gtk_window_move(GTK_WINDOW(window), x, y);

	xmr_settings_get_usr_info(priv->settings, &priv->usr, &priv->pwd);
	if (xmr_settings_get_auto_login(priv->settings))
	{
		if (priv->usr != NULL && priv->pwd != NULL &&
			*priv->usr != 0 && *priv->pwd != 0)
		{
			xmr_window_login(window);
		}
	}
	else
	{
		if (radio_url && *radio_url != 0)
		{
			priv->playlist_url = g_strdup(radio_url);
			gtk_label_set_text(GTK_LABEL(priv->labels[LABEL_RADIO]), radio_name);
		}
		else
		{
			priv->playlist_url = g_strdup(DEFAULT_RADIO_URL);
		}

		xmr_window_get_playlist(window);
	}

	load_radio(window);

	g_free(radio_name);
	g_free(radio_url);
}

static void
load_skin(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	gchar *skin_dir;

	// load user config location
	skin_dir = g_strdup_printf("%s/skin", xmr_config_dir());
	if (skin_dir)
	{
		list_file(skin_dir, FALSE, append_skin, &priv->skin_list);
		g_free(skin_dir);
	}

	// load install location
	list_file(SKINDIR, FALSE, append_skin, &priv->skin_list);
	
	// always set gtk theme first
	// set_gtk_theme(window);

	if (g_list_length(priv->skin_list) > 0)
	{
		GList *p = priv->skin_list;
		gboolean no_skin_match = TRUE;

		while(p)
		{
			GtkWidget *item;
			SkinInfo *skin_info = (SkinInfo *)p->data;
			skin_info->data = window;
			item = append_skin_to_menu(window, skin_info);

			if (no_skin_match && g_strcmp0(skin_info->name, priv->skin) == 0)
			{
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	
				set_skin(window, skin_info->file);

				no_skin_match = FALSE;
			}
			p = p->next;
		}

		if (no_skin_match)
		{
			set_skin(window, ((SkinInfo *)priv->skin_list->data)->file);
		}
	}
}

static void
load_radio(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	XmrDb *db = xmr_db_new();

	if (xmr_db_is_empty(db))
	{
		update_radio_list(window);
	}
	else
	{
		gint i;

		for(i=0; i<3; ++i)
		{
			GSList *list = NULL;
			GSList *p;

			list = xmr_db_get_radio_list(db, radio_style[i]);
			if (g_slist_length(list) == 0)
				continue ;

			p = list;
			while(p)
			{
				RadioInfo *radio_info = (RadioInfo *)p->data;

				XmrRadio *xmr_radio = xmr_radio_new_with_info(radio_info->logo,
							radio_info->name, radio_info->url);

				xmr_radio_chooser_append(XMR_RADIO_CHOOSER(priv->chooser[i]), xmr_radio);

				p = p->next;
			}

			g_slist_free_full(list, (GDestroyNotify)radio_info_free);
		}
	}

	g_object_unref(db);
}

static void
append_skin(const gchar *skin,
			gpointer data)
{
	GList **list = (GList **)data;
	SkinInfo *skin_info = xmr_skin_info_new();

	g_return_if_fail(skin_info != NULL);

	XmrSkin *xmr_skin = xmr_skin_new();

	do
	{
		if (!xmr_skin_load(xmr_skin, skin))
		{
			xmr_debug("Failed to load skin: %s", skin);
			break;
		}

		xmr_skin_get_info(xmr_skin, skin_info);

		*list = g_list_append(*list, skin_info);
	}
	while(0);

	g_object_unref(xmr_skin);
}

static GtkWidget *
append_skin_to_menu(XmrWindow *window, SkinInfo *info)
{
	XmrWindowPrivate *priv = window->priv;
	GtkWidget *item = NULL;

	if (info->name == NULL)
	{
		gchar *name = g_path_get_basename(info->file);
		if (name == NULL)
			return NULL;
		item = gtk_radio_menu_item_new_with_label(priv->skin_item_group, name);
		g_free(name);
	}
	else
	{
		item = gtk_radio_menu_item_new_with_label(priv->skin_item_group, info->name);
	}

	gtk_widget_show(item);
	priv->skin_item_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));

	gtk_menu_shell_append(GTK_MENU_SHELL(priv->skin_menu), item);

	g_signal_connect(item, "activate", G_CALLBACK(on_skin_menu_item_activate), info);

	return item;
}

static gint
hex_to_int(gchar ch)
{
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;

	return ch - '0';
}

static void
hex_color_to_rgba(const gchar *hex_color,
			GdkRGBA *rgba)
{
	gfloat value = 0;
	const gchar *p = hex_color;

	p++;	// skip '#'
	// r
	value = hex_to_int(*p++) * 16 + hex_to_int(*p++);
	rgba->red = value/255.0;

	// g
	value = hex_to_int(*p++) * 16 + hex_to_int(*p++);
	rgba->green = value/255.0;

	// b
	value = hex_to_int(*p++) * 16 + hex_to_int(*p);
	rgba->blue = value/255.0;

	// currently just ignore transparency
	rgba->alpha = 1.0;
}

static void
set_widget_fg_color(GtkWidget *widget,
			const gchar *color_str)
{
	GdkRGBA rgba = { 0 };
	gint COLOR_LEN = strlen("#FFFFFF");

	if (color_str == NULL ||
		strlen(color_str) != COLOR_LEN) // set to black color
	{
		rgba.alpha = 1.0;
	}
	else
	{
		hex_color_to_rgba(color_str, &rgba);
	}

	gtk_widget_override_color(widget,
				GTK_STATE_FLAG_NORMAL,
				&rgba);
}

static void
set_widget_font(GtkWidget *widget,
			const gchar *font_desc)
{
	PangoFontDescription * pfd;
	if (font_desc == NULL){
		pfd = pango_font_description_from_string("Sans 10");
	}else{
		pfd = pango_font_description_from_string(font_desc);
	}

	if (pfd)
	{
		gtk_widget_modify_font(widget, pfd);

		pango_font_description_free(pfd);
	}
}

static GtkBuilder *
create_builder_with_file(const gchar *file)
{
	GtkBuilder *builder = NULL;

	builder = gtk_builder_new();
	g_return_val_if_fail(builder != NULL, NULL);

	gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_file(builder, file, NULL);
	gtk_builder_connect_signals(builder, NULL);

	return builder;
}

static void
init_pref_window(XmrWindow *window,
			GtkWidget *pref)
{
	XmrWindowPrivate *priv = window->priv;
	GtkWidget *notebook;
	GtkWidget *widget;

	notebook = GTK_WIDGET(gtk_builder_get_object(priv->ui_pref, "nb_pref"));
	g_return_if_fail(notebook != NULL);

	widget = GTK_WIDGET(gtk_builder_get_object(priv->ui_pref, "box_skin"));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
				gtk_label_new(_("Skin")));

	widget = GTK_WIDGET(gtk_builder_get_object(priv->ui_pref, "grid_radio"));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
				gtk_label_new(_("Radio")));

	widget = peas_gtk_plugin_manager_new(NULL);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
				gtk_label_new(_("Plugins")));
}

static void
do_login(XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;
	static GtkWidget *dialog_login;

	if (dialog_login == NULL)
	{
		GtkWidget *button_login;
		GtkWidget *entry_usr, *entry_pwd;
		GtkWidget *checkbox;

		window->priv->ui_login = create_builder_with_file(UIDIR"/login.ui");
		g_return_if_fail(window->priv->ui_login != NULL);

		dialog_login = GTK_WIDGET(gtk_builder_get_object(window->priv->ui_login, "dialog_login"));
		gtk_window_set_transient_for(GTK_WINDOW(dialog_login), GTK_WINDOW(window));

		button_login = GTK_WIDGET(gtk_builder_get_object(window->priv->ui_login, "button_login"));

		g_signal_connect(button_login, "button-release-event",
					G_CALLBACK(on_login_dialog_button_login_clicked), window);

		entry_usr = GTK_WIDGET(gtk_builder_get_object(priv->ui_login, "entry_usr"));
		entry_pwd = GTK_WIDGET(gtk_builder_get_object(priv->ui_login, "entry_pwd"));
		checkbox = GTK_WIDGET(gtk_builder_get_object(priv->ui_login, "cb_auto_login"));

		gtk_entry_set_text(GTK_ENTRY(entry_usr), priv->usr);
		gtk_entry_set_text(GTK_ENTRY(entry_pwd), priv->pwd);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
					xmr_settings_get_auto_login(priv->settings)
					);

	}

	gtk_dialog_run(GTK_DIALOG(dialog_login));
	gtk_widget_hide(dialog_login);
}

static void
change_radio(XmrWindow *window,
			const gchar *name,
			const gchar *url)
{
	XmrWindowPrivate *priv = window->priv;

	if (xmr_player_playing(priv->player))
		xmr_player_pause(priv->player);

	g_list_free_full(priv->playlist, (GDestroyNotify)song_info_free);
	priv->playlist = NULL;

	g_free(priv->playlist_url);

	priv->playlist_url = (url == NULL ? NULL : g_strdup(url));

	gtk_label_set_text(GTK_LABEL(priv->labels[LABEL_RADIO]), name);
	xmr_window_get_playlist(window);

	xmr_settings_set_radio(priv->settings, name, (url == NULL ? "" : url));

	g_signal_emit(window, signals[RADIO_CHANGED], 0, name, url);
}

static gboolean
on_login_dialog_button_login_clicked(GtkButton *button,
			GdkEvent  *event,
			XmrWindow *window)
{
	XmrWindowPrivate *priv = window->priv;

	static GtkWidget *entry_usr, *entry_pwd;
	static GtkWidget *checkbox;

	const gchar *usr, *pwd;

	if (entry_usr == NULL)
	{
		entry_usr = GTK_WIDGET(gtk_builder_get_object(priv->ui_login, "entry_usr"));
		entry_pwd = GTK_WIDGET(gtk_builder_get_object(priv->ui_login, "entry_pwd"));
		checkbox = GTK_WIDGET(gtk_builder_get_object(priv->ui_login, "cb_auto_login"));
	}

	usr = gtk_entry_get_text(GTK_ENTRY(entry_usr));
	pwd = gtk_entry_get_text(GTK_ENTRY(entry_pwd));

	if (is_text_empty(usr) || is_text_empty(pwd))
	{
		xmr_message(GTK_WIDGET(window),
					_("User name and password may not empty"),
					_("Login"));
		return TRUE;
	}

	g_free(priv->usr);
	g_free(priv->pwd);

	priv->usr = g_strdup(usr);
	priv->pwd = g_strdup(pwd);

	xmr_settings_set_auto_login(priv->settings,
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox))
				);

	xmr_window_login(window);

	return FALSE;
}

static gboolean
is_text_empty(const gchar *text)
{
	const gchar *p;

	if (text == NULL)
		return TRUE;

	p = text;

	while(p && *p)
	{
		if (*p != ' '){
			return FALSE;
		}
		p++;
	}

	return TRUE;
}

static gchar *
radio_logo_url_to_uri(RadioInfo *radio)
{
	gchar *uri = NULL;

	g_return_val_if_fail(radio != NULL, NULL);

	uri = g_strdup_printf("%s/%s",
				xmr_radio_icon_dir(),
				g_path_get_basename(radio->logo)
				);

	return uri;
}

static gboolean
on_delete_event(XmrWindow *window,
			GdkEvent *event,
			gpointer data)
{
	xmr_window_quit(window);

	return TRUE;
}

void
xmr_window_quit(XmrWindow *window)
{
	gint x, y;

	gtk_window_get_position(GTK_WINDOW(window), &x, &y);

	xmr_settings_set_window_pos(window->priv->settings, x, y);

	gtk_widget_destroy(GTK_WIDGET(window));
}

static void
on_extension_added(PeasExtensionSet *set,
		    PeasPluginInfo   *info,
		    PeasActivatable  *activatable,
		    gpointer data)
{
	peas_activatable_activate(activatable);
}

static void
on_extension_removed(PeasExtensionSet *set,
		      PeasPluginInfo   *info,
		      PeasActivatable  *activatable,
		      gpointer data)
{
	peas_activatable_deactivate(activatable);
}
