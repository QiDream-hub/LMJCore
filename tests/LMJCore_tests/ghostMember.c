#include "../../core/include/lmjcore.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 自定义指针生成器（使用简单的递增ID）
static int test_ptr_generator(void *ctx, uint8_t out[LMJCORE_PTR_LEN]) {
  static uint64_t counter = 0;

  out[0] = LMJCORE_OBJ; // 对象类型

  // 使用大端序填充计数器
  for (int i = 0; i < 8; i++) {
    out[1 + i] = (counter >> (56 - i * 8)) & 0xFF;
  }

  counter++;
  return LMJCORE_SUCCESS;
}

// 创建幽灵成员的工具函数（通过直接操作LMDB）
static int create_ghost_member_direct(MDB_env *mdb_env, MDB_dbi main_dbi,
                                      const lmjcore_ptr obj_ptr,
                                      const char *member_name,
                                      const char *value) {
  MDB_txn *txn = NULL;
  int rc = mdb_txn_begin(mdb_env, NULL, 0, &txn);
  if (rc != MDB_SUCCESS) {
    return rc;
  }

  // 直接在main库写入，不注册到arr库
  size_t key_len = LMJCORE_PTR_LEN + strlen(member_name);
  uint8_t key[key_len];

  memcpy(key, obj_ptr, LMJCORE_PTR_LEN);
  memcpy(key + LMJCORE_PTR_LEN, member_name, strlen(member_name));

  MDB_val mdb_key = {.mv_size = key_len, .mv_data = key};
  MDB_val mdb_val = {.mv_size = strlen(value), .mv_data = (void *)value};

  rc = mdb_put(txn, main_dbi, &mdb_key, &mdb_val, 0);
  if (rc != MDB_SUCCESS) {
    mdb_txn_abort(txn);
    return rc;
  }

  rc = mdb_txn_commit(txn);
  return rc;
}

// 访问内部结构（仅供测试使用）
struct lmjcore_env_internal {
  MDB_env *mdb_env;
  MDB_dbi main_dbi;
  MDB_dbi arr_dbi;
  lmjcore_ptr_generator_fn ptr_generator;
  void *ptr_gen_ctx;
};

// 打印指针的字符串表示
static void print_ptr(const lmjcore_ptr ptr) {
  char str_buf[LMJCORE_PTR_STRING_BUF_SIZE];
  lmjcore_ptr_to_string(ptr, str_buf, sizeof(str_buf));
  printf("%s", str_buf);
}

// 打印审计报告
static void print_audit_report(const lmjcore_audit_report *report,
                               const uint8_t *report_buf) {
  if (!report) {
    printf("审计报告为空\n");
    return;
  }

  printf("=== 审计报告 ===\n");
  printf("发现 %zu 个幽灵成员:\n", report->audit_cont);

  for (size_t i = 0; i < report->audit_cont; i++) {
    const lmjcore_audit_descriptor *desc = &report->audit_descriptor[i];

    // 打印指针
    printf("\n[%zu] 对象指针: ", i + 1);
    print_ptr(desc->ptr);
    printf("\n");

    // 打印成员名
    printf("  成员名: ");
    const char *member_name =
        (const char *)(report_buf + desc->member.member_name.value_offset);
    fwrite(member_name, 1, desc->member.member_name.value_len, stdout);
    printf("\n");

    // 打印幽灵值
    printf("  幽灵值: ");
    if (desc->member.member_value.value_len > 0) {
      const char *member_value =
          (const char *)(report_buf + desc->member.member_value.value_offset);
      fwrite(member_value, 1, desc->member.member_value.value_len, stdout);
    } else {
      printf("(空值)");
    }
    printf("\n");
  }
  printf("================\n\n");
}

// 验证对象是否存在幽灵成员
static int verify_no_ghost_members(lmjcore_env *env,
                                   const lmjcore_ptr obj_ptr) {
  lmjcore_txn *txn = NULL;
  int rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn);
  if (rc != LMJCORE_SUCCESS) {
    fprintf(stderr, "创建只读事务失败: %d\n", rc);
    return rc;
  }

  // 分配审计缓冲区
  size_t report_buf_size = 4096;
  uint8_t *report_buf = malloc(report_buf_size);
  if (!report_buf) {
    lmjcore_txn_abort(txn);
    return LMJCORE_ERROR_MEMORY_ALLOCATION_FAILED;
  }

  lmjcore_audit_report *report = NULL;
  rc = lmjcore_audit_object(txn, obj_ptr, report_buf, report_buf_size, &report);

  if (rc == LMJCORE_SUCCESS) {
    if (report->audit_cont > 0) {
      printf("❌ 发现 %zu 个幽灵成员未修复\n", report->audit_cont);
      print_audit_report(report, report_buf);
      rc = -1; // 自定义错误码，表示存在幽灵成员
    } else {
      printf("✅ 对象无幽灵成员\n");
    }
  } else {
    fprintf(stderr, "审计失败: %d (错误码: %d)\n", rc, rc);
  }

  free(report_buf);
  lmjcore_txn_abort(txn);
  return rc;
}

int main() {
  printf("=== LMJCore 幽灵成员审计测试（修复版）===\n\n");

  // 1. 初始化环境
  lmjcore_env *env = NULL;
  const char *test_db_path = "./lmjcore_db/test_ghost_db_fixed";

  int rc =
      lmjcore_init(test_db_path, 1024 * 1024 * 10, // 10MB
                   0 | LMJCORE_ENV_NOSUBDIR, test_ptr_generator, NULL, &env);
  if (rc != LMJCORE_SUCCESS) {
    fprintf(stderr, "初始化失败: %d\n", rc);
    return 1;
  }
  printf("✅ 环境初始化成功\n");

  // 2. 创建测试对象
  lmjcore_txn *txn = NULL;
  rc = lmjcore_txn_begin(env, NULL, 0, &txn);
  if (rc != LMJCORE_SUCCESS) {
    fprintf(stderr, "创建写事务失败: %d\n", rc);
    lmjcore_cleanup(env);
    return 1;
  }

  lmjcore_ptr test_obj;
  rc = lmjcore_obj_create(txn, test_obj);
  if (rc != LMJCORE_SUCCESS) {
    fprintf(stderr, "创建对象失败: %d\n", rc);
    lmjcore_txn_abort(txn);
    lmjcore_cleanup(env);
    return 1;
  }

  printf("✅ 创建测试对象: ");
  print_ptr(test_obj);
  printf("\n");

  // 3. 先正常写入一些成员
  printf("\n--- 步骤1: 正常写入成员 ---\n");
  rc = lmjcore_obj_member_put(txn, test_obj, (uint8_t *)"name", 4,
                              (uint8_t *)"Test User", 9);
  printf("写入 name: %s\n", rc == LMJCORE_SUCCESS ? "成功" : "失败");

  rc = lmjcore_obj_member_put(txn, test_obj, (uint8_t *)"age", 3,
                              (uint8_t *)"25", 2);
  printf("写入 age: %s\n", rc == LMJCORE_SUCCESS ? "成功" : "失败");

  // 提交事务
  rc = lmjcore_txn_commit(txn);
  if (rc != LMJCORE_SUCCESS) {
    fprintf(stderr, "提交事务失败: %d\n", rc);
    lmjcore_cleanup(env);
    return 1;
  }
  printf("✅ 事务提交成功\n");

  // 4. 故意创建幽灵成员（需要直接操作LMDB）
  printf("\n--- 步骤2: 创建幽灵成员 ---\n");

  // 获取内部结构以访问LMDB句柄
  struct lmjcore_env_internal *env_internal =
      (struct lmjcore_env_internal *)env;

  // 幽灵成员1
  rc = create_ghost_member_direct(env_internal->mdb_env, env_internal->main_dbi,
                                  test_obj, "ghost_field1", "幽灵数据1");
  printf("创建幽灵成员1: %s\n", rc == LMJCORE_SUCCESS ? "成功" : "失败");

  // 幽灵成员2
  rc = create_ghost_member_direct(env_internal->mdb_env, env_internal->main_dbi,
                                  test_obj, "ghost_field2", "幽灵数据2");
  printf("创建幽灵成员2: %s\n", rc == LMJCORE_SUCCESS ? "成功" : "失败");

  // 5. 审计幽灵成员
  printf("\n--- 步骤3: 执行审计 ---\n");

  // 打开只读事务进行审计
  rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn);
  if (rc != LMJCORE_SUCCESS) {
    fprintf(stderr, "创建只读事务失败: %d\n", rc);
    lmjcore_cleanup(env);
    return 1;
  }

  // 分配审计缓冲区
  size_t report_buf_size = 4096;
  uint8_t *report_buf = malloc(report_buf_size);
  if (!report_buf) {
    fprintf(stderr, "内存分配失败\n");
    lmjcore_txn_abort(txn);
    lmjcore_cleanup(env);
    return 1;
  }

  lmjcore_audit_report *report = NULL;
  rc =
      lmjcore_audit_object(txn, test_obj, report_buf, report_buf_size, &report);

  if (rc == LMJCORE_SUCCESS) {
    printf("✅ 审计执行成功\n");
    print_audit_report(report, report_buf);
  } else {
    fprintf(stderr, "审计失败: %d\n", rc);
  }

  // 中止只读事务
  lmjcore_txn_abort(txn);

  // 6. 修复幽灵成员
  printf("\n--- 步骤4: 修复幽灵成员 ---\n");

  if (report && report->audit_cont > 0) {
    // 打开写事务进行修复
    rc = lmjcore_txn_begin(env, NULL, 0, &txn);
    if (rc != LMJCORE_SUCCESS) {
      fprintf(stderr, "创建修复事务失败: %d\n", rc);
      free(report_buf);
      lmjcore_cleanup(env);
      return 1;
    }

    rc = lmjcore_repair_object(txn, report_buf, report_buf_size, report);
    if (rc == LMJCORE_SUCCESS) {
      printf("✅ 修复成功，删除了 %zu 个幽灵成员\n", report->audit_cont);
    } else {
      fprintf(stderr, "修复失败: %d\n", rc);
    }

    // 提交修复事务
    int commit_rc = lmjcore_txn_commit(txn);
    if (commit_rc != LMJCORE_SUCCESS) {
      fprintf(stderr, "提交修复事务失败: %d\n", commit_rc);
    }
  } else {
    printf("⚠️ 无需修复\n");
  }

  // 7. 验证修复结果
  printf("\n--- 步骤5: 验证修复结果 ---\n");
  rc = verify_no_ghost_members(env, test_obj);

  // 8. 完整性检查：读取对象验证
  printf("\n--- 步骤6: 完整性验证 ---\n");
  rc = lmjcore_txn_begin(env, NULL, LMJCORE_TXN_READONLY, &txn);
  if (rc == LMJCORE_SUCCESS) {
    // 分配读取缓冲区
    uint8_t read_buf[1024];
    lmjcore_result_obj *result = NULL;

    rc = lmjcore_obj_get(txn, test_obj, read_buf, sizeof(read_buf), &result);
    if (rc == LMJCORE_SUCCESS) {
      printf("✅ 对象读取成功\n");
      printf("  有效成员数: %zu\n", result->member_count);
      printf("  读取错误数: %zu\n", result->error_count);

      // 打印成员列表
      if (result->member_count > 0) {
        printf("  成员列表:\n");
        for (size_t i = 0; i < result->member_count; i++) {
          const lmjcore_member_descriptor *desc = &result->members[i];
          printf("    - ");
          fwrite(read_buf + desc->member_name.value_offset, 1,
                 desc->member_name.value_len, stdout);
          printf("\n");
        }
      }
    } else {
      printf("❌ 对象读取失败: %d\n", rc);
    }

    lmjcore_txn_abort(txn);
  }

  // 清理资源
  free(report_buf);
  lmjcore_cleanup(env);

  printf("\n=== 测试完成 ===\n");
  return 0;
}