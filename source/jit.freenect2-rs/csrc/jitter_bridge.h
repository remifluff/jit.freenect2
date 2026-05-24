#pragma once

/**
 * jitter_bridge.h — C ABI wrappers around Max/Jitter SDK calls.
 *
 * All Max/Jitter types are erased to void* so that Rust can call these
 * functions without needing Max SDK headers.  Symbol pointers (t_symbol*)
 * are represented as void*.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
   Symbols / gensym
   ----------------------------------------------------------------------- */
void* jb_gensym(const char* name);
void* jb_jit_symbol_unique(void);

/* Predefined symbol accessors (avoid exposing global C symbols to Rust) */
void* jb_sym_jit_mop(void);
void* jb_sym_jit_matrix(void);
void* jb_sym_nothing(void);
void* jb_sym_name(void);
void* jb_sym_type(void);
void* jb_sym_dim(void);
void* jb_sym_planecount(void);
void* jb_sym_mindim(void);
void* jb_sym_maxdim(void);
void* jb_sym_minplanecount(void);
void* jb_sym_maxplanecount(void);
void* jb_sym_types(void);
void* jb_sym_outputmode(void);
void* jb_sym_char_(void);      /* "char"    */
void* jb_sym_float32(void);
void* jb_sym_long_(void);      /* "long"    */
void* jb_sym_symbol_(void);    /* "symbol"  */
void* jb_sym_lock(void);
void* jb_sym_getdata(void);
void* jb_sym_getinfo(void);
void* jb_sym_getindex(void);
void* jb_sym_getoutput(void);
void* jb_sym_getinputlist(void);
void* jb_sym_getoutputlist(void);
void* jb_sym_getmatrix(void);
void* jb_sym_matrix_calc(void);

/* -----------------------------------------------------------------------
   Jitter class registration
   ----------------------------------------------------------------------- */
void* jb_jit_class_new(const char* name, void* new_fn, void* free_fn, long size);
void  jb_jit_class_register(void* c);
void  jb_jit_class_addmethod_cant(void* c, void* fn, const char* name);
void  jb_jit_class_addmethod_no_args(void* c, void* fn, const char* name);
void  jb_jit_class_addattr(void* c, void* attr);
void  jb_jit_class_addadornment(void* c, void* adornment);
void* jb_jit_class_findbyname(void* sym);

/* -----------------------------------------------------------------------
   Jitter MOP
   ----------------------------------------------------------------------- */
void* jb_jit_mop_new(long n_inputs, long n_outputs);
void  jb_jit_mop_output_nolink(void* mop, long idx);  /* 1-indexed */

/* -----------------------------------------------------------------------
   Jitter object operations
   ----------------------------------------------------------------------- */
void* jb_jit_object_new_0(void* classname_sym);
void* jb_jit_object_new_with_sym(const char* classname, void* sym_arg);
void  jb_jit_object_free(void* obj);
void* jb_jit_object_alloc(void* jit_class);

/* -----------------------------------------------------------------------
   Jitter object method calls (typed wrappers for jit_object_method)
   ----------------------------------------------------------------------- */
/** jit_object_method(obj, _jit_sym_getindex, idx) → void* */
void* jb_jit_object_method_getindex(void* obj, long idx);

/** jit_object_method(obj, _jit_sym_getoutput, idx) → void* (1-indexed) */
void* jb_jit_object_method_getoutput(void* mop, long idx);

/** jit_object_method(mop, _jit_sym_getinputlist) → void* */
void* jb_jit_object_method_getinputlist(void* mop);

/** jit_object_method(mop, _jit_sym_getoutputlist) → void* */
void* jb_jit_object_method_getoutputlist(void* mop);

/** jit_object_method(mop_io, _jit_sym_getmatrix) → void* */
void* jb_jit_object_method_getmatrix(void* mop_io);

/** jit_object_method(mat, _jit_sym_getinfo, info_ptr) */
void  jb_jit_object_method_getinfo(void* mat, void* info_ptr);

/** jit_object_method(mat, _jit_sym_getdata, &data_ptr) */
void  jb_jit_object_method_getdata(void* mat, void** data_out);

/** jit_object_method(mat, _jit_sym_lock, val) → previous lock value */
long  jb_jit_object_method_lock(void* mat, long val);

/** jit_object_method(jit_ob, _jit_sym_matrix_calc, inputs, outputs) → error code */
long  jb_jit_call_matrix_calc(void* jit_ob, void* inputs, void* outputs);

/** jit_object_method(obj, _jit_sym_jit_matrix, argc, argv) */
void  jb_jit_object_method_jit_matrix(void* obj, long argc, void* argv);

/** jit_object_method(obj, _jit_sym_ioproc, jit_mop_ioproc_copy_adapt) */
void  jb_jit_mop_set_ioproc_copy_adapt(void* mop_io);

/** max_jit_obex_adornment_get(x, sym) */
void* jb_max_jit_obex_adornment_get(void* x, void* sym);

/* -----------------------------------------------------------------------
   Jitter attribute operations (jit_attr_* / jit_object_method on attr obj)
   ----------------------------------------------------------------------- */
void  jb_jit_attr_setlong(void* obj, void* sym, long val);
void  jb_jit_attr_setlong_array(void* obj, void* sym, long count, long* vals);
void  jb_jit_attr_setsym(void* obj, void* sym, void* val_sym);
void* jb_jit_attr_getsym(void* obj, void* sym);

/* -----------------------------------------------------------------------
   Jitter attribute creation
   ----------------------------------------------------------------------- */
/** Create a jit_attr_offset for a 'long' field; no getter/setter. */
void* jb_jit_attr_offset_new_long(const char* name, long flags, long offset);

/**
 * Create a jit_attr_offset for a 'float32' field with an optional setter.
 * setter may be NULL.
 */
void* jb_jit_attr_offset_new_float32(const char* name, long flags,
                                     void* setter, long offset);

/** object_addattr_parse(attr, attrname, _sym_symbol, 0, parsestr) */
void  jb_object_addattr_parse(void* attr, const char* attrname, const char* parsestr);

/* -----------------------------------------------------------------------
   Max class registration
   ----------------------------------------------------------------------- */
void* jb_max_class_new(const char* name, void* new_fn, void* free_fn, long size);
void  jb_max_class_addmethod(void* c, void* fn, const char* name);
void  jb_max_class_addmethod_usurp_low(void* c, void* fn, const char* name);
void  jb_max_class_addmethod_cant(void* c, void* fn, const char* name);
void  jb_max_class_register_box(void* c);

/* -----------------------------------------------------------------------
   Max class MOP / obex setup
   ----------------------------------------------------------------------- */
void  jb_max_jit_class_obex_setup(void* c, long obex_offset);
void  jb_max_jit_class_mop_wrap(void* max_c, void* jit_c, long flags);
void  jb_max_jit_class_wrap_standard(void* max_c, void* jit_c, long flags);

/* -----------------------------------------------------------------------
   Max class attribute helpers
   ----------------------------------------------------------------------- */
/**
 * Add a CLASS_ATTR_LONG-equivalent attribute with optional style and label.
 * style and label may each be NULL to skip that sub-attribute.
 * Example: jb_max_class_attr_long(c, "output_texture", offset, "onoff", "Output Texture");
 */
void  jb_max_class_attr_long(void* c, const char* name, long offset,
                              const char* style, const char* label);

/**
 * Add a CLASS_ATTR_SYM-equivalent attribute with optional label and custom setter.
 * setter may be NULL if no custom setter is needed.
 */
void  jb_max_class_attr_sym(void* c, const char* name, long offset,
                             const char* label, void* setter);

/* -----------------------------------------------------------------------
   Max object helpers
   ----------------------------------------------------------------------- */
void* jb_max_jit_object_alloc(void* max_class, void* jit_classname_sym);
void  jb_max_jit_object_free(void* x);
void  jb_max_jit_mop_setup_simple(void* x, void* jit_ob, long argc, void* argv);
void  jb_max_jit_attr_args(void* x, long argc, void* argv);
void  jb_max_jit_mop_free(void* x);
void* jb_max_jit_obex_jitob_get(void* x);
void* jb_max_jit_mop_getoutput(void* x, long idx);     /* 1-indexed */
void* jb_max_jit_mop_io_getoutlet(void* mop_io);
void  jb_max_jit_mop_outputmatrix(void* x);

/* -----------------------------------------------------------------------
   Qelem
   ----------------------------------------------------------------------- */
void* jb_qelem_new(void* owner, void* fn);
void  jb_qelem_free(void* qelem);
void  jb_qelem_set(void* qelem);

/* -----------------------------------------------------------------------
   Outlet
   ----------------------------------------------------------------------- */
void  jb_outlet_anything(void* outlet, void* sel_sym, long argc, void* argv);

/* -----------------------------------------------------------------------
   Atom helpers (atoms are 16-byte structs; caller manages storage)
   ----------------------------------------------------------------------- */
void  jb_atom_setsym(void* atom, void* sym);
void* jb_atom_getsym(const void* atom);
float jb_atom_getfloat(const void* atom);

/* -----------------------------------------------------------------------
   Error / logging
   ----------------------------------------------------------------------- */
void  jb_object_error(void* obj, const char* msg);
void  jb_jit_error_code(void* obj, long err);
void  jb_post(const char* msg);

/* -----------------------------------------------------------------------
   Assist string helper
   ----------------------------------------------------------------------- */
/**
 * Write the outlet description string for outlet `arg` into `dst` (512-byte buffer).
 * output_texture non-zero selects "(texture)" prefix over "(matrix)".
 */
void  jb_assist_outlet(char* dst, int output_texture, long arg);

#ifdef __cplusplus
} /* extern "C" */
#endif
