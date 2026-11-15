import ctypes, os

LIB = "/Volumes/work/cloud-phone-hub/out/qemu/dist/darwin/lib/libvulkan.1.dylib"
os.environ.setdefault("VK_ICD_FILENAMES",
    "/Volumes/work/cloud-phone-hub/out/qemu/dist/darwin/lib/MoltenVK_icd.json")

vk = ctypes.CDLL(LIB)
vk.vkCreateInstance.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.POINTER(ctypes.c_void_p)]
vk.vkEnumeratePhysicalDevices.argtypes = [ctypes.c_void_p,
                                          ctypes.POINTER(ctypes.c_uint32),
                                          ctypes.POINTER(ctypes.c_void_p)]
vk.vkGetPhysicalDeviceQueueFamilyProperties.argtypes = [ctypes.c_void_p,
                                                        ctypes.POINTER(ctypes.c_uint32),
                                                        ctypes.c_void_p]
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


class VkQueueFamilyProperties(ctypes.Structure):
    _fields_ = [
        ("queueFlags", ctypes.c_uint32),
        ("queueCount", ctypes.c_uint32),
        ("timestampValidBits", ctypes.c_uint32),
        ("minImageTransferGranularityWidth", ctypes.c_uint32),
        ("minImageTransferGranularityHeight", ctypes.c_uint32),
        ("minImageTransferGranularityDepth", ctypes.c_uint32),
    ]


app = VkApplicationInfo(0, None, b"vkqueues", 1, b"vkqueues", 1, (1 << 22) | (1 << 12))
ci = VkInstanceCreateInfo(1, None, 0, ctypes.pointer(app), 0, None, 0, None)
inst = ctypes.c_void_p()
res = vk.vkCreateInstance(ctypes.byref(ci), None, ctypes.byref(inst))
print("vkCreateInstance ->", res, flush=True)
if res != 0:
    raise SystemExit(1)

count = ctypes.c_uint32(0)
vk.vkEnumeratePhysicalDevices(inst, ctypes.byref(count), None)
devs = (ctypes.c_void_p * count.value)()
vk.vkEnumeratePhysicalDevices(inst, ctypes.byref(count), devs)

names = {0x1: "TRANSFER", 0x2: "GRAPHICS", 0x4: "COMPUTE",
         0x8: "SPARSE", 0x10: "PROTECTED"}
qcount = ctypes.c_uint32(0)
vk.vkGetPhysicalDeviceQueueFamilyProperties(devs[0], ctypes.byref(qcount), None)
props = (VkQueueFamilyProperties * qcount.value)()
vk.vkGetPhysicalDeviceQueueFamilyProperties(devs[0], ctypes.byref(qcount), props)
print("queueFamilies =", qcount.value, flush=True)
for i, p in enumerate(props):
    flags = "|".join(n for bit, n in names.items() if p.queueFlags & bit) or "0"
    print("family[%d] flags=%s(0x%x) queueCount=%d" % (i, flags, p.queueFlags, p.queueCount),
          flush=True)

vk.vkDestroyInstance(inst, None)
