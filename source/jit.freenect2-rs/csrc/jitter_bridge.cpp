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

extern "C" {

/* -----------------------------------------------------------------------
   Jitter class registration — variadic (terminator 0 required)
   ----------------------------------------------------------------------- */

void* jb_jit_class_new(const char* name, void* new_fn, void* free_fn, long size) {
    return (void*)jit_class_new(name, (method)new_fn, (method)free_fn, size, 0);
}

void jb_jit_class_addmethod_cant(void* c, void* fn, const char* name) {
    jit_class_addmethod((t_class*)c, (method)fn, name, A_CANT, 0);
}

void jb_jit_class_addmethod_no_args(void* c, void* fn, const char* name) {
    jit_class_addmethod((t_class*)c, (method)fn, name, 0);
}

/* -----------------------------------------------------------------------
   Jitter MOP — variadic (jit_object_new with symbol + args)
   ----------------------------------------------------------------------- */

void* jb_jit_mop_new(long n_inputs, long n_outputs) {
    return (void*)jit_object_new(_jit_sym_jit_mop, n_inputs, n_outputs);
}

/* -----------------------------------------------------------------------
   Jitter object creation — variadic
   ----------------------------------------------------------------------- */

void* jb_jit_object_new_0(void* classname_sym) {
    return (void*)jit_object_new((t_symbol*)classname_sym);
}

void* jb_jit_object_new_with_sym(const char* classname, void* sym_arg) {
    return (void*)jit_object_new(gensym(classname), (t_symbol*)sym_arg);
}

/* -----------------------------------------------------------------------
   Jitter object method calls — variadic (jit_object_method dispatch)
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

/* -----------------------------------------------------------------------
   Jitter attribute creation — variadic (jit_object_new with attr symbol)
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
    object_addattr_parse((t_object*)attr, attrname, gensym("symbol"), 0, parsestr);
}

/* -----------------------------------------------------------------------
   Max class registration — variadic (class_new / class_addmethod)
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
   Max class attribute helpers — multi-step
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
        t_object* the_attr = (t_object*)class_attr_get((t_class*)c, gensym(name));
        if (the_attr) {
            object_method(the_attr, gensym("setmethod"), gensym("set"), (method)setter);
        }
    }
}

/* -----------------------------------------------------------------------
   Error / logging — printf format-string safety
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
   Outlet tooltip helper
   ----------------------------------------------------------------------- */

void jb_assist_outlet(char* dst, int output_texture, long arg) {
    const char* fmt = output_texture ? "(texture)" : "(matrix)";
    switch (arg) {
        case 0: snprintf(dst, 512, "%s colour 1920x1080 char ARGB",           fmt); break;
        case 1: snprintf(dst, 512, "%s bigdepth 1920x1080 float32 mm",        fmt); break;
        case 2: snprintf(dst, 512, "%s registered 512x424 char ARGB",         fmt); break;
        case 3: snprintf(dst, 512, "%s ir 512x424 float32 0-65535",           fmt); break;
        case 4: snprintf(dst, 512, "%s depth 512x424 float32 mm",             fmt); break;
        case 5: snprintf(dst, 512, "dumpout");                                       break;
        default: dst[0] = '\0'; break;
    }
}

} /* extern "C" */
