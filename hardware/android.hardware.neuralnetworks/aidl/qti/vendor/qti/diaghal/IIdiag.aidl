/*
 * vendor.qti.diaghal —— 高通 diag HAL v1（接口名 Idiag）。
 *
 * 逆向来源：原厂 lib64/vendor.qti.diaghal-V1-ndk.so + bin/diag-router
 *（原厂 rc: service vendor.diag-router /vendor/bin/diag-router，注册 Idiag/default）。
 * 事务表（AArch64 反汇编，engine/out/tmp/rev2/rev_any.py 抽取；签名由
 * llvm-nm -D -C 的 BpIdiag::* 印证）：
 *   code 1 close()  -> int
 *   code 2 ioctl(int cmd, in ParcelableMemory memory, int flag) -> int
 *   code 3 open(in IIdiagcallback callback) -> int
 *   code 4 write(in ParcelableMemory memory, int length) -> int
 * 注意声明顺序即事务号：close 在 ioctl 之前（与原厂 .so 一致），改序会让客户端
 * 调到错方法。
 * 接口 hash = 1180103e4052662bcb692e2ae3a1d6b326fc5dfb（.so 内唯一 40 位十六进制串），
 * 逐字写进 1/.hash。
 */
package vendor.qti.diaghal;

import vendor.qti.diaghal.IIdiagcallback;
import vendor.qti.diaghal.ParcelableMemory;

@VintfStability
interface IIdiag {
  int close();
  int ioctl(int cmd, in ParcelableMemory memory, int flag);
  int open(in IIdiagcallback callback);
  int write(in ParcelableMemory memory, int length);
}
