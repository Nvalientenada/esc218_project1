#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// ---------- Pin mapping ----------
#define GREEN_LED   GPIO_NUM_10
#define BLUE_LED    GPIO_NUM_11
#define BUZZER      GPIO_NUM_12

#define IGNITION_BTN GPIO_NUM_4

#define DRIVER_SEAT  GPIO_NUM_5
#define PASS_SEAT    GPIO_NUM_6
#define DRIVER_BELT  GPIO_NUM_7
#define PASS_BELT    GPIO_NUM_8

// ---------- Timing ----------
#define LOOP_DELAY_MS     20
#define DEBOUNCE_MS       30
#define BEEP_ON_MS        200
#define BEEP_OFF_MS       200

// ---------- Helpers ----------
static inline void delay_ms(int ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

// active-low input: pressed/closed = 0
static inline bool is_pressed(gpio_num_t pin) {
    return gpio_get_level(pin) == 0;
}

static void setup_input(gpio_num_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_pullup_en(pin);
    gpio_pulldown_dis(pin);
}

static void setup_output(gpio_num_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
}

// ---------- Main ----------
void app_main(void)
{
    // Outputs
    setup_output(GREEN_LED);
    setup_output(BLUE_LED);
    setup_output(BUZZER);

    // Inputs
    setup_input(IGNITION_BTN);
    setup_input(DRIVER_SEAT);
    setup_input(PASS_SEAT);
    setup_input(DRIVER_BELT);
    setup_input(PASS_BELT);

    bool welcome_printed = false;
    bool attempt_used = false;
    bool engine_started = false;
    bool ignition_inhibited = false;

    bool ign_prev = false;

    while (1) {

        // Read sensors
        bool driverSeat = is_pressed(DRIVER_SEAT);
        bool passSeat   = is_pressed(PASS_SEAT);
        bool driverBelt = is_pressed(DRIVER_BELT);
        bool passBelt   = is_pressed(PASS_BELT);

        // Welcome message (once)
        if (driverSeat && !welcome_printed) {
            printf("Welcome to enhanced alarm system model 218-W25\n");
            welcome_printed = true;
        }

        // Ready condition
        bool ready = driverSeat && passSeat && driverBelt && passBelt;

        // Green LED before ignition
        if (!attempt_used) {
            gpio_set_level(GREEN_LED, ready);
        }

        // Read ignition button + edge detect
        bool ign_now = is_pressed(IGNITION_BTN);
        bool ign_edge = (!ign_prev && ign_now);
        ign_prev = ign_now;

        // FINAL STATES (after ignition)
        if (attempt_used) {

            if (engine_started) {
                gpio_set_level(BLUE_LED, 1);
                gpio_set_level(GREEN_LED, 0);
                gpio_set_level(BUZZER, 0);
                delay_ms(LOOP_DELAY_MS);
                continue;
            }

            if (ignition_inhibited) {
                gpio_set_level(GREEN_LED, 0);
                gpio_set_level(BLUE_LED, 0);

                gpio_set_level(BUZZER, 1);
                delay_ms(BEEP_ON_MS);
                gpio_set_level(BUZZER, 0);
                delay_ms(BEEP_OFF_MS);
                continue;
            }
        }

        // IGNITION ATTEMPT (ONE TIME)
        if (!attempt_used && ign_edge) {

            delay_ms(DEBOUNCE_MS);
            if (!is_pressed(IGNITION_BTN)) {
                delay_ms(LOOP_DELAY_MS);
                continue;
            }

            attempt_used = true;

            if (ready) {
                engine_started = true;
                gpio_set_level(BLUE_LED, 1);
                gpio_set_level(GREEN_LED, 0);
                printf("Engine started.\n");
            } else {
                ignition_inhibited = true;
                printf("Ignition inhibited\n");

                if (!driverSeat) printf("Driver seat not occupied\n");
                if (!passSeat)   printf("Passenger seat not occupied\n");
                if (!driverBelt) printf("Driver seatbelt not fastened\n");
                if (!passBelt)   printf("Passenger seatbelt not fastened\n");
            }
        }

        delay_ms(LOOP_DELAY_MS);
    }
}
