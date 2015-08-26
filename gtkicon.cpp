#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <gtkmm/action.h>
#include <gtkmm/actiongroup.h>
#include <gtkmm/window.h>
#include <gtkmm/button.h>
#include <gtkmm/treeview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/notebook.h>
#include <gtkmm/main.h>
#include <gtkmm/box.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/stock.h>
#include <gtkmm/image.h>
#include <glibmm/markup.h>
#include <gtkmm/statusicon.h>
#include <gtkmm/uimanager.h>
#include <stdint.h>
#include <dbus/dbus.h>
#include <locale.h>
#include <libintl.h>

#include <vector>
#include <map>
#include <set>
#include <memory>

#define DOMAIN_PREFIX "com.funcoil."

#define _(str) gettext(str)

using namespace Gtk;
using namespace std;
using namespace Glib;

#define LOAD_ICON(PATH) Gdk::Pixbuf::create_from_file(PATH)

class GtkTrayIcon {
	private:
		string mScriptPath;
		Glib::RefPtr<Gtk::StatusIcon> mIcon;
	protected:
		void on_icon_activate() {
			system(mScriptPath.c_str()); // Maybe unsafe?
		}
	public:
		GtkTrayIcon(const string & imagePath, const string &script) : mScriptPath(script) {
			Glib::RefPtr<Gdk::Pixbuf> iconImage = LOAD_ICON(imagePath);

			mIcon = StatusIcon::create(iconImage);
			mIcon->signal_activate().connect(sigc::mem_fun(*this, &GtkTrayIcon::on_icon_activate));
			mIcon->set_visible();
		}

		void changeIcon(const string &path) {
			try {
				Glib::RefPtr<Gdk::Pixbuf> newIcon = LOAD_ICON(path);
				mIcon->set(newIcon);
			} catch(...) { // Just ignore incorrect path
			}
		}

		void changeScript(const string &path) {
			mScriptPath = path;
		}

};

class AppControl {
	public:
		GtkTrayIcon &icon;
		bool running;

		AppControl(GtkTrayIcon &icon) : icon(icon), running(true) {}
};

static DBusHandlerResult
filter_func (DBusConnection *connection,
             DBusMessage *message,
             void *user_data)
{
	(void)connection;
	AppControl &appCtrl(*(AppControl *)user_data);
	dbus_bool_t handled = FALSE;
	char *path = NULL;
	DBusError dberr;
	dbus_error_init (&dberr);

	if (dbus_message_is_signal (message, DOMAIN_PREFIX "gtkicon", "changeIcon")) {
		dbus_message_get_args (message, &dberr, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID);
		if (dbus_error_is_set (&dberr)) {
			fprintf (stderr, _("Error getting message args: %s"), dberr.message);
		} else {
			appCtrl.icon.changeIcon(path);

			handled = TRUE;
		}
	} else if(dbus_message_is_signal (message, DOMAIN_PREFIX "gtkicon", "changeScript")) {
		dbus_message_get_args (message, &dberr, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID);
		if (dbus_error_is_set (&dberr)) {
			fprintf (stderr, _("Error getting message args: %s"), dberr.message);
		} else {
			appCtrl.icon.changeScript(path);

			handled = TRUE;
		}
	} else if(dbus_message_is_signal (message, DOMAIN_PREFIX "gtkicon", "quit")) {
		dbus_message_get_args (message, &dberr, DBUS_TYPE_INVALID);

		if (dbus_error_is_set (&dberr)) {
			fprintf (stderr, _("Error getting message args: %s"), dberr.message);
		} else {
			appCtrl.running = false;
			handled = TRUE;
		}
	}

	dbus_error_free (&dberr);

	return (handled ? DBUS_HANDLER_RESULT_HANDLED : DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
}

int main(int argc, char *argv[]) {
	if(argc < 3) {
		fprintf(stderr, "Missing arguments\nUsage: %s ICON SCRIPT\n", argv[0]);
		return 2;
	}

	Main app(argc, argv, DOMAIN_PREFIX "gtkicon");

	DBusError dberr;
	DBusConnection *dbconn;

	GtkTrayIcon icon(argv[1], argv[2]);
	AppControl appctrl(icon);

	dbus_error_init (&dberr);
	dbconn = dbus_bus_get (DBUS_BUS_SESSION, &dberr);
	if (dbus_error_is_set (&dberr)) {
		fprintf (stderr, _("getting session bus failed: %s\n"), dberr.message);
		dbus_error_free (&dberr);
		return 1;
	}

	dbus_bus_request_name (dbconn, DOMAIN_PREFIX "gtkicon",
	                       DBUS_NAME_FLAG_REPLACE_EXISTING, &dberr);
	if (dbus_error_is_set (&dberr)) {
		fprintf (stderr, _("requesting name failed: %s\n"), dberr.message);
		dbus_error_free (&dberr);
		return 1;
	}

	if (!dbus_connection_add_filter (dbconn, filter_func, &appctrl, NULL))
		return 1;

	dbus_bus_add_match (dbconn,
	                    "type='signal',interface='" DOMAIN_PREFIX "gtkicon'",
	                    &dberr);

	if (dbus_error_is_set (&dberr)) {
		fprintf (stderr, _("Could not match: %s"), dberr.message);
		dbus_error_free (&dberr);
		return 1;
	}

	while(appctrl.running) {
		app.iteration(false);
		dbus_connection_read_write_dispatch (dbconn, 10);
	}

	return 0;
}
