#include "../../Toolkit/result_parser/include/result_parser.h"
#include "../../core/include/lmjcore.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// 辅助：构建一个模拟的对象结果缓冲区
// 布局: [header][desc0][desc1]...[value_data...][name_data...]
// 注意：值数据从后往前写，名称也从后往前（但本例中我们统一从尾部开始分配）
static void build_mock_obj_result(uint8_t *buf, size_t buf_size,
                                  lmjcore_result_obj **out_head,
                                  const char *names[], const char *values[],
                                  size_t count) {

  // 计算所需空间
  size_t header_size =
      sizeof(lmjcore_result_obj) + sizeof(lmjcore_member_descriptor) * count;
  size_t data_size = 0;
  for (size_t i = 0; i < count; ++i) {
    data_size += strlen(names[i]) + strlen(values[i]);
  }

  assert(header_size + data_size <= buf_size);

  // 清零
  memset(buf, 0, buf_size);

  lmjcore_result_obj *head = (lmjcore_result_obj *)buf;
  head->error_count = 0;
  head->member_count = count;

  lmjcore_member_descriptor *descs =
      (lmjcore_member_descriptor *)(buf + sizeof(lmjcore_result_obj));

  // 数据从尾部开始写入（模拟 lmjcore 的实际布局）
  uint8_t *data_ptr = buf + buf_size;

  for (size_t i = 0; i < count; ++i) {
    size_t vlen = strlen(values[i]);
    size_t nlen = strlen(names[i]);

    // 先写 value
    data_ptr -= vlen;
    memcpy(data_ptr, values[i], vlen);
    descs[i].member_value.value_offset = data_ptr - buf;
    descs[i].member_value.value_len = vlen;

    // 再写 name
    data_ptr -= nlen;
    memcpy(data_ptr, names[i], nlen);
    descs[i].member_name.value_offset = data_ptr - buf;
    descs[i].member_name.value_len = nlen;
  }

  *out_head = head;
}

// 辅助：构建模拟的数组结果缓冲区
static void build_mock_arr_result(uint8_t *buf, size_t buf_size,
                                  lmjcore_result_arr **out_head,
                                  const char *elements[], size_t count) {

  size_t header_size =
      sizeof(lmjcore_result_arr) + sizeof(lmjcore_descriptor) * count;
  size_t data_size = 0;
  for (size_t i = 0; i < count; ++i) {
    data_size += strlen(elements[i]);
  }

  assert(header_size + data_size <= buf_size);

  memset(buf, 0, buf_size);

  lmjcore_result_arr *head = (lmjcore_result_arr *)buf;
  head->error_count = 0;
  head->element_count = count;

  lmjcore_descriptor *descs =
      (lmjcore_descriptor *)(buf + sizeof(lmjcore_result_arr));
  uint8_t *data_ptr = buf + buf_size;

  for (size_t i = 0; i < count; ++i) {
    size_t elen = strlen(elements[i]);
    data_ptr -= elen;
    memcpy(data_ptr, elements[i], elen);
    descs[i].value_offset = data_ptr - buf;
    descs[i].value_len = elen;
  }

  *out_head = head;
}

// 辅助：检查错误信息
static void test_error_parsing() {
  uint8_t buf[256];
  memset(buf, 0, sizeof(buf));

  lmjcore_result_obj *obj = (lmjcore_result_obj *)buf;
  obj->error_count = 2;
  obj->errors[0].error_code = LMJCORE_ERROR_ENTITY_NOT_FOUND;
  obj->errors[1].error_code = LMJCORE_ERROR_MEMBER_MISSING;

  assert(lmjcore_parser_error_count(buf) == 2);
  assert(lmjcore_parser_has_error(buf, LMJCORE_ERROR_ENTITY_NOT_FOUND));
  assert(lmjcore_parser_has_error(buf, LMJCORE_ERROR_MEMBER_MISSING));

  const lmjcore_read_error *err;
  assert(lmjcore_parser_get_error(buf, 0, &err) == LMJCORE_SUCCESS);
  assert(err->error_code == LMJCORE_ERROR_ENTITY_NOT_FOUND);

  assert(lmjcore_parser_get_error(buf, 1, &err) == LMJCORE_SUCCESS);
  assert(err->error_code == LMJCORE_ERROR_MEMBER_MISSING);

  assert(lmjcore_parser_get_error(buf, 2, &err) ==
         LMJCORE_ERROR_ENTITY_NOT_FOUND);
}

int main(void) {
  printf("🧪 开始测试 result_parser...\n");

  // ===== 测试对象解析 =====
  {
    const char *names[] = {"name", "age", "city"};
    const char *values[] = {"Alice", "30", "Beijing"};
    uint8_t buf[1024];
    lmjcore_result_obj *obj_result;

    build_mock_obj_result(buf, sizeof(buf), &obj_result, names, values, 3);

    // 测试成员数量
    assert(lmjcore_parser_obj_member_count(obj_result) == 3);

    // 测试按索引获取
    const uint8_t *name, *val;
    size_t nlen, vlen;
    assert(lmjcore_parser_obj_get_member(obj_result, buf, 0, &name, &nlen, &val,
                                         &vlen) == LMJCORE_SUCCESS);
    assert(nlen == 4 && memcmp(name, "name", 4) == 0);
    assert(vlen == 5 && memcmp(val, "Alice", 5) == 0);

    // 测试查找成员
    assert(lmjcore_parser_obj_find_member(obj_result, buf, (uint8_t *)"city", 4,
                                          &val, &vlen) == LMJCORE_SUCCESS);
    assert(vlen == 7 && memcmp(val, "Beijing", 7) == 0);

    assert(lmjcore_parser_obj_find_member(obj_result, buf, (uint8_t *)"missing",
                                          7, NULL, NULL) ==
           LMJCORE_ERROR_ENTITY_NOT_FOUND);

    // 越界索引
    assert(lmjcore_parser_obj_get_member(obj_result, buf, 5, NULL, NULL, NULL,
                                         NULL) ==
           LMJCORE_ERROR_ENTITY_NOT_FOUND);
  }

  // ===== 测试数组解析 =====
  {
    const char *elems[] = {"apple", "banana", "cherry"};
    uint8_t buf[1024];
    lmjcore_result_arr *arr_result;

    build_mock_arr_result(buf, sizeof(buf), &arr_result, elems, 3);

    assert(lmjcore_parser_arr_element_count(arr_result) == 3);

    const uint8_t *el;
    size_t elen;
    assert(lmjcore_parser_arr_get_element(arr_result, buf, 1, &el, &elen) ==
           LMJCORE_SUCCESS);
    assert(elen == 6 && memcmp(el, "banana", 6) == 0);

    size_t idx;
    assert(lmjcore_parser_arr_find_element(arr_result, buf, (uint8_t *)"cherry",
                                           6, &idx) == LMJCORE_SUCCESS);
    assert(idx == 2);

    assert(lmjcore_parser_arr_find_element(arr_result, buf, (uint8_t *)"grape",
                                           5, NULL) ==
           LMJCORE_ERROR_ENTITY_NOT_FOUND);

    assert(lmjcore_parser_arr_get_element(arr_result, buf, 10, NULL, NULL) ==
           LMJCORE_ERROR_ENTITY_NOT_FOUND);
  }

  // ===== 测试错误解析 =====
  test_error_parsing();

  printf("✅ 所有测试通过！\n");
  return 0;
}