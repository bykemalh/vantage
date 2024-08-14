#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define LENOVO_VANTAGE_TYPE_APPLICATION (lenovo_vantage_application_get_type())

G_DECLARE_FINAL_TYPE (LenovoVantageApplication, lenovo_vantage_application, LENOVO_VANTAGE, APPLICATION, AdwApplication)

LenovoVantageApplication *lenovo_vantage_application_new (const char        *application_id,
                                                          GApplicationFlags  flags);

G_END_DECLS
