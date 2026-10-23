# autonomous-car

## Goal

Build an autonomous car with a LiDAR, a camera, an IMU, and two drive motors with encoders, then use it for autonomous navigation. The project brings together several core robotics ideas, including:

- Motion planning for exploration
- Localization and mapping

## Current status

Hardware in use so far:

- Car chassis (got it for free)
- 2 drive motors with attached encoders
- L298N motor controller
- ESP32
- Steering servo (mounted on the chassis, not connected yet)

The car is currently powered by a wall-plug power supply.

## Next steps

- [ ] Buy an onboard power supply so the car doesn't need to be plugged into the wall
- [ ] Wire up the steering servo and make sure it works
- [ ] 3D print a plate for the top of the chassis so the Arduino, ESP32, motor controller, etc. sit snugly

## Progress

### Commit 1 — rear motors + encoders ([`3eb7616`](https://github.com/Ph1so/autonomous-car/commit/3eb7616))

Both rear wheels can spin forward and in reverse, and the wheel encoders track the spin.

![Both rear motors spinning forward and in reverse while encoder ticks print over serial](media/commit1-rear-motors.gif)
