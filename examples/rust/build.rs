use std::env;

fn main() {
    let lib_dir = env::var("HCNET_THERMAL_LIB_DIR")
        .unwrap_or_else(|_| "../../build".to_string());
    println!("cargo:rustc-link-search=native={lib_dir}");
    println!("cargo:rustc-link-lib=static=hcnet_thermal");

    let sdk_lib_dir = env::var("HCNET_SDK_LIB_DIR")
        .unwrap_or_else(|_| "../../lib".to_string());
    println!("cargo:rustc-link-search=native={sdk_lib_dir}");

    let sdk_lib_name = env::var("HCNET_SDK_LIB_NAME").unwrap_or_else(|_| {
        if cfg!(windows) {
            "HCNetSDK".to_string()
        } else {
            "hcnetsdk".to_string()
        }
    });
    println!("cargo:rustc-link-lib=dylib={sdk_lib_name}");

    if cfg!(target_os = "linux") {
        println!("cargo:rustc-link-lib=dylib=stdc++");
    }
    if cfg!(windows) {
        for library in ["ws2_32", "winmm", "iphlpapi", "shlwapi"] {
            println!("cargo:rustc-link-lib=dylib={library}");
        }
    }
}
