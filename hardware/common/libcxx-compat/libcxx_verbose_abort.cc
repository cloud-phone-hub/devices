// 旧版 libc++.so（android14 线的 lineage-21.0）不导出 std::__libcpp_verbose_abort，
// 而 NDK 头（LLVM 15 起）的断言 / 异常路径会无条件引用它：__throw_length_error 这类
// 头内联函数一被用到，产物就多出一个未定义符号。这里按 libc++ 自己的语义补一份实现
// （格式化打印后 abort）。两条硬约束：
//   1. 不能包含任何 libc++ 头 —— 旧版头会反过来引用同一个缺失符号；
//   2. 命名空间必须写成 std + inline namespace __1，才能与 libc++ 的
//      _ZNSt3__122__libcpp_verbose_abortEPKcz 对上号。
//
// 只在目标 libc++.so 缺该符号时才由 cdh-vendor 编进产物（判定见
// build/internal/halbuild/libcxx_compat.go）；15/16 线的 libc++.so 自带实现，
// 这份源码不会被用到。

#include <stdarg.h>

extern "C" {
int vdprintf(int, const char*, va_list);
void abort(void) __attribute__((noreturn));
}

namespace std {
inline namespace __1 {
[[noreturn]] void __libcpp_verbose_abort(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  vdprintf(2, format, ap);
  va_end(ap);
  abort();
}
}  // namespace __1
}  // namespace std
