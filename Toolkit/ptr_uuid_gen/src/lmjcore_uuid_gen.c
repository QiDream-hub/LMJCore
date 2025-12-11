// lmjcore_uuid_gen.c
#include "../include/lmjcore_uuid_gen.h"
#include <string.h>

// 平台检测
#if defined(_WIN32) || defined(_WIN64)
#define LMJCORE_WINDOWS 1
#include <windows.h>
#if defined(NTDDI_WIN10_NI) && NTDDI_VERSION >= NTDDI_WIN10_NI
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#define USE_BCRYPT
#else
#include <objbase.h> // CoCreateGuid
#pragma comment(lib, "ole32.lib")
#endif
#else
#include <fcntl.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <stdlib.h> // arc4random_buf
#endif
#endif

// 内部：生成 16 字节随机数据到 buf
static int generate_random_bytes(uint8_t *buf, size_t len) {
#if defined(LMJCORE_WINDOWS)
#ifdef USE_BCRYPT
  NTSTATUS status =
      BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (!BCRYPT_SUCCESS(status)) {
    return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
  }
#else
  GUID guid;
  for (size_t i = 0; i < len; i += sizeof(GUID)) {
    if (CoCreateGuid(&guid) != S_OK) {
      return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
    }
    memcpy(buf + i, &guid,
           (i + sizeof(GUID) <= len) ? sizeof(GUID) : (len - i));
  }
#endif
#elif defined(__APPLE__) || defined(__FreeBSD__)
  arc4random_buf(buf, len);
#else
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
  }
  ssize_t total = 0;
  while (total < (ssize_t)len) {
    ssize_t n = read(fd, buf + total, len - total);
    if (n <= 0) {
      close(fd);
      return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
    }
    total += n;
  }
  close(fd);
#endif
  return LMJCORE_SUCCESS;
}

// 主函数：UUIDv4 生成器
int lmjcore_uuidv4_ptr_gen(void *ctx, lmjcore_ptr out) {
  (void)ctx;
  if (!out) {
    return LMJCORE_ERROR_INVALID_PARAM;
  }

  out[0] = (uint8_t)0;

  // 生成 16 字节随机数据
  int ret = generate_random_bytes(&out[1], 16);
  if (ret != LMJCORE_SUCCESS) {
    return ret;
  }

  // 强制设置 UUIDv4 版本位（第 7 字节高 4 位 = 0100）
  out[7] = (out[7] & 0x0F) | 0x40;
  // 强制设置 variant 位（第 9 字节高 2 位 = 10xx）
  out[9] = (out[9] & 0x3F) | 0x80;

  return LMJCORE_SUCCESS;
}