import ctypes, os, sys, faulthandler
faulthandler.enable()

LIB = "/Volumes/work/cloud-phone-hub/out/qemu/dist/darwin/lib/libvulkan.1.dylib"
os.environ.setdefault("VK_ICD_FILENAMES",
    "/Volumes/work/cloud-phone-hub/out/qemu/dist/darwin/lib/MoltenVK_icd.json")

vk = ctypes.CDLL(LIB)
vk.vkCreateInstance.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.POINTER(ctypes.c_void_p)]
vk.vkEnumeratePhysicalDevices.argtypes = [ctypes.c_void_p,
                                          ctypes.POINTER(ctypes.c_uint32),
                                          ctypes.POINTER(ctypes.c_void_p)]
vk.vkGetPhysicalDeviceProperties.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
vk.vkDestroyInstance.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

class VkApplicationInfo(ctypes.Structure):
    _fields_ = [
        ("sType", ctypes.c_int),
        ("pNext", ctypes.c_void_p),
        ("pApplicationName", ctypes.c_char_p),
        ("applicationVersion", ctypes.c_uint32),
        ("pEngineName", ctypes.c_char_p),
        ("engineVersion", ctypes.c_uint32),
        ("apiVersion", ctypes.c_uint32),
    ]

class VkInstanceCreateInfo(ctypes.Structure):
    _fields_ = [
        ("sType", ctypes.c_int),
        ("pNext", ctypes.c_void_p),
        ("flags", ctypes.c_uint32),
        ("pApplicationInfo", ctypes.POINTER(VkApplicationInfo)),
        ("enabledLayerCount", ctypes.c_uint32),
        ("ppEnabledLayerNames", ctypes.c_void_p),
        ("enabledExtensionCount", ctypes.c_uint32),
        ("ppEnabledExtensionNames", ctypes.c_void_p),
    ]

app = VkApplicationInfo(0, None, b"vkprobe", 1, b"vkprobe", 1, (1 << 22) | (1 << 12))
ci = VkInstanceCreateInfo(1, None, 0, ctypes.pointer(app), 0, None, 0, None)

inst = ctypes.c_void_p()
res = vk.vkCreateInstance(ctypes.byref(ci), None, ctypes.byref(inst))
print("vkCreateInstance ->", res, flush=True)
if res != 0:
    sys.exit(1)

count = ctypes.c_uint32(0)
res = vk.vkEnumeratePhysicalDevices(inst, ctypes.byref(count), None)
print("vkEnumeratePhysicalDevices ->", res, "count =", count.value, flush=True)

if count.value:
    devs = (ctypes.c_void_p * count.value)()
    vk.vkEnumeratePhysicalDevices(inst, ctypes.byref(count), devs)
    raw = ctypes.create_string_buffer(4096)
    vk.vkGetPhysicalDeviceProperties(devs[0], ctypes.byref(raw))
    api = int.from_bytes(raw.raw[0:4], "little")
    name = raw.raw[20:276].split(b"\0")[0].decode(errors="replace")
    print("device0:", name, "api=%d.%d.%d" % (api >> 22, (api >> 12) & 0x3ff, api & 0xfff), flush=True)

vk.vkDestroyInstance(inst, None)
