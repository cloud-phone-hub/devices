# 外部构建入口：由 03_build_modules 在内核构建卷里调用
# （make -C <内核树> M=<本目录> modules），也可手工 dddk 编译。
KERNEL_SRC ?= $(KDIR)

ifeq ($(strip $(KERNEL_SRC)),)
$(error KERNEL_SRC or KDIR is required; run this project through `cdh device build --only build_modules`)
endif

M := $(CURDIR)

.PHONY: modules clean

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean
