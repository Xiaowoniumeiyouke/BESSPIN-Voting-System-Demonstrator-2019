/**
 * Smart Ballot Box API
 * @refine sbb.lando
 */

// General includes
#include <stdio.h>
#include <string.h>

#include "votingdefs.h"

// Subsystem includes
#include "sbb.h"
#include "sbb_crypto.h"
#include "sbb_logging.h"

// Timeouts
#define BALLOT_DETECT_TIMEOUT_MS 10000
#define CAST_OR_SPOIL_TIMEOUT_MS 30000
#define SPOIL_EJECT_TIMEOUT_MS 6000
#define CAST_INGEST_TIMEOUT_MS 6000

osd_timer_ticks_t ballot_detect_timeout = 0;
osd_timer_ticks_t cast_or_spoil_timeout = 0;

char barcode[BARCODE_MAX_LENGTH] = {0};
barcode_length_t barcode_length = 0;

bool initialize(void)
{
    osd_gpio_set_as_input(BUTTON_CAST_IN);
    osd_gpio_set_as_input(BUTTON_SPOIL_IN);
    osd_gpio_set_as_input(PAPER_SENSOR_IN);
    osd_gpio_set_as_input(PAPER_SENSOR_OUT);
    osd_gpio_set_as_output(MOTOR_0);
    osd_gpio_set_as_output(MOTOR_1);
    osd_gpio_set_as_output(BUTTON_CAST_LED);
    osd_gpio_set_as_output(BUTTON_SPOIL_LED);
    the_state.button_illumination = 0;
    // Logging is not set up yet...we could do that here I suppose
    the_state.L  = INITIALIZE;
    the_state.M  = MOTORS_OFF;
    the_state.D  = INITIALIZED_DISPLAY;
    the_state.BS = BARCODE_NOT_PRESENT;
    the_state.FS = LOG_OK;
    the_state.P  = NO_PAPER_DETECTED;
    the_state.button_illumination = 0;
    __assume_strings_OK();
    barcode_length = 0;
#ifdef SBB_DO_LOGGING
    return (LOG_FS_OK == Log_IO_Initialize());
#else
    return true;
#endif
}

/* global invariant Button_lighting_conditions_power_on:
   \forall cast_button_light cbl, spoil_button_light sbl;
   \at(lights_off(cbl, sbl), DevicesInitialized);
*/

/* global invariant Paper_ejected_on_power_on:
   \forall paper_present p; \at(p == none, DevicesInitialized);
*/

/* global invariant Motor_initial_state:
   \forall motor m; \at(!motor_running(m), DevicesInitialized);
*/
barcode_validity is_barcode_valid(barcode_t the_barcode, barcode_length_t its_length) {
    return crypto_check_barcode_valid(the_barcode, its_length);
}

bool is_cast_button_pressed(void) { return the_state.B == CAST_BUTTON_DOWN; }

bool is_spoil_button_pressed(void) { return the_state.B == SPOIL_BUTTON_DOWN; }

void set_received_barcode(barcode_t the_barcode, barcode_length_t its_length)
{
    osd_assert(its_length <= BARCODE_MAX_LENGTH);
    memcpy(barcode, the_barcode, its_length);
    barcode_length = its_length;
}

bool has_a_barcode(void)
{
    return the_state.BS == BARCODE_PRESENT_AND_RECORDED;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
barcode_length_t what_is_the_barcode(barcode_t the_barcode)
{
    char *ptr = &barcode[0];
 Go:
    memcpy(the_barcode, ptr, barcode_length);
 After:
    //@ assert Eq1:   Barcode_Eq{Go,Here}(ptr, \at(ptr, Go), barcode_length);
    return barcode_length;
}
#pragma GCC diagnostic pop

void spoil_button_light_on(void)
{
    osd_gpio_write(BUTTON_SPOIL_LED);
    the_state.button_illumination |= spoil_button_mask;
}

void spoil_button_light_off(void)
{
    osd_gpio_clear(BUTTON_SPOIL_LED);
    the_state.button_illumination &= ~spoil_button_mask;
}

void cast_button_light_on(void)
{
    osd_gpio_write(BUTTON_CAST_LED);
    the_state.button_illumination |= cast_button_mask;
}

void cast_button_light_off(void)
{
    osd_gpio_clear(BUTTON_CAST_LED);
    the_state.button_illumination &= ~cast_button_mask;
}

void move_motor_forward(void)
{
    osd_gpio_clear(MOTOR_0);
    osd_gpio_write(MOTOR_1);
    CHANGE_STATE(the_state, M, MOTORS_TURNING_FORWARD);
}

void move_motor_back(void)
{
    osd_gpio_write(MOTOR_0);
    osd_gpio_clear(MOTOR_1);
    CHANGE_STATE(the_state, M, MOTORS_TURNING_BACKWARD);
}

void stop_motor(void)
{
    osd_gpio_clear(MOTOR_0);
    osd_gpio_clear(MOTOR_1);
    CHANGE_STATE(the_state, M, MOTORS_OFF);
}

void clear_display(void)
{
    osd_clear_lcd();
}

void display_this_text_no_log(const char *the_text, uint8_t its_length)
{
    osd_lcd_printf((char *)the_text, its_length);
}

void display_this_text(const char *the_text, uint8_t its_length)
{
    osd_lcd_printf((char *)the_text, its_length);
    CHANGE_STATE(the_state, D, SHOWING_TEXT);
}

void display_this_2_line_text(const char *line_1, uint8_t length_1,
                              const char *line_2, uint8_t length_2)
{
    CHANGE_STATE(the_state, D, SHOWING_TEXT);
    osd_lcd_printf_two_lines((char *)line_1, length_1, (char *)line_2, length_2);
}

bool ballot_detected(void) { return (the_state.P == PAPER_DETECTED); }

void eject_ballot(void)
{
    move_motor_back();
    // run the motor for a bit to get the paper back over the switch
    osd_msleep(SPOIL_EJECT_TIMEOUT_MS);
    stop_motor();
}

void spoil_ballot(void)
{
    spoil_button_light_off();
    cast_button_light_off();
    __assume_strings_OK();
    display_this_text(spoiling_ballot_text, strlen(spoiling_ballot_text));
    eject_ballot();
}

void cast_ballot(void)
{
    move_motor_forward();
    osd_msleep(CAST_INGEST_TIMEOUT_MS);
    stop_motor();
}

void go_to_standby(void)
{
    if (the_state.M != MOTORS_OFF)
    {
        stop_motor();
    }
    cast_button_light_off();
    spoil_button_light_off();
    display_this_2_line_text(welcome_text, strlen(welcome_text),
                             insert_ballot_text, strlen(insert_ballot_text));
    the_state.D  = SHOWING_TEXT;
    the_state.P  = NO_PAPER_DETECTED;
    the_state.BS = BARCODE_NOT_PRESENT;
    the_state.B  = ALL_BUTTONS_UP;
    the_state.L  = STANDBY;
    the_state.S  = INNER;
    //@assert FS_ASM_valid(the_state);
    /* CHANGE_STATE(the_state, D, SHOWING_TEXT); */
    /* CHANGE_STATE(the_state, P, NO_PAPER_DETECTED); */
    /* CHANGE_STATE(the_state, BS, BARCODE_NOT_PRESENT); */
    /* CHANGE_STATE(the_state, S, INNER); */
    /* CHANGE_STATE(the_state, B, ALL_BUTTONS_UP); */
    /* CHANGE_STATE(the_state, L, STANDBY); */
}

//@ assigns ballot_detect_timeout;
void ballot_detect_timeout_reset(void)
{
    ballot_detect_timeout =
        osd_get_ticks() + osd_msec_to_ticks(BALLOT_DETECT_TIMEOUT_MS);
}

bool ballot_detect_timeout_expired(void)
{
    return (osd_get_ticks() > ballot_detect_timeout);
}

//@ assigns cast_or_spoil_timeout;
void cast_or_spoil_timeout_reset(void)
{
    cast_or_spoil_timeout =
        osd_get_ticks() + osd_msec_to_ticks(CAST_OR_SPOIL_TIMEOUT_MS);
}

bool cast_or_spoil_timeout_expired(void)
{
    return (osd_get_ticks() > cast_or_spoil_timeout);
}

bool get_current_time(uint32_t *year, uint16_t *month, uint16_t *day,
                      uint16_t *hour, uint16_t *minute)
{
    static struct voting_system_time_t time;
#ifdef HARDCODE_CURRENT_TIME
    time.year = CURRENT_YEAR - 2000;
    time.month = CURRENT_MONTH;
    time.day = CURRENT_DAY;
    time.hour = CURRENT_HOUR;
    time.minute = CURRENT_MINUTE;
#else
    osd_assert(osd_read_time(&time) == 0);
#endif /* HARDCODE_CURRENT_TIME */

    *year = (uint32_t)time.year + 2000;
    *month = (uint16_t)time.month;
    *day = (uint16_t)time.day;
    *hour = (uint16_t)time.hour;
    *minute = (uint16_t)time.minute;

#ifdef VOTING_SYSTEM_DEBUG
    // A character array to hold the string representation of the time
    static char time_str[20];
    osd_format_time_str(&time, time_str);
    printf("Get current time: %s\r\n",time_str);
#endif
    return true;
}
