use std::ffi::{CStr, CString};
use std::io;
use std::os::raw::{c_char, c_int, c_void};
use std::ptr;

const MAX_JPEG_SIZE: usize = 8 * 1024 * 1024;
const QUALITY_BEST: c_int = 0;

extern "C" {
    fn hcnet_thermal_login(
        host: *const c_char,
        port: u16,
        username: *const c_char,
        password: *const c_char,
        channel: i32,
        out_handle: *mut *mut c_void,
        sdk_error: *mut u32,
    ) -> c_int;

    fn hcnet_thermal_capture_jpeg(
        handle: *mut c_void,
        quality: c_int,
        buffer: *mut u8,
        buffer_size: usize,
        out_size: *mut usize,
        sdk_error: *mut u32,
    ) -> c_int;

    fn hcnet_thermal_logout(handle: *mut c_void, sdk_error: *mut u32) -> c_int;

    fn hcnet_thermal_strerror(status: c_int) -> *const c_char;
}

fn error_message(status: c_int, sdk_error: u32) -> String {
    let base = unsafe {
        let message = hcnet_thermal_strerror(status);
        if message.is_null() {
            format!("error {status}")
        } else {
            CStr::from_ptr(message).to_string_lossy().into_owned()
        }
    };
    if sdk_error == 0 {
        base
    } else {
        format!("{base} (SDK error {sdk_error})")
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let host = CString::new("192.168.1.64")?;
    let username = CString::new("admin")?;
    let password = CString::new("abcd1234")?;

    let mut handle: *mut c_void = ptr::null_mut();
    let mut sdk_error: u32 = 0;
    let status = unsafe {
        hcnet_thermal_login(
            host.as_ptr(),
            8000,
            username.as_ptr(),
            password.as_ptr(),
            1,
            &mut handle,
            &mut sdk_error,
        )
    };
    if status != 0 {
        return Err(io::Error::new(
            io::ErrorKind::Other,
            format!("login failed: {}", error_message(status, sdk_error)),
        )
        .into());
    }

    let mut jpeg = vec![0u8; MAX_JPEG_SIZE];
    let mut jpeg_size: usize = 0;
    let status = unsafe {
        hcnet_thermal_capture_jpeg(
            handle,
            QUALITY_BEST,
            jpeg.as_mut_ptr(),
            jpeg.len(),
            &mut jpeg_size,
            &mut sdk_error,
        )
    };
    if status != 0 {
        unsafe {
            hcnet_thermal_logout(handle, &mut sdk_error);
        }
        return Err(io::Error::new(
            io::ErrorKind::Other,
            format!("capture failed: {}", error_message(status, sdk_error)),
        )
        .into());
    }

    jpeg.truncate(jpeg_size);
    std::fs::write("thermal.jpg", &jpeg)?;

    let status = unsafe { hcnet_thermal_logout(handle, &mut sdk_error) };
    if status != 0 {
        return Err(io::Error::new(
            io::ErrorKind::Other,
            format!("logout failed: {}", error_message(status, sdk_error)),
        )
        .into());
    }

    println!("saved {} bytes to thermal.jpg", jpeg_size);
    Ok(())
}
