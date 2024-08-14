#include "config.h"

#include "lenovo-vantage-application.h"
#include "lenovo-vantage-window.h"

struct _LenovoVantageApplication
{
	AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE(LenovoVantageApplication, lenovo_vantage_application, ADW_TYPE_APPLICATION)

LenovoVantageApplication *
lenovo_vantage_application_new(const char *application_id,
							   GApplicationFlags flags)
{
	g_return_val_if_fail(application_id != NULL, NULL);

	return g_object_new(LENOVO_VANTAGE_TYPE_APPLICATION,
						"application-id", application_id,
						"flags", flags,
						NULL);
}

static void
lenovo_vantage_application_activate(GApplication *app)
{
	GtkWindow *window;

	g_assert(LENOVO_VANTAGE_IS_APPLICATION(app));

	window = gtk_application_get_active_window(GTK_APPLICATION(app));

	if (window == NULL)
		window = g_object_new(LENOVO_VANTAGE_TYPE_WINDOW,
							  "application", app,
							  NULL);

	gtk_window_present(window);
}

static void
lenovo_vantage_application_class_init(LenovoVantageApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

	app_class->activate = lenovo_vantage_application_activate;
}

static void
lenovo_vantage_application_about_action(GSimpleAction *action,
										GVariant *parameter,
										gpointer user_data)
{
	static const char *developers[] = {"Kemal Hafizoglu", NULL};
	static const char *designers[] = {"Kemal Hafizoglu", NULL};
	// static const char *translators = "Kemal Hafızoğlu English , Türkçe";
	LenovoVantageApplication *self = user_data;
	GtkWindow *window = NULL;

	g_assert(LENOVO_VANTAGE_IS_APPLICATION(self));
	window = gtk_application_get_active_window(GTK_APPLICATION(self));

	adw_show_about_window(window,
						  "application-name", "Lenovo Vantage",
						  "application-icon", "me.bykemalh.vantage",
						  "developer-name", "Kemal Hafizoglu",
						  "version", "1.0.1",
						  "developers", developers,
						  "designers", designers,
						  // "translator-credits", translators,
						  "copyright", "© 2024 Kemal Hafizoglu",
						  "website", "https://bykemalh.me",
						  "issue-url", "https://github.com/bykemalh/lenovo-vantage",
						  "license-type", GTK_LICENSE_GPL_3_0,
						  NULL);
}
static void
lenovo_vantage_application_quit_action(GSimpleAction *action,
									   GVariant *parameter,
									   gpointer user_data)
{
	LenovoVantageApplication *self = user_data;

	g_assert(LENOVO_VANTAGE_IS_APPLICATION(self));

	g_application_quit(G_APPLICATION(self));
}

static const GActionEntry app_actions[] = {
	{"quit", lenovo_vantage_application_quit_action},
	{"about", lenovo_vantage_application_about_action},
};

static void
lenovo_vantage_application_init(LenovoVantageApplication *self)
{
	g_action_map_add_action_entries(G_ACTION_MAP(self),
									app_actions,
									G_N_ELEMENTS(app_actions),
									self);
	gtk_application_set_accels_for_action(GTK_APPLICATION(self),
										  "app.quit",
										  (const char *[]){"<primary>q", NULL});
}
