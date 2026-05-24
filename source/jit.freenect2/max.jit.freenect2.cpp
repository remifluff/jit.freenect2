#include "jit.common.h"
#include "kinect_wrapper.h"
#include "max.jit.mop.h"

#define DEPTH_WIDTH  512
#define DEPTH_HEIGHT 424
#define COLOR_WIDTH  1920
#define COLOR_HEIGHT 1080

// Max object instance data
typedef struct _max_jit_freenect2 {
    t_object ob;
    void *obex;
    void *qelem;            // For thread-safe callback
    long output_texture;    // 0=matrix output (default), 1=GL texture output
    t_symbol *drawto;       // GL context name for texture mode
    void *tex_objects[6];   // jit_gl_texture objects (lazily created)
    t_symbol *tex_names[6]; // unique texture name symbols per output
} t_max_jit_freenect2;


// prototypes
BEGIN_USING_C_LINKAGE
t_jit_err jit_freenect2_init(void);
void * max_jit_freenect2_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_freenect2_free(t_max_jit_freenect2 *x);
void max_jit_freenect2_outputmatrix(t_max_jit_freenect2 *x);
void max_jit_freenect2_bang(t_max_jit_freenect2 *x);
void max_jit_freenect2_qfn(t_max_jit_freenect2 *x);
void max_jit_freenect2_setup_callback(t_max_jit_freenect2 *x);
void max_jit_freenect2_stop(t_max_jit_freenect2 *x);
void max_jit_freenect2_trigger_output(void *x); // Static callback trigger
void max_jit_freenect2_assist(t_max_jit_freenect2 *x, void *b, long msg, long arg, char *s);
t_max_err max_jit_freenect2_drawto_set(t_max_jit_freenect2 *x, void *attr, long argc, t_atom *argv);
END_USING_C_LINKAGE

// globals
static void *max_jit_freenect2_class = NULL;


/************************************************************************************/

void ext_main(void *r) {
    t_class *max_class, *jit_class;

    jit_freenect2_init();

    max_class = class_new("jit.freenect2", (method)max_jit_freenect2_new, (method)max_jit_freenect2_free, sizeof(t_max_jit_freenect2), NULL, A_GIMME, 0);
    max_jit_class_obex_setup(max_class, calcoffset(t_max_jit_freenect2, obex));

    jit_class = (t_class *)jit_class_findbyname(gensym("jit_freenect2"));
    max_jit_class_mop_wrap(max_class, jit_class,  MAX_JIT_MOP_FLAGS_OWN_ADAPT);
    max_jit_class_wrap_standard(max_class, jit_class, 0);

    class_addmethod(max_class, (method)max_jit_freenect2_outputmatrix, "outputmatrix", A_USURP_LOW, 0);
    class_addmethod(max_class, (method)max_jit_freenect2_bang, "bang", 0);
    class_addmethod(max_class, (method)max_jit_freenect2_setup_callback, "start", 0);
    class_addmethod(max_class, (method)max_jit_freenect2_stop, "stop", 0);
    class_addmethod(max_class, (method)max_jit_freenect2_assist, "assist", A_CANT, 0);

    // output_texture attribute: switch between matrix (0) and GL texture (1) output
    CLASS_ATTR_LONG(max_class, "output_texture", 0, t_max_jit_freenect2, output_texture);
    CLASS_ATTR_STYLE_LABEL(max_class, "output_texture", 0, "onoff", "Output Texture");

    // drawto attribute: GL context name used when output_texture=1
    CLASS_ATTR_SYM(max_class, "drawto", 0, t_max_jit_freenect2, drawto);
    CLASS_ATTR_LABEL(max_class, "drawto", 0, "Draw To");
    CLASS_ATTR_ACCESSORS(max_class, "drawto", NULL, (method)max_jit_freenect2_drawto_set);

    class_register(CLASS_BOX, max_class);
    max_jit_freenect2_class = max_class;
}

/************************************************************************************/

void * max_jit_freenect2_new(t_symbol *s, long argc, t_atom *argv) {
    t_max_jit_freenect2 *x;
    void *o;

    x = (t_max_jit_freenect2 *)max_jit_object_alloc((t_class *)max_jit_freenect2_class, gensym("jit_freenect2"));

    if (x) {
        o = jit_object_new(gensym("jit_freenect2"));

        if (o) {
            max_jit_mop_setup_simple(x, o, argc, argv);
            max_jit_attr_args(x, argc, argv);
            t_atom_long depthdim[2] = { DEPTH_WIDTH, DEPTH_HEIGHT };
            t_atom_long colordim[2] = { COLOR_WIDTH, COLOR_HEIGHT };

            // output 1: raw color (1920x1080, char, 4-plane ARGB)
            void *output = max_jit_mop_getoutput(x, 1);
            jit_attr_setsym(output, _jit_sym_type, _jit_sym_char);
            jit_attr_setlong_array(output, _jit_sym_dim, 2, colordim);
            jit_attr_setlong(output, _jit_sym_planecount, 4);

            // output 2: ir (512x424, float32, 1-plane, range 0-65535)
            output = max_jit_mop_getoutput(x, 2);
            jit_attr_setsym(output, _jit_sym_type, _jit_sym_float32);
            jit_attr_setlong_array(output, _jit_sym_dim, 2, depthdim);
            jit_attr_setlong(output, _jit_sym_planecount, 1);

            // output 3: raw depth (512x424, float32, 1-plane, mm)
            output = max_jit_mop_getoutput(x, 3);
            jit_attr_setsym(output, _jit_sym_type, _jit_sym_float32);
            jit_attr_setlong_array(output, _jit_sym_dim, 2, depthdim);
            jit_attr_setlong(output, _jit_sym_planecount, 1);

            // output 4: undistorted depth (512x424, float32, 1-plane, mm)
            output = max_jit_mop_getoutput(x, 4);
            jit_attr_setsym(output, _jit_sym_type, _jit_sym_float32);
            jit_attr_setlong_array(output, _jit_sym_dim, 2, depthdim);
            jit_attr_setlong(output, _jit_sym_planecount, 1);

            // output 5: registered color (512x424, char, 4-plane ARGB)
            output = max_jit_mop_getoutput(x, 5);
            jit_attr_setsym(output, _jit_sym_type, _jit_sym_char);
            jit_attr_setlong_array(output, _jit_sym_dim, 2, depthdim);
            jit_attr_setlong(output, _jit_sym_planecount, 4);

            // output 6: bigdepth (1920x1080, float32, 1-plane, mm, 0=no reading)
            output = max_jit_mop_getoutput(x, 6);
            jit_attr_setsym(output, _jit_sym_type, _jit_sym_float32);
            jit_attr_setlong_array(output, _jit_sym_dim, 2, colordim);
            jit_attr_setlong(output, _jit_sym_planecount, 1);

            // Initialize texture output fields
            x->output_texture = 0;
            x->drawto = _jit_sym_nothing;
            for (int i = 0; i < 6; i++) {
                x->tex_objects[i] = NULL;
                x->tex_names[i] = jit_symbol_unique();
            }

            // Create queue element for thread-safe output
            x->qelem = qelem_new(x, (method)max_jit_freenect2_qfn);
        } else {
            jit_object_error((t_object *)x, "jit.freenect2: could not allocate object");
            object_free((t_object *)x);
            x = NULL;
        }
    }

    return (x);
}

void max_jit_freenect2_free(t_max_jit_freenect2 *x) {
    if (x->qelem) {
        qelem_free(x->qelem);           // Free the qelem
    }

    // Free any GL texture objects
    for (int i = 0; i < 6; i++) {
        if (x->tex_objects[i]) {
            jit_object_free(x->tex_objects[i]);
            x->tex_objects[i] = NULL;
        }
    }

    max_jit_mop_free(x);
    jit_object_free(max_jit_obex_jitob_get(x));
    max_jit_object_free(x);
}

/************************************************************************************/

void max_jit_freenect2_outputmatrix(t_max_jit_freenect2 *x) {
    void *mop = max_jit_obex_adornment_get(x, _jit_sym_jit_mop);
    t_jit_err err;

    if (!mop) return;

    err = (t_jit_err)jit_object_method(max_jit_obex_jitob_get(x),
                                       _jit_sym_matrix_calc,
                                       jit_object_method(mop, _jit_sym_getinputlist),
                                       jit_object_method(mop, _jit_sym_getoutputlist));
    if (err) {
        jit_error_code(x, err);
        return;
    }

    if (!x->output_texture) {
        // Matrix mode: standard MOP output
        max_jit_mop_outputmatrix(x);
    } else {
        // Texture mode: upload each matrix to a jit_gl_texture and output its name
        for (long i = 1; i <= 6; i++) {
            void *mop_io = max_jit_mop_getoutput(x, i);
            if (!mop_io) continue;

            void *outlet = max_jit_mop_io_getoutlet(mop_io);
            if (!outlet) continue;

            void *matrix = jit_object_method(mop_io, _jit_sym_getmatrix);
            if (!matrix) continue;

            t_symbol *mat_name = jit_attr_getsym(matrix, _jit_sym_name);
            if (!mat_name || mat_name == _jit_sym_nothing) continue;

            // Lazily create jit_gl_texture bound to the specified GL context
            if (!x->tex_objects[i-1]) {
                if (x->drawto == _jit_sym_nothing) {
                    object_error((t_object *)x, "jit.freenect2: set @drawto to a GL context name to use @output_texture 1");
                    return;
                }
                x->tex_objects[i-1] = jit_object_new(gensym("jit_gl_texture"), x->drawto);
                if (x->tex_objects[i-1]) {
                    jit_attr_setsym(x->tex_objects[i-1], _jit_sym_name, x->tex_names[i-1]);
                }
            }

            if (x->tex_objects[i-1]) {
                // Upload matrix data into the GL texture
                t_atom mat_atom;
                atom_setsym(&mat_atom, mat_name);
                jit_object_method(x->tex_objects[i-1], _jit_sym_jit_matrix, 1, &mat_atom);

                // Output the texture name symbol through the outlet
                t_atom tex_atom;
                atom_setsym(&tex_atom, x->tex_names[i-1]);
                outlet_anything(outlet, gensym("jit_gl_texture"), 1, &tex_atom);
            }
        }
    }
}

void max_jit_freenect2_bang(t_max_jit_freenect2 *x) {
    max_jit_freenect2_outputmatrix(x);
}

void max_jit_freenect2_qfn(t_max_jit_freenect2 *x) {
    max_jit_freenect2_outputmatrix(x);
}

void max_jit_freenect2_trigger_output(void *x) {
    t_max_jit_freenect2 *max_obj = (t_max_jit_freenect2 *)x;

    if (max_obj && max_obj->qelem) {
        qelem_set(max_obj->qelem); // Schedule output
    }
}

// drawto setter: frees existing GL texture objects so they're recreated with the new context
t_max_err max_jit_freenect2_drawto_set(t_max_jit_freenect2 *x, void *attr, long argc, t_atom *argv) {
    if (argc && argv) {
        x->drawto = atom_getsym(argv);
        for (int i = 0; i < 6; i++) {
            if (x->tex_objects[i]) {
                jit_object_free(x->tex_objects[i]);
                x->tex_objects[i] = NULL;
            }
        }
    }
    return MAX_ERR_NONE;
}

// Set up frame callback
void max_jit_freenect2_setup_callback(t_max_jit_freenect2 *x) {
    void *jit_ob = max_jit_obex_jitob_get(x);

    if (jit_ob) {
        // Get the kinect wrapper from the jitter object
        kinect_wrapper *kinect = (kinect_wrapper *)jit_object_method(jit_ob, gensym("get_kinect_wrapper"));

        if (kinect) {
            kinect->setFrameCallback(max_jit_freenect2_trigger_output, x);
        }
    }
}

// Stop frame callback
void max_jit_freenect2_stop(t_max_jit_freenect2 *x) {
    void *jit_ob = max_jit_obex_jitob_get(x);

    if (jit_ob) {
        // Get the kinect wrapper from the jitter object
        kinect_wrapper *kinect = (kinect_wrapper *)jit_object_method(jit_ob, gensym("get_kinect_wrapper"));

        if (kinect) {
            // Clear the callback by passing nullptr
            kinect->setFrameCallback(nullptr, nullptr);
        }
    }
}

void max_jit_freenect2_assist(t_max_jit_freenect2 *x, void *b, long msg, long arg, char *s) {
    if (msg == ASSIST_OUTLET) {
        const char *fmt = x->output_texture ? "(texture)" : "(matrix)";
        switch (arg) {
            case 0:
                sprintf(s, "%s color 1920x1080 char ARGB", fmt);
                break;

            case 1:
                sprintf(s, "%s ir 512x424 float32 0-65535", fmt);
                break;

            case 2:
                sprintf(s, "%s depth 512x424 float32 mm", fmt);
                break;

            case 3:
                sprintf(s, "%s undistorted 512x424 float32 mm", fmt);
                break;

            case 4:
                sprintf(s, "%s registered 512x424 char ARGB", fmt);
                break;

            case 5:
                sprintf(s, "%s bigdepth 1920x1080 float32 mm", fmt);
                break;

            case 6:
                sprintf(s, "dumpout");
                break;
        }
    }
}
