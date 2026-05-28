#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle to the KinectImpl C++ object.
 */
typedef void* KinectHandle;

/**
 * Pointers to raw frame data returned by kinect_wait_frames / kinect_register_frames.
 *
 * color_data      : XBGR 1920×1080 (4 bytes/pixel; X=byte0, B=byte1, G=byte2, R=byte3)
 * ir_data         : float32 512×424
 * depth_data      : float32 512×424, millimetres
 * registered_data : XBGR 512×424 (set after kinect_register_frames when use_rgb=1)
 * bigdepth_data   : float32 1924×1082; valid 1920×1080 data starts at offset +1922 floats
 *                   (set after kinect_register_frames when use_rgb=1)
 */
typedef struct KinectFramePtrs {
    const unsigned char* color_data;
    const float*         ir_data;
    const float*         depth_data;
    const unsigned char* registered_data;
    const float*         bigdepth_data;
} KinectFramePtrs;

/** Callback type — fired from the libfreenect2 processing thread when a Depth frame arrives. */
typedef void (*KinectCallback)(void* user);

/** Allocate a new KinectImpl (does NOT open the device). */
KinectHandle kinect_create(void);

/** Destroy the KinectImpl; closes the device first if open. */
void kinect_destroy(KinectHandle h);

/**
 * Open the first connected Kinect v2 device.
 *
 * pipeline_type: 0=CPU, 1=OpenGL (default), 2=OpenCL
 * Returns 0 on success, -1 on failure.
 */
int kinect_open(KinectHandle h, int pipeline_type, float min_depth, float max_depth);

/** Update depth processing range on an already-open device. */
void kinect_set_depth_config(KinectHandle h, float min_depth, float max_depth);

/**
 * Set (or clear) the frame callback.
 * The callback is fired from the libfreenect2 thread; cb=NULL disables it.
 */
void kinect_set_callback(KinectHandle h, KinectCallback cb, void* user);

/** Returns 1 if the device is currently open. */
int kinect_is_open(KinectHandle h);

/** Non-blocking check: returns 1 if at least one new frame set is queued. */
int kinect_has_new_frames(KinectHandle h);

/**
 * Block until a new frame set is ready (1-second timeout).
 * Fills *out with color/ir/depth raw data pointers.
 * undistorted/registered/bigdepth are null until kinect_register_frames() is called.
 * Returns 0 on success, -1 on timeout/error.
 */
int kinect_wait_frames(KinectHandle h, KinectFramePtrs* out);

/**
 * Run depth registration (must be called after kinect_wait_frames).
 * If use_rgb != 0, also fills registered_data and bigdepth_data in *out.
 * Always fills undistorted_data.
 */
void kinect_register_frames(KinectHandle h, KinectFramePtrs* out, int use_rgb);

/** Release the current frame set back to libfreenect2 so new frames can queue. */
void kinect_release_frames(KinectHandle h);

/** Stop and close the device (idempotent). */
void kinect_close(KinectHandle h);

#ifdef __cplusplus
} /* extern "C" */
#endif
