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
void            jit_freenect2_copy_depthdata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_rgbdata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_raw_colordata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);
void            jit_freenect2_copy_raw_irdata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);void            jit_freenect2_copy_depthcolor_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop);t_jit_err jit_freenect2_max_depth_set(t_jit_freenect2 *x, void *attr, long ac, t_atom *av);
t_jit_err jit_freenect2_min_depth_set(t_jit_freenect2 *x, void *attr, long ac, t_atom *av);
END_USING_C_LINKAGE


// globals
static void *s_jit_freenect2_class = NULL;

/************************************************************************************/

t_jit_err jit_freenect2_init(void) {
    long attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    t_jit_object *attr;
    t_jit_object *mop;
    t_jit_object *output2;
    t_jit_object *output3;
    t_jit_object *output4;
    t_jit_object *output5;

    s_jit_freenect2_class = jit_class_new("jit_freenect2", (method)jit_freenect2_new, (method)jit_freenect2_free, sizeof(t_jit_freenect2), 0);

    mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 0, 5);
    jit_class_addadornment(s_jit_freenect2_class, mop);

    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_matrix_calc, "matrix_calc", A_CANT, 0);
    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_open, "open", 0);
    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_close, "close", 0);
    jit_class_addmethod(s_jit_freenect2_class, (method)jit_freenect2_get_kinect_wrapper, "get_kinect_wrapper", A_CANT, 0);

    jit_mop_output_nolink(mop, 1);
    jit_mop_output_nolink(mop, 3);
    jit_mop_output_nolink(mop, 4);
    jit_mop_output_nolink(mop, 5);

    // output2: registered color (512x424, char, 4-plane)
    output2 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 2);
    jit_attr_setlong(output2, _jit_sym_minplanecount, 4);
    jit_attr_setlong(output2, _jit_sym_maxplanecount, 4);
    t_atom_long dim[2] = {DEPTH_WIDTH, DEPTH_HEIGHT};
    jit_attr_setlong_array(output2, _jit_sym_mindim, 2, dim);
    jit_attr_setlong_array(output2, _jit_sym_maxdim, 2, dim);
    jit_attr_setlong(output2, _jit_sym_types, 0);
    jit_attr_setlong(output2, _jit_sym_outputmode, 2);

    // output3: raw color (1920x1080, char, 4-plane)
    output3 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 3);
    jit_attr_setlong(output3, _jit_sym_minplanecount, 4);
    jit_attr_setlong(output3, _jit_sym_maxplanecount, 4);
    t_atom_long rawcolor_dim[2] = {COLOR_WIDTH, COLOR_HEIGHT};
    jit_attr_setlong_array(output3, _jit_sym_mindim, 2, rawcolor_dim);
    jit_attr_setlong_array(output3, _jit_sym_maxdim, 2, rawcolor_dim);
    jit_attr_setlong(output3, _jit_sym_types, 0);
    jit_attr_setlong(output3, _jit_sym_outputmode, 2);

    // output4: raw IR (512x424, float32, 1-plane)
    output4 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 4);
    jit_attr_setlong(output4, _jit_sym_minplanecount, 1);
    jit_attr_setlong(output4, _jit_sym_maxplanecount, 1);
    t_atom_long ir_dim[2] = {DEPTH_WIDTH, DEPTH_HEIGHT};
    jit_attr_setlong_array(output4, _jit_sym_mindim, 2, ir_dim);
    jit_attr_setlong_array(output4, _jit_sym_maxdim, 2, ir_dim);
    jit_attr_setlong(output4, _jit_sym_outputmode, 2);

    // output5: depth-composited RGBA (1920x1080, float32, 4-plane)
    // R/G/B = normalised colour (0-1), A = depth in metres (0 = no reading)
    output5 = (t_jit_object *)jit_object_method(mop, _jit_sym_getoutput, 5);
    jit_attr_setlong(output5, _jit_sym_minplanecount, 4);
    jit_attr_setlong(output5, _jit_sym_maxplanecount, 4);
    t_atom_long depthcolor_dim[2] = {COLOR_WIDTH, COLOR_HEIGHT};
    jit_attr_setlong_array(output5, _jit_sym_mindim, 2, depthcolor_dim);
    jit_attr_setlong_array(output5, _jit_sym_maxdim, 2, depthcolor_dim);
    jit_attr_setlong(output5, _jit_sym_outputmode, 2);

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
    long rgb_savelock = 0, depth_savelock = 0, rawcolor_savelock = 0, rawir_savelock = 0, depthcolor_savelock = 0;
    t_jit_matrix_info rgb_minfo, depth_minfo, rawcolor_minfo, rawir_minfo, depthcolor_minfo;
    char *rgb_bp, *depth_bp, *rawcolor_bp, *rawir_bp, *depthcolor_bp;
    void *rgb_matrix, *depth_matrix, *rawcolor_matrix, *rawir_matrix, *depthcolor_matrix;

    if (!x->kinect->isOpen || !x->kinect->hasNewFrames()) {
        return JIT_ERR_NONE;
    }

    depth_matrix    = jit_object_method(outputs, _jit_sym_getindex, 0);
    rgb_matrix      = jit_object_method(outputs, _jit_sym_getindex, 1);
    rawcolor_matrix = jit_object_method(outputs, _jit_sym_getindex, 2);
    rawir_matrix    = jit_object_method(outputs, _jit_sym_getindex, 3);
    depthcolor_matrix = jit_object_method(outputs, _jit_sym_getindex, 4);

    if (x && depth_matrix && rgb_matrix && rawcolor_matrix && rawir_matrix && depthcolor_matrix) {
        depth_savelock      = (long)jit_object_method(depth_matrix,      _jit_sym_lock, 1);
        rgb_savelock        = (long)jit_object_method(rgb_matrix,        _jit_sym_lock, 1);
        rawcolor_savelock   = (long)jit_object_method(rawcolor_matrix,   _jit_sym_lock, 1);
        rawir_savelock      = (long)jit_object_method(rawir_matrix,      _jit_sym_lock, 1);
        depthcolor_savelock = (long)jit_object_method(depthcolor_matrix, _jit_sym_lock, 1);

        jit_object_method(depth_matrix,      _jit_sym_getinfo, &depth_minfo);
        jit_object_method(rgb_matrix,        _jit_sym_getinfo, &rgb_minfo);
        jit_object_method(rawcolor_matrix,   _jit_sym_getinfo, &rawcolor_minfo);
        jit_object_method(rawir_matrix,      _jit_sym_getinfo, &rawir_minfo);
        jit_object_method(depthcolor_matrix, _jit_sym_getinfo, &depthcolor_minfo);

        jit_object_method(depth_matrix,      _jit_sym_getdata, &depth_bp);
        jit_object_method(rgb_matrix,        _jit_sym_getdata, &rgb_bp);
        jit_object_method(rawcolor_matrix,   _jit_sym_getdata, &rawcolor_bp);
        jit_object_method(rawir_matrix,      _jit_sym_getdata, &rawir_bp);
        jit_object_method(depthcolor_matrix, _jit_sym_getdata, &depthcolor_bp);

        if (!rgb_bp) {
            err = JIT_ERR_INVALID_INPUT; goto out;
        }
        if (!depth_bp) {
            err = JIT_ERR_INVALID_OUTPUT; goto out;
        }

        x->kinect->getframes();
        x->kinect->registerFrames();

        if (x->output_rgb) { jit_freenect2_copy_rgbdata(x, rgb_minfo.dimcount, &rgb_minfo, rgb_bp); }
        jit_freenect2_copy_depthdata(x, depth_minfo.dimcount, &depth_minfo, depth_bp);
        if (rawcolor_bp) { jit_freenect2_copy_raw_colordata(x, rawcolor_minfo.dimcount, &rawcolor_minfo, rawcolor_bp); }
        if (rawir_bp)    { jit_freenect2_copy_raw_irdata(x, rawir_minfo.dimcount, &rawir_minfo, rawir_bp); }
        if (depthcolor_bp && x->output_rgb) { jit_freenect2_copy_depthcolor_data(x, depthcolor_minfo.dimcount, &depthcolor_minfo, depthcolor_bp); }

        x->kinect->release();
    } else {
        return JIT_ERR_INVALID_PTR;
    }

 out:
    jit_object_method(depthcolor_matrix, _jit_sym_lock, depthcolor_savelock);
    jit_object_method(rawir_matrix,    _jit_sym_lock, rawir_savelock);
    jit_object_method(rawcolor_matrix, _jit_sym_lock, rawcolor_savelock);
    jit_object_method(rgb_matrix,      _jit_sym_lock, rgb_savelock);
    jit_object_method(depth_matrix,    _jit_sym_lock, depth_savelock);
    return err;
}

/*********************************RGB************************************************/
void jit_freenect2_looprgb(t_jit_freenect2 *x, t_jit_op_info *out_opinfo, t_jit_matrix_info *out_minfo, char *bop) {
    long xPos, yPos;

    // Correctly access the frame data as unsigned char pointer
    unsigned char *frame_data = (unsigned char *)x->kinect->registered.data;

    out_opinfo->p = bop;
    unsigned char *op = (unsigned char *)out_opinfo->p;
    unsigned char *aPos;

    for (yPos = 0; yPos < DEPTH_HEIGHT; yPos++) {
        for (xPos = 0; xPos < DEPTH_WIDTH; xPos++) {
            // Flip horizontally
            long flipped_x = DEPTH_WIDTH - 1 - xPos;
            unsigned char *source_pixel = frame_data + (yPos * DEPTH_WIDTH + flipped_x) * 4;
            aPos = source_pixel + 3;

            //TA: alpha
            *op = *aPos;
            op++;
            aPos--;
            //TA: red
            *op = *aPos;
            op++;
            aPos--;
            //TA: green
            *op = *aPos;
            op++;
            aPos--;
            //TA: blue
            *op = *aPos;
            op++;
            aPos--;
        }
    }
}

void jit_freenect2_copy_rgbdata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    t_jit_op_info out_opinfo;

    if (dimcount < 1) {
        return; // safety
    }

    //else:
    jit_freenect2_looprgb(x, &out_opinfo, out_minfo, bop);
}

/********************************DEPTH***********************************************/
void jit_freenect2_loopdepth(t_jit_freenect2 *x, t_jit_op_info *out_opinfo, t_jit_matrix_info *out_minfo, char *bop) {
    int xPos, yPos;

    out_opinfo->p = bop;
    float *op = (float *)out_opinfo->p;

    float x_coord, y_coord, z_coord;

    for (yPos = 0; yPos < DEPTH_HEIGHT; yPos++) {
        for (xPos = DEPTH_WIDTH - 1; xPos >= 0; xPos--) {
            //AB: get 3D point from depth
            x->kinect->getPoint3D(yPos, xPos, x_coord, y_coord, z_coord);

            *op = -x_coord;
            op++;

            *op = -y_coord;
            op++;

            *op = -z_coord;
            op++;
        }
    }
}

void jit_freenect2_copy_depthdata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    t_jit_op_info out_opinfo;

    if (dimcount < 1) {
        return; // safety
    }

    // else:
    jit_freenect2_loopdepth(x, &out_opinfo, out_minfo, bop);
}

/*******************************RAW COLOR********************************************/
void jit_freenect2_looprawcolor(t_jit_freenect2 *x, t_jit_op_info *out_opinfo, t_jit_matrix_info *out_minfo, char *bop) {
    long xPos, yPos;

    libfreenect2::Frame *color_frame = x->kinect->frame(Color);
    if (!color_frame) return;

    unsigned char *frame_data = (unsigned char *)color_frame->data;
    unsigned char *op = (unsigned char *)bop;

    for (yPos = 0; yPos < COLOR_HEIGHT; yPos++) {
        for (xPos = 0; xPos < COLOR_WIDTH; xPos++) {
            // Flip horizontally; source is BGRX, write ARGB
            long flipped_x = COLOR_WIDTH - 1 - xPos;
            unsigned char *src = frame_data + (yPos * COLOR_WIDTH + flipped_x) * 4;
            *op++ = src[3]; // A
            *op++ = src[2]; // R
            *op++ = src[1]; // G
            *op++ = src[0]; // B
        }
    }
}

void jit_freenect2_copy_raw_colordata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    t_jit_op_info out_opinfo;

    if (dimcount < 1) return;

    jit_freenect2_looprawcolor(x, &out_opinfo, out_minfo, bop);
}

/*******************************RAW IR***********************************************/
void jit_freenect2_looprawir(t_jit_freenect2 *x, t_jit_op_info *out_opinfo, t_jit_matrix_info *out_minfo, char *bop) {
    long xPos, yPos;

    libfreenect2::Frame *ir_frame = x->kinect->frame(Ir);
    if (!ir_frame) return;

    float *frame_data = (float *)ir_frame->data;
    float *op = (float *)bop;

    for (yPos = 0; yPos < DEPTH_HEIGHT; yPos++) {
        for (xPos = 0; xPos < DEPTH_WIDTH; xPos++) {
            long flipped_x = DEPTH_WIDTH - 1 - xPos;
            *op++ = frame_data[yPos * DEPTH_WIDTH + flipped_x];
        }
    }
}

void jit_freenect2_copy_raw_irdata(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    t_jit_op_info out_opinfo;

    if (dimcount < 1) return;

    jit_freenect2_looprawir(x, &out_opinfo, out_minfo, bop);
}

/***************************DEPTH-COMPOSITED RGBA***********************************
 * float32 4-plane 1920x1080
 * plane 0 = R (0-1), plane 1 = G (0-1), plane 2 = B (0-1)
 * plane 3 = depth in metres (0.0 = no reading)
 *
 * Source colour: raw BGRX camera frame (1920x1080)
 * Source depth:  bigdepth (filter_map stored in kinect->bigdepth.data).
 *   The real pixel data starts at offset_filter_map = 1920*filter_height_half + filter_width_half
 *   = 1920*1 + 2 = 1922 floats (hardcoded to match registration.cpp constants).
 *   Values are in mm; divide by 1000 to get metres. infinity = no depth.
 ***********************************************************************************/
void jit_freenect2_copy_depthcolor_data(t_jit_freenect2 *x, long dimcount, t_jit_matrix_info *out_minfo, char *bop) {
    if (dimcount < 1) return;

    libfreenect2::Frame *color_frame = x->kinect->frame(Color);
    if (!color_frame) return;

    const int bigdepth_offset = 1922; // 1920 * filter_height_half(1) + filter_width_half(2)
    unsigned char *color_data  = (unsigned char *)color_frame->data;
    float         *bd          = (float *)x->kinect->bigdepth.data + bigdepth_offset;
    float         *op          = (float *)bop;
    const float    inf         = std::numeric_limits<float>::infinity();

    for (long yPos = 0; yPos < COLOR_HEIGHT; yPos++) {
        for (long xPos = 0; xPos < COLOR_WIDTH; xPos++) {
            // Mirror horizontally to match the other outputs
            long fx = COLOR_WIDTH - 1 - xPos;

            // Colour: source is BGRX bytes, convert to normalised float RGB
            unsigned char *src = color_data + (yPos * COLOR_WIDTH + fx) * 4;
            *op++ = src[2] / 255.0f; // R
            *op++ = src[1] / 255.0f; // G
            *op++ = src[0] / 255.0f; // B

            // Depth: mm → metres, 0 where no reading
            float d = bd[yPos * 1920 + fx];
            *op++ = (d == inf || d <= 0.0f) ? 0.0f : d / 1000.0f;
        }
    }
}
