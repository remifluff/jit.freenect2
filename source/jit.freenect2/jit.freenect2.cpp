#include <iostream>
#include <limits>
#include "jit.common.h"
#include "kinect_wrapper.h"

// matrix dimensions
#define DEPTH_WIDTH  512
#define DEPTH_HEIGHT 424
#define COLOR_WIDTH  1920
#define COLOR_HEIGHT 1080

// Our Jitter object instance data
typedef struct _jit_freenect2 {
    t_object ob;
    long depth_processor;
    float max_depth;
    float min_depth;
    long output_rgb;
    kinect_wrapper *kinect;
} t_jit_freenect2;


// prototypes
BEGIN_USING_C_LINKAGE
t_jit_err        jit_freenect2_init(void);
t_jit_freenect2 * jit_freenect2_new(void);
void            jit_freenect2_free(t_jit_freenect2 *x);
t_jit_err        jit_freenect2_matrix_calc(t_jit_freenect2 *x, void *inputs, void *outputs);
void            jit_freenect2_open(t_jit_freenect2 *x);
void            jit_freenect2_close(t_jit_freenect2 *x);
kinect_wrapper * jit_freenect2_get_kinect_wrapper(t_jit_freenect2 *x);
void            jit_freenect2_copy_color_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_ir_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_depth_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_undistorted_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_registered_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_bigdepth_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
t_jit_err jit_freenect2_max_depth_set(t_jit_freenect2 *x, void *attr, long ac, t_atom *av);
t_jit_err jit_freenect2_min_depth_set(t_jit_freenect2 *x, void *attr, long ac, t_atom *av);
END_USING_C_LINKAGE


// globals
static void *s_jit_freenect2_class = NULL;

/************************************************************************************/

t_jit_err jit_freenect2_init(void) {
    long attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    t_jit_object *attr;
    t_jit_object *mop;
    t_jit_object *output1, *output2, *output3, *output4, *output5, *output6;
    t_atom_long color_dim[2] = {COLOR_WIDTH, COLOR_HEIGHT};
    t_atom_long depth_dim[2] = {DEPTH_WIDTH, DEPTH_HEIGHT};

    s_jit_freenect2_class = jit_class_new("jit_freenect2", (method)jit_freenect2_new, (method)jit_freenect2_free, sizeof(t_jit_freenect2), 0);

    mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 0, 6);
    jit_class_addadornment(s_jit_freenect2_class, mop);

    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_matrix_calc, "matrix_calc", A_CANT, 0);
    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_open, "open", 0);
    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_close, "close", 0);
    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_get_kinect_wrapper, "get_kinect_wrapper", A_CANT, 0);

    jit_mop_output_nolink(mop, 1);
    jit_mop_output_nolink(mop, 2);
    jit_mop_output_nolink(mop, 3);
    jit_mop_output_nolink(mop, 4);
    jit_mop_output_nolink(mop, 5);
    jit_mop_output_nolink(mop, 6);

    // output1: raw color (1920x1080, char, 4-plane ARGB)
    output1 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 1);
    jit_attr_setlong(output1, _jit_sym_minplanecount, 4);
    jit_attr_setlong(output1, _jit_sym_maxplanecount, 4);
    jit_attr_setlong_array(output1, _jit_sym_mindim, 2, color_dim);
    jit_attr_setlong_array(output1, _jit_sym_maxdim, 2, color_dim);
    jit_attr_setlong(output1, _jit_sym_types, 0);
    jit_attr_setlong(output1, _jit_sym_outputmode, 2);

    // output2: IR (512x424, float32, 1-plane, range 0-65535)
    output2 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 2);
    jit_attr_setlong(output2, _jit_sym_minplanecount, 1);
    jit_attr_setlong(output2, _jit_sym_maxplanecount, 1);
    jit_attr_setlong_array(output2, _jit_sym_mindim, 2, depth_dim);
    jit_attr_setlong_array(output2, _jit_sym_maxdim, 2, depth_dim);
    jit_attr_setlong(output2, _jit_sym_outputmode, 2);

    // output3: raw depth (512x424, float32, 1-plane, mm)
    output3 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 3);
    jit_attr_setlong(output3, _jit_sym_minplanecount, 1);
    jit_attr_setlong(output3, _jit_sym_maxplanecount, 1);
    jit_attr_setlong_array(output3, _jit_sym_mindim, 2, depth_dim);
    jit_attr_setlong_array(output3, _jit_sym_maxdim, 2, depth_dim);
    jit_attr_setlong(output3, _jit_sym_outputmode, 2);

    // output4: undistorted depth (512x424, float32, 1-plane, mm)
    output4 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 4);
    jit_attr_setlong(output4, _jit_sym_minplanecount, 1);
    jit_attr_setlong(output4, _jit_sym_maxplanecount, 1);
    jit_attr_setlong_array(output4, _jit_sym_mindim, 2, depth_dim);
    jit_attr_setlong_array(output4, _jit_sym_maxdim, 2, depth_dim);
    jit_attr_setlong(output4, _jit_sym_outputmode, 2);

    // output5: registered color (512x424, char, 4-plane ARGB)
    output5 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 5);
    jit_attr_setlong(output5, _jit_sym_minplanecount, 4);
    jit_attr_setlong(output5, _jit_sym_maxplanecount, 4);
    jit_attr_setlong_array(output5, _jit_sym_mindim, 2, depth_dim);
    jit_attr_setlong_array(output5, _jit_sym_maxdim, 2, depth_dim);
    jit_attr_setlong(output5, _jit_sym_types, 0);
    jit_attr_setlong(output5, _jit_sym_outputmode, 2);

    // output6: bigdepth (1920x1080, float32, 1-plane, mm, 0=no reading)
    output6 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 6);
    jit_attr_setlong(output6, _jit_sym_minplanecount, 1);
    jit_attr_setlong(output6, _jit_sym_maxplanecount, 1);
    jit_attr_setlong_array(output6, _jit_sym_mindim, 2, color_dim);
    jit_attr_setlong_array(output6, _jit_sym_maxdim, 2, color_dim);
    jit_attr_setlong(output6, _jit_sym_outputmode, 2);

    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset,
                                          "depth_processor", _jit_sym_long, attrflags,
                                          (method)NULL, (method)NULL, calcoffset(t_jit_freenect2, depth_processor));
    object_addattr_parse(attr, "label", _jit_sym_symbol, 0, "\"Depth Processor\"");
    object_addattr_parse(attr, "style", _jit_sym_symbol, 0, "enumindex");
    object_addattr_parse(attr, "enumvals", _jit_sym_symbol, 0, "CPU OpenGL OpenCL");
    jit_class_addattr(s_jit_freenect2_class, attr);

    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset,
                                          "max_depth", _jit_sym_float32, attrflags,
                                          (method)NULL, (method)jit_freenect2_max_depth_set, calcoffset(t_jit_freenect2, max_depth));
    object_addattr_parse(attr, "label", _jit_sym_symbol, 0, "\"Maximum Depth\"");
    jit_class_addattr(s_jit_freenect2_class, attr);
    
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset,
                                          "min_depth", _jit_sym_float32, attrflags,
                                          (method)NULL, (method)jit_freenect2_min_depth_set, calcoffset(t_jit_freenect2, min_depth));
    object_addattr_parse(attr, "label", _jit_sym_symbol, 0, "\"Minimum Depth\"");
    jit_class_addattr(s_jit_freenect2_class, attr);
    
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset, "output_color", _jit_sym_long, attrflags, (method)NULL, (method)NULL, calcoffset(t_jit_freenect2, output_rgb));
    object_addattr_parse(attr, "label", _jit_sym_symbol, 0, "\"Output Color\"");
    object_addattr_parse(attr, "style", _jit_sym_symbol, 0, "onoff");
    jit_class_addattr(s_jit_freenect2_class, attr);

    jit_class_register(s_jit_freenect2_class);
    return JIT_ERR_NONE;
}

/************************************************************************************/

t_jit_freenect2 * jit_freenect2_new(void) {
    t_jit_freenect2 *x = NULL;

    x = (t_jit_freenect2 *)jit_object_alloc(s_jit_freenect2_class);

    if (x) {
        x->depth_processor = 1;
        x->max_depth = 4.5f;
        x->min_depth = 0.5f;
        x->output_rgb = 0;
        x->kinect = new kinect_wrapper();
        x->kinect->use_rgb = &x->output_rgb;
    }

    return x;
}

void jit_freenect2_free(t_jit_freenect2 *x) {
    if (x->kinect != NULL) {
        delete x->kinect;
    }
}

/************************************************************************************/

void jit_freenect2_open(t_jit_freenect2 *x) {
    if (x->kinect->isOpen) {
        post("Device already open");
        return;
    }

    x->kinect->open(x->depth_processor);
}

t_jit_err jit_freenect2_max_depth_set(t_jit_freenect2 *x, void *attr, long ac, t_atom *av) {
    x->max_depth = atom_getfloat(av);
    x->kinect->setMaxDepth(x->max_depth);
}

t_jit_err jit_freenect2_min_depth_set(t_jit_freenect2 *x, void *attr, long ac, t_atom *av) {
    x->min_depth = atom_getfloat(av);
    x->kinect->setMinDepth(x->min_depth);
}

kinect_wrapper * jit_freenect2_get_kinect_wrapper(t_jit_freenect2 *x) {
    return x->kinect;
}

void jit_freenect2_close(t_jit_freenect2 *x) {
    x->kinect->close();
    post("Device closed");
}

/************************************************************************************/

t_jit_err jit_freenect2_matrix_calc(t_jit_freenect2 *x, void *inputs, void *outputs) {
    t_jit_err err = JIT_ERR_NONE;
    long color_savelock=0, ir_savelock=0, depth_savelock=0, undistorted_savelock=0, registered_savelock=0, bigdepth_savelock=0;
    t_jit_matrix_info color_minfo, ir_minfo, depth_minfo, undistorted_minfo, registered_minfo, bigdepth_minfo;
    char *color_bp, *ir_bp, *depth_bp, *undistorted_bp, *registered_bp, *bigdepth_bp;
    void *color_matrix, *ir_matrix, *depth_matrix, *undistorted_matrix, *registered_matrix, *bigdepth_matrix;

    if (!x->kinect->isOpen || !x->kinect->hasNewFrames()) {
        return JIT_ERR_NONE;
    }

    color_matrix       = jit_object_method(outputs, _jit_sym_getindex, 0);
    ir_matrix          = jit_object_method(outputs, _jit_sym_getindex, 1);
    depth_matrix       = jit_object_method(outputs, _jit_sym_getindex, 2);
    undistorted_matrix = jit_object_method(outputs, _jit_sym_getindex, 3);
    registered_matrix  = jit_object_method(outputs, _jit_sym_getindex, 4);
    bigdepth_matrix    = jit_object_method(outputs, _jit_sym_getindex, 5);

    if (x && color_matrix && ir_matrix && depth_matrix && undistorted_matrix && registered_matrix && bigdepth_matrix) {
        color_savelock       = (long)jit_object_method(color_matrix,       _jit_sym_lock, 1);
        ir_savelock          = (long)jit_object_method(ir_matrix,          _jit_sym_lock, 1);
        depth_savelock       = (long)jit_object_method(depth_matrix,       _jit_sym_lock, 1);
        undistorted_savelock = (long)jit_object_method(undistorted_matrix, _jit_sym_lock, 1);
        registered_savelock  = (long)jit_object_method(registered_matrix,  _jit_sym_lock, 1);
        bigdepth_savelock    = (long)jit_object_method(bigdepth_matrix,    _jit_sym_lock, 1);

        jit_object_method(color_matrix,       _jit_sym_getinfo, &color_minfo);
        jit_object_method(ir_matrix,          _jit_sym_getinfo, &ir_minfo);
        jit_object_method(depth_matrix,       _jit_sym_getinfo, &depth_minfo);
        jit_object_method(undistorted_matrix, _jit_sym_getinfo, &undistorted_minfo);
        jit_object_method(registered_matrix,  _jit_sym_getinfo, &registered_minfo);
        jit_object_method(bigdepth_matrix,    _jit_sym_getinfo, &bigdepth_minfo);

        jit_object_method(color_matrix,       _jit_sym_getdata, &color_bp);
        jit_object_method(ir_matrix,          _jit_sym_getdata, &ir_bp);
        jit_object_method(depth_matrix,       _jit_sym_getdata, &depth_bp);
        jit_object_method(undistorted_matrix, _jit_sym_getdata, &undistorted_bp);
        jit_object_method(registered_matrix,  _jit_sym_getdata, &registered_bp);
        jit_object_method(bigdepth_matrix,    _jit_sym_getdata, &bigdepth_bp);

        if (!color_bp) { err = JIT_ERR_INVALID_OUTPUT; goto out; }

        x->kinect->getframes();
        x->kinect->registerFrames();

        jit_freenect2_copy_color_data(x, color_minfo.dimcount, &color_minfo, color_bp);
        if (ir_bp)          { jit_freenect2_copy_ir_data(x, ir_minfo.dimcount, &ir_minfo, ir_bp); }
        if (depth_bp)       { jit_freenect2_copy_depth_data(x, depth_minfo.dimcount, &depth_minfo, depth_bp); }
        if (undistorted_bp) { jit_freenect2_copy_undistorted_data(x, undistorted_minfo.dimcount, &undistorted_minfo, undistorted_bp); }
        if (registered_bp && x->output_rgb) { jit_freenect2_copy_registered_data(x, registered_minfo.dimcount, &registered_minfo, registered_bp); }
        if (bigdepth_bp && x->output_rgb)   { jit_freenect2_copy_bigdepth_data(x, bigdepth_minfo.dimcount, &bigdepth_minfo, bigdepth_bp); }

        x->kinect->release();
    } else {
        return JIT_ERR_INVALID_PTR;
    }

 out:
    jit_object_method(bigdepth_matrix,    _jit_sym_lock, bigdepth_savelock);
    jit_object_method(registered_matrix,  _jit_sym_lock, registered_savelock);
    jit_object_method(undistorted_matrix, _jit_sym_lock, undistorted_savelock);
    jit_object_method(depth_matrix,       _jit_sym_lock, depth_savelock);
    jit_object_method(ir_matrix,          _jit_sym_lock, ir_savelock);
    jit_object_method(color_matrix,       _jit_sym_lock, color_savelock);
    return err;
}

/**** COLOR: raw 1920x1080, BGRX -> ARGB char ****/
void jit_freenect2_copy_color_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    if (dimcount < 1) return;
    libfreenect2::Frame *f = x->kinect->frame(Color);
    if (!f) return;
    unsigned char *src = (unsigned char *)f->data;
    unsigned char *op  = (unsigned char *)bop;
    for (long y = 0; y < COLOR_HEIGHT; y++) {
        for (long xi = 0; xi < COLOR_WIDTH; xi++) {
            long fx = COLOR_WIDTH - 1 - xi;
            unsigned char *p = src + (y * COLOR_WIDTH + fx) * 4;
            *op++ = p[3]; // A (X byte)
            *op++ = p[2]; // R
            *op++ = p[1]; // G
            *op++ = p[0]; // B
        }
    }
}

/**** IR: 512x424, float32, range 0-65535 ****/
void jit_freenect2_copy_ir_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    if (dimcount < 1) return;
    libfreenect2::Frame *f = x->kinect->frame(Ir);
    if (!f) return;
    float *src = (float *)f->data;
    float *op  = (float *)bop;
    for (long y = 0; y < DEPTH_HEIGHT; y++) {
        for (long xi = 0; xi < DEPTH_WIDTH; xi++) {
            long fx = DEPTH_WIDTH - 1 - xi;
            *op++ = src[y * DEPTH_WIDTH + fx];
        }
    }
}

/**** RAW DEPTH: 512x424, float32, mm (non-positive/NaN/inf = invalid) ****/
void jit_freenect2_copy_depth_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    if (dimcount < 1) return;
    libfreenect2::Frame *f = x->kinect->frame(Depth);
    if (!f) return;
    float *src = (float *)f->data;
    float *op  = (float *)bop;
    for (long y = 0; y < DEPTH_HEIGHT; y++) {
        for (long xi = 0; xi < DEPTH_WIDTH; xi++) {
            long fx = DEPTH_WIDTH - 1 - xi;
            *op++ = src[y * DEPTH_WIDTH + fx];
        }
    }
}

/**** UNDISTORTED DEPTH: 512x424, float32, mm (from apply() or undistortDepth()) ****/
void jit_freenect2_copy_undistorted_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    if (dimcount < 1) return;
    float *src = (float *)x->kinect->undistorted.data;
    float *op  = (float *)bop;
    for (long y = 0; y < DEPTH_HEIGHT; y++) {
        for (long xi = 0; xi < DEPTH_WIDTH; xi++) {
            long fx = DEPTH_WIDTH - 1 - xi;
            *op++ = src[y * DEPTH_WIDTH + fx];
        }
    }
}

/**** REGISTERED COLOR: 512x424, BGRX -> ARGB char (from apply(), requires @output_color 1) ****/
void jit_freenect2_copy_registered_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    if (dimcount < 1) return;
    unsigned char *src = (unsigned char *)x->kinect->registered.data;
    unsigned char *op  = (unsigned char *)bop;
    for (long y = 0; y < DEPTH_HEIGHT; y++) {
        for (long xi = 0; xi < DEPTH_WIDTH; xi++) {
            long fx = DEPTH_WIDTH - 1 - xi;
            unsigned char *p = src + (y * DEPTH_WIDTH + fx) * 4;
            *op++ = p[3]; // A
            *op++ = p[2]; // R
            *op++ = p[1]; // G
            *op++ = p[0]; // B
        }
    }
}

/**** BIGDEPTH: 1920x1080, float32, mm, 0=no reading (from apply(), requires @output_color 1)
 * filter_map stored in bigdepth.data; valid 1920x1080 data begins at
 * offset_filter_map = 1920*filter_height_half(1) + filter_width_half(2) = 1922 floats. ****/
void jit_freenect2_copy_bigdepth_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    if (dimcount < 1) return;
    const int offset = 1922;
    float *src = (float *)x->kinect->bigdepth.data + offset;
    float *op  = (float *)bop;
    const float inf = std::numeric_limits<float>::infinity();
    for (long y = 0; y < COLOR_HEIGHT; y++) {
        for (long xi = 0; xi < COLOR_WIDTH; xi++) {
            long fx = COLOR_WIDTH - 1 - xi;
            float d = src[y * 1920 + fx];
            *op++ = (d == inf || d <= 0.0f) ? 0.0f : d;
        }
    }
}
