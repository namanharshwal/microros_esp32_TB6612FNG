#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <math.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <geometry_msgs/msg/twist.h>

#include <driver/gpio.h>
#include <driver/ledc.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// --- MACROS ---
#define constrain(amt, low, high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

// --- CONFIGURATION CONSTANTS ---
#define FRAME_TIME 100 
#define SLEEP_TIME 10
#define LED_BUILTIN 2 

// --- TB6612FNG PINOUT ---
// PWM (Speed) Pins
#define PIN_ENA 19  // Left Speed
#define PIN_ENB 23  // Right Speed

// Direction Pins
#define PIN_IN1 18  // Left Dir 1
#define PIN_IN2 5   // Left Dir 2
#define PIN_IN3 21  // Right Dir 1
#define PIN_IN4 22  // Right Dir 2

// Standby Pin (Must be HIGH for motor to run)
#define PIN_STBY 4 

// --- PWM SETTINGS ---
// We only need 2 PWM channels now (one for each enable pin)
#define PWM_CHANNEL_LEFT  LEDC_CHANNEL_0
#define PWM_CHANNEL_RIGHT LEDC_CHANNEL_1

#define PWM_FREQUENCY 5000 
#define PWM_RESOLUTION LEDC_TIMER_12_BIT
#define PWM_TIMER LEDC_TIMER_0
#define PWM_MODE LEDC_HIGH_SPEED_MODE // TB6612 handles high freq well

// Dead Zone Correction (12-bit: 0-4095)
#define PWM_MOTOR_MIN 400   
#define PWM_MOTOR_MAX 4095   

geometry_msgs__msg__Twist msg;

// Function forward declarations
void setupPins();
void setupRos();
void cmd_vel_callback(const void *msgin);
void timer_callback(rcl_timer_t *timer, int64_t last_call_time);
float fmap(float val, float in_min, float in_max, float out_min, float out_max);

// Main
void appMain(void *arg) {
    setupPins();
    setupRos();
}

void setupPins() {

    // 1. Setup Builtin LED
    gpio_reset_pin(LED_BUILTIN);
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);

    // 2. Setup Direction & Standby Pins as Digital Outputs
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    // Bitmask for IN1, IN2, IN3, IN4, STBY
    io_conf.pin_bit_mask = (1ULL << PIN_IN1) | (1ULL << PIN_IN2) | 
                           (1ULL << PIN_IN3) | (1ULL << PIN_IN4) | 
                           (1ULL << PIN_STBY);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // 3. Enable the Driver (STBY must be HIGH)
    gpio_set_level(PIN_STBY, 1);

    // 4. Configure PWM Timer
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQUENCY,
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    // 5. Configure PWM Channels (ENA, ENB)
    ledc_channel_config_t pwm_left_cfg = {
        .channel    = PWM_CHANNEL_LEFT,
        .duty       = 0,
        .gpio_num   = PIN_ENA,
        .speed_mode = PWM_MODE,
        .hpoint     = 0,
        .timer_sel  = PWM_TIMER
    };
    ledc_channel_config(&pwm_left_cfg);

    ledc_channel_config_t pwm_right_cfg = {
        .channel    = PWM_CHANNEL_RIGHT,
        .duty       = 0,
        .gpio_num   = PIN_ENB,
        .speed_mode = PWM_MODE,
        .hpoint     = 0,
        .timer_sel  = PWM_TIMER
    };
    ledc_channel_config(&pwm_right_cfg);
}

void setupRos() {
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

    rcl_node_t node;
    RCCHECK(rclc_node_init_default(&node, "tb6612_robot", "", &support));

    rcl_subscription_t subscriber;
    RCCHECK(rclc_subscription_init_default(
        &subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "/cmd_vel"));

    rcl_timer_t timer;
    RCCHECK(rclc_timer_init_default(
        &timer,
        &support,
        RCL_MS_TO_NS(FRAME_TIME),
        timer_callback));

    rclc_executor_t executor;
    RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
    RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &msg, &cmd_vel_callback, ON_NEW_DATA));
    RCCHECK(rclc_executor_add_timer(&executor, &timer));

    printf("TB6612 ROBOT READY\n");

    while (1) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(SLEEP_TIME));
        usleep(SLEEP_TIME * 1000);
    }

    RCCHECK(rcl_subscription_fini(&subscriber, &node));
    RCCHECK(rcl_node_fini(&node));
    vTaskDelete(NULL);
}

void cmd_vel_callback(const void *msgin) {
    // Data is stored in 'msg' automatically
}

void timer_callback(rcl_timer_t *timer, int64_t last_call_time) {
    if (timer == NULL) return;

    // Toggle LED to show life
    gpio_set_level(LED_BUILTIN, !gpio_get_level(LED_BUILTIN));

    // 1. Calculate Target Speeds
    float linear = constrain(msg.linear.x, -1, 1);
    float angular = constrain(msg.angular.z, -1, 1);

    float left = (linear - angular);
    float right = (linear + angular);

    // 2. Convert to PWM Duty Cycle
    uint16_t pwmLeft = (uint16_t) fmap(fabs(left), 0, 1, PWM_MOTOR_MIN, PWM_MOTOR_MAX);
    uint16_t pwmRight = (uint16_t) fmap(fabs(right), 0, 1, PWM_MOTOR_MIN, PWM_MOTOR_MAX);

    // Stop completely if command is near zero
    if(fabs(left) < 0.05) pwmLeft = 0;
    if(fabs(right) < 0.05) pwmRight = 0;

    // 3. SET LEFT MOTOR DIRECTION (IN1, IN2)
    if (left > 0.05) { // Forward
        gpio_set_level(PIN_IN1, 1);
        gpio_set_level(PIN_IN2, 0);
    } else if (left < -0.05) { // Backward
        gpio_set_level(PIN_IN1, 0);
        gpio_set_level(PIN_IN2, 1);
    } else { // Stop
        gpio_set_level(PIN_IN1, 0);
        gpio_set_level(PIN_IN2, 0);
    }

    // 4. SET RIGHT MOTOR DIRECTION (IN3, IN4)
    if (right > 0.05) { // Forward
        gpio_set_level(PIN_IN3, 1);
        gpio_set_level(PIN_IN4, 0);
    } else if (right < -0.05) { // Backward
        gpio_set_level(PIN_IN3, 0);
        gpio_set_level(PIN_IN4, 1);
    } else { // Stop
        gpio_set_level(PIN_IN3, 0);
        gpio_set_level(PIN_IN4, 0);
    }

    // 5. UPDATE SPEED (PWM on ENA, ENB)
    ledc_set_duty(PWM_MODE, PWM_CHANNEL_LEFT, pwmLeft);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL_LEFT);

    ledc_set_duty(PWM_MODE, PWM_CHANNEL_RIGHT, pwmRight);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL_RIGHT);

    // Debugging
    // printf("L:%.2f (%d) R:%.2f (%d)\n", left, pwmLeft, right, pwmRight);
}

// Helper functions
float fmap(float val, float in_min, float in_max, float out_min, float out_max) {
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
