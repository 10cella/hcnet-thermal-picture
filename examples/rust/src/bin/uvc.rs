use hcnet_thermal_example::UvcCamera;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    if !UvcCamera::available() {
        return Err("the linked library was built without OpenCV UVC support".into());
    }

    let mut camera = UvcCamera::open("0", 640, 480, 3)?;
    let jpeg = camera.capture_jpeg(95)?;
    std::fs::write("uvc.jpg", &jpeg)?;

    camera.close()?;
    println!("saved {} bytes to uvc.jpg", jpeg.len());
    Ok(())
}
