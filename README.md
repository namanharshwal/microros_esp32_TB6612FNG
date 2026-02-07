micro-ROS Diff Drive Robot on ESP32 (Wi‑Fi, ROS 2 Humble)
This repository contains a micro‑ROS FreeRTOS application for an ESP32 DevKit v1 driving a 2‑wheeled differential drive robot (with castor wheel) using a TB6612FNG dual motor driver.
The ESP32 connects over Wi‑Fi (UDP) to a micro‑ROS agent and subscribes to /cmd_vel (geometry_msgs/Twist) to control the robot.

Features
micro‑ROS client on ESP32 (FreeRTOS app) subscribing to /cmd_vel.

Differential drive mixing from 
linear.x
linear.x and 
angular.z
angular.z to left/right wheel speeds.

PWM speed control and direction control via TB6612FNG driver.

Dead‑zone handling so very small commands stop the motors.

On‑board LED (GPIO 2) blinks as a heartbeat in the timer callback.

Main application file:
firmware/freertos_apps/apps/diff_drive_robot/app.c

Hardware Requirements
ESP32 DevKit v1 (ESP32‑WROOM based board).

TB6612FNG dual motor driver module.

Two DC gear motors (left and right) and one passive castor wheel.

Battery pack for motors (e.g. 2S/3S Li‑ion/LiPo or suitable DC source).

Common GND between ESP32 and motor driver.

Wiring: ESP32 ↔ TB6612FNG ↔ Motors
The pin mapping comes directly from app.c:

ESP32 GPIO assignments
Built‑in LED: GPIO 2

TB6612FNG PWM (speed) pins:

PIN_ENA (left speed): GPIO 19

PIN_ENB (right speed): GPIO 23

TB6612FNG direction pins:

PIN_IN1 (left dir 1): GPIO 18

PIN_IN2 (left dir 2): GPIO 5

PIN_IN3 (right dir 1): GPIO 21

PIN_IN4 (right dir 2): GPIO 22

TB6612FNG standby:

PIN_STBY: GPIO 4 (must be HIGH to enable motors)

TB6612FNG module side
Typical TB6612FNG pins:

Logic power (VCC): connect to ESP32 3.3 V.

Motor power (VM): connect to motor battery positive (e.g. 6–12 V, according to your motors and driver rating).

GND: connect to ESP32 GND and battery negative (common ground).

STBY: connect to ESP32 GPIO 4.

AIN1, AIN2, PWMA:

AIN1 → ESP32 GPIO 18

AIN2 → ESP32 GPIO 5

PWMA → ESP32 GPIO 19

BIN1, BIN2, PWMB:

BIN1 → ESP32 GPIO 21

BIN2 → ESP32 GPIO 22

PWMB → ESP32 GPIO 23

Motor outputs:

AO1/AO2 → left motor terminals.

BO1/BO2 → right motor terminals.

If forward/backward directions are inverted compared to your expectations, swap the two wires of that motor on AO1/AO2 or BO1/BO2.

Software Stack
Ubuntu 22.04

ROS 2 Humble

micro_ros_setup for firmware workflow and agent setup.
​

micro‑ROS ESP32 Wi‑Fi transport over UDP.

Optional keyboard teleop node publishing to /cmd_vel (e.g. teleop_twist_keyboard).
​

Repository Structure
Relevant part of the firmware workspace:

text
micro_ros_ws/
└── firmware/
    └── freertos_apps/
        └── apps/
            └── diff_drive_robot/
                ├── app.c
                └── app-colcon.meta
app.c implements:

GPIO and PWM setup.

micro‑ROS node tb6612_robot.

Subscription to /cmd_vel (geometry_msgs/msg/Twist).

Timer callback doing:

LED heartbeat.

Mixing of linear/angular velocity to left/right motor commands.

Direction and PWM updates.

micro‑ROS Workspace Setup (ROS 2 Humble, Ubuntu 22.04)
Install ROS 2 Humble and micro_ros_setup as usual (follow upstream docs if not already installed).
​

Create and build micro‑ROS workspace (if not done yet):

bash
mkdir -p ~/micro_ros_ws/src
cd ~/micro_ros_ws
# clone micro_ros_setup etc. as per official docs, then:
colcon build
Source the workspace:

bash
cd ~/micro_ros_ws
source install/local_setup.bash
Create the firmware workspace for ESP32 (only once):

bash
ros2 run micro_ros_setup create_firmware_ws.sh freertos esp32
Configure firmware to use your diff_drive app with UDP transport and your agent IP:

bash
# Replace <AGENT_IP> with your PC’s IP address on the Wi‑Fi network
ros2 run micro_ros_setup configure_firmware.sh diff_drive_robot -t udp -i <AGENT_IP> -p 8888
Example:

bash
ros2 run micro_ros_setup configure_firmware.sh diff_drive_robot -t udp -i 192.168.1.100 -p 8888
This sets transport to UDP and stores agent IP/port in the firmware configuration.
​

Wi‑Fi Credentials & ESP32 Network Configuration
You must configure the ESP32’s Wi‑Fi SSID/password and link it to the micro‑ROS agent IP/port.
​

Open the ESP32 menuconfig from the micro‑ROS firmware workspace:

bash
cd ~/micro_ros_ws
ros2 run micro_ros_setup build_firmware.sh menuconfig
In the menu:

Go to: micro-ROS Transport Settings → WiFi Configuration.

Set:

WiFi SSID: your router’s SSID.

WiFi password: your Wi‑Fi password.

Ensure transport type is UDP.

Confirm Agent IP and Agent Port (8888) match the configure_firmware.sh parameters.
​

Save and exit menuconfig.

Build firmware:

bash
ros2 run micro_ros_setup build_firmware.sh
After flashing (next section), the ESP32 will:

Connect to the configured Wi‑Fi AP using your SSID/password.

Open a UDP connection to the configured agent IP and port.

Start the micro‑ROS node and subscribe to /cmd_vel.

Building & Flashing the Firmware to ESP32
From your workspace:

bash
cd ~/micro_ros_ws
source install/local_setup.bash

ros2 run micro_ros_setup build_firmware.sh
ros2 run micro_ros_setup flash_firmware.sh
Connect the ESP32 DevKit v1 over USB before running the flash command.

The script detects the serial port and uploads the built firmware.
​

Starting the micro‑ROS Agent (UDP, Docker)
On your ROS 2 Humble PC (same machine whose IP you used in firmware configuration):

bash
cd ~/micro_ros_ws
source install/local_setup.bash

docker run -it --rm --net=host microros/micro-ros-agent:humble udp4 --port 8888 -v6
This starts a UDPv4 micro‑ROS agent on port 8888, listening for the ESP32 client.

In another terminal, you can check topics:

bash
source /opt/ros/humble/setup.bash
ros2 topic list
You should see /cmd_vel (and other micro‑ROS topics/types once the ESP32 connects).

Running the Robot with Teleop (/cmd_vel)
Any node publishing geometry_msgs/Twist on /cmd_vel will drive this robot.
A common choice is teleop_twist_keyboard.
​

Install and run teleop_twist_keyboard
Install:

bash
sudo apt install ros-humble-teleop-twist-keyboard
Run (in a terminal where ROS 2 is sourced):

bash
source /opt/ros/humble/setup.bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard
Ensure the teleop node is publishing to /cmd_vel (default). Use keys to move:

i, j, k, l, etc. for linear and angular commands.

The ESP32 node converts:

linear.x (−1 to 1) to forward/backward speed.

angular.z (−1 to 1) to left/right turning.

You can also use any other teleop node or your own controller that publishes geometry_msgs/Twist on /cmd_vel.

How the Control Logic Works (From app.c)
The node tb6612_robot subscribes to /cmd_vel and stores the latest message in a global geometry_msgs__msg__Twist msg.

A timer (every FRAME_TIME ms = 100 ms) calls timer_callback.

In timer_callback:

The built‑in LED (GPIO 2) toggles as a heartbeat.

linear = constrain(msg.linear.x, -1, 1) and angular = constrain(msg.angular.z, -1, 1)

Left and right target values:

left
=
linear
−
angular
,
right
=
linear
+
angular
left=linear−angular,right=linear+angular
Magnitudes are mapped to PWM (12‑bit, 0–4095) using:

PWM
=
fmap
(
∣
val
∣
,
0
,
1
,
PWM_MOTOR_MIN
,
PWM_MOTOR_MAX
)
PWM=fmap(∣val∣,0,1,PWM_MOTOR_MIN,PWM_MOTOR_MAX)
where PWM_MOTOR_MIN = 400 and PWM_MOTOR_MAX = 4095.

Very small commands (|left| or |right| < 0.05) are treated as zero (PWM set to 0).

Motor direction pins are set as:

Left forward: IN1=1, IN2=0; backward: IN1=0, IN2=1; stop: IN1=0, IN2=0.

Right forward: IN3=1, IN4=0; backward: IN3=0, IN4=1; stop: IN3=0, IN4=0.

PWM duty updated on:

Left: channel LEDC_CHANNEL_0 at GPIO 19.

Right: channel LEDC_CHANNEL_1 at GPIO 23.

This yields classical differential drive behavior for a 2‑wheel robot.

Quick Start Checklist
Wire ESP32 ↔ TB6612FNG ↔ motors according to the pin map above.

Install ROS 2 Humble and micro_ros_setup on Ubuntu 22.04.

Create firmware workspace and configure with diff_drive_robot app in UDP mode using your PC IP.

In menuconfig, set Wi‑Fi SSID/password and confirm agent IP and port 8888.

Build and flash the firmware to your ESP32.

Start the micro‑ROS agent Docker container on the PC.

Run a teleop node publishing /cmd_vel (e.g. teleop_twist_keyboard).

Place robot on ground, supply motor battery, and drive.

Notes & Tips
Always share GND between ESP32 and motor supply to avoid erratic behavior.

If motors only twitch or do not move, check:

STBY (GPIO 4) is HIGH.

VM has sufficient voltage.

Wi‑Fi is connected and agent is running.

If turning direction or forward/backward feels inverted, swap motor leads or swap left/right direction pins in hardware.
