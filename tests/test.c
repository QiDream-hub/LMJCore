#include <lmdb.h>
#include <stdio.h>
#include <string.h>

int test1() {
  MDB_env *env = NULL;
  MDB_txn *txn = NULL;
  MDB_dbi db;
  MDB_val key, value;
  int rc;

  // 1. 创建环境
  rc = mdb_env_create(&env);
  if (rc != MDB_SUCCESS) {
    fprintf(stderr, "Failed to create environment: %s\n", mdb_strerror(rc));
    return 1;
  }

  // 2. 打开环境
  rc = mdb_env_open(env, "./lmjcore_db", 0, 0664);
  if (rc != MDB_SUCCESS) {
    fprintf(stderr, "Failed to open environment: %s\n", mdb_strerror(rc));
    mdb_env_close(env);
    return 1;
  }

  // 3. 开始写入事务（去掉 MDB_RDONLY）
  rc = mdb_txn_begin(env, NULL, 0, &txn);
  if (rc != MDB_SUCCESS) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    mdb_env_close(env);
    return 1;
  }

  // 4. 打开数据库（写入时需要 MDB_CREATE）
  rc = mdb_dbi_open(txn, NULL, MDB_CREATE, &db);
  if (rc != MDB_SUCCESS) {
    fprintf(stderr, "Failed to open database: %s\n", mdb_strerror(rc));
    mdb_txn_abort(txn);
    mdb_env_close(env);
    return 1;
  }

  // 5. 准备要写入的key和value
  char key_str[] = "test_key";
  char value_str[] = "test_value";

  key.mv_size = strlen(key_str);
  key.mv_data = key_str;

  value.mv_size = strlen(value_str);
  value.mv_data = value_str;

  // 6. 写入数据
  rc = mdb_put(txn, db, &key, &value, 0);
  if (rc != MDB_SUCCESS) {
    fprintf(stderr, "Failed to write data: %s\n", mdb_strerror(rc));
    mdb_txn_abort(txn);
  } else {
    printf("Data written successfully.\n");
    MDB_val r_value;
    rc = mdb_get(txn, db, &key, &r_value);
    printf("%d,%.*s", rc, (int)r_value.mv_size, (char *)r_value.mv_data);
    rc = mdb_txn_commit(txn); // 写入事务必须 commit
    if (rc != MDB_SUCCESS) {
      fprintf(stderr, "Failed to commit transaction: %s\n", mdb_strerror(rc));
    }
  }

  // 7. 清理资源
  mdb_dbi_close(env, db);
  mdb_env_close(env);

  return 0;
}

void test2() { printf("%s", mdb_strerror(9999)); }

int main() {
  test2();
  return 0;
}