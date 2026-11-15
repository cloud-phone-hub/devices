// C-compatible wrapper for resetprop
// Provides extern "C" API for Rust FFI

#include <cstring>
#include <string>
#include "resetprop.hpp"

// Buffer size for property values
#define PROP_BUF_SIZE 256

extern "C" {

// Initialize the property system
void resetprop_init() {
    // InitOnce is called automatically by the other functions
    // But we expose it for explicit initialization
    // The Initialize struct in resetprop.cpp handles this
}

// Get property value
// Returns 0 on success, -1 if not found
// Output buffer must be at least PROP_BUF_SIZE bytes
int resetprop_get(const char* name, char* out_buf, int buf_size) {
    auto val = get_prop(name);
    if (val.empty()) {
        return -1;
    }
    strncpy(out_buf, val.c_str(), buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    return 0;
}

// Set property value
// Returns 0 on success, non-zero on error
int resetprop_set(const char* name, const char* value) {
    return set_prop(name, value, false);
}

// Set property value via property_service ONLY (no prop-area reset/rebuild).
// Returns 0 on success, non-zero on error
int resetprop_set_svc(const char* name, const char* value) {
    return set_prop_svc(name, value);
}

// Set property value, bypassing property_service
// Returns 0 on success, non-zero on error
int resetprop_set_direct(const char* name, const char* value) {
    return set_prop(name, value, true);
}

// Delete property
// Returns 0 on success, non-zero on error
int resetprop_delete(const char* name) {
    return delete_prop(name);
}

// Get property SELinux context
// Returns 0 on success, -1 if not found
int resetprop_get_context(const char* name, char* out_buf, int buf_size) {
    auto ctx = get_prop_context(name);
    if (ctx.empty()) {
        return -1;
    }
    strncpy(out_buf, ctx.c_str(), buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    return 0;
}

// Reset properties in a context file
void resetprop_reset_file(const char* filename) {
    reset_prop_file(filename);
}

// Load properties from file
void resetprop_load_file(const char* filename) {
    load_prop_file(filename);
}

} // extern "C"
