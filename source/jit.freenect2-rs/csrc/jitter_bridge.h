#pragma once

/**
 * jitter_bridge.h — C++ wrappers for Max/Jitter SDK calls that cannot be
 * called directly from Rust (variadic, multi-step, or require Max headers).
 *
 * Trivial one-liner wrappers have been replaced with direct extern "C"
 * declarations in lib.rs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Jitter class registration */
void* jb_jit_class_new(const char* name, void* new_fn, void* free_fn, long size);
void  jb_jit_class_addmethod_cant(void* c, void* fn, const char* name);
void  jb_jit_class_addmethod_no_args(void* c, void* fn, const char* name);

/* Jitter MOP */
void* jb_jit_mop_new(long n_inputs, long n_outputs);

/* Jitter object creation */
void* jb_jit_object_new_0(void* classname_sym);
void* jb_jit_object_new_with_sym(const char* classname, void* sym_arg);

/* Jitter method dispatch (jit_object_method wrappers) */
void* jb_jit_object_method_getindex(void* obj, long idx);
void* jb_jit_object_method_getoutput(void* mop, long idx);
void* jb_jit_object_method_getinputlist(void* mop);
void* jb_jit_object_method_getoutputlist(void* mop);
void* jb_jit_object_method_getmatrix(void* mop_io);
void  jb_jit_object_method_getinfo(void* mat, void* info_ptr);
void  jb_jit_object_method_getdata(void* mat, void** data_out);
long  jb_jit_object_method_lock(void* mat, long val);
long  jb_jit_call_matrix_calc(void* jit_ob, void* inputs, void* outputs);
void  jb_jit_object_method_jit_matrix(void* obj, long argc, void* argv);

/* Jitter attribute creation */
void* jb_jit_attr_offset_new_long(const char* name, long flags, long offset);
void* jb_jit_attr_offset_new_float32(const char* name, long flags, void* setter, long offset);
void  jb_object_addattr_parse(void* attr, const char* attrname, const char* parsestr);

/* Max class registration */
void* jb_max_class_new(const char* name, void* new_fn, void* free_fn, long size);
void  jb_max_class_addmethod(void* c, void* fn, const char* name);
void  jb_max_class_addmethod_usurp_low(void* c, void* fn, const char* name);
void  jb_max_class_addmethod_cant(void* c, void* fn, const char* name);
void  jb_max_class_register_box(void* c);

/* Max class attribute helpers */
void  jb_max_class_attr_long(void* c, const char* name, long offset,
                              const char* style, const char* label);
void  jb_max_class_attr_sym(void* c, const char* name, long offset,
                             const char* label, void* setter);

/* Error / logging */
void  jb_object_error(void* obj, const char* msg);
void  jb_jit_error_code(void* obj, long err);
void  jb_post(const char* msg);

/* Outlet tooltip */
void  jb_assist_outlet(char* dst, int output_texture, long arg);

#ifdef __cplusplus
} /* extern "C" */
#endif
