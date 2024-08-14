#ifndef LENOVO_VANTAGE_WINDOW_H
#define LENOVO_VANTAGE_WINDOW_H

#include <adwaita.h>

G_BEGIN_DECLS

#define LENOVO_VANTAGE_TYPE_WINDOW (lenovo_vantage_window_get_type())
G_DECLARE_FINAL_TYPE(LenovoVantageWindow, lenovo_vantage_window, LENOVO_VANTAGE, WINDOW, AdwApplicationWindow)

struct _LenovoVantageWindow
{
    AdwApplicationWindow parent_instance;

	    /* Template widgets */
	    AdwHeaderBar *header_bar;
	    GtkLabel *label;
	    AdwPreferencesGroup *laptop_info_group;
	    AdwActionRow *serialn_row;
	    AdwActionRow *modeln_row;
	    AdwActionRow *biosv_row;

	    GtkLevelBar *battery_level_bar;
	    GtkLabel *battery_label;
	    guint timer_id;
	    GtkLabel *cpu_label;
	    GtkLevelBar *cpu_level_bar;
	    GtkLabel *memory_label;
	    GtkLevelBar *memory_level_bar;

	    GtkButton *copy_serial_btn;
	    GtkButton *show_serial_btn;
	    AdwToastOverlay *toast_overlay;
	    GtkButton *copy_model_btn;
            GtkButton *copy_bios_btn;

	    GtkCheckButton *radio1;
	    GtkCheckButton *radio2;
	    GtkCheckButton *radio3;

	    AdwActionRow *row1;
	    AdwActionRow *row2;
	    AdwActionRow *row3;

	    GtkCheckButton *radio4;
	    GtkCheckButton *radio5;
	    GtkCheckButton *radio6;

	    AdwActionRow *row4;
	    AdwActionRow *row5;
	    AdwActionRow *row6;

	    AdwActionRow *wifi_row;
	    GtkSwitch *wifi_switch;
	    AdwActionRow *mic_row;
	    GtkSwitch *mic_switch;
	    AdwActionRow *conservation_mode_row;
	    GtkSwitch *conservation_mode_switch;
	    AdwActionRow *always_on_usb_row;
	    GtkSwitch *always_on_usb_switch;
	    AdwActionRow *fn_lock_row;
	    GtkSwitch *fn_lock_switch;
	    AdwActionRow *touchpad_row;
	    GtkSwitch *touchpad_switch;
	    AdwActionRow *camera_row;
	    GtkSwitch *camera_switch;

	    AdwActionRow *os_row;
	    AdwActionRow *host_row;
	    AdwActionRow *kernel_row;
	    AdwActionRow *uptime_row;
	    AdwActionRow *package_row;
	    AdwActionRow *shell_row;
	    AdwActionRow *res_row;
	    AdwActionRow *de_row;
	    AdwActionRow *wm_row;
	    AdwActionRow *cpu_row;
	    AdwActionRow *gpu_row;
	    AdwActionRow *memory_row;

	GtkMenuButton *menu_button;

	GtkWidget *nav_view;
	AdwNavigationView *settings_navigation_view;
    	GtkButton *system_update_button;
};

G_END_DECLS

#endif
