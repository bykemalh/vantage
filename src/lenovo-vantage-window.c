// window.c
#include "config.h"
#include <X11/Xlib.h>
#include "lenovo-vantage-window.h"
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gspawn.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libintl.h>
#include <locale.h>

G_DEFINE_FINAL_TYPE(LenovoVantageWindow, lenovo_vantage_window, ADW_TYPE_APPLICATION_WINDOW)

#define CONSERVATION_MODE_PATH "/sys/bus/platform/drivers/ideapad_acpi/VPC2004:00/conservation_mode"
#define USB_CHARGING_PATH "/sys/bus/platform/drivers/ideapad_acpi/VPC2004:00/usb_charging"
#define FN_LOCK_PATH "/sys/bus/platform/drivers/ideapad_acpi/VPC2004:00/fn_lock"

#define MAX_LENGTH 256
#define UPDATE_INTERVAL 1000

static char*
read_sysfs_file(const char* path) {
    FILE* file;
    char* line;
    size_t len;

    file = fopen(path, "r");
    if (!file) {
        return strdup("Unknown");
    }

    line = malloc(MAX_LENGTH);
    if (line == NULL) {
        fclose(file);
        return strdup("Unknown");
    }

    if (fgets(line, MAX_LENGTH, file) == NULL) {
        free(line);
        fclose(file);
        return strdup("Unknown");
    }

    fclose(file);

    len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
        line[len-1] = '\0';
    }

    return line;
}

static gboolean
update_battery_info(gpointer user_data) {
    LenovoVantageWindow *self = LENOVO_VANTAGE_WINDOW(user_data);
    char* capacity_str = read_sysfs_file("/sys/class/power_supply/BAT0/capacity");
    char* status = read_sysfs_file("/sys/class/power_supply/BAT0/status");
    int capacity = atoi(capacity_str);

    char battery_label[MAX_LENGTH];
    snprintf(battery_label, MAX_LENGTH, "%d%% (%s)", capacity, status);

    // UI'ı güncelle
    gtk_level_bar_set_value(self->battery_level_bar, (double)capacity / 100.0);
    gtk_label_set_text(self->battery_label, battery_label);

    free(capacity_str);
    free(status);

    return G_SOURCE_CONTINUE;
}

static void
on_show_serial_clicked(GtkButton *button, gpointer user_data)
{
    LenovoVantageWindow *self = LENOVO_VANTAGE_WINDOW(user_data);
    gchar *stdout_buf = NULL;
    gchar *stderr_buf = NULL;
    gint exit_status;
    GError *error = NULL;

    // sudo komutuyla DMI bilgisini okuyan bir betik çalıştır
    if (g_spawn_command_line_sync("pkexec sh -c 'dmidecode -s system-serial-number'",
                                  &stdout_buf, &stderr_buf, &exit_status, &error)) {
        if (exit_status == 0) {
            // Çıktıdaki boşlukları temizle
            g_strstrip(stdout_buf);
            // serialn_row'un alt yazısını güncelle
            adw_action_row_set_subtitle(self->serialn_row, stdout_buf);
        } else {
            g_warning("Command failed: %s", stderr_buf);
            adw_action_row_set_subtitle(self->serialn_row, "Failed to retrieve serial number");
        }
    } else {
        g_warning("Failed to execute command: %s", error->message);
        adw_action_row_set_subtitle(self->serialn_row, "Failed to retrieve serial number");
        g_error_free(error);
    }

    g_free(stdout_buf);
    g_free(stderr_buf);
}

static void
on_copy_serial_clicked(GtkButton *button, gpointer user_data)
{
    LenovoVantageWindow *self = LENOVO_VANTAGE_WINDOW(user_data);
    const gchar *serial_number = adw_action_row_get_subtitle(self->serialn_row);

    if (serial_number && *serial_number && strcmp(serial_number, "Unknown") != 0) {
        GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(self));
        gdk_clipboard_set_text(clipboard, serial_number);

        adw_toast_overlay_add_toast(self->toast_overlay,
            adw_toast_new("Serial number copied to clipboard"));
    } else {
        adw_toast_overlay_add_toast(self->toast_overlay,
            adw_toast_new("No valid serial number available to copy"));
    }
}

static void
on_copy_model_clicked(GtkButton *button, gpointer user_data)
{
    LenovoVantageWindow *self = LENOVO_VANTAGE_WINDOW(user_data);
    const gchar *model_number = adw_action_row_get_subtitle(self->modeln_row);
    if (model_number && *model_number && strcmp(model_number, "Unknown") != 0) {
        GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(self));
        gdk_clipboard_set_text(clipboard, model_number);
        adw_toast_overlay_add_toast(self->toast_overlay,
            adw_toast_new("Model number copied to clipboard"));
    } else {
        adw_toast_overlay_add_toast(self->toast_overlay,
            adw_toast_new("No valid model number available to copy"));
    }
}

static void
on_copy_bios_clicked(GtkButton *button, gpointer user_data)
{
    LenovoVantageWindow *self = LENOVO_VANTAGE_WINDOW(user_data);
    const gchar *bios_version = adw_action_row_get_subtitle(self->biosv_row);
    if (bios_version && *bios_version && strcmp(bios_version, "Unknown") != 0) {
        GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(self));
        gdk_clipboard_set_text(clipboard, bios_version);
        adw_toast_overlay_add_toast(self->toast_overlay,
            adw_toast_new("BIOS version copied to clipboard"));
    } else {
        adw_toast_overlay_add_toast(self->toast_overlay,
            adw_toast_new("No valid BIOS version available to copy"));
    }
}

static void
get_laptop_info(LenovoVantageWindow *self) {
    char* product_name = read_sysfs_file("/sys/class/dmi/id/product_name");
    char* serial_number = read_sysfs_file("/sys/class/dmi/id/product_serial");
    char* product_version = read_sysfs_file("/sys/class/dmi/id/product_version");
    char* bios_version = read_sysfs_file("/sys/class/dmi/id/bios_version");

    if (self->laptop_info_group) {
        adw_preferences_group_set_title(self->laptop_info_group, product_version);
    }
    if (self->serialn_row) {
        adw_action_row_set_subtitle(self->serialn_row, serial_number);
    }
    if (self->modeln_row) {
        adw_action_row_set_subtitle(self->modeln_row, product_name);
    }
    if (self->biosv_row) {
        adw_action_row_set_subtitle(self->biosv_row, bios_version);
    }

    free(product_name);
    free(serial_number);
    free(product_version);
    free(bios_version);
}

static char*
get_shell(void) {
    return g_strdup(getenv("SHELL"));
}

static char*
get_de(void) {
    const char* de = g_getenv("XDG_CURRENT_DESKTOP");
    return de ? g_strdup(de) : g_strdup("Unknown");
}

static char*
get_wm(void) {
    const char* wayland_display = g_getenv("WAYLAND_DISPLAY");
    const char* x11_display = g_getenv("DISPLAY");

    if (wayland_display) {
        return g_strdup("Wayland");
    } else if (x11_display) {
        return g_strdup("X11");
    } else {
        return g_strdup("Unknown");
    }
}

static char*
get_resolution(void) {
    GdkDisplay* display = gdk_display_get_default();
    if (display) {
        GListModel* monitors = gdk_display_get_monitors(display);
        if (monitors) {
            GdkMonitor* monitor = g_list_model_get_item(monitors, 0);
            if (monitor) {
                GdkRectangle geometry;
                gdk_monitor_get_geometry(monitor, &geometry);
                g_object_unref(monitor);
                return g_strdup_printf("%dx%d", geometry.width, geometry.height);
            }
        }
    }
    return g_strdup("Unknown");
}

static char*
get_cpu_info(void) {
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    char line[256];
    char* cpu_model = NULL;
    if (cpuinfo) {
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    cpu_model = g_strdup(g_strstrip(colon + 1));
                    break;
                }
            }
        }
        fclose(cpuinfo);
    }
    return cpu_model ? cpu_model : g_strdup("Unknown");
}

static char*
get_gpu_info(void) {
    FILE* fp;
    char buffer[256];
    char* result = NULL;

    fp = popen("lspci | grep -i vga", "r");
    if (fp == NULL) return g_strdup("Unknown");

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        char* colon = strchr(buffer, ':');
        if (colon) {
            result = g_strdup(g_strstrip(colon + 1));
        }
    }
    pclose(fp);
    return result ? result : g_strdup("Unknown");
}

static char*
get_uptime(void) {
    struct sysinfo s_info;
    int error;
    long uptime;
    int days, hours, mins;

    error = sysinfo(&s_info);
    if(error != 0) {
        return g_strdup("Unknown");
    }
    uptime = s_info.uptime;
    days = uptime / (60*60*24);
    hours = (uptime % (60*60*24)) / (60*60);
    mins = (uptime % (60*60)) / 60;
    return g_strdup_printf("%d days, %d hours, %d minutes", days, hours, mins);
}

static char*
get_package_count(void) {
    FILE* fp;
    char buffer[256];
    char* result = NULL;
    const char* commands[] = {
        "cat /etc/os-release | grep '^ID=' | cut -d'=' -f2",
        "lsb_release -si",
        "cat /etc/issue | cut -d' ' -f1",
        NULL
    };
    char distro[64] = {0};
    char command[256];
    int i;

    // Dağıtımı belirleme
    for (i = 0; commands[i] != NULL; i++) {
        fp = popen(commands[i], "r");
        if (fp && fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0; // Yeni satır karakterini kaldır
            strncpy(distro, buffer, sizeof(distro) - 1);
            pclose(fp);
            break;
        }
        if (fp) pclose(fp);
    }

    // Dağıtıma özgü paket sayma komutu
    if (strcasecmp(distro, "fedora") == 0 || strcasecmp(distro, "rhel") == 0 || strcasecmp(distro, "centos") == 0) {
        snprintf(command, sizeof(command), "dnf list installed | wc -l");
    } else if (strcasecmp(distro, "ubuntu") == 0 || strcasecmp(distro, "debian") == 0) {
        snprintf(command, sizeof(command), "dpkg --get-selections | wc -l");
    } else if (strcasecmp(distro, "arch") == 0 || strcasecmp(distro, "manjaro") == 0) {
        snprintf(command, sizeof(command), "pacman -Q | wc -l");
    } else if (strcasecmp(distro, "opensuse") == 0 || strcasecmp(distro, "suse") == 0) {
        snprintf(command, sizeof(command), "zypper search --installed-only | wc -l");
    } else {
        // Desteklenmeyen dağıtım
        return strdup("Unknown");
    }

    fp = popen(command, "r");
    if (fp == NULL) {
        return strdup("Unknown");
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0; // Yeni satır karakterini kaldır
        result = strdup(buffer);
    }
    pclose(fp);

    return result ? result : strdup("Unknown");
}

static char*
get_distro_info(void) {
    FILE* fp;
    char buffer[256];
    char* result = NULL;
    char* start;
    char* end;

    // Önce host sistemdeki /etc/os-release dosyasını kontrol et
    fp = fopen("/run/host/etc/os-release", "r");
    if (fp != NULL) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strncmp(buffer, "PRETTY_NAME=", 12) == 0) {
                start = strchr(buffer, '"');
                if (start) {
                    end = strrchr(start + 1, '"');
                    if (end) {
                        *end = '\0';
                        result = g_strdup(start + 1);
                        break;
                    }
                }
            }
        }
        fclose(fp);
    }

    // Eğer dosyadan okuyamazsak, hostnamectl ile host sistemde komut çalıştır
    if (result == NULL) {
        fp = popen("flatpak-spawn --host hostnamectl | grep 'Operating System'", "r");
        if (fp != NULL) {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                start = strchr(buffer, ':');
                if (start) {
                    start += 2; // ':' ve boşluktan sonrası
                    end = strchr(start, '\n');
                    if (end) {
                        *end = '\0';
                        result = g_strdup(start);
                    }
                }
            }
            pclose(fp);
        }
    }

    // Hala bulunamadıysa, "Unknown" döndür
    if (result == NULL) {
        result = g_strdup("Unknown");
    }

    return result;
}
static double
get_cpu_usage(void)
{
    static long long prev_total = 0, prev_idle = 0;
    long long total = 0, idle = 0;
    double usage;
    FILE* file;
    char buffer[1024];
    long long user, nice, system, idle_time, iowait, irq, softirq;
    long long total_diff, idle_diff;

    file = fopen("/proc/stat", "r");
    if (file == NULL) {
        return 0.0;
    }

    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        sscanf(buffer, "cpu %lld %lld %lld %lld %lld %lld %lld",
               &user, &nice, &system, &idle_time, &iowait, &irq, &softirq);

        idle = idle_time + iowait;
        total = user + nice + system + idle + irq + softirq;
    }
    fclose(file);

    total_diff = total - prev_total;
    idle_diff = idle - prev_idle;

    usage = (total_diff - idle_diff) / (double)total_diff;

    prev_total = total;
    prev_idle = idle;

    return usage;
}

static double
get_memory_usage(void)
{
    long total_mem = 0, free_mem = 0, buffers = 0, cached = 0;
    double usage;
    FILE* file;
    char buffer[1024];
    long used_mem;

    file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        return 0.0;
    }

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (sscanf(buffer, "MemTotal: %ld kB", &total_mem) == 1) continue;
        if (sscanf(buffer, "MemFree: %ld kB", &free_mem) == 1) continue;
        if (sscanf(buffer, "Buffers: %ld kB", &buffers) == 1) continue;
        if (sscanf(buffer, "Cached: %ld kB", &cached) == 1) continue;
    }
    fclose(file);

    used_mem = total_mem - free_mem - buffers - cached;
    usage = (double)used_mem / total_mem;

    return usage;
}

static gboolean
update_system_info(gpointer user_data)
{
    LenovoVantageWindow *self = LENOVO_VANTAGE_WINDOW(user_data);

    double cpu_usage = get_cpu_usage();
    double memory_usage = get_memory_usage();

    char cpu_text[32], memory_text[32];
    snprintf(cpu_text, sizeof(cpu_text), "CPU: %.1f%%", cpu_usage * 100);
    snprintf(memory_text, sizeof(memory_text), "Memory: %.1f%%", memory_usage * 100);

    gtk_label_set_text(self->cpu_label, cpu_text);
    gtk_level_bar_set_value(self->cpu_level_bar, cpu_usage);

    gtk_label_set_text(self->memory_label, memory_text);
    gtk_level_bar_set_value(self->memory_level_bar, memory_usage);

    return G_SOURCE_CONTINUE;
}

static void
get_system_info (LenovoVantageWindow *self)
{
  struct utsname uname_data;
  FILE *meminfo;
  char line[256];
  char* distro_info = get_distro_info();
  char hostname[256];
  unsigned long total_memory = 0;
  char *os_info, *kernel_info, *host_info, *memory_info;
  char *shell_info, *de_info, *wm_info, *res_info, *cpu_info, *gpu_info, *uptime_info, *package_info;

  g_return_if_fail (LENOVO_VANTAGE_IS_WINDOW (self));

  // Kernel ve host bilgilerini al
  uname(&uname_data);
  gethostname(hostname, sizeof(hostname));


  // Bellek bilgisini al
  meminfo = fopen("/proc/meminfo", "r");
  if (meminfo) {
    while (fgets(line, sizeof(line), meminfo)) {
      if (sscanf(line, "MemTotal: %lu kB", &total_memory) == 1) {
        break;
      }
    }
    fclose(meminfo);
  }

  // Bilgileri güncelle
  os_info = g_strdup_printf("%s", distro_info);
  kernel_info = g_strdup_printf("%s", uname_data.release);
  host_info = g_strdup_printf("%s", hostname);
  memory_info = g_strdup_printf("%.1f GB", (float)total_memory / (1024 * 1024));
  shell_info = get_shell();
  de_info = get_de();
  wm_info = get_wm();
  res_info = get_resolution();
  cpu_info = get_cpu_info();
  gpu_info = get_gpu_info();
  uptime_info = get_uptime();
  package_info = get_package_count();

  adw_action_row_set_subtitle(self->os_row, os_info);
  adw_action_row_set_subtitle(self->kernel_row, kernel_info);
  adw_action_row_set_subtitle(self->host_row, host_info);
  adw_action_row_set_subtitle(self->memory_row, memory_info);
  adw_action_row_set_subtitle(self->shell_row, shell_info);
  adw_action_row_set_subtitle(self->de_row, de_info);
  adw_action_row_set_subtitle(self->wm_row, wm_info);
  adw_action_row_set_subtitle(self->res_row, res_info);
  adw_action_row_set_subtitle(self->cpu_row, cpu_info);
  adw_action_row_set_subtitle(self->gpu_row, gpu_info);
  adw_action_row_set_subtitle(self->uptime_row, uptime_info);
  adw_action_row_set_subtitle(self->package_row, package_info);

  g_free(os_info);
  g_free(kernel_info);
  g_free(host_info);
  g_free(memory_info);
  g_free(shell_info);
  g_free(de_info);
  g_free(wm_info);
  g_free(res_info);
  g_free(cpu_info);
  g_free(gpu_info);
  g_free(uptime_info);
  g_free(package_info);
}

static char*
get_current_power_profile(void)
{
    GDBusConnection *connection;
    GError *error = NULL;
    GVariant *result;
    char *profile = NULL;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error != NULL) {
        g_warning("Failed to connect to system bus: %s", error->message);
        g_error_free(error);
        return NULL;
    }

    result = g_dbus_connection_call_sync(connection,
                                         "net.hadess.PowerProfiles",
                                         "/net/hadess/PowerProfiles",
                                         "org.freedesktop.DBus.Properties",
                                         "Get",
                                         g_variant_new("(ss)", "net.hadess.PowerProfiles", "ActiveProfile"),
                                         G_VARIANT_TYPE("(v)"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);

    if (error != NULL) {
        g_warning("Failed to get power profile: %s", error->message);
        g_error_free(error);
    } else {
        GVariant *value;
        g_variant_get(result, "(v)", &value);
        profile = g_strdup(g_variant_get_string(value, NULL));
        g_variant_unref(value);
        g_variant_unref(result);
    }

    g_object_unref(connection);

    return profile;
}

static void
set_power_profile(const char* profile_name)
{
    GDBusConnection *connection;
    GError *error = NULL;
    GVariant *result;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error != NULL) {
        g_warning("Failed to connect to system bus: %s", error->message);
        g_error_free(error);
        return;
    }

    result = g_dbus_connection_call_sync(connection,
                                         "net.hadess.PowerProfiles",
                                         "/net/hadess/PowerProfiles",
                                         "org.freedesktop.DBus.Properties",
                                         "Set",
                                         g_variant_new("(ssv)", "net.hadess.PowerProfiles", "ActiveProfile", g_variant_new_string(profile_name)),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);

    if (error != NULL) {
        g_warning("Failed to set power profile: %s", error->message);
        g_error_free(error);
    } else {
        g_variant_unref(result);
    }

    g_object_unref(connection);
}

static void
on_power_mode_row_activated(AdwActionRow *row, GtkCheckButton *radio)
{
    const char* mode;

    gtk_check_button_set_active(radio, TRUE);

    if (radio == LENOVO_VANTAGE_WINDOW(gtk_widget_get_root(GTK_WIDGET(row)))->radio1) {
        mode = "performance";
    } else if (radio == LENOVO_VANTAGE_WINDOW(gtk_widget_get_root(GTK_WIDGET(row)))->radio2) {
        mode = "balanced";
    } else if (radio == LENOVO_VANTAGE_WINDOW(gtk_widget_get_root(GTK_WIDGET(row)))->radio3) {
        mode = "power-saver";
    } else {
        return;
    }

    set_power_profile(mode);
}

static void on_device_control_row_activated(AdwActionRow *row, GtkSwitch *toggle)
{
    gboolean current_state = gtk_switch_get_active(toggle);
    gtk_switch_set_active(toggle, !current_state);
}

static int get_conservation_mode_status(void)
{
    FILE *file;
    int status;

    file = fopen(CONSERVATION_MODE_PATH, "r");
    if (file == NULL) {
        g_warning("Error opening conservation mode file");
        return -1;
    }
    fscanf(file, "%d", &status);
    fclose(file);
    return status;
}

static void set_conservation_mode_status(int status)
{
    char command[256];
    int result;

    snprintf(command, sizeof(command), "pkexec sh -c 'echo %d > %s'", status, CONSERVATION_MODE_PATH);

    result = system(command);
    if (result == 0) {
        g_print("Conservation mode %s\n", status ? "enabled" : "disabled");
    } else {
        g_warning("Failed to change conservation mode status");
    }
}

static void on_conservation_mode_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data)
{
    int current_status;

    set_conservation_mode_status(state);
    current_status = get_conservation_mode_status();
    if (current_status != -1) {
        gtk_switch_set_active(widget, current_status);
    }
}

static void initialize_conservation_mode_switch(LenovoVantageWindow *self)
{
    int status = get_conservation_mode_status();
    if (status != -1) {
        gtk_switch_set_active(self->conservation_mode_switch, status);
    }
    g_signal_connect(self->conservation_mode_switch, "state-set", G_CALLBACK(on_conservation_mode_switch_state_set), NULL);
}

static int get_fn_lock_status(void)
{
    FILE *file;
    int status;

    file = fopen(FN_LOCK_PATH, "r");
    if (file == NULL) {
        g_warning("Error opening fn lock file");
        return -1;
    }
    fscanf(file, "%d", &status);
    fclose(file);
    return status;
}

static void set_fn_lock_status(int status)
{
    char command[256];
    int result;

    snprintf(command, sizeof(command), "pkexec sh -c 'echo %d > %s'", status, FN_LOCK_PATH);

    result = system(command);
    if (result == 0) {
        g_print("Fn lock mode %s\n", status ? "enabled" : "disabled");
    } else {
        g_warning("Failed to change fn lock mode status");
    }
}

static void on_fn_lock_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data)
{
    int current_status;

    set_fn_lock_status(state);
    current_status = get_fn_lock_status();
    if (current_status != -1) {
        gtk_switch_set_active(widget, current_status);
    }
}

static void initialize_fn_lock_switch(LenovoVantageWindow *self)
{
    int status = get_fn_lock_status();
    if (status != -1) {
        gtk_switch_set_active(self->fn_lock_switch, status);
    }
    g_signal_connect(self->fn_lock_switch, "state-set", G_CALLBACK(on_fn_lock_switch_state_set), NULL);
}


static int get_always_on_usb_status(void)
{
    FILE *file;
    int status;

    file = fopen(USB_CHARGING_PATH, "r");
    if (file == NULL) {
        g_warning("Error opening always-on USB file");
        return -1;
    }
    fscanf(file, "%d", &status);
    fclose(file);
    return status;
}

static void set_always_on_usb_status(int status)
{
    char command[256];
    int result;

    snprintf(command, sizeof(command), "pkexec sh -c 'echo %d > %s'", status, USB_CHARGING_PATH);

    result = system(command);
    if (result == 0) {
        g_print("Always-on USB mode %s\n", status ? "enabled" : "disabled");
    } else {
        g_warning("Failed to change always-on USB mode status");
    }
}

static void on_always_on_usb_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data)
{
    int current_status;

    set_always_on_usb_status(state);
    current_status = get_always_on_usb_status();
    if (current_status != -1) {
        gtk_switch_set_active(widget, current_status);
    }
}

static void initialize_always_on_usb_switch(LenovoVantageWindow *self)
{
    int status = get_always_on_usb_status();
    if (status != -1) {
        gtk_switch_set_active(self->always_on_usb_switch, status);
    }
    g_signal_connect(self->always_on_usb_switch, "state-set", G_CALLBACK(on_always_on_usb_switch_state_set), NULL);
}


static void set_camera_status(int status)
{
    char command[256];
    int result;

    snprintf(command, sizeof(command), "pkexec sh -c '%s uvcvideo'", status ? "modprobe" : "modprobe -r");

    result = system(command);
    if (result == 0) {
        g_print("Camera %s\n", status ? "enabled" : "disabled");
    } else {
        g_warning("Failed to change camera status");
    }
}

static void set_wifi_status(int status)
{
    char command[256];
    int result;

    snprintf(command, sizeof(command), "nmcli radio wifi %s", status ? "on" : "off");

    result = system(command);
    if (result == 0) {
        g_print("WiFi %s\n", status ? "enabled" : "disabled");
    } else {
        g_warning("Failed to change WiFi status");
    }
}

static void on_camera_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data)
{
    set_camera_status(state);
    // Burada kamera durumunu kontrol edip switch'i güncelleme kodu eklenebilir
}

static void on_wifi_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data)
{
    set_wifi_status(state);
    // Burada WiFi durumunu kontrol edip switch'i güncelleme kodu eklenebilir
}

static void initialize_camera_switch(LenovoVantageWindow *self)
{
    // Kamera başlangıç durumunu kontrol etme kodu buraya eklenebilir
    g_signal_connect(self->camera_switch, "state-set", G_CALLBACK(on_camera_switch_state_set), NULL);
}

static void initialize_wifi_switch(LenovoVantageWindow *self)
{
    // WiFi başlangıç durumunu kontrol etme kodu buraya eklenebilir
    g_signal_connect(self->wifi_switch, "state-set", G_CALLBACK(on_wifi_switch_state_set), NULL);
}

static int
get_current_keyboard_light_level(void)
{
    FILE *f;
    int level;

    f = fopen("/sys/class/leds/platform::kbd_backlight/brightness", "r");
    if (f == NULL) {
        f = fopen("/sys/devices/pci0000:00/0000:00:14.3/PNP0C09:00/VPC2004:00/leds/platform::kbd_backlight/brightness", "r");
        if (f == NULL) {
            perror("fopen");
            return -1;
        }
    }

    if (fscanf(f, "%d", &level) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return level;
}


#include <gio/gio.h>

static int
get_max_keyboard_brightness(void)
{
    GError *error = NULL;
    GDBusConnection *connection;
    GVariant *result;
    int max_brightness = -1;

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL) {
        g_warning("Failed to connect to system bus: %s", error->message);
        g_error_free(error);
        return -1;
    }

    result = g_dbus_connection_call_sync(connection,
                                         "org.freedesktop.UPower",
                                         "/org/freedesktop/UPower/KbdBacklight",
                                         "org.freedesktop.UPower.KbdBacklight",
                                         "GetMaxBrightness",
                                         NULL,
                                         G_VARIANT_TYPE("(i)"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);

    if (result == NULL) {
        g_warning("Failed to get max keyboard brightness: %s", error->message);
        g_error_free(error);
    } else {
        g_variant_get(result, "(i)", &max_brightness);
        g_variant_unref(result);
    }

    g_object_unref(connection);
    return max_brightness;
}

static void
set_keyboard_light(int level)
{
    GError *error = NULL;
    GDBusConnection *connection;
    GVariant *result;
    int max_brightness;

    max_brightness = get_max_keyboard_brightness();
    if (max_brightness < 0) {
        g_warning("Could not get max keyboard brightness");
        return;
    }

    if (level > max_brightness) {
        level = max_brightness;
    }

    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL) {
        g_warning("Failed to connect to system bus: %s", error->message);
        g_error_free(error);
        return;
    }

    result = g_dbus_connection_call_sync(connection,
                                         "org.freedesktop.UPower",
                                         "/org/freedesktop/UPower/KbdBacklight",
                                         "org.freedesktop.UPower.KbdBacklight",
                                         "SetBrightness",
                                         g_variant_new("(i)", level),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);

    if (result == NULL) {
        g_warning("Failed to set keyboard brightness: %s", error->message);
        g_error_free(error);
    } else {
        g_variant_unref(result);
    }

    g_object_unref(connection);
}

static void
on_keyboard_light_row_activated(AdwActionRow *row, gpointer user_data)
{
    GtkCheckButton *radio;
    const char *title;
    radio = GTK_CHECK_BUTTON(user_data);
    if (gtk_check_button_get_active(radio)) {
        return;
    }
    gtk_check_button_set_active(radio, TRUE);
    title = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
    if (g_strcmp0(title, "Off") == 0) {
        set_keyboard_light(0);
    } else if (g_strcmp0(title, "Low") == 0) {
        set_keyboard_light(1);
    } else if (g_strcmp0(title, "High") == 0) {
        set_keyboard_light(2);
    }
}

static void
initialize_keyboard_light(LenovoVantageWindow *self)
{
    int current_level = get_current_keyboard_light_level();

    switch (current_level) {
        case 0:
            gtk_check_button_set_active(self->radio4, TRUE);
            break;
        case 1:
            gtk_check_button_set_active(self->radio5, TRUE);
            break;
        case 2:
            gtk_check_button_set_active(self->radio6, TRUE);
            break;
        default:
            gtk_check_button_set_active(self->radio4, TRUE);
            break;
	}
}

static void
run_update_command(GtkWidget *widget, gpointer data) {
    GError *error = NULL;
    const gchar *command = "gnome-terminal -- bash -c 'echo \"Updating system...\"; sudo dnf update; echo \"Update complete. Press any key to close.\"; read -n 1'";

    if (!g_spawn_command_line_async(command, &error)) {
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
    }
}

static void
lenovo_vantage_window_class_init(LenovoVantageWindowClass *klass)
{
    GtkCssProvider *provider;
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, "/me/bykemalh/vantage/lenovo-vantage-window.ui");

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(provider, "/me/bykemalh/vantage/style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref (provider);

    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, laptop_info_group);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, serialn_row);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, modeln_row);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, biosv_row);

    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, battery_level_bar);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, battery_label);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, cpu_label);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, cpu_level_bar);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, memory_label);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, memory_level_bar);

    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, os_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, kernel_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, host_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, memory_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, shell_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, de_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, wm_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, res_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, cpu_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, gpu_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, uptime_row);
    gtk_widget_class_bind_template_child (widget_class, LenovoVantageWindow, package_row);

		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, row1);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, row2);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, row3);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, radio1);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, radio2);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, radio3);

    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, wifi_row);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, mic_row);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, wifi_switch);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, mic_switch);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, conservation_mode_row);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, conservation_mode_switch);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, always_on_usb_row);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, always_on_usb_switch);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, fn_lock_row);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, fn_lock_switch);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, touchpad_row);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, touchpad_switch);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, camera_row);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, camera_switch);

		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, copy_serial_btn);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, show_serial_btn);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, toast_overlay);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, copy_model_btn);
    gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, copy_bios_btn);

		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, row4);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, row5);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, row6);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, radio4);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, radio5);
		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, radio6);

		gtk_widget_class_bind_template_child(widget_class, LenovoVantageWindow, system_update_button);
}

static void
lenovo_vantage_window_init(LenovoVantageWindow *self)
{
    char *current_profile;
    const char* lang = g_getenv("LANG");

    gtk_widget_init_template(GTK_WIDGET(self));

    gtk_widget_set_size_request(GTK_WIDGET(self->laptop_info_group), 400, -1);
    gtk_widget_set_size_request(GTK_WIDGET(self->serialn_row), -1, 60);
    gtk_widget_set_size_request(GTK_WIDGET(self->modeln_row), -1, 60);
    gtk_widget_set_size_request(GTK_WIDGET(self->biosv_row), -1, 60);

    get_system_info(self);
    get_laptop_info(self);
    update_battery_info(self);

    g_timeout_add(1000, update_system_info, self);
    self->timer_id = g_timeout_add(UPDATE_INTERVAL, update_battery_info, self);

    current_profile = get_current_power_profile();
    if (current_profile) {
        if (strcmp(current_profile, "performance") == 0) {
            gtk_check_button_set_active(self->radio1, TRUE);
        } else if (strcmp(current_profile, "balanced") == 0) {
            gtk_check_button_set_active(self->radio2, TRUE);
        } else if (strcmp(current_profile, "power-saver") == 0) {
            gtk_check_button_set_active(self->radio3, TRUE);
        }
        g_free(current_profile);
    }

    // Connect signals for power mode rows
    g_signal_connect(self->row1, "activated", G_CALLBACK(on_power_mode_row_activated), self->radio1);
    g_signal_connect(self->row2, "activated", G_CALLBACK(on_power_mode_row_activated), self->radio2);
    g_signal_connect(self->row3, "activated", G_CALLBACK(on_power_mode_row_activated), self->radio3);

    // Connect signals for device control rows
    g_signal_connect(self->wifi_row, "activated", G_CALLBACK(on_device_control_row_activated), self->wifi_switch);
    g_signal_connect(self->mic_row, "activated", G_CALLBACK(on_device_control_row_activated), self->mic_switch);
    g_signal_connect(self->conservation_mode_row, "activated", G_CALLBACK(on_device_control_row_activated), self->conservation_mode_switch);
    g_signal_connect(self->always_on_usb_row, "activated", G_CALLBACK(on_device_control_row_activated), self->always_on_usb_switch);
    g_signal_connect(self->fn_lock_row, "activated", G_CALLBACK(on_device_control_row_activated), self->fn_lock_switch);
    g_signal_connect(self->touchpad_row, "activated", G_CALLBACK(on_device_control_row_activated), self->touchpad_switch);
    g_signal_connect(self->camera_row, "activated", G_CALLBACK(on_device_control_row_activated), self->camera_switch);

    g_signal_connect(self->show_serial_btn, "clicked", G_CALLBACK(on_show_serial_clicked), self);
    g_signal_connect(self->copy_serial_btn, "clicked", G_CALLBACK(on_copy_serial_clicked), self);
    g_signal_connect(self->copy_model_btn, "clicked", G_CALLBACK(on_copy_model_clicked), self);
    g_signal_connect(self->copy_bios_btn, "clicked", G_CALLBACK(on_copy_bios_clicked), self);

    g_signal_connect(self->row4, "activated", G_CALLBACK(on_keyboard_light_row_activated), self->radio4);
    g_signal_connect(self->row5, "activated", G_CALLBACK(on_keyboard_light_row_activated), self->radio5);
    g_signal_connect(self->row6, "activated", G_CALLBACK(on_keyboard_light_row_activated), self->radio6);

    initialize_conservation_mode_switch(self);
    initialize_always_on_usb_switch(self);
    initialize_camera_switch(self);
    initialize_wifi_switch(self);
    initialize_fn_lock_switch(self);
    initialize_keyboard_light(self);

    setlocale(LC_ALL, "");
    bindtextdomain("lenovo-vantage", "/usr/share/locale");
    textdomain("lenovo-vantage");

    if (lang && strncmp(lang, "tr", 2) == 0) {
        setlocale(LC_ALL, "tr_TR.UTF-8");
    }
    g_signal_connect(self->system_update_button, "clicked", G_CALLBACK(run_update_command), self);
}
