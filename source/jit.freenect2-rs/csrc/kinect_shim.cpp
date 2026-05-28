#include "kinect_shim.h"

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>

using namespace libfreenect2;

/* -----------------------------------------------------------------------
   Custom frame listener: extends SyncMultiFrameListener to fire a user
   callback on the libfreenect2 thread when a Depth frame arrives.
   ----------------------------------------------------------------------- */
class CallbackListener : public SyncMultiFrameListener {
public:
    CallbackListener()
        : SyncMultiFrameListener(Frame::Color | Frame::Ir | Frame::Depth)
        , cb_(nullptr)
        , user_(nullptr)
    {}

    void setCallback(KinectCallback cb, void* user) {
        cb_   = cb;
        user_ = user;
    }

    bool onNewFrame(Frame::Type type, Frame* frame) override {
        bool ret = SyncMultiFrameListener::onNewFrame(type, frame);
        if (type == Frame::Depth && cb_) {
            cb_(user_);
        }
        return ret;
    }

private:
    KinectCallback cb_;
    void*          user_;
};

/* -----------------------------------------------------------------------
   Internal implementation struct
   ----------------------------------------------------------------------- */
struct KinectImpl {
    Freenect2        ctx;
    Freenect2Device* device;
    Registration*    registration;
    Frame            undistorted;      /* 512 × 424 × 4 bytes */
    Frame            registered;       /* 512 × 424 × 4 bytes */
    /* bigdepth_raw is the backing allocation for bigdepth.data.
       It is 64 bytes larger than the frame data so that bigdepth.data
       has 64 bytes of valid memory before it.  The SIMD-vectorised
       filter-write loop inside libfreenect2::RegistrationImpl::apply()
       performs backward 16-byte NEON loads that can reach up to 12 bytes
       before bigdepth.data; without this padding those loads underflow
       the malloc region and trigger an EXC_BAD_ACCESS (SIGSEGV). */
    unsigned char*   bigdepth_raw;     /* raw allocation: 64-byte pad + frame data */
    Frame            bigdepth;         /* 1924 × 1082 × 4 bytes (filter border) */
    CallbackListener listener;
    FrameMap         frames;
    bool             is_open;
    bool             frames_held;

    KinectImpl()
        : device(nullptr)
        , registration(nullptr)
        , undistorted(512, 424, 4)
        , registered(512, 424, 4)
        , bigdepth_raw(new unsigned char[1924 * 1082 * 4 + 64]())
        , bigdepth(1924, 1082, 4, bigdepth_raw + 64)
        , is_open(false)
        , frames_held(false)
    {}

    ~KinectImpl() {
        delete[] bigdepth_raw;
    }
};

/* -----------------------------------------------------------------------
   C ABI
   ----------------------------------------------------------------------- */
extern "C" {

KinectHandle kinect_create(void) {
    return new KinectImpl();
}

void kinect_destroy(KinectHandle h) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    if (impl->is_open) {
        kinect_close(h);
    }
    delete impl;
}

int kinect_open(KinectHandle h, int pipeline_type, float min_depth, float max_depth) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    if (impl->is_open) return 0;

    if (impl->ctx.enumerateDevices() == 0) return -1;

    PacketPipeline* pipeline = nullptr;
    switch (pipeline_type) {
        case 0:  pipeline = new CpuPacketPipeline();   break;
        case 2:  pipeline = new OpenCLPacketPipeline(); break;
        default: pipeline = new OpenGLPacketPipeline(); break;
    }

    impl->device = impl->ctx.openDefaultDevice(pipeline);
    if (!impl->device) return -1;

    /* Configure depth range via the public Freenect2Device::setConfiguration API,
       avoiding the need for internal depth_packet_processor.h headers. */
    Freenect2Device::Config config;
    config.MinDepth              = min_depth;
    config.MaxDepth              = max_depth;
    config.EnableBilateralFilter = true;
    config.EnableEdgeAwareFilter = true;
    impl->device->setConfiguration(config);

    impl->device->setColorFrameListener(&impl->listener);
    impl->device->setIrAndDepthFrameListener(&impl->listener);

    if (!impl->device->start()) {
        impl->device->close();
        impl->device = nullptr;
        return -1;
    }

    impl->registration = new Registration(
        impl->device->getIrCameraParams(),
        impl->device->getColorCameraParams()
    );
    impl->is_open = true;
    return 0;
}

void kinect_set_depth_config(KinectHandle h, float min_depth, float max_depth) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    if (!impl->device) return;

    Freenect2Device::Config config;
    config.MinDepth              = min_depth;
    config.MaxDepth              = max_depth;
    config.EnableBilateralFilter = true;
    config.EnableEdgeAwareFilter = true;
    impl->device->setConfiguration(config);
}

void kinect_set_callback(KinectHandle h, KinectCallback cb, void* user) {
    static_cast<KinectImpl*>(h)->listener.setCallback(cb, user);
}

int kinect_is_open(KinectHandle h) {
    return static_cast<KinectImpl*>(h)->is_open ? 1 : 0;
}

int kinect_has_new_frames(KinectHandle h) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    return (impl->is_open && impl->listener.hasNewFrame()) ? 1 : 0;
}

int kinect_wait_frames(KinectHandle h, KinectFramePtrs* out) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    if (!impl->is_open || impl->frames_held) return -1;

    /* Block up to 1 second for a complete (Color+IR+Depth) frame set. */
    if (!impl->listener.waitForNewFrame(impl->frames, 1000)) {
        return -1; /* timeout */
    }
    impl->frames_held = true;

    Frame* color = impl->frames[Frame::Color];
    Frame* ir    = impl->frames[Frame::Ir];
    Frame* depth = impl->frames[Frame::Depth];

    out->color_data       = color ? reinterpret_cast<const unsigned char*>(color->data) : nullptr;
    out->ir_data          = ir    ? reinterpret_cast<const float*>(ir->data)             : nullptr;
    out->depth_data       = depth ? reinterpret_cast<const float*>(depth->data)          : nullptr;
    /* Filled in by kinect_register_frames: */
    out->registered_data  = nullptr;
    out->bigdepth_data    = nullptr;
    return 0;
}

void kinect_register_frames(KinectHandle h, KinectFramePtrs* out, int use_rgb) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    if (!impl->is_open || !impl->registration || !impl->frames_held) return;

    Frame* color = impl->frames[Frame::Color];
    Frame* depth = impl->frames[Frame::Depth];

    if (use_rgb) {
        impl->registration->apply(
            color, depth,
            &impl->undistorted, &impl->registered,
            true, &impl->bigdepth
        );
        out->registered_data = reinterpret_cast<const unsigned char*>(impl->registered.data);
        out->bigdepth_data   = reinterpret_cast<const float*>(impl->bigdepth.data);
    } else {
        impl->registration->undistortDepth(depth, &impl->undistorted);
    }
}

void kinect_release_frames(KinectHandle h) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    if (!impl->frames_held) return;
    impl->listener.release(impl->frames);
    impl->frames_held = false;
}

void kinect_close(KinectHandle h) {
    KinectImpl* impl = static_cast<KinectImpl*>(h);
    if (!impl->is_open) return;

    impl->listener.setCallback(nullptr, nullptr);
    if (impl->frames_held) kinect_release_frames(h);

    impl->device->stop();
    impl->device->close();
    delete impl->registration;
    impl->registration = nullptr;
    impl->device       = nullptr;
    impl->is_open      = false;
}

} /* extern "C" */
