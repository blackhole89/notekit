#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

GSettingsBackend* nk_json_settings_backend_new(const gchar* location);

G_END_DECLS

#endif // __SETTINGS_H__
