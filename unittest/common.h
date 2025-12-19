#pragma once
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Detect C++ standard library
#if defined(_MSC_VER) && !defined(__clang__)
  #define USING_MSVC_STL 1       // MSVC STL (Windows / MSVC)
#else
  #define USING_MSVC_STL 0
#endif

#if defined(_LIBCPP_VERSION)
  #define USING_LIBCXX 1         // libc++ (Apple/LLVM, common on macOS)
#else
  #define USING_LIBCXX 0
#endif

#if defined(__GLIBCXX__) && !USING_LIBCXX
  #define USING_LIBSTDCXX 1      // libstdc++ (GNU, common on Linux)
#else
  #define USING_LIBSTDCXX 0
#endif
#define SQLWCHAR_LITERAL(c) ((SQLWCHAR)(c))
