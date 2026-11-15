/*
 * vendor.qti.diaghal —— 服务端回调（v1）。
 *
 * 逆向来源：原厂 lib64/vendor.qti.diaghal-V1-ndk.so 的 BpIdiagcallback。
 * 事务表：code 1 send_data(ParcelableMemory const&, int, int*)，同步（非 oneway），
 * 写序 int32(1) → ParcelableMemory → int32，读回 StatusHeader + int。
 * 语义：diag 收到数据后回调上层（diag-router 用它把读到的日志推给客户端）。
 */
package vendor.qti.diaghal;

import vendor.qti.diaghal.ParcelableMemory;

@VintfStability
interface IIdiagcallback {
  int send_data(in ParcelableMemory memory, int length);
}
