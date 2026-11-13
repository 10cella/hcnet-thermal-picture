//! Safe Rust wrappers around the `hcnet_thermal` C ABI.
//!
//! The crate links the static library built by the project's CMake build
//! and exposes handles for both camera types:
//!
//! - [`ThermalCamera`]: Hikvision thermal camera over the vendor SDK
//! - [`UvcCamera`]: generic UVC camera through OpenCV

use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_int, c_void};

/// Recommended caller buffer size for one JPEG frame.
pub const MAX_JPEG_SIZE: usize = 8 * 1024 * 1024;

pub const QUALITY_BEST: c_int = 0;
pub const QUALITY_BETTER: c_int = 1;
pub const QUALITY_NORMAL: c_int = 2;

unsafe extern "C" {
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

    fn hcnet_thermal_uvc_available() -> c_int;

    fn hcnet_thermal_uvc_open(
        device: *const c_char,
        width: c_int,
        height: c_int,
        warmup_frames: c_int,
        out_handle: *mut *mut c_void,
    ) -> c_int;

    fn hcnet_thermal_uvc_capture_jpeg(
        handle: *mut c_void,
        jpeg_quality: c_int,
        buffer: *mut u8,
        buffer_size: usize,
        out_size: *mut usize,
    ) -> c_int;

    fn hcnet_thermal_uvc_close(handle: *mut c_void) -> c_int;

    fn hcnet_thermal_strerror(status: c_int) -> *const c_char;
}

#[derive(Debug)]
pub struct Error {
    pub status: c_int,
    pub message: String,
}

impl Error {
    fn new(status: c_int, sdk_error: u32) -> Self {
        let mut message = status_message(status);
        if sdk_error != 0 {
            message.push_str(&format!(" (SDK error {sdk_error})"));
        }
        Self { status, message }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.message)
    }
}

impl std::error::Error for Error {}

fn status_message(status: c_int) -> String {
    unsafe {
        let message = hcnet_thermal_strerror(status);
        if message.is_null() {
            format!("error {status}")
        } else {
            CStr::from_ptr(message).to_string_lossy().into_owned()
        }
    }
}

fn jpeg_buffer() -> Vec<u8> {
    vec![0u8; MAX_JPEG_SIZE]
}

/// A logged-in Hikvision thermal camera.
pub struct ThermalCamera {
    handle: *mut c_void,
}

impl ThermalCamera {
    pub fn login(
        host: &str,
        port: u16,
        username: &str,
        password: &str,
        channel: i32,
    ) -> Result<Self, Error> {
        let host = CString::new(host).map_err(|_| Error::new(-1, 0))?;
        let username = CString::new(username).map_err(|_| Error::new(-1, 0))?;
        let password = CString::new(password).map_err(|_| Error::new(-1, 0))?;

        let mut handle: *mut c_void = std::ptr::null_mut();
        let mut sdk_error = 0u32;
        let status = unsafe {
            hcnet_thermal_login(
                host.as_ptr(),
                port,
                username.as_ptr(),
                password.as_ptr(),
                channel,
                &mut handle,
                &mut sdk_error,
            )
        };
        if status != 0 {
            return Err(Error::new(status, sdk_error));
        }
        Ok(Self { handle })
    }

    /// Capture one JPEG. `quality` is 0 (best), 1 (better) or 2 (normal).
    pub fn capture(&self, quality: c_int) -> Result<Vec<u8>, Error> {
        let mut buffer = jpeg_buffer();
        loop {
            let mut out_size = 0usize;
            let mut sdk_error = 0u32;
            let status = unsafe {
                hcnet_thermal_capture_jpeg(
                    self.handle,
                    quality,
                    buffer.as_mut_ptr(),
                    buffer.len(),
                    &mut out_size,
                    &mut sdk_error,
                )
            };
            if status == -5 && out_size > buffer.len() {
                buffer.resize(out_size, 0);
                continue;
            }
            if status != 0 {
                return Err(Error::new(status, sdk_error));
            }
            buffer.truncate(out_size);
            return Ok(buffer);
        }
    }

    pub fn logout(&mut self) -> Result<(), Error> {
        if self.handle.is_null() {
            return Ok(());
        }
        let mut sdk_error = 0u32;
        let status = unsafe { hcnet_thermal_logout(self.handle, &mut sdk_error) };
        self.handle = std::ptr::null_mut();
        if status != 0 {
            return Err(Error::new(status, sdk_error));
        }
        Ok(())
    }
}

impl Drop for ThermalCamera {
    fn drop(&mut self) {
        let _ = self.logout();
    }
}

/// A UVC camera opened through the library's OpenCV backend.
pub struct UvcCamera {
    handle: *mut c_void,
}

impl UvcCamera {
    /// Whether the linked library was built with OpenCV UVC support.
    pub fn available() -> bool {
        unsafe { hcnet_thermal_uvc_available() != 0 }
    }

    /// `device` is a camera index such as `"0"` or a device path such as
    /// `"/dev/video0"`.
    pub fn open(
        device: &str,
        width: i32,
        height: i32,
        warmup_frames: i32,
    ) -> Result<Self, Error> {
        let device = CString::new(device).map_err(|_| Error::new(-1, 0))?;
        let mut handle: *mut c_void = std::ptr::null_mut();
        let status = unsafe {
            hcnet_thermal_uvc_open(
                device.as_ptr(),
                width,
                height,
                warmup_frames,
                &mut handle,
            )
        };
        if status != 0 {
            return Err(Error::new(status, 0));
        }
        Ok(Self { handle })
    }

    /// Capture one frame and encode it as JPEG. `jpeg_quality` is 1..100.
    pub fn capture_jpeg(&self, jpeg_quality: c_int) -> Result<Vec<u8>, Error> {
        let mut buffer = jpeg_buffer();
        loop {
            let mut out_size = 0usize;
            let status = unsafe {
                hcnet_thermal_uvc_capture_jpeg(
                    self.handle,
                    jpeg_quality,
                    buffer.as_mut_ptr(),
                    buffer.len(),
                    &mut out_size,
                )
            };
            if status == -5 && out_size > buffer.len() {
                buffer.resize(out_size, 0);
                continue;
            }
            if status != 0 {
                return Err(Error::new(status, 0));
            }
            buffer.truncate(out_size);
            return Ok(buffer);
        }
    }

    pub fn close(&mut self) -> Result<(), Error> {
        if self.handle.is_null() {
            return Ok(());
        }
        let status = unsafe { hcnet_thermal_uvc_close(self.handle) };
        self.handle = std::ptr::null_mut();
        if status != 0 {
            return Err(Error::new(status, 0));
        }
        Ok(())
    }
}

impl Drop for UvcCamera {
    fn drop(&mut self) {
        let _ = self.close();
    }
}
