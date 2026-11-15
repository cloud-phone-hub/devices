
#include <cstdbool>
#include <dlfcn.h>
#include <sys/types.h>
#include <vector>
#include <map>
#include <string>
#include <set>

#include "logging.hpp"
#include "resetprop.hpp"

#include <system_properties/prop_info.h>
#include <system_properties/prop_area.h>

using namespace std;

#ifdef APPLET_STUB_MAIN
#define system_property_set             __system_property_set
#define system_property_find            __system_property_find
#define system_property_read_callback   __system_property_read_callback
#define system_property_foreach         __system_property_foreach
#define system_property_read(...)
#else
static int (*system_property_set)(const char*, const char*);
static int (*system_property_read)(const prop_info*, char*, char*);
static const prop_info *(*system_property_find)(const char*);
static void (*system_property_read_callback)(
        const prop_info*, void (*)(void*, const char*, const char*, uint32_t), void*);
static int (*system_property_foreach)(void (*)(const prop_info*, void*), void*);
#endif

struct PropFlags {
    void setSkipSvc() { flags |= 1; }
    void setPersist() { flags |= (1 << 1); }
    void setContext() { flags |= (1 << 2); }
    void setPersistOnly() { flags |= (1 << 3); setPersist(); }
    bool isSkipSvc() const { return flags & 1; }
    bool isPersist() const { return flags & (1 << 1); }
    bool isContext() const { return flags & (1 << 2); }
    bool isPersistOnly() const { return flags & (1 << 3); }
private:
    uint32_t flags = 0;
};

[[noreturn]] static void usage(char* arg0) {
    fprintf(stderr,
R"EOF(resetprop - System Property Manipulation Tool

Usage: %s [flags] [arguments...]

Read mode arguments:
   (no arguments)    print all properties
   NAME              get property

Write mode arguments:
   NAME VALUE        set property NAME as VALUE
   -f,--file   FILE  load and set properties from FILE
   -d,--delete NAME  delete property

General flags:
   -h,--help         show this message
   -v                print verbose output to stderr

Read mode flags:
   -p      also read persistent props from storage
   -P      only read persistent props from storage
   -Z      get property context instead of value

Write mode flags:
   -n      set properties bypassing property_service
   -N      set ro properties using property_service
   -p      always write persistent prop changes to storage

)EOF", arg0);
    exit(1);
}

static bool check_legal_property_name(const char *name) {
    size_t namelen = strlen(name);

    if (namelen < 1) goto illegal;
    if (name[0] == '.') goto illegal;
    if (name[namelen - 1] == '.') goto illegal;

    /* Only allow alphanumeric, plus '.', '-', '@', ':', or '_' */
    /* Don't allow ".." to appear in a property name */
    for (size_t i = 0; i < namelen; i++) {
        if (name[i] == '.') {
            // i=0 is guaranteed to never have a dot. See above.
            if (name[i-1] == '.') goto illegal;
            continue;
        }
        if (name[i] == '_' || name[i] == '-' || name[i] == '@' || name[i] == ':') continue;
        if (name[i] >= 'a' && name[i] <= 'z') continue;
        if (name[i] >= 'A' && name[i] <= 'Z') continue;
        if (name[i] >= '0' && name[i] <= '9') continue;
        goto illegal;
    }

    return true;

illegal:
    LOGE("Illegal property name: [%s]\n", name);
    return false;
}

void reset_properties(const std::set<std::string>& contexts){
    for(const auto& context : contexts){
        if(context.empty()){
            continue;
        }
        // LOGI("reset_properties %s\n", context.c_str());
        // 重置属性context
        reset_prop_file(context.c_str());
    }
}

static void read_prop_with_cb(const prop_info *pi, void *cb) {
    if (system_property_read_callback) {
        auto callback = [](void *cb, const char *name, const char *value, uint32_t) {
            static_cast<prop_cb*>(cb)->exec(name, value);
        };
        system_property_read_callback(pi, callback, cb);
    } else {
        char name[PROP_NAME_MAX];
        char value[PROP_VALUE_MAX];
        name[0] = '\0';
        value[0] = '\0';
        system_property_read(pi, name, value);
        static_cast<prop_cb*>(cb)->exec(name, value);
    }
}

template<class StringType>
struct prop_to_string : prop_cb {
    void exec(const char *, const char *value) override {
        val = value;
    }
    StringType val;
};

// Direct property modification via mmap RW (Magisk-style).
// Re-maps the property context file as read-write, then updates the target
// property directly in shared memory. This bypasses both the property service
// (SELinux property_service check) and the read-only mapping limitation.
static int set_prop_direct_rw(const char *name, const char *value) {
    // Get the SELinux context for this property (also the filename in /dev/__properties__/)
    auto context = get_prop_context(name);
    if (context.empty()) {
        LOGW("resetprop: cannot find context for [%s]\n", name);
        return -1;
    }

    auto file = std::string("/dev/__properties__/") + context;
    bool is_rw = true;
    prop_area *pa = prop_area::map_prop_area(file.c_str(), &is_rw);
    if (pa == nullptr || !is_rw) {
        LOGW("resetprop: cannot mmap RW [%s] (is_rw=%d)\n", file.c_str(), is_rw);
        return -1;
    }

    // Read all properties from the RW-mapped area, overriding the target one
    struct OverrideData {
        const char* target_name;
        const char* target_value;
        std::map<std::string, std::string>* props;
    };
    std::map<std::string, std::string> props;
    OverrideData data{name, value, &props};

    pa->foreach([](const prop_info* pi, void* cookie){
        auto* d = static_cast<OverrideData*>(cookie);
        if (strcmp(pi->name, d->target_name) == 0) {
            d->props->emplace(pi->name, d->target_value);
        } else {
            if (pi->is_long()) {
                d->props->emplace(pi->name, pi->long_value());
            } else {
                d->props->emplace(pi->name, pi->value);
            }
        }
    }, &data);

    // Check if target property was found
    if (props.find(name) == props.end()) {
        // Property doesn't exist yet, add it
        props.emplace(name, value);
    }

    // Reset the area and re-add all properties (with the overridden value)
    pa->reset();
    for (auto& [key, val] : props) {
        pa->add(key.data(), strlen(key.data()), val.data(), strlen(val.data()));
    }

    LOGI("resetprop: set [%s] = [%s] via direct mmap RW\n", name, value);
    return 0;
}

static int set_prop(const char *name, const char *value, PropFlags flags) {
    if (!check_legal_property_name(name))
        return 1;

    [[maybe_unused]]
    const char *msg = flags.isSkipSvc() ? "direct modification" : "property_service";

    // For direct mode, use mmap RW approach to bypass read-only memory limitation
    if (flags.isSkipSvc()) {
        int ret = set_prop_direct_rw(name, value);
        if (ret == 0) {
            // Refresh the property cache for this context
            std::set<std::string> contexts;
            contexts.insert(get_prop_context(name));
            reset_properties(contexts);
        }
        return ret;
    }

    auto pi = const_cast<prop_info *>(__system_property_find(name));

    // Always delete existing read-only properties, because they could be
    // long properties and cannot directly go through __system_property_update
    if (pi != nullptr && str_starts(name, "ro.")) {
        // Skip pruning nodes as we will add it back ASAP
        __system_property_delete(name, false);
        pi = nullptr;
    }

    int ret;
    if (pi != nullptr) {
        ret = system_property_set(name, value);
    } else {
        ret = system_property_set(name, value);
    }

    // When bypassing property_service, persistent props won't be stored in storage.
    // Explicitly handle this situation.
    if (ret == 0 && flags.isSkipSvc() && flags.isPersist() && str_starts(name, "persist.")) {
        ret = persist_set_prop(name, value) ? 0 : 1;
    }

    if (ret) {
        LOGW("resetprop: set prop error\n");
    }else{
        std::set<std::string> contexts;
        contexts.insert(get_prop_context(name));
        reset_properties(contexts);
    }

    return ret;
}

template<class StringType>
static StringType get_prop(const char *name, PropFlags flags) {
    if (!check_legal_property_name(name))
        return {};

    prop_to_string<StringType> cb;

    if (flags.isContext()) {
        auto context = __system_property_get_context(name) ?: "";
        LOGD("resetprop: prop context [%s]: [%s]\n", name, context);
        cb.exec(name, context);
        return cb.val;
    }

    if (!flags.isPersistOnly()) {
        if (auto pi = system_property_find(name)) {
            read_prop_with_cb(pi, &cb);
            LOGD("resetprop: get prop [%s]: [%s]\n", name, cb.val.c_str());
        }
    }

    if (cb.val.empty() && flags.isPersist() && str_starts(name, "persist."))
        persist_get_prop(name, &cb);
    if (cb.val.empty())
        LOGD("resetprop: prop [%s] does not exist\n", name);

    return cb.val;
}

static void print_props(PropFlags flags) {
    prop_list list;
    prop_collector collector(list);
    if (!flags.isPersistOnly())
        system_property_foreach(read_prop_with_cb, &collector);
    if (flags.isPersist()) {}
        persist_get_props(&collector);
    for (auto &[key, val] : list) {
        const char *v = flags.isContext() ?
                (__system_property_get_context(key.data()) ?: "") :
                val.data();
        printf("[%s]: [%s]\n", key.data(), v);
    }
}

static int delete_prop(const char *name, PropFlags flags) {
    if (!check_legal_property_name(name))
        return 1;

    LOGD("resetprop: delete prop [%s]\n", name);

    int ret = __system_property_delete(name, true);
    if (flags.isPersist() && str_starts(name, "persist.")) {
        if (persist_delete_prop(name))
            ret = 0;
        ret = 1;
    }
    std::set<std::string> contexts;
    contexts.insert(get_prop_context(name));
    reset_properties(contexts);
    return ret;
}

static void load_file(const char *filename, PropFlags flags) {
    LOGD("resetprop: Parse prop file [%s]\n", filename);
    std::set<std::string> contexts;
    parse_prop_file(filename, [&](auto key, auto val) -> bool {
        const char* key_str = key.data();
        set_prop(key_str, val.data(), flags);
        contexts.insert(get_prop_context(key_str));
        return true;
    });
    reset_properties(contexts);
}

void reset_prop_file(const char *filename){
    auto file = std::string("/dev/__properties__/") + filename;
    bool is_rw = true;
    prop_area *pa = prop_area::map_prop_area(file.c_str(), &is_rw);
    if (pa == nullptr || !is_rw) {
        LOGW("resetprop: cannot mmap RW for reset [%s] (is_rw=%d)\n", file.c_str(), is_rw);
        return;
    }
    std::map<std::string, std::string> props;
    pa->foreach([](const prop_info* pi, [[maybe_unused]]void* cookie){
        //bool add(const char* name, unsigned int namelen, const char* value, unsigned int valuelen);
        std::map<std::string, std::string> *props = (std::map<std::string, std::string> *)cookie;
        if(pi->is_long()){
            auto value = pi->long_value();
            props->emplace(pi->name, value);
        }else{
            props->emplace(pi->name, pi->value);
        }
    }, &props);
    pa->reset();
    for(auto &[key, val] : props){
        pa->add(key.data(), strlen(key.data()), val.data(), strlen(val.data()));
        // LOGD("resetprop: add prop [%s]: [%s]\n", key.data(), val.data());
    }
}

std::string get_prop_context(const char *name){
    return __system_property_get_context(name) ?: "";
}


// void reset_prop_file(const char *filename){
//     auto cmd = std::string("mv /dev/__properties__/") + filename + " /dev/__properties__/" + filename + ".bak";
//     system(cmd.c_str());
//     auto file = std::string("/dev/__properties__/") + filename;
//     // auto rm_cmd = std::string("rm ") + file;
//     // system(rm_cmd.c_str());
//     bool fsetxattr_failed;
//     prop_area *pa = prop_area::map_prop_area_rw(file.c_str(), filename, &fsetxattr_failed);
//     PropFlags flags;
//     flags.setSkipSvc();
//     auto bak_file = std::string("/dev/__properties__/") + filename + ".bak";
   
//     bool is_rw = false;
//     prop_area *bak_pa = prop_area::map_prop_area(bak_file.c_str(), &is_rw);
//     //bool foreach (void (*propfn)(const prop_info* pi, void* cookie), void* cookie);
//     bak_pa->foreach([](const prop_info* pi, [[maybe_unused]]void* cookie){
//         //bool add(const char* name, unsigned int namelen, const char* value, unsigned int valuelen);
//         prop_area *pa = (prop_area *)cookie;
//         if(pi->is_long()){
//             auto value = pi->long_value();
//            pa->add(pi->name, strlen(pi->name), value, strlen(value));
//         }else{
//             pa->add(pi->name, strlen(pi->name), pi->value, strlen(pi->value));
//         }
//     }, pa);
//     prop_area::unmap_prop_area(&bak_pa);

//     system(bak_file.c_str());
// }


struct Initialize {
    Initialize() {
#ifndef APPLET_STUB_MAIN
#define DLOAD(name) (*(void **) &name = dlsym(RTLD_DEFAULT, "__" #name))
        // Load platform implementations
        DLOAD(system_property_set);
        DLOAD(system_property_read);
        DLOAD(system_property_find);
        DLOAD(system_property_read_callback);
        DLOAD(system_property_foreach);
#undef DLOAD
#endif
        if (__system_properties_init()) {
            LOGE("resetprop: __system_properties_init error\n");
        }
    }
};

static void InitOnce() {
    static struct Initialize init;
}

#define consume_next(val)    \
if (argc != 2) usage(argv0); \
val = argv[1];               \
stop_parse = true;           \

#ifndef RESETPROP_LIB
int main(int argc, char *argv[]) {
    PropFlags flags;
    char *argv0 = argv[0];
    // set_log_level_state(LogLevel::Debug, false);

    const char *prop_file = nullptr;
    const char *prop_to_rm = nullptr;

    --argc;
    ++argv;

    bool ro_use_svc = false;

    // Parse flags and -- options
    while (argc && argv[0][0] == '-') {
        bool stop_parse = false;
        for (int idx = 1; true; ++idx) {
            switch (argv[0][idx]) {
            case '-':
                if (argv[0] == "--file"sv) {
                    consume_next(prop_file);
                } else if (argv[0] == "--delete"sv) {
                    consume_next(prop_to_rm);
                } else {
                    usage(argv0);
                }
                break;
            case 'd':
                consume_next(prop_to_rm);
                continue;
            case 'f':
                consume_next(prop_file);
                continue;
            case 'n':
                flags.setSkipSvc();
                continue;
            case 'p':
                flags.setPersist();
                continue;
            case 'P':
                flags.setPersistOnly();
                continue;
            case 'v':
                // set_log_level_state(LogLevel::Debug, true);
                continue;
            case 'Z':
                flags.setContext();
                continue;
            case 'N':
                ro_use_svc = true;
                continue;
            case '\0':
                break;
            default:
                usage(argv0);
            }
            break;
        }
        --argc;
        ++argv;
        if (stop_parse)
            break;
    }

    InitOnce();

    if (prop_to_rm) {
        return delete_prop(prop_to_rm, flags);
    }

    if (prop_file) {
        load_file(prop_file, flags);
        return 0;
    }

    switch (argc) {
    case 0:
        print_props(flags);
        return 0;
    case 1: {
        auto val = get_prop<string>(argv[0], flags);
        if (val.empty())
            return 1;
        printf("%s\n", val.data());
        return 0;
    }
    case 2: {
        auto name = argv[0];
        if (str_starts(name, "ro.") && !ro_use_svc) {
            flags.setSkipSvc();
        }
        return set_prop(name, argv[1], flags);
    }
    default:
        usage(argv0);
    }
}
#endif // RESETPROP_LIB

/***************
 * Public APIs
 ****************/

template<class StringType>
static StringType get_prop_impl(const char *name, bool persist) {
    InitOnce();
    PropFlags flags;
    if (persist) flags.setPersist();
    return get_prop<StringType>(name, flags);
}

string get_prop(const char *name, bool persist) {
    return get_prop_impl<string>(name, persist);
}

int delete_prop(const char *name, bool persist) {
    InitOnce();
    PropFlags flags;
    if (persist) flags.setPersist();
    return delete_prop(name, flags);
}

int set_prop(const char *name, const char *value, bool skip_svc) {
    InitOnce();
    PropFlags flags;
    if (skip_svc) flags.setSkipSvc();
    return set_prop(name, value, flags);
}

// 仅经 property_service 写入，不做任何属性区 reset/重建。
// set_prop()/set_prop_direct_rw() 成功后都会 reset_properties -> reset_prop_file
// 全量重建对应上下文属性区；radio_prop 等热区有框架/HAL 并发写，重建窗口
// 内的并发更新会破坏 trie，init 后续读属性即 SIGSEGV（oriole 实测）。
// 非 ro. 键的常规写入一律用本接口；persist.* 由 property_service 自行落盘。
int set_prop_svc(const char *name, const char *value) {
    InitOnce();
    if (!check_legal_property_name(name))
        return 1;
#ifndef APPLET_STUB_MAIN
    if (system_property_set == nullptr)
        return 1;
#endif
    return system_property_set(name, value);
}

void load_prop_file(const char *filename, bool skip_svc) {
    InitOnce();
    PropFlags flags;
    if (skip_svc) flags.setSkipSvc();
    load_file(filename, flags);
}
