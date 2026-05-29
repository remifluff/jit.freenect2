/*!
 * jit.k2nect-rs — Rust port of jit.k2nect
 *
 * Architecture:
 *   lib.rs  (this file)  ─▶  jitter_bridge.cpp  (Max/Jitter C ABI)
 *                        ─▶  kinect_shim.cpp     (libfreenect2 C ABI)
 *
 * The outer Max object (TMaxJitK2nect) wraps the inner Jitter object
 * (TJitK2nect) following the standard Jitter MOP pattern.
 *
 * Five outputs (all horizontally mirrored):
 *   0 – colour     1920×1080  char    4-plane ARGB
 *   1 – bigdepth   1920×1080  float32 1-plane mm  (requires output_color=1)
 *   2 – registered  512×424    char    4-plane ARGB (requires output_color=1)
 *   3 – ir          512×424    float32 1-plane 0-65535
 *   4 – depth       512×424    float32 1-plane mm
 */

#![allow(non_snake_case, clippy::missing_safety_doc)]

use std::ffi::{c_char, c_float, c_int, c_void};
use memoffset::offset_of;

/* -----------------------------------------------------------------------
   Null-terminated string literal helper
   ----------------------------------------------------------------------- */
macro_rules! cstr {
    ($s:literal) => {
        concat!($s, "\0").as_ptr() as *const c_char
    };
}

/* -----------------------------------------------------------------------
   Error codes (t_jit_err = t_atom_long = long = i64 on 64-bit macOS)
   ----------------------------------------------------------------------- */
const JIT_ERR_NONE: i64           = 0;
const JIT_ERR_INVALID_OUTPUT: i64 = 0x494E564F; /* FOUR_CHAR('INVO') */
const JIT_ERR_INVALID_PTR: i64    = 0x494E5650; /* FOUR_CHAR('INVP') */
const MAX_ERR_NONE: i64           = 0;

/* MAX_JIT_MOP_FLAGS_OWN_ADAPT — we handle matrix adaptation ourselves */
const MAX_JIT_MOP_FLAGS_OWN_ADAPT: i64 = 0x00000001;

/* -----------------------------------------------------------------------
   Kinect dimensions
   ----------------------------------------------------------------------- */
const COLOR_WIDTH:  i64 = 1920;
const COLOR_HEIGHT: i64 = 1080;
const DEPTH_WIDTH:  i64 = 512;
const DEPTH_HEIGHT: i64 = 424;
/* bigdepth valid data starts at this float offset inside bigdepth.data     */
/* = filter_height_half(1)*1920 + filter_width_half(2) = 1920 + 2 = 1922   */
const BIGDEPTH_OFFSET: usize = 1922;

/* -----------------------------------------------------------------------
   Jitter inner object — must match t_jit_k2nect in jit.k2nect.cpp
   Layout (64 bytes, no padding needed on LP64):
     ob[32]  depth_processor[8]  max_depth[4]  min_depth[4]  output_rgb[8]  kinect[8]
   ----------------------------------------------------------------------- */
#[repr(C)]
pub struct TJitK2nect {
    ob:              [u8; 32],
    depth_processor: i64,
    max_depth:       c_float,
    min_depth:       c_float,
    output_rgb:      i64,
    kinect:          *mut c_void, /* KinectHandle */
}

/* -----------------------------------------------------------------------
   Max outer object — must match t_max_jit_k2nect in max.jit.k2nect.cpp
   Layout (160 bytes):
     ob[32]  obex[8]  qelem[8]  output_texture[8]  drawto[8]
     tex_objects[48]  tex_names[48]
   ----------------------------------------------------------------------- */
#[repr(C)]
pub struct TMaxJitK2nect {
    ob:             [u8; 32],
    obex:           *mut c_void,
    qelem:          *mut c_void,
    output_texture: i64,
    drawto:         *mut c_void, /* t_symbol* */
    tex_objects:    [*mut c_void; 5],
    tex_names:      [*mut c_void; 5], /* t_symbol* */
}

/* -----------------------------------------------------------------------
   t_jit_matrix_info mirror (552 bytes, LP64 macOS)
   ----------------------------------------------------------------------- */
#[repr(C)]
#[derive(Clone)]
pub struct TJitMatrixInfo {
    size:       i64,
    type_sym:   *mut c_void,
    flags:      i64,
    dimcount:   i64,
    dim:        [i64; 32],
    dimstride:  [i64; 32],
    planecount: i64,
}
impl Copy for TJitMatrixInfo {}

/* -----------------------------------------------------------------------
   t_atom mirror (16 bytes)
   ----------------------------------------------------------------------- */
#[repr(C)]
struct Atom {
    a_type: i16,
    _pad:   [u8; 6],
    a_w:    u64,
}
impl Atom {
    fn new() -> Self { Atom { a_type: 0, _pad: [0; 6], a_w: 0 } }
}

/* -----------------------------------------------------------------------
   KinectFramePtrs mirror
   ----------------------------------------------------------------------- */
#[repr(C)]
struct KinectFramePtrs {
    color_data:       *const u8,
    ir_data:          *const c_float,
    depth_data:       *const c_float,
    registered_data:  *const u8,
    bigdepth_data:    *const c_float,
}
impl KinectFramePtrs {
    fn new() -> Self {
        KinectFramePtrs {
            color_data:       std::ptr::null(),
            ir_data:          std::ptr::null(),
            depth_data:       std::ptr::null(),
            registered_data:  std::ptr::null(),
            bigdepth_data:    std::ptr::null(),
        }
    }
}

/* -----------------------------------------------------------------------
   Global class pointers
   ----------------------------------------------------------------------- */
static mut S_JIT_CLASS: *mut c_void = std::ptr::null_mut();
static mut S_MAX_CLASS: *mut c_void = std::ptr::null_mut();

/* -----------------------------------------------------------------------
   FFI: kinect_shim.cpp
   ----------------------------------------------------------------------- */
extern "C" {
    fn kinect_create() -> *mut c_void;
    fn kinect_destroy(h: *mut c_void);
    fn kinect_open(h: *mut c_void, pipeline_type: c_int,
                   min_depth: c_float, max_depth: c_float) -> c_int;
    fn kinect_set_depth_config(h: *mut c_void, min_depth: c_float, max_depth: c_float);
    fn kinect_set_callback(h: *mut c_void,
                           cb: Option<unsafe extern "C" fn(*mut c_void)>,
                           user: *mut c_void);
    fn kinect_is_open(h: *mut c_void) -> c_int;
    fn kinect_has_new_frames(h: *mut c_void) -> c_int;
    fn kinect_wait_frames(h: *mut c_void, out: *mut KinectFramePtrs) -> c_int;
    fn kinect_register_frames(h: *mut c_void, out: *mut KinectFramePtrs, use_rgb: c_int);
    fn kinect_release_frames(h: *mut c_void);
    fn kinect_close(h: *mut c_void);
}

/* -----------------------------------------------------------------------
   FFI: Jitter predefined symbol globals  (JIT_EX_DATA t_symbol* — jit.symbols.h)
   ----------------------------------------------------------------------- */
extern "C" {
    static _jit_sym_jit_mop:       *mut c_void;
    static _jit_sym_nothing:       *mut c_void;
    static _jit_sym_name:          *mut c_void;
    static _jit_sym_char:          *mut c_void;
    static _jit_sym_float32:       *mut c_void;
    static _jit_sym_mindim:        *mut c_void;
    static _jit_sym_maxdim:        *mut c_void;
    static _jit_sym_minplanecount: *mut c_void;
    static _jit_sym_maxplanecount: *mut c_void;
    static _jit_sym_types:         *mut c_void;
    static _jit_sym_outputmode:    *mut c_void;
    static _jit_sym_planecount:    *mut c_void;
    static _jit_sym_type:          *mut c_void;
    static _jit_sym_dim:           *mut c_void;
}

/* -----------------------------------------------------------------------
   FFI: direct Max/Jitter API
   ----------------------------------------------------------------------- */
extern "C" {
    fn gensym(name: *const c_char) -> *mut c_void;
    fn jit_symbol_unique() -> *mut c_void;
    fn jit_class_register(c: *mut c_void);
    fn jit_class_addattr(c: *mut c_void, attr: *mut c_void);
    fn jit_class_addadornment(c: *mut c_void, adornment: *mut c_void);
    fn jit_class_findbyname(sym: *mut c_void) -> *mut c_void;
    fn jit_mop_output_nolink(mop: *mut c_void, idx: i64);
    fn jit_object_free(obj: *mut c_void);
    fn jit_object_alloc(jit_class: *mut c_void) -> *mut c_void;
    fn jit_attr_setlong(obj: *mut c_void, sym: *mut c_void, val: i64);
    fn jit_attr_setlong_array(obj: *mut c_void, sym: *mut c_void, count: i64, vals: *mut i64);
    fn jit_attr_setsym(obj: *mut c_void, sym: *mut c_void, val_sym: *mut c_void);
    fn jit_attr_getsym(obj: *mut c_void, sym: *mut c_void) -> *mut c_void;
    fn max_jit_class_obex_setup(c: *mut c_void, obex_offset: i64);
    fn max_jit_class_mop_wrap(max_c: *mut c_void, jit_c: *mut c_void, flags: i64);
    fn max_jit_class_wrap_standard(max_c: *mut c_void, jit_c: *mut c_void, flags: i64);
    fn max_jit_object_alloc(max_class: *mut c_void, jit_classname_sym: *mut c_void) -> *mut c_void;
    fn max_jit_object_free(x: *mut c_void);
    fn max_jit_mop_setup_simple(x: *mut c_void, jit_ob: *mut c_void, argc: i64, argv: *mut c_void);
    fn max_jit_attr_args(x: *mut c_void, argc: i16, argv: *mut c_void);
    fn max_jit_mop_free(x: *mut c_void);
    fn max_jit_obex_jitob_get(x: *mut c_void) -> *mut c_void;
    fn max_jit_obex_adornment_get(x: *mut c_void, sym: *mut c_void) -> *mut c_void;
    fn max_jit_mop_getoutput(x: *mut c_void, idx: i64) -> *mut c_void;
    fn max_jit_mop_io_getoutlet(mop_io: *mut c_void) -> *mut c_void;
    fn max_jit_mop_outputmatrix(x: *mut c_void);
    fn qelem_new(owner: *mut c_void, fn_: *mut c_void) -> *mut c_void;
    fn qelem_free(qelem: *mut c_void);
    fn qelem_set(qelem: *mut c_void);
    fn outlet_anything(outlet: *mut c_void, sel_sym: *mut c_void, argc: i16, argv: *mut c_void);
    fn atom_setsym(atom: *mut c_void, sym: *mut c_void);
    fn atom_getsym(atom: *const c_void) -> *mut c_void;
}

/* -----------------------------------------------------------------------
   FFI: jitter_bridge.cpp — variadic / multi-step C++ wrappers
   ----------------------------------------------------------------------- */
extern "C" {
    fn jb_jit_class_new(name: *const c_char, new_fn: *mut c_void,
                        free_fn: *mut c_void, size: i64) -> *mut c_void;
    fn jb_jit_class_addmethod_cant(c: *mut c_void, fn_: *mut c_void, name: *const c_char);
    fn jb_jit_class_addmethod_no_args(c: *mut c_void, fn_: *mut c_void, name: *const c_char);
    fn jb_jit_mop_new(n_inputs: i64, n_outputs: i64) -> *mut c_void;
    fn jb_jit_object_new_0(classname_sym: *mut c_void) -> *mut c_void;
    fn jb_jit_object_new_with_sym(classname: *const c_char, sym_arg: *mut c_void) -> *mut c_void;
    fn jb_jit_object_method_getindex(obj: *mut c_void, idx: i64) -> *mut c_void;
    fn jb_jit_object_method_getoutput(mop: *mut c_void, idx: i64) -> *mut c_void;
    fn jb_jit_object_method_getinputlist(mop: *mut c_void) -> *mut c_void;
    fn jb_jit_object_method_getoutputlist(mop: *mut c_void) -> *mut c_void;
    fn jb_jit_object_method_getmatrix(mop_io: *mut c_void) -> *mut c_void;
    fn jb_jit_object_method_getinfo(mat: *mut c_void, info_ptr: *mut c_void);
    fn jb_jit_object_method_getdata(mat: *mut c_void, data_out: *mut *mut c_void);
    fn jb_jit_object_method_lock(mat: *mut c_void, val: i64) -> i64;
    fn jb_jit_call_matrix_calc(jit_ob: *mut c_void, inputs: *mut c_void,
                                outputs: *mut c_void) -> i64;
    fn jb_jit_object_method_jit_matrix(obj: *mut c_void, argc: i64, argv: *mut c_void);
    fn jb_jit_attr_offset_new_long(name: *const c_char, flags: i64, offset: i64) -> *mut c_void;
    fn jb_jit_attr_offset_new_float32(name: *const c_char, flags: i64,
                                       setter: *mut c_void, offset: i64) -> *mut c_void;
    fn jb_object_addattr_parse(attr: *mut c_void, attrname: *const c_char,
                                parsestr: *const c_char);
    fn jb_max_class_new(name: *const c_char, new_fn: *mut c_void,
                        free_fn: *mut c_void, size: i64) -> *mut c_void;
    fn jb_max_class_addmethod(c: *mut c_void, fn_: *mut c_void, name: *const c_char);
    fn jb_max_class_addmethod_usurp_low(c: *mut c_void, fn_: *mut c_void, name: *const c_char);
    fn jb_max_class_addmethod_cant(c: *mut c_void, fn_: *mut c_void, name: *const c_char);
    fn jb_max_class_register_box(c: *mut c_void);
    fn jb_max_class_attr_long(c: *mut c_void, name: *const c_char, offset: i64,
                               style: *const c_char, label: *const c_char);
    fn jb_max_class_attr_sym(c: *mut c_void, name: *const c_char, offset: i64,
                              label: *const c_char, setter: *mut c_void);
    fn jb_object_error(obj: *mut c_void, msg: *const c_char);
    fn jb_jit_error_code(obj: *mut c_void, err: i64);
    fn jb_post(msg: *const c_char);
    fn jb_assist_outlet(dst: *mut c_char, output_texture: c_int, arg: i64);
}

/* -----------------------------------------------------------------------
   JIT INNER OBJECT
   ----------------------------------------------------------------------- */

/// One-time Jitter class registration; called from ext_main.
#[no_mangle]
pub unsafe extern "C" fn jit_k2nect_init() -> i64 {
    let attrflags: i64 = 0x08040000; /* JIT_ATTR_GET_DEFER_LOW (0x00040000) | JIT_ATTR_SET_USURP_LOW (0x08000000) */

    S_JIT_CLASS = jb_jit_class_new(
        cstr!("jit_k2nect_rs"),
        jit_k2nect_new as usize as *mut c_void,
        jit_k2nect_free as usize as *mut c_void,
        std::mem::size_of::<TJitK2nect>() as i64,
    );

    /* MOP: 0 inputs, 5 outputs */
    let mop = jb_jit_mop_new(0, 5);
    jit_class_addadornment(S_JIT_CLASS, mop);

    /* Methods */
    jb_jit_class_addmethod_cant(S_JIT_CLASS,
        jit_k2nect_matrix_calc as usize as *mut c_void, cstr!("matrix_calc"));
    jb_jit_class_addmethod_no_args(S_JIT_CLASS,
        jit_k2nect_open as usize as *mut c_void, cstr!("open"));
    jb_jit_class_addmethod_no_args(S_JIT_CLASS,
        jit_k2nect_close as usize as *mut c_void, cstr!("close"));
    jb_jit_class_addmethod_cant(S_JIT_CLASS,
        jit_k2nect_get_kinect_wrapper as usize as *mut c_void,
        cstr!("get_kinect_wrapper"));

    /* Unlink all 5 outputs so we control type/dim independently */
    for i in 1i64..=5 {
        jit_mop_output_nolink(mop, i);
    }

    /* Configure output constraints */
    let mut color_dim: [i64; 2] = [COLOR_WIDTH, COLOR_HEIGHT];
    let mut depth_dim: [i64; 2] = [DEPTH_WIDTH, DEPTH_HEIGHT];

    let configure = |idx: i64,
                     min_pc: i64, max_pc: i64,
                     mindim: *mut i64, maxdim: *mut i64,
                     force_char: bool| {
        let out = jb_jit_object_method_getoutput(mop, idx);
        jit_attr_setlong(out, _jit_sym_minplanecount, min_pc);
        jit_attr_setlong(out, _jit_sym_maxplanecount, max_pc);
        jit_attr_setlong_array(out, _jit_sym_mindim, 2, mindim);
        jit_attr_setlong_array(out, _jit_sym_maxdim, 2, maxdim);
        if force_char {
            jit_attr_setlong(out, _jit_sym_types, 0); /* char only */
        }
        jit_attr_setlong(out, _jit_sym_outputmode, 2);
    };

    /* output 1: colour 1920×1080 char 4-plane */
    configure(1, 4, 4, color_dim.as_mut_ptr(), color_dim.as_mut_ptr(), true);
    /* output 2: bigdepth 1920×1080 float32 1-plane */
    configure(2, 1, 1, color_dim.as_mut_ptr(), color_dim.as_mut_ptr(), false);
    /* output 3: registered 512×424 char 4-plane */
    configure(3, 4, 4, depth_dim.as_mut_ptr(), depth_dim.as_mut_ptr(), true);
    /* output 4: ir 512×424 float32 1-plane */
    configure(4, 1, 1, depth_dim.as_mut_ptr(), depth_dim.as_mut_ptr(), false);
    /* output 5: depth 512×424 float32 1-plane */
    configure(5, 1, 1, depth_dim.as_mut_ptr(), depth_dim.as_mut_ptr(), false);

    /* Attributes */

    /* depth_processor: long, enumindex (0=CPU, 1=OpenGL, 2=OpenCL) */
    let attr = jb_jit_attr_offset_new_long(
        cstr!("depth_processor"), attrflags,
        offset_of!(TJitK2nect, depth_processor) as i64);
    jb_object_addattr_parse(attr, cstr!("label"), cstr!("\"Depth Processor\""));
    jb_object_addattr_parse(attr, cstr!("style"), cstr!("enumindex"));
    jb_object_addattr_parse(attr, cstr!("enumvals"), cstr!("CPU OpenGL OpenCL"));
    jit_class_addattr(S_JIT_CLASS, attr);

    /* max_depth: float32 */
    let attr = jb_jit_attr_offset_new_float32(
        cstr!("max_depth"), attrflags,
        std::ptr::null_mut(),
        offset_of!(TJitK2nect, max_depth) as i64);
    jb_object_addattr_parse(attr, cstr!("label"), cstr!("\"Maximum Depth\""));
    jit_class_addattr(S_JIT_CLASS, attr);

    /* min_depth: float32 */
    let attr = jb_jit_attr_offset_new_float32(
        cstr!("min_depth"), attrflags,
        std::ptr::null_mut(),
        offset_of!(TJitK2nect, min_depth) as i64);
    jb_object_addattr_parse(attr, cstr!("label"), cstr!("\"Minimum Depth\""));
    jit_class_addattr(S_JIT_CLASS, attr);

    /* output_color: long, onoff — maps to field output_rgb */
    let attr = jb_jit_attr_offset_new_long(
        cstr!("output_color"), attrflags,
        offset_of!(TJitK2nect, output_rgb) as i64);
    jb_object_addattr_parse(attr, cstr!("label"), cstr!("\"Output Color\""));
    jb_object_addattr_parse(attr, cstr!("style"), cstr!("onoff"));
    jit_class_addattr(S_JIT_CLASS, attr);

    jit_class_register(S_JIT_CLASS);
    JIT_ERR_NONE
}

#[no_mangle]
pub unsafe extern "C" fn jit_k2nect_new() -> *mut TJitK2nect {
    let x = jit_object_alloc(S_JIT_CLASS) as *mut TJitK2nect;
    if x.is_null() { return std::ptr::null_mut(); }

    (*x).depth_processor = 1; /* OpenGL default */
    (*x).max_depth = 4.5;
    (*x).min_depth = 0.5;
    (*x).output_rgb = 0;
    (*x).kinect = kinect_create();
    x
}

#[no_mangle]
pub unsafe extern "C" fn jit_k2nect_free(x: *mut TJitK2nect) {
    if x.is_null() { return; }
    if !(*x).kinect.is_null() {
        kinect_destroy((*x).kinect);
        (*x).kinect = std::ptr::null_mut();
    }
}

#[no_mangle]
pub unsafe extern "C" fn jit_k2nect_open(x: *mut TJitK2nect) {
    if x.is_null() || (*x).kinect.is_null() { return; }
    if kinect_is_open((*x).kinect) != 0 {
        jb_post(cstr!("jit.k2nect: device already open"));
        return;
    }
    let ret = kinect_open((*x).kinect,
                          (*x).depth_processor as c_int,
                          (*x).min_depth, (*x).max_depth);
    if ret != 0 {
        jb_object_error(x as *mut c_void, cstr!("jit.k2nect: could not open device"));
    } else {
        jb_post(cstr!("jit.k2nect: device is ready"));
    }
}

#[no_mangle]
pub unsafe extern "C" fn jit_k2nect_close(x: *mut TJitK2nect) {
    if x.is_null() || (*x).kinect.is_null() { return; }
    kinect_close((*x).kinect);
    jb_post(cstr!("jit.k2nect: device closed"));
}

/// Returns the raw KinectHandle so the Max wrapper can set the frame callback.
#[no_mangle]
pub unsafe extern "C" fn jit_k2nect_get_kinect_wrapper(x: *mut TJitK2nect)
    -> *mut c_void
{
    if x.is_null() { return std::ptr::null_mut(); }
    (*x).kinect
}

/* -----------------------------------------------------------------------
   matrix_calc — called by the MOP each time a bang arrives (via outputmatrix)
   ----------------------------------------------------------------------- */
#[no_mangle]
pub unsafe extern "C" fn jit_k2nect_matrix_calc(
    x: *mut TJitK2nect, _inputs: *mut c_void, outputs: *mut c_void,
) -> i64 {
    if x.is_null() { return JIT_ERR_INVALID_PTR; }
    if (*x).kinect.is_null()
        || kinect_is_open((*x).kinect) == 0
        || kinect_has_new_frames((*x).kinect) == 0
    {
        return JIT_ERR_NONE;
    }

    /* Retrieve five output matrix handles (0-indexed) */
    let mats: [*mut c_void; 5] = [
        jb_jit_object_method_getindex(outputs, 0),
        jb_jit_object_method_getindex(outputs, 1),
        jb_jit_object_method_getindex(outputs, 2),
        jb_jit_object_method_getindex(outputs, 3),
        jb_jit_object_method_getindex(outputs, 4),
    ];
    if mats.iter().any(|m| m.is_null()) {
        return JIT_ERR_INVALID_PTR;
    }

    /* Lock all matrices */
    let locks: [i64; 5] = mats.map(|m| jb_jit_object_method_lock(m, 1));

    /* Retrieve matrix info and data pointers */
    let zero_info = TJitMatrixInfo { size: 0, type_sym: std::ptr::null_mut(),
        flags: 0, dimcount: 0, dim: [0i64; 32], dimstride: [0i64; 32], planecount: 0 };
    let mut infos = [zero_info; 5];
    let mut bps   = [std::ptr::null_mut::<c_void>(); 5];
    for i in 0..5 {
        jb_jit_object_method_getinfo(mats[i], &mut infos[i] as *mut _ as *mut c_void);
        jb_jit_object_method_getdata(mats[i], &mut bps[i]);
    }

    let mut err = JIT_ERR_NONE;

    if bps[0].is_null() {
        err = JIT_ERR_INVALID_OUTPUT;
    } else {
        /* Get frames from Kinect (blocks up to 1 s) */
        let mut ptrs = KinectFramePtrs::new();
        if kinect_wait_frames((*x).kinect, &mut ptrs) == 0 {
            /* Apply current depth range config (picks up any attribute changes) */
            kinect_set_depth_config((*x).kinect, (*x).min_depth, (*x).max_depth);
            kinect_register_frames((*x).kinect, &mut ptrs, (*x).output_rgb as c_int);

            /* outlet 0: colour */
            if !ptrs.color_data.is_null() && infos[0].dimcount >= 1 {
                copy_bgrx_to_argb_mirrored(ptrs.color_data, bps[0] as *mut u8,
                                           COLOR_WIDTH as usize, COLOR_HEIGHT as usize);
            }
            /* outlet 1: bigdepth */
            if (*x).output_rgb != 0 {
                if !ptrs.bigdepth_data.is_null() && infos[1].dimcount >= 1 && !bps[1].is_null() {
                    copy_bigdepth_data(ptrs.bigdepth_data, bps[1] as *mut c_float);
                }
                /* outlet 2: registered */
                if !ptrs.registered_data.is_null() && infos[2].dimcount >= 1 && !bps[2].is_null() {
                    copy_bgrx_to_argb_mirrored(ptrs.registered_data, bps[2] as *mut u8,
                                               DEPTH_WIDTH as usize, DEPTH_HEIGHT as usize);
                }
            }
            /* outlet 3: ir */
            if !ptrs.ir_data.is_null() && infos[3].dimcount >= 1 {
                copy_float_data_mirrored(ptrs.ir_data, bps[3] as *mut c_float,
                                         DEPTH_WIDTH as usize, DEPTH_HEIGHT as usize);
            }
            /* outlet 4: depth */
            if !ptrs.depth_data.is_null() && infos[4].dimcount >= 1 {
                copy_float_data_mirrored(ptrs.depth_data, bps[4] as *mut c_float,
                                         DEPTH_WIDTH as usize, DEPTH_HEIGHT as usize);
            }
            kinect_release_frames((*x).kinect);
        }
    }

    /* Unlock all matrices (reverse order) */
    for i in (0..5).rev() {
        jb_jit_object_method_lock(mats[i], locks[i]);
    }

    err
}

/* -----------------------------------------------------------------------
   Pixel copy helpers
   ----------------------------------------------------------------------- */

/// BGRX → ARGB with horizontal mirror, arbitrary dimensions.
unsafe fn copy_bgrx_to_argb_mirrored(src: *const u8, dst: *mut u8, w: usize, h: usize) {
    let mut op = dst;
    for y in 0..h {
        for xi in 0..w {
            let p = src.add((y * w + (w - 1 - xi)) * 4);
            *op.add(0) = *p.add(3); /* A (X byte from Kinect) */
            *op.add(1) = *p.add(2); /* R */
            *op.add(2) = *p.add(1); /* G */
            *op.add(3) = *p.add(0); /* B */
            op = op.add(4);
        }
    }
}

/// Horizontal-mirror copy for a float32 plane of any width × height.
unsafe fn copy_float_data_mirrored(src: *const c_float, dst: *mut c_float,
                                   w: usize, h: usize) {
    let mut op = dst;
    for y in 0..h {
        for xi in 0..w {
            let fx = w - 1 - xi;
            *op = *src.add(y * w + fx);
            op = op.add(1);
        }
    }
}

/// bigdepth: float32 1920×1080 with horizontal mirror.
/// Source data starts at offset +BIGDEPTH_OFFSET floats inside bigdepth.data.
/// Infinite/zero values are replaced with 0 (invalid reading).
unsafe fn copy_bigdepth_data(raw_src: *const c_float, dst: *mut c_float) {
    let src = raw_src.add(BIGDEPTH_OFFSET);
    let w   = COLOR_WIDTH as usize;
    let h   = COLOR_HEIGHT as usize;
    let mut op = dst;
    for y in 0..h {
        for xi in 0..w {
            let fx = w - 1 - xi;
            let d  = *src.add(y * 1920 + fx);
            *op = if d.is_infinite() || d <= 0.0 { 0.0 } else { d };
            op = op.add(1);
        }
    }
}

/* -----------------------------------------------------------------------
   MAX OUTER OBJECT
   ----------------------------------------------------------------------- */

/// ext_main entry point — called when Max loads the external bundle.
#[no_mangle]
pub unsafe extern "C" fn ext_main(_r: *mut c_void) {
    /* Register the inner Jitter class first */
    jit_k2nect_init();

    /* Create and configure the outer Max class */
    S_MAX_CLASS = jb_max_class_new(
        cstr!("jit.k2nect_rs"),
        max_jit_k2nect_new as usize as *mut c_void,
        max_jit_k2nect_free as usize as *mut c_void,
        std::mem::size_of::<TMaxJitK2nect>() as i64,
    );

    let obex_offset = offset_of!(TMaxJitK2nect, obex) as i64;
    max_jit_class_obex_setup(S_MAX_CLASS, obex_offset);

    let jit_class = jit_class_findbyname(gensym(cstr!("jit_k2nect_rs")));
    max_jit_class_mop_wrap(S_MAX_CLASS, jit_class, MAX_JIT_MOP_FLAGS_OWN_ADAPT);
    max_jit_class_wrap_standard(S_MAX_CLASS, jit_class, 0);

    /* Methods */
    jb_max_class_addmethod_usurp_low(S_MAX_CLASS,
        max_jit_k2nect_outputmatrix as usize as *mut c_void, cstr!("outputmatrix"));
    jb_max_class_addmethod(S_MAX_CLASS,
        max_jit_k2nect_bang as usize as *mut c_void, cstr!("bang"));
    jb_max_class_addmethod(S_MAX_CLASS,
        max_jit_k2nect_setup_callback as usize as *mut c_void, cstr!("start"));
    jb_max_class_addmethod(S_MAX_CLASS,
        max_jit_k2nect_stop as usize as *mut c_void, cstr!("stop"));
    jb_max_class_addmethod_cant(S_MAX_CLASS,
        max_jit_k2nect_assist as usize as *mut c_void, cstr!("assist"));

    /* Attributes */
    jb_max_class_attr_long(
        S_MAX_CLASS,
        cstr!("output_texture"),
        offset_of!(TMaxJitK2nect, output_texture) as i64,
        cstr!("onoff"),
        cstr!("Output Texture"),
    );
    jb_max_class_attr_sym(
        S_MAX_CLASS,
        cstr!("drawto"),
        offset_of!(TMaxJitK2nect, drawto) as i64,
        cstr!("Draw To"),
        max_jit_k2nect_drawto_set as usize as *mut c_void,
    );

    jb_max_class_register_box(S_MAX_CLASS);
}

/* -----------------------------------------------------------------------
   max_jit_k2nect_new
   ----------------------------------------------------------------------- */
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_new(
    _s: *mut c_void, argc: i64, argv: *mut c_void,
) -> *mut TMaxJitK2nect {
    let x = max_jit_object_alloc(S_MAX_CLASS, gensym(cstr!("jit_k2nect_rs")))
        as *mut TMaxJitK2nect;
    if x.is_null() { return std::ptr::null_mut(); }

    let jit_ob = jb_jit_object_new_0(gensym(cstr!("jit_k2nect_rs")));
    if jit_ob.is_null() {
        jb_object_error(x as *mut c_void, cstr!("jit.k2nect: could not allocate object"));
        max_jit_object_free(x as *mut c_void);
        return std::ptr::null_mut();
    }

    max_jit_mop_setup_simple(x as *mut c_void, jit_ob, argc, argv);
    max_jit_attr_args(x as *mut c_void, argc as i16, argv);

    /* Set output matrix types/dimensions in the Max MOP wrappers (1-indexed) */
    let mut color_dim: [i64; 2] = [COLOR_WIDTH, COLOR_HEIGHT];
    let mut depth_dim: [i64; 2] = [DEPTH_WIDTH, DEPTH_HEIGHT];

    let setup_output = |idx: i64,
                        type_sym: *mut c_void,
                        dims: &mut [i64; 2],
                        planecount: i64| {
        let out = max_jit_mop_getoutput(x as *mut c_void, idx);
        if !out.is_null() {
            jit_attr_setsym(out, _jit_sym_type, type_sym);
            jit_attr_setlong_array(out, _jit_sym_dim, 2, dims.as_mut_ptr());
            jit_attr_setlong(out, _jit_sym_planecount, planecount);
        }
    };

    setup_output(1, _jit_sym_char,    &mut color_dim, 4); /* colour      */
    setup_output(2, _jit_sym_float32, &mut color_dim, 1); /* bigdepth    */
    setup_output(3, _jit_sym_char,    &mut depth_dim, 4); /* registered  */
    setup_output(4, _jit_sym_float32, &mut depth_dim, 1); /* ir          */
    setup_output(5, _jit_sym_float32, &mut depth_dim, 1); /* depth       */

    /* Initialise texture-mode fields */
    (*x).output_texture = 0;
    (*x).drawto         = _jit_sym_nothing;
    for i in 0..5 {
        (*x).tex_objects[i] = std::ptr::null_mut();
        (*x).tex_names[i]   = jit_symbol_unique();
    }

    /* Queue element for thread-safe output */
    (*x).qelem = qelem_new(
        x as *mut c_void,
        max_jit_k2nect_qfn as usize as *mut c_void,
    );

    x
}

/* -----------------------------------------------------------------------
   max_jit_k2nect_free
   ----------------------------------------------------------------------- */
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_free(x: *mut TMaxJitK2nect) {
    if x.is_null() { return; }

    if !(*x).qelem.is_null() {
        qelem_free((*x).qelem);
        (*x).qelem = std::ptr::null_mut();
    }

    for i in 0..5 {
        if !(*x).tex_objects[i].is_null() {
            jit_object_free((*x).tex_objects[i]);
            (*x).tex_objects[i] = std::ptr::null_mut();
        }
    }

    max_jit_mop_free(x as *mut c_void);

    let jit_ob = max_jit_obex_jitob_get(x as *mut c_void);
    if !jit_ob.is_null() {
        jit_object_free(jit_ob);
    }
    max_jit_object_free(x as *mut c_void);
}

/* -----------------------------------------------------------------------
   outputmatrix — triggered by bang or qfn
   ----------------------------------------------------------------------- */
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_outputmatrix(x: *mut TMaxJitK2nect) {
    if x.is_null() { return; }

    let mop = max_jit_obex_adornment_get(x as *mut c_void, _jit_sym_jit_mop);
    if mop.is_null() { return; }

    let jit_ob = max_jit_obex_jitob_get(x as *mut c_void);
    if jit_ob.is_null() { return; }

    let inputs  = jb_jit_object_method_getinputlist(mop);
    let outputs = jb_jit_object_method_getoutputlist(mop);
    let err     = jb_jit_call_matrix_calc(jit_ob, inputs, outputs);
    if err != JIT_ERR_NONE {
        jb_jit_error_code(x as *mut c_void, err);
        return;
    }

    if (*x).output_texture == 0 {
        /* Matrix mode: standard MOP output */
        max_jit_mop_outputmatrix(x as *mut c_void);
    } else {
        /* Texture mode: upload each matrix to a jit_gl_texture and output its name */
        for i in 1i64..=5 {
            let mop_io = max_jit_mop_getoutput(x as *mut c_void, i);
            if mop_io.is_null() { continue; }

            let outlet = max_jit_mop_io_getoutlet(mop_io);
            if outlet.is_null() { continue; }

            let matrix = jb_jit_object_method_getmatrix(mop_io);
            if matrix.is_null() { continue; }

            let mat_name = jit_attr_getsym(matrix, _jit_sym_name);
            if mat_name.is_null() || mat_name == _jit_sym_nothing { continue; }

            /* Lazily create the GL texture object bound to the drawto context */
            let idx = (i - 1) as usize;
            if (*x).tex_objects[idx].is_null() {
                if (*x).drawto == _jit_sym_nothing {
                    jb_object_error(x as *mut c_void,
                        cstr!("jit.k2nect: set @drawto to a GL context name to use @output_texture 1"));
                    return;
                }
                (*x).tex_objects[idx] = jb_jit_object_new_with_sym(
                    cstr!("jit_gl_texture"), (*x).drawto);
                if !(*x).tex_objects[idx].is_null() {
                    jit_attr_setsym((*x).tex_objects[idx], _jit_sym_name,
                                    (*x).tex_names[idx]);
                }
            }

            if !(*x).tex_objects[idx].is_null() {
                /* Send the matrix name to the texture object */
                let mut mat_atom = Atom::new();
                atom_setsym(&mut mat_atom as *mut _ as *mut c_void, mat_name);
                jb_jit_object_method_jit_matrix(
                    (*x).tex_objects[idx], 1, &mut mat_atom as *mut _ as *mut c_void);

                /* Output the texture name symbol through the outlet */
                let mut tex_atom = Atom::new();
                atom_setsym(&mut tex_atom as *mut _ as *mut c_void, (*x).tex_names[idx]);
                outlet_anything(
                    outlet,
                    gensym(cstr!("jit_gl_texture")),
                    1i16,
                    &mut tex_atom as *mut _ as *mut c_void,
                );
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_bang(x: *mut TMaxJitK2nect) {
    max_jit_k2nect_outputmatrix(x);
}

#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_qfn(x: *mut TMaxJitK2nect) {
    max_jit_k2nect_outputmatrix(x);
}

/// Called from the libfreenect2 thread — schedules outputmatrix on the main thread.
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_trigger_output(user: *mut c_void) {
    let x = user as *mut TMaxJitK2nect;
    if !x.is_null() && !(*x).qelem.is_null() {
        qelem_set((*x).qelem);
    }
}

/// `start` message — connect the Kinect callback so output fires automatically.
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_setup_callback(x: *mut TMaxJitK2nect) {
    if x.is_null() { return; }
    let jit_ob = max_jit_obex_jitob_get(x as *mut c_void);
    if jit_ob.is_null() { return; }
    let kinect = jit_k2nect_get_kinect_wrapper(jit_ob as *mut TJitK2nect);
    if !kinect.is_null() {
        kinect_set_callback(kinect,
            Some(max_jit_k2nect_trigger_output),
            x as *mut c_void);
    }
}

/// `stop` message — disconnect the Kinect callback.
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_stop(x: *mut TMaxJitK2nect) {
    if x.is_null() { return; }
    let jit_ob = max_jit_obex_jitob_get(x as *mut c_void);
    if jit_ob.is_null() { return; }
    let kinect = jit_k2nect_get_kinect_wrapper(jit_ob as *mut TJitK2nect);
    if !kinect.is_null() {
        kinect_set_callback(kinect, None, std::ptr::null_mut());
    }
}

/// Inlet/outlet tooltip strings shown in the Max patcher.
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_assist(
    x: *mut TMaxJitK2nect, _b: *mut c_void, msg: i64, arg: i64, s: *mut c_char,
) {
    const ASSIST_OUTLET: i64 = 2;
    if msg == ASSIST_OUTLET {
        let out_tex = if x.is_null() { 0 } else { (*x).output_texture as c_int };
        jb_assist_outlet(s, out_tex, arg);
    }
}

/// `drawto` attribute setter — frees existing GL texture objects when the context changes.
#[no_mangle]
pub unsafe extern "C" fn max_jit_k2nect_drawto_set(
    x: *mut TMaxJitK2nect, _attr: *mut c_void, argc: i64, argv: *const c_void,
) -> i64 {
    if x.is_null() { return MAX_ERR_NONE; }
    if argc > 0 && !argv.is_null() {
        (*x).drawto = atom_getsym(argv);
        for i in 0..5 {
            if !(*x).tex_objects[i].is_null() {
                jit_object_free((*x).tex_objects[i]);
                (*x).tex_objects[i] = std::ptr::null_mut();
            }
        }
    }
    MAX_ERR_NONE
}
