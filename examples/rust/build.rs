use std::env;
use std::process::Command;

fn main() {
    for variable in [
        "HCNET_THERMAL_LIB_DIR",
        "HCNET_SDK_LIB_DIR",
        "HCNET_SDK_LIB_NAME",
        "HCNET_OPENCV_LIBS",
        "HCNET_THERMAL_WITH_UVC",
    ] {
        println!("cargo:rerun-if-env-changed={variable}");
    }

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
    if cfg!(target_os = "macos") {
        println!("cargo:rustc-link-lib=dylib=c++");
    }
    if cfg!(windows) {
        for library in ["ws2_32", "winmm", "iphlpapi", "shlwapi"] {
            println!("cargo:rustc-link-lib=dylib={library}");
        }
    }

    link_opencv();
}

/// Link OpenCV when the static library was built with UVC support.
///
/// Set HCNET_THERMAL_WITH_UVC=0 to skip this, or HCNET_OPENCV_LIBS to pass
/// linker flags explicitly (for example "-L/path/to/opencv/lib -lopencv_core4").
fn link_opencv() {
    if env::var("HCNET_THERMAL_WITH_UVC")
        .map(|value| value == "0")
        .unwrap_or(false)
    {
        return;
    }

    let flags = env::var("HCNET_OPENCV_LIBS").ok().or_else(|| {
        let output = Command::new("pkg-config")
            .args(["--libs", "opencv4"])
            .output()
            .ok()?;
        if !output.status.success() {
            return None;
        }
        Some(String::from_utf8_lossy(&output.stdout).into_owned())
    });

    let Some(flags) = flags else {
        println!(
            "cargo:warning=OpenCV not found; set HCNET_OPENCV_LIBS or \
             HCNET_THERMAL_WITH_UVC=0"
        );
        return;
    };

    for token in flags.split_whitespace() {
        if let Some(name) = token.strip_prefix("-l") {
            println!("cargo:rustc-link-lib=dylib={name}");
        } else if let Some(path) = token.strip_prefix("-L") {
            println!("cargo:rustc-link-search=native={path}");
        }
    }
}
