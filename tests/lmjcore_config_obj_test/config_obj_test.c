#include "../../Toolkit/config_obj_toolkit/include/lmjcore_config.h"
#include <lmdb.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_config_basic_operations() {
    printf("Testing config object basic operations...\n");
    
    lmjcore_env *env = NULL;
    lmjcore_txn *txn = NULL;
    
    // 初始化环境
    int ret = lmjcore_init("./lmjcore_db/config_test/_1", 1024 * 1024, 0 | LMJCORE_NOSUBDIR, NULL, NULL, &env);
    assert(ret == LMJCORE_SUCCESS);
    
    // 开始写事务
    ret = lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 测试1: 检查配置对象初始不存在
    int exist = lmjcore_config_object_exist(txn);
    assert(exist == 0);
    
    // 测试2: 确保配置对象存在
    ret = lmjcore_config_object_ensure(txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 测试3: 确认配置对象现在存在
    exist = lmjcore_config_object_exist(txn);
    assert(exist == 1);
    
    // 测试4: 设置配置项
    const char *test_key = "database.host";
    const char *test_value = "localhost";
    ret = lmjcore_config_set(txn, (const uint8_t*)test_key, strlen(test_key),
                            (const uint8_t*)test_value, strlen(test_value));
    assert(ret == LMJCORE_SUCCESS);
    
    // 测试5: 获取配置项
    uint8_t value_buf[256];
    size_t value_size = 0;
    ret = lmjcore_config_get(txn, (const uint8_t*)test_key, strlen(test_key),
                            value_buf, sizeof(value_buf), &value_size);
    assert(ret == LMJCORE_SUCCESS);
    assert(value_size == strlen(test_value));
    assert(memcmp(value_buf, test_value, value_size) == 0);
    
    // 测试6: 获取完整配置对象
    uint8_t result_buf[1024];
    lmjcore_result *result_head = NULL;
    ret = lmjcore_config_get_all(txn, LMJCORE_MODE_STRICT, 
                                result_buf, sizeof(result_buf), &result_head);
    assert(ret == LMJCORE_SUCCESS);
    assert(result_head != NULL);
    assert(result_head->count >= 1);
    
    // 提交事务
    ret = lmjcore_txn_commit(txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 清理
    lmjcore_cleanup(env);
    
    printf("All config object tests passed!\n");
}

void test_config_lazy_initialization() {
    printf("Testing config object lazy initialization...\n");
    
    lmjcore_env *env = NULL;
    lmjcore_txn *txn = NULL;
    
    // 初始化环境
    int ret = lmjcore_init("./lmjcore_db/config_test/_2", 1024 * 1024, 0 | LMJCORE_NOSUBDIR, NULL, NULL, &env);
    assert(ret == LMJCORE_SUCCESS);
    
    // 开始写事务
    ret = lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 测试: 直接设置配置项（应该自动创建配置对象）
    const char *test_key = "app.name";
    const char *test_value = "LMJCore";
    ret = lmjcore_config_set(txn, (const uint8_t*)test_key, strlen(test_key),
                            (const uint8_t*)test_value, strlen(test_value));
    assert(ret == LMJCORE_SUCCESS);
    
    // 验证配置对象确实存在
    int exist = lmjcore_config_object_exist(txn);
    assert(exist == 1);
    
    // 验证配置项可读取
    uint8_t value_buf[256];
    size_t value_size = 0;
    ret = lmjcore_config_get(txn, (const uint8_t*)test_key, strlen(test_key),
                            value_buf, sizeof(value_buf), &value_size);
    assert(ret == LMJCORE_SUCCESS);
    assert(value_size == strlen(test_value));
    assert(memcmp(value_buf, test_value, value_size) == 0);
    
    // 提交事务
    ret = lmjcore_txn_commit(txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 清理
    lmjcore_cleanup(env);
    
    printf("Config lazy initialization test passed!\n");
}

void test_config_error_handling() {
    printf("Testing config object error handling...\n");
    
    lmjcore_env *env = NULL;
    lmjcore_txn *txn = NULL;
    
    // 初始化环境
    int ret = lmjcore_init("./lmjcore_db/config_test/_3", 1024 * 1024, 0 | LMJCORE_NOSUBDIR, NULL, NULL, &env);
    assert(ret == LMJCORE_SUCCESS);
    
    // 开始读事务
    ret = lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 测试1: 获取不存在的配置对象
    uint8_t value_buf[256];
    size_t value_size = 0;
    const char *test_key = "nonexistent.key";
    ret = lmjcore_config_get(txn, (const uint8_t*)test_key, strlen(test_key),
                            value_buf, sizeof(value_buf), &value_size);
    assert(ret == LMJCORE_ERROR_ENTITY_NOT_FOUND);
    
    // 测试2: 获取不存在的配置项（但配置对象存在）
    // 首先创建配置对象
    lmjcore_txn_abort(txn); // 结束读事务
    ret = lmjcore_txn_begin(env, LMJCORE_TXN_WRITE, &txn);
    assert(ret == LMJCORE_SUCCESS);
    
    ret = lmjcore_config_object_ensure(txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 现在尝试获取不存在的配置项
    lmjcore_txn_commit(txn);
    ret = lmjcore_txn_begin(env, LMJCORE_TXN_READONLY, &txn);
    assert(ret == LMJCORE_SUCCESS);

    ret = lmjcore_config_get(txn, (const uint8_t*)test_key, strlen(test_key),
                            value_buf, sizeof(value_buf), &value_size);
    assert(ret == LMJCORE_ERROR_MEMBER_NOT_FOUND);
    
    // 测试3: 无效参数
    ret = lmjcore_config_set(NULL, (const uint8_t*)test_key, strlen(test_key),
                            (const uint8_t*)"value", 5);
    assert(ret == LMJCORE_ERROR_INVALID_PARAM);
    
    ret = lmjcore_config_set(txn, NULL, strlen(test_key),
                            (const uint8_t*)"value", 5);
    assert(ret == LMJCORE_ERROR_INVALID_PARAM);
    
    // 提交事务
    ret = lmjcore_txn_commit(txn);
    assert(ret == LMJCORE_SUCCESS);
    
    // 清理
    lmjcore_cleanup(env);
    
    printf("Config error handling test passed!\n");
}

int main() {
    printf("Starting config object tests...\n\n");
    
    test_config_basic_operations();
    printf("\n");
    
    test_config_lazy_initialization();
    printf("\n");
    
    test_config_error_handling();
    printf("\n");
    
    printf("All config object tests completed successfully!\n");
    return 0;
}