/**
 * jitter_bridge.cpp — C ABI wrappers around the Max/Jitter SDK.
 *
 * All functions are declared extern "C" and erase Max types to void*
 * so that Rust can call them without Max SDK headers.
 */

#include "jitter_bridge.h"

/* Max/Jitter SDK headers */
#include "ext.h"
#include "ext_obex.h"
#include "ext_obex_util.h"
#include "ext_assist.h"
#include "jit.common.h"
#include "max.jit.mop.h"
#include "jit.max.h"

#include <stdio.h>

/* -----------------------------------------------------------------------
   Symbols / gensym
   ----------------------------------------------------------------------- */
extern "C" {

void* jb_gensym(const char* name) {
    return (void*)gensym(name);
}

void* jb_jit_symbol_unique(void) {
    return (void*)jit_symbol_unique();
}

/* Predefined symbol accessors */
void* jb_sym_jit_mop(void)        { return (void*)_jit_sym_jit_mop; }
void* jb_sym_jit_matrix(void)     { return (void*)_jit_sym_jit_matrix; }
void* jb_sym_nothing(void)        { return (void*)_jit_sym_nothing; }
void* jb_sym_name(void)           { return (void*)_jit_sym_name; }
void* jb_sym_type(void)           { return (void*)_jit_sym_type; }
void* jb_sym_dim(void)            { return (void*)_jit_sym_dim; }
void* jb_sym_planecount(void)     { return (void*)_jit_sym_planecount; }
void* jb_sym_mindim(void)         { return (void*)_jit_sym_mindim; }
void* jb_sym_maxdim(void)         { return (void*)_jit_sym_maxdim; }
void* jb_sym_minplanecount(void)  { return (void*)_jit_sym_minplanecount; }
void* jb_sym_maxplanecount(void)  { return (void*)_jit_sym_maxplanecount; }
void* jb_sym_types(void)          { return (void*)_jit_sym_types; }
void* jb_sym_outputmode(void)     { return (void*)_jit_sym_outputmode; }
void* jb_sym_char_(void)          { return (void*)_jit_sym_char; }
void* jb_sym_float32(void)        { return (void*)_jit_sym_float32; }
void* jb_sym_long_(void)          { return (void*)_jit_sym_long; }
void* jb_sym_symbol_(void)        { return (void*)gensym("symbol"); }
void* jb_sym_lock(void)           { return (void*)_jit_sym_lock; }
void* jb_sym_getdata(void)        { return (void*)_jit_sym_getdata; }
void* jb_sym_getinfo(void)        { return (void*)_jit_sym_getinfo; }
void* jb_sym_getindex(void)       { return (void*)_jit_sym_getindex; }
void* jb_sym_getoutput(void)      { return (void*)_jit_sym_getoutput; }
void* jb_sym_getinputlist(void)   { return (void*)_jit_sym_getinputlist; }
void* jb_sym_getoutputlist(void)  { return (void*)_jit_sym_getoutputlist; }
void* jb_sym_getmatrix(void)      { return (void*)_jit_sym_getmatrix; }
void* jb_sym_matrix_calc(void)    { return (void*)_jit_sym_matrix_calc; }

/* -----------------------------------------------------------------------
   Jitter class registration
   ----------------------------------------------------------------------- */

void* jb_jit_class_new(const char* name, void* new_fn, void* free_fn, long size) {
    return (void*)jit_class_new(name, (method)new_fn, (method)free_fn, size, 0);
}

void jb_jit_class_register(void* c) {
    jit_class_register((t_class*)c);
}

void jb_jit_class_addmethod_cant(void* c, void* fn, const char* name) {
    jit_class_addmethod((t_class*)c, (method)fn, name, A_CANT, 0);
}

void jb_jit_class_addmethod_no_args(void* c, void* fn, const char* name) {
    jit_class_addmethod((t_class*)c, (method)fn, name, 0);
}

void jb_jit_class_addattr(void* c, void* attr) {
    jit_class_addattr((t_class*)c, (t_object*)attr);
}

void jb_jit_class_addadornment(void* c, void* adornment) {
    jit_class_addadornment((t_class*)c, (t_object*)adornment);
}

void* jb_jit_class_findbyname(void* sym) {
    return (void*)jit_class_findbyname((t_symbol*)sym);
}

/* -----------------------------------------------------------------------
   Jitter MOP
   ----------------------------------------------------------------------- */

void* jb_jit_mop_new(long n_inputs, long n_outputs) {
    return (void*)jit_object_new(_jit_sym_jit_mop, n_inputs, n_outputs);
}

void jb_jit_mop_output_nolink(void* mop, long idx) {
    jit_mop_output_nolink(mop, idx);
}

/* -----------------------------------------------------------------------
   Jitter object operations
   ----------------------------------------------------------------------- */

void* jb_jit_object_new_0(void* classname_sym) {
    return (void*)jit_object_new((t_symbol*)classname_sym);
}

void* jb_jit_object_new_with_sym(const char* classname, void* sym_arg) {
    return (void*)jit_object_new(gensym(classname), (t_symbol*)sym_arg);
}

void jb_jit_object_free(void* obj) {
    jit_object_free(obj);
}

void* jb_jit_object_alloc(void* jit_class) {
    return (void*)jit_object_alloc((t_class*)jit_class);
}

/* -----------------------------------------------------------------------
   Jitter object method calls
   ----------------------------------------------------------------------- */

void* jb_jit_object_method_getindex(void* obj, long idx) {
    return (void*)jit_object_method(obj, _jit_sym_getindex, idx);
}

void* jb_jit_object_method_getoutput(void* mop, long idx) {
    return (void*)jit_object_method(mop, _jit_sym_getoutput, idx);
}

void* jb_jit_object_method_getinputlist(void* mop) {
    return (void*)jit_object_method(mop, _jit_sym_getinputlist);
}

void* jb_jit_object_method_getoutputlist(void* mop) {
    return (void*)jit_object_method(mop, _jit_sym_getoutputlist);
}

void* jb_jit_object_method_getmatrix(void* mop_io) {
    return (void*)jit_object_method(mop_io, _jit_sym_getmatrix);
}

void jb_jit_object_method_getinfo(void* mat, void* info_ptr) {
    jit_object_method(mat, _jit_sym_getinfo, info_ptr);
}

void jb_jit_object_method_getdata(void* mat, void** data_out) {
    jit_object_method(mat, _jit_sym_getdata, data_out);
}

long jb_jit_object_method_lock(void* mat, long val) {
    return (long)(t_atom_long)jit_object_method(mat, _jit_sym_lock, (t_atom_long)val);
}

long jb_jit_call_matrix_calc(void* jit_ob, void* inputs, void* outputs) {
    return (long)(t_jit_err)jit_object_method(jit_ob, _jit_sym_matrix_calc, inputs, outputs);
}

void jb_jit_object_method_jit_matrix(void* obj, long argc, void* argv) {
    jit_object_method(obj, _jit_sym_jit_matrix, (long)argc, (t_atom*)argv);
}

void jb_jit_mop_set_ioproc_copy_adapt(void* mop_io) {
    jit_object_method(mop_io, _jit_sym_ioproc, jit_mop_ioproc_copy_adapt);
}

void* jb_max_jit_obex_adornment_get(void* x, void* sym) {
    return (void*)max_jit_obex_adornment_get(x, (t_symbol*)sym);
}

/* -----------------------------------------------------------------------
   Jitter attribute operations
   ----------------------------------------------------------------------- */

void jb_jit_attr_setlong(void* obj, void* sym, long val) {
    jit_attr_setlong(obj, (t_symbol*)sym, (t_atom_long)val);
}

void jb_jit_attr_setlong_array(void* obj, void* sym, long count, long* vals) {
    jit_attr_setlong_array(obj, (t_symbol*)sym, count, (t_atom_long*)vals);
}

void jb_jit_attr_setsym(void* obj, void* sym, void* val_sym) {
    jit_attr_setsym(obj, (t_symbol*)sym, (t_symbol*)val_sym);
}

void* jb_jit_attr_getsym(void* obj, void* sym) {
    return (void*)jit_attr_getsym(obj, (t_symbol*)sym);
}

/* -----------------------------------------------------------------------
   Jitter attribute creation
   ----------------------------------------------------------------------- */

void* jb_jit_attr_offset_new_long(const char* name, long flags, long offset) {
    return (void*)jit_object_new(
        _jit_sym_jit_attr_offset,
        name, _jit_sym_long, (long)flags,
        (method)NULL, (method)NULL,
        offset
    );
}

void* jb_jit_attr_offset_new_float32(const char* name, long flags,
                                      void* setter, long offset) {
    return (void*)jit_object_new(
        _jit_sym_jit_attr_offset,
        name, _jit_sym_float32, (long)flags,
        (method)NULL, (method)setter,
        offset
    );
}

void jb_object_addattr_parse(void* attr, const char* attrname, const char* parsestr) {
    /* _sym_symbol expands to _common_symbols->ps_symbol which is not exported
       in Max's flat namespace — use gensym("symbol") instead (same result). */
    object_addattr_parse((t_object*)attr, attrname, gensym("symbol"), 0, parsestr);
}

/* -----------------------------------------------------------------------
   Max class registration
   ----------------------------------------------------------------------- */

void* jb_max_class_new(const char* name, void* new_fn, void* free_fn, long size) {
    return (void*)class_new(name, (method)new_fn, (method)free_fn, (long)size,
                            NULL, A_GIMME, 0);
}

void jb_max_class_addmethod(void* c, void* fn, const char* name) {
    class_addmethod((t_class*)c, (method)fn, name, 0);
}

void jb_max_class_addmethod_usurp_low(void* c, void* fn, const char* name) {
    class_addmethod((t_class*)c, (method)fn, name, A_USURP_LOW, 0);
}

void jb_max_class_addmethod_cant(void* c, void* fn, const char* name) {
    class_addmethod((t_class*)c, (method)fn, name, A_CANT, 0);
}

void jb_max_class_register_box(void* c) {
    class_register(CLASS_BOX, (t_class*)c);
}

/* -----------------------------------------------------------------------
   Max class MOP / obex setup
   ----------------------------------------------------------------------- */

void jb_max_jit_class_obex_setup(void* c, long obex_offset) {
    max_jit_class_obex_setup((t_class*)c, obex_offset);
}

void jb_max_jit_class_mop_wrap(void* max_c, void* jit_c, long flags) {
    max_jit_class_mop_wrap((t_class*)max_c, (t_class*)jit_c, flags);
}

void jb_max_jit_class_wrap_standard(void* max_c, void* jit_c, long flags) {
    max_jit_class_wrap_standard((t_class*)max_c, (t_class*)jit_c, flags);
}

/* -----------------------------------------------------------------------
   Max class attribute helpers
   ----------------------------------------------------------------------- */

void jb_max_class_attr_long(void* c, const char* name, long offset,
                             const char* style, const char* label) {
    t_object* attr = attr_offset_new(name, gensym("long"), 0,
                                     (method)NULL, (method)NULL, offset);
    class_addattr((t_class*)c, attr);
    if (style && style[0]) {
        class_attr_addattr_parse((t_class*)c, name, "style",
                                 gensym("symbol"), 0, style);
    }
    if (label && label[0]) {
        class_attr_addattr_format((t_class*)c, name, "label",
                                  gensym("symbol"), 0, "s", gensym(label));
    }
}

void jb_max_class_attr_sym(void* c, const char* name, long offset,
                            const char* label, void* setter) {
    t_object* attr = attr_offset_new(name, gensym("symbol"), 0,
                                     (method)NULL, (method)NULL, offset);
    class_addattr((t_class*)c, attr);
    if (label && label[0]) {
        class_attr_addattr_format((t_class*)c, name, "label",
                                  gensym("symbol"), 0, "s", gensym(label));
    }
    if (setter) {
        /* Equivalent to CLASS_ATTR_ACCESSORS(c, name, NULL, setter) */
        t_object* the_attr = (t_object*)class_attr_get((t_class*)c, gensym(name));
        if (the_attr) {
            object_method(the_attr, gensym("setmethod"), gensym("set"), (method)setter);
        }
    }
}

/* -----------------------------------------------------------------------
   Max object helpers
   ----------------------------------------------------------------------- */

void* jb_max_jit_object_alloc(void* max_class, void* jit_classname_sym) {
    return (void*)max_jit_object_alloc((t_class*)max_class,
                                       (t_symbol*)jit_classname_sym);
}

void jb_max_jit_object_free(void* x) {
    max_jit_object_free(x);
}

void jb_max_jit_mop_setup_simple(void* x, void* jit_ob, long argc, void* argv) {
    max_jit_mop_setup_simple(x, jit_ob, (long)argc, (t_atom*)argv);
}

void jb_max_jit_attr_args(void* x, long argc, void* argv) {
    max_jit_attr_args(x, (short)argc, (t_atom*)argv);
}

void jb_max_jit_mop_free(void* x) {
    max_jit_mop_free(x);
}

void* jb_max_jit_obex_jitob_get(void* x) {
    return (void*)max_jit_obex_jitob_get(x);
}

void* jb_max_jit_mop_getoutput(void* x, long idx) {
    return (void*)max_jit_mop_getoutput(x, (long)idx);
}

void* jb_max_jit_mop_io_getoutlet(void* mop_io) {
    return (void*)max_jit_mop_io_getoutlet(mop_io);
}

void jb_max_jit_mop_outputmatrix(void* x) {
    max_jit_mop_outputmatrix(x);
}

/* -----------------------------------------------------------------------
   Qelem
   ----------------------------------------------------------------------- */

void* jb_qelem_new(void* owner, void* fn) {
    return (void*)qelem_new(owner, (method)fn);
}

void jb_qelem_free(void* qelem) {
    qelem_free((t_qelem*)qelem);
}

void jb_qelem_set(void* qelem) {
    qelem_set((t_qelem*)qelem);
}

/* -----------------------------------------------------------------------
   Outlet
   ----------------------------------------------------------------------- */

void jb_outlet_anything(void* outlet, void* sel_sym, long argc, void* argv) {
    outlet_anything(outlet, (t_symbol*)sel_sym, (short)argc, (t_atom*)argv);
}

/* -----------------------------------------------------------------------
   Atom helpers
   ----------------------------------------------------------------------- */

void jb_atom_setsym(void* atom, void* sym) {
    atom_setsym((t_atom*)atom, (t_symbol*)sym);
}

void* jb_atom_getsym(const void* atom) {
    return (void*)atom_getsym((const t_atom*)atom);
}

float jb_atom_getfloat(const void* atom) {
    return atom_getfloat((const t_atom*)atom);
}

/* -----------------------------------------------------------------------
   Error / logging
   ----------------------------------------------------------------------- */

void jb_object_error(void* obj, const char* msg) {
    object_error((t_object*)obj, "%s", msg);
}

void jb_jit_error_code(void* obj, long err) {
    jit_error_code(obj, (t_jit_err)err);
}

void jb_post(const char* msg) {
    post("%s", msg);
}

/* -----------------------------------------------------------------------
   Assist string helper
   ----------------------------------------------------------------------- */

void jb_assist_outlet(char* dst, int output_texture, long arg) {
    const char* fmt = output_texture ? "(texture)" : "(matrix)";
    switch (arg) {
        case 0: snprintf(dst, 512, "%s color 1920x1080 char ARGB",            fmt); break;
        case 1: snprintf(dst, 512, "%s ir 512x424 float32 0-65535",           fmt); break;
        case 2: snprintf(dst, 512, "%s depth 512x424 float32 mm",             fmt); break;
        case 3: snprintf(dst, 512, "%s undistorted 512x424 float32 mm",       fmt); break;
        case 4: snprintf(dst, 512, "%s registered 512x424 char ARGB",         fmt); break;
        case 5: snprintf(dst, 512, "%s bigdepth 1920x1080 float32 mm",        fmt); break;
        case 6: snprintf(dst, 512, "dumpout");                                       break;
        default: dst[0] = '\0'; break;
    }
}

} /* extern "C" */
