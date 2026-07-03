//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "ten_utils/value/value.h"

#include "core_protocols/msgpack/common/common.h"
#include "core_protocols/msgpack/common/value.h"
#include "include_internal/ten_utils/value/value_kv.h"
#include "include_internal/ten_utils/value/value_set.h"
#include "msgpack/object.h"
#include "msgpack/pack.h"
#include "msgpack/unpack.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_kv.h"

bool ten_msgpack_value_deserialize(ten_value_t *value,
                                   msgpack_unpacker *unpacker,
                                   msgpack_unpacked *unpacked) {
  TEN_ASSERT(unpacker, "Invalid argument.");
  TEN_ASSERT(unpacked, "Invalid argument.");

  bool result = true;

  msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
  if (rc == MSGPACK_UNPACK_SUCCESS) {
    if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_POSITIVE_INTEGER) {
      switch (MSGPACK_DATA_I64) {
      case TEN_TYPE_INT8: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_int8(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_INT16: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_int16(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_INT32: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_int32(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_INT64: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_int64(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT8: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_uint8(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT16: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_uint16(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT32: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_uint32(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT64: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_uint64(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_FLOAT32: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_float32(value, MSGPACK_DATA_F64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_FLOAT64: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_float64(value, MSGPACK_DATA_F64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_BOOL: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_set_bool(value, MSGPACK_DATA_BOOL);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_STRING: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_STR) {
            ten_value_set_string_with_size(value, MSGPACK_DATA_STR_PTR,
                                           MSGPACK_DATA_STR_SIZE);
            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_BUF: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_BIN) {
            ten_buf_t *buf = ten_value_peek_buf(value, NULL);
            TEN_ASSERT(buf, "Invalid argument.");
            TEN_ASSERT(ten_buf_check_integrity(buf), "Invalid argument.");

            // To overwrite the old buffer, we deinit it first.
            ten_buf_deinit(buf);
            ten_buf_init_with_copying_data(buf, (uint8_t *)MSGPACK_DATA_BIN_PTR,
                                           MSGPACK_DATA_BIN_SIZE);
            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        }
        break;
      }

      case TEN_TYPE_ARRAY: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_POSITIVE_INTEGER) {
            ten_list_t array = TEN_LIST_INIT_VAL;

            size_t array_items_cnt = MSGPACK_DATA_I64;
            for (size_t i = 0; i < array_items_cnt; i++) {
              ten_value_t *item =
                  ten_msgpack_create_value_through_deserialization(unpacker,
                                                                   unpacked);
              if (!item) {
                goto error;
              }
              ten_list_push_ptr_back(
                  &array, item,
                  (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
            }

            ten_value_set_array_with_move(value, &array);
            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_OBJECT: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_POSITIVE_INTEGER) {
            ten_list_t kv_list = TEN_LIST_INIT_VAL;

            size_t object_kv_cnt = MSGPACK_DATA_I64;
            for (size_t i = 0; i < object_kv_cnt; i++) {
              ten_value_kv_t *kv =
                  ten_msgpack_create_value_kv_through_deserialization(unpacker,
                                                                      unpacked);
              if (!kv) {
                goto error;
              }
              ten_list_push_ptr_back(
                  &kv_list, kv,
                  (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
            }

            ten_value_set_object_with_move(value, &kv_list);
            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      default:
        TEN_ASSERT(0, "Need to implement more types.");
        break;
      }
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  } else {
    TEN_ASSERT(0, "Should not happen.");
  }

  goto done;

error:
  result = false;

done:
  return result;
}

bool ten_msgpack_value_deserialize_inplace(ten_value_t *value,
                                           msgpack_unpacker *unpacker,
                                           msgpack_unpacked *unpacked) {
  TEN_ASSERT(unpacker, "Invalid argument.");
  TEN_ASSERT(unpacked, "Invalid argument.");

  bool result = true;

  msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
  if (rc == MSGPACK_UNPACK_SUCCESS) {
    if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_POSITIVE_INTEGER) {
      switch (MSGPACK_DATA_I64) {
      case TEN_TYPE_INT8: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_int8(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_INT16: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_int16(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_INT32: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_int32(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_INT64: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_int64(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT8: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_uint8(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT16: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_uint16(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT32: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_uint32(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_UINT64: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_uint64(value, MSGPACK_DATA_U64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_FLOAT32: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_float32(value, MSGPACK_DATA_F64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_FLOAT64: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_float64(value, MSGPACK_DATA_F64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_BOOL: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          ten_value_init_bool(value, MSGPACK_DATA_I64);
          goto done;
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_STRING: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_STR) {
            ten_value_init_string_with_size(value, MSGPACK_DATA_STR_PTR,
                                            MSGPACK_DATA_STR_SIZE);
            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_BUF: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_BIN) {
            ten_value_init_buf(value, 0);

            ten_buf_t *buf = ten_value_peek_buf(value, NULL);
            TEN_ASSERT(buf, "Invalid argument.");
            TEN_ASSERT(ten_buf_check_integrity(buf), "Invalid argument.");

            ten_buf_init_with_copying_data(buf, (uint8_t *)MSGPACK_DATA_BIN_PTR,
                                           MSGPACK_DATA_BIN_SIZE);
            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        }
        break;
      }

      case TEN_TYPE_ARRAY: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_POSITIVE_INTEGER) {
            ten_value_init_array_with_move(value, NULL);

            size_t array_items_cnt = MSGPACK_DATA_I64;
            for (size_t i = 0; i < array_items_cnt; i++) {
              ten_value_t *item =
                  ten_msgpack_create_value_through_deserialization(unpacker,
                                                                   unpacked);
              if (!item) {
                goto error;
              }
              ten_list_push_ptr_back(
                  &value->content.array, item,
                  (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
            }

            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      case TEN_TYPE_OBJECT: {
        msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
        if (rc == MSGPACK_UNPACK_SUCCESS) {
          if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_POSITIVE_INTEGER) {
            ten_value_init_object_with_move(value, NULL);

            size_t object_kv_cnt = MSGPACK_DATA_I64;
            for (size_t i = 0; i < object_kv_cnt; i++) {
              ten_value_kv_t *kv =
                  ten_msgpack_create_value_kv_through_deserialization(unpacker,
                                                                      unpacked);
              if (!kv) {
                goto error;
              }
              ten_list_push_ptr_back(
                  &value->content.object, kv,
                  (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
            }

            goto done;
          } else {
            TEN_ASSERT(0, "Should not happen.");
          }
        } else {
          TEN_ASSERT(0, "Should not happen.");
        }
        break;
      }

      default:
        TEN_ASSERT(0, "Need to implement more types.");
        break;
      }
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  } else {
    TEN_ASSERT(0, "Should not happen.");
  }

  goto done;

error:
  ten_value_deinit(value);
  result = false;

done:
  return result;
}

ten_value_t *ten_msgpack_create_value_through_deserialization(
    msgpack_unpacker *unpacker, msgpack_unpacked *unpacked) {
  ten_value_t *result = ten_value_create_invalid();
  if (ten_msgpack_value_deserialize_inplace(result, unpacker, unpacked)) {
    return result;
  } else {
    ten_value_destroy(result);
    return NULL;
  }
}

ten_value_kv_t *ten_msgpack_create_value_kv_through_deserialization(
    msgpack_unpacker *unpacker, msgpack_unpacked *unpacked) {
  TEN_ASSERT(unpacker, "Invalid argument.");
  TEN_ASSERT(unpacked, "Invalid argument.");

  ten_value_kv_t *result = NULL;

  msgpack_unpack_return rc = msgpack_unpacker_next(unpacker, unpacked);
  if (rc == MSGPACK_UNPACK_SUCCESS) {
    if (MSGPACK_DATA_TYPE == MSGPACK_OBJECT_STR) {
      result = ten_value_kv_create_vempty("%.*s", MSGPACK_DATA_STR_SIZE,
                                          MSGPACK_DATA_STR_PTR);
      result->value =
          ten_msgpack_create_value_through_deserialization(unpacker, unpacked);
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  } else {
    TEN_ASSERT(0, "Should not happen.");
  }

  return result;
}

void ten_msgpack_value_serialize(ten_value_t *value, msgpack_packer *pck) {
  TEN_ASSERT(value, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(value), "Invalid argument.");
  TEN_ASSERT(pck, "Invalid argument.");

  // Pack the type of value first.
  int rc = msgpack_pack_int32(pck, ten_value_get_type(value));
  TEN_ASSERT(rc == 0, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Pack the data of value second.
  switch (ten_value_get_type(value)) {
  case TEN_TYPE_INT8:
    rc = msgpack_pack_int8(pck, ten_value_get_int8(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_INT16:
    rc = msgpack_pack_int16(pck, ten_value_get_int16(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_INT32:
    rc = msgpack_pack_int32(pck, ten_value_get_int32(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_INT64:
    rc = msgpack_pack_int64(pck, ten_value_get_int64(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_UINT8:
    rc = msgpack_pack_uint8(pck, ten_value_get_uint8(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_UINT16:
    rc = msgpack_pack_uint16(pck, ten_value_get_uint16(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_UINT32:
    rc = msgpack_pack_uint32(pck, ten_value_get_uint32(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_UINT64:
    rc = msgpack_pack_uint64(pck, ten_value_get_uint64(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_FLOAT32:
    rc = msgpack_pack_float(pck, ten_value_get_float32(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_FLOAT64:
    rc = msgpack_pack_double(pck, ten_value_get_float64(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_STRING:
    rc =
        msgpack_pack_str_with_body(pck, ten_value_peek_raw_str(value, &err),
                                   strlen(ten_value_peek_raw_str(value, &err)));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_BOOL:
    rc = msgpack_pack_int32(pck, ten_value_get_bool(value, &err));
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_BUF:
    rc = msgpack_pack_bin_with_body(pck, ten_value_peek_buf(value, NULL)->data,
                                    ten_value_peek_buf(value, NULL)->size);
    TEN_ASSERT(rc == 0, "Should not happen.");
    break;
  case TEN_TYPE_ARRAY:
    // Pack array size first.
    //
    // Note: We can _not_ use msgpack_pack_array() here, because the
    // value_kv itself contains many fields, so that many msgpack_objects
    // would be generated during the serialization of the value_kv. And that
    // is not a correct way to use msgpack, what the `n` in
    // `msgpack_pack_array(, n)` means is the msgpack_objects in the map. So
    // only if we can know how many msgpack_objects will be generated during
    // the serialization of the value_kv, we can use msgpack_pack_array()
    // here. However, it would be very difficult to know this in advance, so
    // we directly pack the count number here, and let the unpacker to
    // unpack this numbers of value_kv later.
    rc = msgpack_pack_uint32(pck, ten_list_size(&value->content.array));
    TEN_ASSERT(rc == 0, "Should not happen.");

    // Pack data second.
    ten_list_foreach (&value->content.array, iter) {
      ten_value_t *array_item = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(array_item, "Invalid argument.");
      TEN_ASSERT(ten_value_check_integrity(array_item), "Invalid argument.");

      ten_msgpack_value_serialize(array_item, pck);
    }
    break;
  case TEN_TYPE_OBJECT:
    // Pack map size first.
    //
    // Note: We can _not_ use msgpack_pack_map() here, because the value_kv
    // itself contains many fields, so that many msgpack_objects would be
    // generated during the serialization of the value_kv. And that is not a
    // correct way to use msgpack, what the `n` in `msgpack_pack_map(, n)`
    // means is the msgpack_objects in the map. So only if we can know how
    // many msgpack_objects will be generated during the serialization of
    // the value_kv, we can use msgpack_pack_map() here. However, it would
    // be very difficult to know this in advance, so we directly pack the
    // count number here, and let the unpacker to unpack this numbers of
    // value_kv later.
    rc = msgpack_pack_uint32(pck, ten_list_size(&value->content.object));
    TEN_ASSERT(rc == 0, "Should not happen.");

    // Pack data second.
    ten_list_foreach (&value->content.object, iter) {
      ten_value_kv_t *object_item = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(object_item, "Invalid argument.");
      TEN_ASSERT(ten_value_kv_check_integrity(object_item),
                 "Invalid argument.");

      ten_msgpack_value_kv_serialize(object_item, pck);
    }
    break;
  default:
    TEN_ASSERT(0, "Need to implement more types (%d)",
               ten_value_get_type(value));
    break;
  }

  ten_error_deinit(&err);
}

void ten_msgpack_value_kv_serialize(ten_value_kv_t *kv, msgpack_packer *pck) {
  TEN_ASSERT(kv, "Invalid argument.");
  TEN_ASSERT(ten_value_kv_check_integrity(kv), "Invalid argument.");
  TEN_ASSERT(pck, "Invalid argument.");

  int rc = msgpack_pack_str_with_body(
      pck, ten_string_get_raw_str(ten_value_kv_get_key(kv)),
      ten_string_len(ten_value_kv_get_key(kv)));
  TEN_ASSERT(rc == 0, "Should not happen.");

  ten_msgpack_value_serialize(kv->value, pck);
}
