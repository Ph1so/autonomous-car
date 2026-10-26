# Parts list

Checked items are things I have or already ordered. Unchecked items still need to be bought or made.

## Drive and steering

- [x] Aluminum Ackermann car chassis (got it for free :)) )
- [x] 2 rear drive motors with attached encoders (JGB-520, 12V)
- [x] Steering servo (20 kg)
- [x] L298N motor controller
- [x] ESP32 (low-level control for motors, encoders, and steering)

## Compute and sensors

- [x] Raspberry Pi 5 (4 GB RAM)
- [x] RPLIDAR C1 kit (comes with its USB adapter board)
- [ ] IMU (BNO085 breakout), can wait until mapping starts
- [ ] Camera
- [ ] microSD card, 32 GB or more, for Ubuntu and ROS 2 on the Pi

## Power

- [x] 3S 11.1V 2200 mAh LiPo with XT60 plug, Ovonic 2 pack
- [x] 3S 1800 mAh LiPo with T-plug (spare)
- [x] YT-0003S 2S/3S LiPo balance charger
- [x] LiPo safe bag (Tenergy)
- [x] LiPo voltage alarm (1-8S, set to 3.5V per cell)
- [x] XT60 male and female pigtails, 12AWG
- [x] Hobbywing 5A UBEC set to 6V, powers the steering servo
- [x] UCTRONICS 12V to 5V 5A buck converter (2 pack), powers the Pi
- [x] 12V wall adapter with barrel jack, for bench testing
- [ ] Inline kill switch, rated 10A or more
- [ ] Lever nuts or small screw terminal block, to split battery power to the L298N, UBEC, and buck converter
- [ ] USB-A to micro-USB cable, from the buck converter to the Pi
- [ ] T-plug male and female pigtails, so the spare 1800 mAh battery can plug in

## Mounting and wiring

- [x] Jumper wires
- [x] Multimeter
- [ ] Top plate, 3D printed, with holes for the Pi, ESP32, motor controller, buck converter, UBEC, and a battery strap
- [ ] LiDAR mount, the highest point on the car with a clear 360° view
- [ ] M3 screws, nuts, and standoffs
- [ ] Breadboard or small perfboard for shared 3.3V and ground rails
- [ ] Soldering iron access, for the pigtails, kill switch, and power splits

## Later, only if needed

- [ ] Travel router or phone hotspot, if ROS 2 over campus WiFi gives trouble
