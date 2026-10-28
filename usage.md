# Usage

Run these from the repo root.

## ESP32 firmware

Compile and upload:

```sh
arduino-cli compile --upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 firmware/racecar
```

Serial monitor:

```sh
arduino-cli monitor -p /dev/cu.usbserial-0001 -c baudrate=115200
```

## Lidar viewer

Build the Slamtec SDK once, and again after updating the submodule:

```sh
make -C third_party/rplidar_sdk
```

The viewer needs matplotlib (`pip3 install matplotlib`).

Plug in the RPLIDAR C1. It shows up as `/dev/cu.usbserial-110` and needs baud 460800:

```sh
./third_party/rplidar_sdk/output/Darwin/Release/ultra_simple --channel --serial /dev/cu.usbserial-110 460800 | python3 tools/lidar_view.py
```

The plot shows ±3 m around the lidar. Pass `--range` to the viewer to change it, e.g. `python3 tools/lidar_view.py --range 6`.

Stop with Ctrl-C in the terminal, which lets ultra_simple stop the lidar motor before it exits. Closing the window also ends the viewer, but ultra_simple then gets killed by the closed pipe and skips stopping the motor.
