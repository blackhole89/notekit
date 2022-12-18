#include "settings.h"
#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>
#include <json/json.h>
#include <memory>

#define NK_TYPE_JSON_SETTINGS_BACKEND (nk_json_settings_backend_get_type())
#define NK_JSON_SETTINGS_BACKEND(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), NK_TYPE_JSON_SETTINGS_BACKEND, NkJsonSettingsBackend))
#define NK_IS_JSON_SETTINGS_BACKEND(inst) (G_TYPE_CHECK_INSTANCE_TYPE ((inst), NK_TYPE_JSON_SETTINGS_BACKEND))
typedef GSettingsBackendClass NkJsonSettingsBackendClass;

typedef enum {
	PROP_LOCATION = 1,
} NkJsonSettingsBackendProperty;


typedef struct {
	GSettingsBackend parent_instance;
	GPermission* permission;

	gboolean do_save;
	GFile* file;

	Json::Value* config;
} NkJsonSettingsBackend;

#define EXTENSION_PRIORITY 10

G_DEFINE_TYPE_WITH_CODE (NkJsonSettingsBackend, nk_json_settings_backend, G_TYPE_SETTINGS_BACKEND,
	g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME, g_define_type_id, "notekit-json", EXTENSION_PRIORITY))

static const gchar* get_key_from_path(const gchar* path) {
	if (!g_str_has_prefix(path, "/com/github/blackhole89/NoteKit/")) {
		g_critical("Json Settings: Path outside the NoteKit namespace detected: %s\n", path);
		return NULL;
	}

	return &path[32];
}
static GVariant* jsonvalue_to_gvariant(const Json::Value* value, const GVariantType* expected_type) {
	switch (value->type()) {
		case Json::ValueType::nullValue:
			return NULL;
		case Json::ValueType::intValue:
			return g_variant_new_int32(value->asInt());
		case Json::ValueType::uintValue:
			return g_variant_new_int32(value->asUInt());
		case Json::ValueType::realValue:
			return g_variant_new_double(value->asDouble());
		case Json::ValueType::stringValue:
			return g_variant_new_string(value->asCString());
		case Json::ValueType::booleanValue:
			return g_variant_new_boolean(value->asBool());
		case Json::ValueType::arrayValue:
			if (g_variant_type_is_array(expected_type)) {
				GVariantBuilder builder;
				g_variant_builder_init(&builder, expected_type);
				for (Json::Value::ArrayIndex i = 0; i < value->size(); i++) {
					GVariant* variant = jsonvalue_to_gvariant(&(*value)[i], g_variant_type_element(expected_type));
					g_variant_builder_add_value(&builder, variant);
				}
				return g_variant_builder_end(&builder);
			} else if (g_variant_type_is_tuple(expected_type)) {
				GVariantBuilder builder;
				g_variant_builder_init(&builder, expected_type);

				const GVariantType* child_type = g_variant_type_first(expected_type);
				for (Json::Value::ArrayIndex i = 0; i < g_variant_type_n_items(expected_type); i++) {
					GVariant* variant = jsonvalue_to_gvariant(&(*value)[i], child_type);
					g_variant_builder_add_value(&builder, variant);

					child_type = g_variant_type_next(child_type);
				}
				return g_variant_builder_end(&builder);
			} else {
				return NULL;
			}
		default:
			g_warning("Json Settings: Invalid type found?");
			return NULL;
	}
}
static Json::Value gvariant_to_jsonvalue(GVariant* variant) {
	// numeric
	if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN))
		return Json::Value((bool)g_variant_get_boolean(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BYTE))
		return Json::Value(g_variant_get_byte(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT16))
		return Json::Value(g_variant_get_int16(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT16))
		return Json::Value(g_variant_get_uint16(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT32))
		return Json::Value(g_variant_get_int32(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT32))
		return Json::Value(g_variant_get_uint32(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT64))
		return Json::Value((Json::Int64)g_variant_get_int64(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT64))
		return Json::Value((Json::UInt64)g_variant_get_uint64(variant));
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_DOUBLE))
		return Json::Value(g_variant_get_double(variant));
	// string types
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING)) {
		gsize len;
		const gchar* str = g_variant_get_string(variant, &len);
		return Json::Value(str, str+len);
	}
	// arrays & tuples
	else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_ARRAY) || g_variant_is_of_type(variant, G_VARIANT_TYPE_TUPLE)) {
		Json::Value ret = Json::Value(Json::ValueType::arrayValue);

		GVariantIter* iter = g_variant_iter_new(variant);
		GVariant* child;
		while ((child = g_variant_iter_next_value(iter)))
			ret.append(gvariant_to_jsonvalue(child));
		g_variant_iter_free(iter);

		return ret;
	} else {
		g_warning("Json Settings: Unsupported object \"%s\" found\n", g_variant_get_type_string(variant));
		return Json::Value(Json::ValueType::nullValue);
	}
}

void nk_json_settings_backend_save(NkJsonSettingsBackend* self);

static void nk_json_settings_backend_init(NkJsonSettingsBackend* self) {
	self->do_save = TRUE;
	self->file = NULL;
	self->config = NULL;
}

static void nk_json_settings_backend_object_finalize(GObject* object) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(object);

	g_object_unref(self->permission);
	g_object_unref(self->file);

	if (self->config) {
		delete self->config;
		self->config = NULL;
	}
}

static void nk_json_settings_backend_object_constructed(GObject* object) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(object);

	self->permission = g_simple_permission_new(TRUE);

	self->config = new Json::Value;

	if (!self->file)
		self->file = g_file_new_build_filename(g_get_user_config_dir(), "notekit", "notekit.json", NULL);

	gchar* config; gsize config_len;
	GError* err = NULL;
	if (g_file_load_contents(self->file, NULL, &config, &config_len, NULL, &err)) {
		Json::CharReaderBuilder builder;
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		reader->parse(config, config+config_len, self->config, nullptr);
		g_free(config);
	} else {
		*self->config = Json::Value(Json::ValueType::objectValue);
		if (err->code == G_IO_ERROR_NOT_FOUND) {
			GFile* parent = g_file_get_parent(self->file);
			GError* dir_err = NULL;
			if (!g_file_make_directory_with_parents(parent, NULL, &dir_err)) {
				if (dir_err->code != G_IO_ERROR_EXISTS) {
					g_critical("Failure creating config dir: %s\n", dir_err->message);
					self->do_save = FALSE;
				}
				g_error_free(dir_err);
			}
			g_object_unref(parent);
		} else {
			g_critical("Failure reading config: %s\n", err->message);
			self->do_save = FALSE;
		}
		g_error_free(err);
	}
}

static void nk_json_settings_backend_object_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(object);

	switch (prop_id) {
		case PROP_LOCATION:
			g_value_set_string(value, g_file_peek_path(self->file));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}
static void nk_json_settings_backend_object_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(object);

	switch (prop_id) {
		case PROP_LOCATION:
			g_assert(self->file == NULL);
			if (g_value_get_string(value))
				self->file = g_file_new_build_filename(g_value_get_string(value), "notekit.json", NULL);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static GVariant* nk_json_settings_backend_read(GSettingsBackend* backend, const gchar* key, const GVariantType* expected_type, gboolean default_value) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(backend);

	if (default_value)
		return NULL;

	Json::Value val = (*self->config)[get_key_from_path(key)];
	return jsonvalue_to_gvariant(&val, expected_type);
}

static gboolean nk_json_settings_backend_write(GSettingsBackend *backend, const gchar* key, GVariant* value, gpointer origin_tag) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(backend);

	if (value) {
		(*self->config)[get_key_from_path(key)] = gvariant_to_jsonvalue(value);
		g_variant_unref(g_variant_ref_sink(value));
	} else {
		self->config->removeMember(get_key_from_path(key));
	}
	g_settings_backend_changed(backend, key, origin_tag);
	nk_json_settings_backend_save(self);
	return TRUE;
}

static gboolean nk_json_settings_backend_write_one(gpointer key, gpointer value, gpointer data) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(data);

	if (value) {
		(*self->config)[get_key_from_path((const gchar*)key)] = gvariant_to_jsonvalue((GVariant*)value);
		g_variant_unref(g_variant_ref_sink((GVariant*)value));
	} else {
		self->config->removeMember(get_key_from_path((const gchar*)key));
	}

	return FALSE;
}

static gboolean nk_json_settings_backend_write_tree(GSettingsBackend* backend, GTree* tree, gpointer origin_tag) {
	g_tree_foreach(tree, nk_json_settings_backend_write_one, backend);
	g_settings_backend_changed_tree(backend, tree, origin_tag);
	nk_json_settings_backend_save(NK_JSON_SETTINGS_BACKEND(backend));
	return TRUE;
}

static void nk_json_settings_backend_reset(GSettingsBackend* backend, const gchar* key, gpointer origin_tag) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(backend);

	self->config->removeMember(get_key_from_path(key));

	g_settings_backend_changed(backend, key, origin_tag);
}

static gboolean nk_json_settings_backend_get_writable(GSettingsBackend*, const gchar*) {
	return TRUE;
}

static GPermission* nk_json_settings_backend_get_permission(GSettingsBackend *backend, const gchar*) {
	NkJsonSettingsBackend* self = NK_JSON_SETTINGS_BACKEND(backend);

	return (GPermission*)g_object_ref(self->permission);
}

static void nk_json_settings_backend_class_init(NkJsonSettingsBackendClass* klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GSettingsBackendClass* backend_class = G_SETTINGS_BACKEND_CLASS(klass);

	object_class->constructed = nk_json_settings_backend_object_constructed;
	object_class->finalize = nk_json_settings_backend_object_finalize;
	object_class->get_property = nk_json_settings_backend_object_get_property;
	object_class->set_property = nk_json_settings_backend_object_set_property;

	backend_class->read = nk_json_settings_backend_read;
	backend_class->write = nk_json_settings_backend_write;
	backend_class->write_tree = nk_json_settings_backend_write_tree;
	backend_class->reset = nk_json_settings_backend_reset;
	backend_class->get_writable = nk_json_settings_backend_get_writable;
	backend_class->get_permission = nk_json_settings_backend_get_permission;

	g_object_class_install_property(object_class, PROP_LOCATION,
		g_param_spec_string("location", "Location", "The location of the config file", NULL, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY))
	);
}

GSettingsBackend* nk_json_settings_backend_new(const gchar* location) {
	return static_cast<GSettingsBackend*>(g_object_new(NK_TYPE_JSON_SETTINGS_BACKEND, "location", location, NULL));
}

void nk_json_settings_backend_save(NkJsonSettingsBackend* self) {
	if (!self->do_save)
		return;

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "\t";
	std::string str = Json::writeString(builder, *self->config);

	GError* err = NULL;
	if (!g_file_replace_contents(self->file, str.c_str(), str.length(), NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &err)) {
		g_critical("Error writing config: %s\n", err->message);
		g_error_free(err);
		return;
	}
}
