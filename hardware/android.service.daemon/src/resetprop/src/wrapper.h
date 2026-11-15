#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the property system
void resetprop_init(void);

// Get property value
// Returns 0 on success, -1 if not found
int resetprop_get(const char* name, char* out_buf, int buf_size);

// Set property value
// Returns 0 on success, non-zero on error
int resetprop_set(const char* name, const char* value);

// Set property value via property_service ONLY (no prop-area reset/rebuild).
// Safe for hot contexts (e.g. radio_prop) with concurrent writers.
// Returns 0 on success, non-zero on error
int resetprop_set_svc(const char* name, const char* value);

// Set property value, bypassing property_service
// Returns 0 on success, non-zero on error
int resetprop_set_direct(const char* name, const char* value);

// Delete property
// Returns 0 on success, non-zero on error
int resetprop_delete(const char* name);

// Get property SELinux context
// Returns 0 on success, -1 if not found
int resetprop_get_context(const char* name, char* out_buf, int buf_size);

// Reset properties in a context file
void resetprop_reset_file(const char* filename);

// Load properties from file
void resetprop_load_file(const char* filename);

#ifdef __cplusplus
}
#endif
