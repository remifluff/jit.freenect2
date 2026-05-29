use std::path::PathBuf;

fn main() {
    let sdk_base = PathBuf::from("../max-sdk-base/c74support");
    let max_includes = sdk_base.join("max-includes");
    let msp_includes = sdk_base.join("msp-includes");
    let jit_includes = sdk_base.join("jit-includes");

    // Compile kinect_shim.cpp (libfreenect2 wrapper with C ABI)
    cc::Build::new()
        .cpp(true)
        .file("csrc/kinect_shim.cpp")
        .include("csrc")
        .include("/usr/local/include")
        .flag("-std=c++14")
        .warnings(false)
        .compile("kinect_shim");

    // Compile jitter_bridge.cpp (Max/Jitter API wrapper with C ABI)
    cc::Build::new()
        .cpp(true)
        .file("csrc/jitter_bridge.cpp")
        .include("csrc")
        .include(&max_includes)
        .include(&msp_includes)
        .include(&jit_includes)
        .define("MAC_VERSION", None)
        .flag("-std=c++14")
        .warnings(false)
        .compile("jitter_bridge");

    // Link against MaxAudioAPI framework (MSP/Max audio, provides core symbols at runtime)
    println!("cargo:rustc-link-search=framework={}", msp_includes.display());
    println!("cargo:rustc-link-lib=framework=MaxAudioAPI");

    // Link against JitterAPI framework
    println!("cargo:rustc-link-search=framework={}", jit_includes.display());
    println!("cargo:rustc-link-lib=framework=JitterAPI");

    // Link against libfreenect2
    println!("cargo:rustc-link-search=native=/usr/local/lib");
    println!("cargo:rustc-link-lib=dylib=freenect2");

    // Allow undefined symbols at link time — Max host provides MaxAPI symbols at runtime
    // via @executable_path/../Frameworks/ dylib resolution when it loads the .mxo bundle.
    println!("cargo:rustc-link-arg=-undefined");
    println!("cargo:rustc-link-arg=dynamic_lookup");

    // Embed rpath so the loader can find libfreenect2 wherever it was installed
    println!("cargo:rustc-link-arg=-Wl,-rpath,/usr/local/lib");

    // Link C++ standard library
    println!("cargo:rustc-link-lib=c++");

    // Rerun build script if C++ sources change
    println!("cargo:rerun-if-changed=csrc/kinect_shim.h");
    println!("cargo:rerun-if-changed=csrc/kinect_shim.cpp");
    println!("cargo:rerun-if-changed=csrc/jitter_bridge.h");
    println!("cargo:rerun-if-changed=csrc/jitter_bridge.cpp");
}
