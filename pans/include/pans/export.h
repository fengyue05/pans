#ifndef PANS_INCLUDE_PANS_EXPORT_H
#define PANS_INCLUDE_PANS_EXPORT_H

#if defined(_WIN32) && defined(PANS_SHARED_LIBRARY)
// 在编译pans库本身的时候，通过__declspec(dllexport)，告诉编译器，把符号导出到DLL
#if defined(PANS_BUILDING_LIBRARY)
#define PANS_API __declspec(dllexport)
#else
// 其它项目使用pans动态库的时候，__declspec(dllimport)告诉编译器，符号来自于DLL
#define PANS_API __declspec(dllimport)
#endif
// 通过__attribute__((visibility("default")))，确保符号对外可见
#elif defined(__GNUC__) && defined(PANS_SHARED_LIBRARY)
#define PANS_API __attribute__((visibility("default")))
#else
#define PANS_API
#endif

#endif // PANS_INCLUDE_PANS_EXPORT_H
