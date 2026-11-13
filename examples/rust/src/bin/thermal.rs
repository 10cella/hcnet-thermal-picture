use hcnet_thermal_example::{ThermalCamera, QUALITY_BEST};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut camera = ThermalCamera::login(
        "192.168.1.64",
        8000,
        "admin",
        "abcd1234",
        1,
    )?;

    let jpeg = camera.capture(QUALITY_BEST)?;
    std::fs::write("thermal.jpg", &jpeg)?;

    camera.logout()?;
    println!("saved {} bytes to thermal.jpg", jpeg.len());
    Ok(())
}
