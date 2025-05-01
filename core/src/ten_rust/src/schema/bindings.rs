#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]
#![allow(improper_ctypes)]
#![allow(improper_ctypes_definitions)]
#![allow(clippy::upper_case_acronyms)]

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ten_schema_t {
    _unused: [u8; 0],
}
extern "C" {
    pub fn ten_schema_create_from_json_str_proxy(
        json_str: *const ::std::os::raw::c_char,
        err_msg: *mut *const ::std::os::raw::c_char,
    ) -> *mut ten_schema_t;
}
extern "C" {
    pub fn ten_schema_destroy_proxy(self_: *const ten_schema_t);
}
extern "C" {
    pub fn ten_schema_adjust_and_validate_json_str_proxy(
        self_: *const ten_schema_t,
        json_str: *const ::std::os::raw::c_char,
        err_msg: *mut *const ::std::os::raw::c_char,
    ) -> bool;
}
extern "C" {
    pub fn ten_schema_is_compatible_proxy(
        self_: *const ten_schema_t,
        target: *const ten_schema_t,
        err_msg: *mut *const ::std::os::raw::c_char,
    ) -> bool;
}
