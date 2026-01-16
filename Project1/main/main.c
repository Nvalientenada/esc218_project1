#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/*
 * Project 1 – Enhanced Car Alarm Upgrade
 *
 * This program simulates a car ignition safety system.
 * The system checks seat occupancy and seatbelt status
 * before allowing the engine to start.
 *
 * Key rules:
 * - A welcome message prints once when the driver sits down.
 * - GREEN LED indicates when all safety conditions are met.
 * - Ignition can be attempted only once.
 * - If ignition succeeds → BLUE LED stays ON forever.
 * - If ignition fails → buzzer beeps forever and reasons are printed.
 */

/* GPIO assignments for outputs */
#define GREEN_LED     GPIO_NUM_10   // Indicates system is ready
#define BLUE_LED      GPIO_NUM_11   // Indicates engine started
#define BUZZER        GPIO_NUM_12   // Audible warning when ignition fails

/* GPIO assignments for inputs (active-low) */
#define IGNITION_BTN  GPIO_NUM_4    // Ignition/start button
#define DRIVER_SEAT   GPIO_NUM_5    // Driver seat sensor
#define PASS_SEAT     GPIO_NUM_6    // Passenger seat sensor
#define DRIVER_BELT   GPIO_NUM_7    // Driver seatbelt sensor
#define PASS_BELT     GPIO_NUM_8    // Passenger seatbelt sensor

/* Timing constants (milliseconds) */
#define LOOP_DELAY_MS  20           // Main loop execution period
#define DEBOUNCE_MS    30           // Debounce time for ignition button
#define BEEP_ON_MS    200           // Buzzer ON time when inhibited
#define BEEP_OFF_MS   200           // Buzzer OFF time when inhibited

/* Converts milliseconds to RTOS ticks and delays the task */
static inline void delay_ms(int ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

/*
 * Reads an active-low input.
 * Returns true when the button or switch is pressed/closed.
 */
static inline bool is_pressed(gpio_num_t pin) {
    return gpio_get_level(pin) == 0;
}

/* Configure a GPIO pin as an active-low input with internal pull-up */
static void setup_input(gpio_num_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_pullup_en(pin);
    gpio_pulldown_dis(pin);
}

/* Configure a GPIO pin as an output and initialize it LOW */
static void setup_output(gpio_num_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
}

void app_main(void)
{
    /* Initialize all outputs */
    setup_output(GREEN_LED);
    setup_output(BLUE_LED);
    setup_output(BUZZER);

    /* Initialize all inputs */
    setup_input(IGNITION_BTN);
    setup_input(DRIVER_SEAT);
    setup_input(PASS_SEAT);
    setup_input(DRIVER_BELT);
    setup_input(PASS_BELT);

    /* State variables that define system behavior */
    bool welcome_printed    = false; // ensures welcome prints once
    bool attempt_used       = false; // locks system after ignition press
    bool engine_started     = false; // ignition success state
    bool ignition_inhibited = false; // ignition failure state

    /* Used for detecting a new ignition button press */
    bool ign_prev = false;

    while (1) {

        /* Read all safety-related inputs */
        bool driverSeat = is_pressed(DRIVER_SEAT);
        bool passSeat   = is_pressed(PASS_SEAT);
        bool driverBelt = is_pressed(DRIVER_BELT);
        bool passBelt   = is_pressed(PASS_BELT);

        /* Print welcome message the first time the driver sits down */
        if (driverSeat && !welcome_printed) {
            printf("Welcome to enhanced alarm system model 218-W25\n");
            welcome_printed = true;
        }

        /* System is ready only if ALL safety conditions are met */
        bool ready = driverSeat && passSeat && driverBelt && passBelt;

        /* Before ignition attempt, GREEN LED reflects readiness */
        if (!attempt_used) {
            gpio_set_level(GREEN_LED, ready ? 1 : 0);
        }

        /* Detect ignition button press edge (released → pressed) */
        bool ign_now  = is_pressed(IGNITION_BTN);
        bool ign_edge = (!ign_prev && ign_now);
        ign_prev = ign_now;

        /*
         * After an ignition attempt, the system remains
         * locked in its final state forever.
         */
        if (attempt_used) {

            /* Engine successfully started */
            if (engine_started) {
                gpio_set_level(BLUE_LED, 1);
                gpio_set_level(GREEN_LED, 0);
                gpio_set_level(BUZZER, 0);
                delay_ms(LOOP_DELAY_MS);
                continue;
            }

            /* Ignition inhibited due to failed safety checks */
            if (ignition_inhibited) {
                gpio_set_level(GREEN_LED, 0);
                gpio_set_level(BLUE_LED, 0);

                /* Audible warning pattern */
                gpio_set_level(BUZZER, 1);
                delay_ms(BEEP_ON_MS);
                gpio_set_level(BUZZER, 0);
                delay_ms(BEEP_OFF_MS);
                continue;
            }
        }

        /*
         * Handle the ignition attempt.
         * This block executes only once due to attempt_used.
         */
        if (!attempt_used && ign_edge) {

            /* Debounce ignition button */
            delay_ms(DEBOUNCE_MS);
            if (!is_pressed(IGNITION_BTN)) {
                delay_ms(LOOP_DELAY_MS);
                continue;
            }

            /* Lock the system after this attempt */
            attempt_used = true;

            /* Decide final outcome based on safety conditions */
            if (ready) {
                engine_started = true;
                gpio_set_level(BLUE_LED, 1);
                gpio_set_level(GREEN_LED, 0);
                gpio_set_level(BUZZER, 0);
                printf("Engine started.\n");
            } else {
                ignition_inhibited = true;
                gpio_set_level(GREEN_LED, 0);
                gpio_set_level(BLUE_LED, 0);
                printf("Ignition inhibited\n");

                /* Print all unmet safety conditions */
                if (!driverSeat) printf("Driver seat not occupied\n");
                if (!passSeat)   printf("Passenger seat not occupied\n");
                if (!driverBelt) printf("Driver seatbelt not fastened\n");
                if (!passBelt)   printf("Passenger seatbelt not fastened\n");
            }
        }

        /* Small delay to control loop speed */
        delay_ms(LOOP_DELAY_MS);
    }
}
