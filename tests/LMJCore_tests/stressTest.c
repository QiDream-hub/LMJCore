#include "../../Toolkit/ptr_uuid_gen/include/lmjcore_uuid_gen.h"
#include "../../core/include/lmjcore.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define TEST_DB_PATH "./lmjcore_db/"
#define NUM_THREADS 16
#define OPERATIONS_PER_THREAD 1000
#define MAX_MEMBER_NAME_LEN 50
#define MAX_VALUE_LEN 100

#define ENV_OP 0

// 时间戳缓冲区大小
#define TIMESTAMP_BUFFER_SIZE 64

typedef struct {
  lmjcore_env *env;
  int thread_id;
  int success_count;
  int error_count;
} thread_context;

// 获取当前时间戳字符串
void get_timestamp(char *buffer, size_t buffer_size) {
  struct timeval tv;
  struct tm *tm_info;
  char time_buffer[64];
  int milliseconds;

  gettimeofday(&tv, NULL);
  tm_info = localtime(&tv.tv_sec);

  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
  milliseconds = tv.tv_usec / 1000;

  snprintf(buffer, buffer_size, "[%s.%03d]", time_buffer, milliseconds);
}

// 带时间戳的打印函数
void print_with_timestamp(const char *format, ...) {
  va_list args;
  char timestamp[TIMESTAMP_BUFFER_SIZE];

  get_timestamp(timestamp, sizeof(timestamp));
  printf("%s ", timestamp);

  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  fflush(stdout); // 确保及时输出
}

// 测试数据生成
void generate_random_string(char *buffer, size_t len) {
  const char charset[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (size_t i = 0; i < len - 1; i++) {
    buffer[i] = charset[rand() % (sizeof(charset) - 1)];
  }
  buffer[len - 1] = '\0';
}

// 压力测试主函数
void *stress_test_thread(void *arg) {
  thread_context *ctx = (thread_context *)arg;
  lmjcore_env *env = ctx->env;

  print_with_timestamp("Thread %d starting...\n", ctx->thread_id);

  for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
    lmjcore_txn *txn = NULL;
    lmjcore_ptr obj_ptr;

    // 开始写事务
    int ret = lmjcore_txn_begin(env, NULL, 0, &txn);
    if (ret != LMJCORE_SUCCESS) {
      ctx->error_count++;
      continue;
    }

    // 创建对象
    ret = lmjcore_obj_create(txn, obj_ptr);
    if (ret != LMJCORE_SUCCESS) {
      ctx->error_count++;
      lmjcore_txn_abort(txn);
      continue;
    }

    // 添加多个成员
    char member_name[MAX_MEMBER_NAME_LEN];
    char value[MAX_VALUE_LEN];
    int num_members = (rand() % 10) + 1; // 1-10个成员

    for (int j = 0; j < num_members; j++) {
      generate_random_string(member_name, sizeof(member_name));
      generate_random_string(value, sizeof(value));

      ret = lmjcore_obj_member_put(txn, obj_ptr, (const uint8_t *)member_name,
                                   strlen(member_name), (const uint8_t *)value,
                                   strlen(value));
      if (ret != LMJCORE_SUCCESS) {
        ctx->error_count++;
        break;
      }
    }

    // 提交事务
    if (lmjcore_txn_commit(txn) == LMJCORE_SUCCESS) {
      ctx->success_count++;
    } else {
      ctx->error_count++;
    }

    // 随机读取测试
    if (i % 10 == 0) { // 每10次操作进行一次读取测试
      lmjcore_txn *read_txn = NULL;
      if (lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &read_txn) ==
          LMJCORE_SUCCESS) {
        uint8_t result_buf[4096];
        lmjcore_result_obj *result = NULL;

        ret = lmjcore_obj_get(read_txn, obj_ptr, result_buf, sizeof(result_buf),
                              &result);
        if (ret == LMJCORE_SUCCESS) {
          ctx->success_count++;
        } else {
          ctx->error_count++;
        }

        lmjcore_txn_commit(read_txn);
      }
    }

    // 进度报告
    if (i % 100 == 0) {
      print_with_timestamp("Thread %d: Completed %d/%d operations\n",
                           ctx->thread_id, i, OPERATIONS_PER_THREAD);
    }
  }

  print_with_timestamp("Thread %d finished: %d success, %d errors\n",
                       ctx->thread_id, ctx->success_count, ctx->error_count);

  return NULL;
}

// 数组压力测试
void *array_stress_test_thread(void *arg) {
  thread_context *ctx = (thread_context *)arg;
  lmjcore_env *env = ctx->env;

  print_with_timestamp("Array Thread %d starting...\n", ctx->thread_id);

  for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
    lmjcore_txn *txn = NULL;
    lmjcore_ptr arr_ptr;

    // 开始写事务
    int ret = lmjcore_txn_begin(env, NULL, 0, &txn);
    if (ret != LMJCORE_SUCCESS) {
      ctx->error_count++;
      continue;
    }

    // 创建数组
    ret = lmjcore_arr_create(txn, arr_ptr);
    if (ret != LMJCORE_SUCCESS) {
      ctx->error_count++;
      lmjcore_txn_abort(txn);
      continue;
    }

    // 添加多个元素
    char value[MAX_VALUE_LEN];
    int num_elements = (rand() % 20) + 1; // 1-20个元素

    for (int j = 0; j < num_elements; j++) {
      generate_random_string(value, sizeof(value));

      ret = lmjcore_arr_append(txn, arr_ptr, (const uint8_t *)value,
                               strlen(value));
      if (ret != LMJCORE_SUCCESS) {
        ctx->error_count++;
        break;
      }
    }

    // 提交事务
    if (lmjcore_txn_commit(txn) == LMJCORE_SUCCESS) {
      ctx->success_count++;
    } else {
      ctx->error_count++;
    }

    // 进度报告
    if (i % 100 == 0) {
      print_with_timestamp("Array Thread %d: Completed %d/%d operations\n",
                           ctx->thread_id, i, OPERATIONS_PER_THREAD);
    }
  }

  print_with_timestamp("Array Thread %d finished: %d success, %d errors\n",
                       ctx->thread_id, ctx->success_count, ctx->error_count);

  return NULL;
}

// 混合操作测试
void *mixed_operations_thread(void *arg) {
  thread_context *ctx = (thread_context *)arg;
  lmjcore_env *env = ctx->env;

  print_with_timestamp("Mixed Thread %d starting...\n", ctx->thread_id);

  lmjcore_ptr persistent_obj;
  int obj_created = 0;

  // 首先创建一个持久化对象
  lmjcore_txn *init_txn = NULL;
  if (lmjcore_txn_begin(env, NULL, 0, &init_txn) == LMJCORE_SUCCESS) {
    if (lmjcore_obj_create(init_txn, persistent_obj) == LMJCORE_SUCCESS) {
      obj_created = 1;
      lmjcore_txn_commit(init_txn);
    } else {
      lmjcore_txn_abort(init_txn);
    }
  }

  for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
    int operation_type = rand() % 4;

    switch (operation_type) {
    case 0: { // 对象创建和操作
      lmjcore_txn *txn = NULL;
      lmjcore_ptr obj_ptr;

      if (lmjcore_txn_begin(env, NULL, 0, &txn) == LMJCORE_SUCCESS) {
        if (lmjcore_obj_create(txn, obj_ptr) == LMJCORE_SUCCESS) {
          char member_name[MAX_MEMBER_NAME_LEN];
          char value[MAX_VALUE_LEN];

          generate_random_string(member_name, sizeof(member_name));
          generate_random_string(value, sizeof(value));

          if (lmjcore_obj_member_put(txn, obj_ptr, (const uint8_t *)member_name,
                                     strlen(member_name),
                                     (const uint8_t *)value,
                                     strlen(value)) == LMJCORE_SUCCESS) {
            ctx->success_count++;
          } else {
            ctx->error_count++;
          }
        }
        lmjcore_txn_commit(txn);
      }
      break;
    }

    case 1: { // 数组操作
      lmjcore_txn *txn = NULL;
      lmjcore_ptr arr_ptr;

      if (lmjcore_txn_begin(env, NULL, 0, &txn) == LMJCORE_SUCCESS) {
        if (lmjcore_arr_create(txn, arr_ptr) == LMJCORE_SUCCESS) {
          char value[MAX_VALUE_LEN];
          int num_elements = (rand() % 5) + 1;

          for (int j = 0; j < num_elements; j++) {
            generate_random_string(value, sizeof(value));
            lmjcore_arr_append(txn, arr_ptr, (const uint8_t *)value,
                               strlen(value));
          }
          ctx->success_count++;
        }
        lmjcore_txn_commit(txn);
      }
      break;
    }

    case 2: { // 读取操作
      if (obj_created) {
        lmjcore_txn *txn = NULL;
        if (lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn) ==
            LMJCORE_SUCCESS) {
          uint8_t result_buf[2048];
          lmjcore_result_obj *result = NULL;

          if (lmjcore_obj_get(txn, persistent_obj, result_buf,
                              sizeof(result_buf), &result) == LMJCORE_SUCCESS) {
            ctx->success_count++;
          } else {
            ctx->error_count++;
          }
          lmjcore_txn_commit(txn);
        }
      }
      break;
    }

    case 3: { // 成员操作（如果持久对象存在）
      if (obj_created) {
        lmjcore_txn *txn = NULL;
        if (lmjcore_txn_begin(env, NULL, 0, &txn) == LMJCORE_SUCCESS) {
          char member_name[MAX_MEMBER_NAME_LEN];
          char value[MAX_VALUE_LEN];

          generate_random_string(member_name, sizeof(member_name));
          generate_random_string(value, sizeof(value));

          if (lmjcore_obj_member_put(
                  txn, persistent_obj, (const uint8_t *)member_name,
                  strlen(member_name), (const uint8_t *)value,
                  strlen(value)) == LMJCORE_SUCCESS) {
            ctx->success_count++;
          } else {
            ctx->error_count++;
          }
          lmjcore_txn_commit(txn);
        }
      }
      break;
    }
    }

    // 进度报告
    if (i % 100 == 0) {
      print_with_timestamp("Mixed Thread %d: Completed %d/%d operations\n",
                           ctx->thread_id, i, OPERATIONS_PER_THREAD);
    }
  }

  print_with_timestamp("Mixed Thread %d finished: %d success, %d errors\n",
                       ctx->thread_id, ctx->success_count, ctx->error_count);

  return NULL;
}

// 内存泄漏和资源测试
void memory_leak_test() {
  print_with_timestamp("Starting memory leak test...\n");

  lmjcore_env *env = NULL;

  // 初始化环境
  if (lmjcore_init(TEST_DB_PATH, 1024 * 1024 * 100, ENV_OP,
                   lmjcore_uuidv4_ptr_gen, NULL, &env) != LMJCORE_SUCCESS) {
    print_with_timestamp(
        "Failed to initialize environment for memory leak test\n");
    return;
  }

  // 执行大量操作
  for (int i = 0; i < 10000; i++) {
    lmjcore_txn *txn = NULL;
    lmjcore_ptr ptr;

    if (lmjcore_txn_begin(env, NULL, 0, &txn) == LMJCORE_SUCCESS) {
      if (i % 2 == 0) {
        lmjcore_obj_create(txn, ptr);
      } else {
        lmjcore_arr_create(txn, ptr);
      }
      lmjcore_txn_commit(txn);
    }

    if (i % 1000 == 0) {
      print_with_timestamp("Memory leak test progress: %d/10000\n", i);
    }
  }

  // 清理
  lmjcore_cleanup(env);
  print_with_timestamp("Memory leak test completed\n");
}

int main() {
  char timestamp[TIMESTAMP_BUFFER_SIZE];
  get_timestamp(timestamp, sizeof(timestamp));

  printf("%s Starting LMJCore Stress Test...\n", timestamp);

  // 初始化随机种子
  srand((unsigned int)time(NULL));

  // 清理之前的测试数据库
  // system("rm -rf " TEST_DB_PATH);

  lmjcore_env *env = NULL;
  pthread_t threads[NUM_THREADS];
  thread_context contexts[NUM_THREADS];

  // 初始化LMJCore环境
  int ret = lmjcore_init(TEST_DB_PATH, 1024 * 1024 * 500, ENV_OP,
                         lmjcore_uuidv4_ptr_gen, NULL, &env);
  if (ret != LMJCORE_SUCCESS) {
    print_with_timestamp("Failed to initialize LMJCore: %d\n", ret);
    return 1;
  }

  print_with_timestamp("LMJCore initialized successfully\n");

  // 测试1: 对象操作压力测试
  print_with_timestamp("\n=== Test 1: Object Operations Stress Test ===\n");
  for (int i = 0; i < NUM_THREADS; i++) {
    contexts[i].env = env;
    contexts[i].thread_id = i;
    contexts[i].success_count = 0;
    contexts[i].error_count = 0;
    pthread_create(&threads[i], NULL, stress_test_thread, &contexts[i]);
  }

  // 等待所有线程完成
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // 统计结果
  int total_success = 0;
  int total_errors = 0;
  for (int i = 0; i < NUM_THREADS; i++) {
    total_success += contexts[i].success_count;
    total_errors += contexts[i].error_count;
  }

  print_with_timestamp(
      "Object Test Results: %d successes, %d errors, Success rate: %.2f%%\n",
      total_success, total_errors,
      (float)total_success / (total_success + total_errors) * 100);

  // 测试2: 数组操作压力测试
  print_with_timestamp("\n=== Test 2: Array Operations Stress Test ===\n");
  for (int i = 0; i < NUM_THREADS; i++) {
    contexts[i].env = env;
    contexts[i].thread_id = i;
    contexts[i].success_count = 0;
    contexts[i].error_count = 0;
    pthread_create(&threads[i], NULL, array_stress_test_thread, &contexts[i]);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  total_success = 0;
  total_errors = 0;
  for (int i = 0; i < NUM_THREADS; i++) {
    total_success += contexts[i].success_count;
    total_errors += contexts[i].error_count;
  }

  print_with_timestamp(
      "Array Test Results: %d successes, %d errors, Success rate: %.2f%%\n",
      total_success, total_errors,
      (float)total_success / (total_success + total_errors) * 100);

  // 测试3: 混合操作测试
  print_with_timestamp("\n=== Test 3: Mixed Operations Stress Test ===\n");
  for (int i = 0; i < NUM_THREADS; i++) {
    contexts[i].env = env;
    contexts[i].thread_id = i;
    contexts[i].success_count = 0;
    contexts[i].error_count = 0;
    pthread_create(&threads[i], NULL, mixed_operations_thread, &contexts[i]);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  total_success = 0;
  total_errors = 0;
  for (int i = 0; i < NUM_THREADS; i++) {
    total_success += contexts[i].success_count;
    total_errors += contexts[i].error_count;
  }

  print_with_timestamp(
      "Mixed Test Results: %d successes, %d errors, Success rate: %.2f%%\n",
      total_success, total_errors,
      (float)total_success / (total_success + total_errors) * 100);

  // 测试4: 内存泄漏测试
  print_with_timestamp("\n=== Test 4: Memory Leak Test ===\n");
  memory_leak_test();

  // 清理环境
  lmjcore_cleanup(env);

  print_with_timestamp("\n=== Stress Test Completed ===\n");
  print_with_timestamp(
      "All tests finished. Check system resources for any leaks.\n");

  return 0;
}