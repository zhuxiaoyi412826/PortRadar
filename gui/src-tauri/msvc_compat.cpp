// MinGW 缺失的 MSVC 运行时符号，供静态链接的 WebView2LoaderStatic（MSVC 编译）使用
// 带 MSVC 修饰名的符号在 build.rs 中用 objcopy --redefine-sym 重命名后链接
#include <cstdint>
#include <cstdlib>

extern "C" {

// /GS 栈保护：提供符号满足链接，检查实现为空操作
std::uintptr_t __security_cookie = 0x2B992DDFA232ull;
void __security_check_cookie(std::uintptr_t) {}

// MSVC 线程安全静态初始化（magic statics）：
// guard 初值 0；footer 置 -1（无符号比较下 >= epoch，后续调用走快速路径）
int _Init_thread_epoch = 0x7FFFFFFF;
void _Init_thread_header(int *) {}
void _Init_thread_footer(int *guard) { *guard = -1; }

// 以下三个在链接前由 objcopy 重命名为 MSVC 修饰名：
//   msvc_new_nothrow -> ??2@YAPEAX_KAEBUnothrow_t@std@@@Z  (operator new(size_t, const std::nothrow_t&))
//   msvc_delete      -> ??3@YAXPEAX@Z                     (operator delete(void*))
//   msvc_nothrow_obj -> ?nothrow@std@@3Unothrow_t@1@B     (std::nothrow 全局对象)
void *msvc_new_nothrow(std::size_t size, const void *) { return std::malloc(size); }
void msvc_delete(void *ptr) { std::free(ptr); }
void msvc_delete_sized(void *ptr, std::size_t) { std::free(ptr); }
// 用非 const + 取地址防优化，确保符号一定被发射（const 零初始化可能被编译器丢弃）
char msvc_nothrow_obj_storage[sizeof(void *)] = {};
const void *msvc_nothrow_keep = &msvc_nothrow_obj_storage;

}
