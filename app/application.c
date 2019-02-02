#include <application.h>

#define REMOTE 0x206206
#define PIN_DATA BC_GPIO_P17
#define PIN_VCC BC_GPIO_P16
#define PIN_GND BC_GPIO_P15
#define SYMBOL 640
#define CMD_UP 0x2
#define CMD_STOP 0x1
#define CMD_DOWN 0x4
#define CMD_PROG 0x8

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

uint32_t rollingCode = 1;

uint8_t frame[7];
volatile uint8_t sync;
volatile bool is_busy;
volatile state_type state;
volatile uint8_t state_i;
volatile uint8_t state_j;

void build_frame(uint8_t button)
{
    frame[0] = 0xA7;              // Encryption key. Doesn't matter much
    frame[1] = button << 4;       // Which button did  you press? The 4 LSB will be the checksum
    frame[2] = rollingCode >> 8;  // Rolling code (big endian)
    frame[3] = rollingCode;       // Rolling code
    frame[4] = REMOTE >> 16;      // Remote address
    frame[5] = REMOTE >> 8;       // Remote address
    frame[6] = REMOTE;            // Remote address

    bc_log_dump(frame, 7, "Frame");

    // Checksum calculation: a XOR of all the nibbles
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < 7; i++)
    {
        checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
    }
    checksum &= 0b1111; // We keep the last 4 bits only


    // Checksum integration
    frame[1] |= checksum; // If a XOR of all the nibbles is equal to 0, the blinds will
                          // consider the checksum ok.

    bc_log_dump(frame, 7, "With checksum");

    // Obfuscation: a XOR of all the bytes
    for (uint8_t i = 1; i < 7; i++)
    {
        frame[i] ^= frame[i - 1];
    }

    bc_log_dump(frame, 7, "Obfuscated");

    bc_log_info("Rolling Code: %d", rollingCode);
    rollingCode++;
    bc_log_info("Next: %d", rollingCode);
}

bool send_command()
{
    if (TIM3->CR1 && TIM_CR1_CEN)
    {
        // TIM3 is busy
        return false;
    }

    bc_system_pll_enable();

    // Enable TIM3 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    // Errata workaround
    RCC->APB1ENR;
    // Disable counter if it is running
    TIM3->CR1 &= ~TIM_CR1_CEN;
    // Set prescaler to 1 microsecond resolution
    TIM3->PSC = 32 - 1;

    TIM3->ARR = 10 - 1;

    TIM3->DIER |= TIM_DIER_UIE;
    // Enable TIM3 interrupts
    NVIC_EnableIRQ(TIM3_IRQn);

    is_busy = true;
    state = STATE_BEGIN;
    sync = 2;

    TIM3->CR1 |= TIM_CR1_CEN;

    return true;
}

void TIM3_IRQHandler(void)
{
    TIM3->SR = ~TIM_DIER_UIE;

    if (state == STATE_BEGIN)
    {
        bc_gpio_set_output(PIN_DATA, 1);
        TIM3->ARR = 9415 - 1;
        state = STATE_FIRSTFRAME1;
    }
    else if (state == STATE_FIRSTFRAME1)
    {
        bc_gpio_set_output(PIN_DATA, 0);
        TIM3->ARR = 49565 - 1;
        state = STATE_FIRSTFRAME2;
    }
    else if (state == STATE_FIRSTFRAME2)
    {
        TIM3->ARR = 40000 - 1;
        state = STATE_HWSYNC;
        state_i = 0;
    }
    else if (state == STATE_HWSYNC)
    {
        if ((state_i % 2) == 0)
        {
            bc_gpio_set_output(PIN_DATA, 1);
        }
        else
        {
            bc_gpio_set_output(PIN_DATA, 0);
        }
        state_i++;
        if (state_i == sync * 2)
        {
            state = STATE_SWSYNC1;
        }
        TIM3->ARR = (4 * SYMBOL) - 1;
    }
    else if (state == STATE_SWSYNC1)
    {
        bc_gpio_set_output(PIN_DATA, 1);
        TIM3->ARR = 4550 - 1;
        state = STATE_SWSYNC2;
    }
    else if (state == STATE_SWSYNC2)
    {
        bc_gpio_set_output(PIN_DATA, 0);
        TIM3->ARR = SYMBOL - 1;
        state = STATE_DATA;
        state_i = 0;
        state_j = 0;
    }
    else if (state == STATE_DATA)
    {
        if (((frame[state_i / 8] >> (7 - (state_i % 8))) & 1) == 1)
        {
            if (state_j == 0)
            {
                bc_gpio_set_output(PIN_DATA, 0);
                state_j = 1;
            }
            else
            {
                bc_gpio_set_output(PIN_DATA, 1);
                state_j = 0;
            }
        }
        else
        {
            if (state_j == 0)
            {
                bc_gpio_set_output(PIN_DATA, 1);
                state_j = 1;
            }
            else
            {
                bc_gpio_set_output(PIN_DATA, 0);
                state_j = 0;
            }
        }

        if (state_j == 0)
        {
            state_i++;
        }
        if (state_i == 56)
        {
            state = STATE_LAST;
        }
    }
    else if (state == STATE_LAST)
    {
        bc_gpio_set_output(PIN_DATA, 0);
        TIM3->ARR = 30415 - 1;
        if (sync == 2)
        {
            sync = 7;
            state = STATE_HWSYNC;
            state_i = 0;
        }
        else
        {
            state = STATE_END;
        }
    }
    else if (state == STATE_END)
    {
        // end of transmission
        TIM3->CR1 &= ~TIM_CR1_CEN;

        is_busy = false;

        bc_system_pll_disable();
    }
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    bool send = false;

    if (event == BC_BUTTON_EVENT_CLICK)
    {
        bc_led_pulse(&led, 200);

        build_frame(CMD_DOWN);
        send = true;
    }

    if (event == BC_BUTTON_EVENT_HOLD)
    {
        bc_led_pulse(&led, 200);

        build_frame(CMD_PROG);
        send = true;
    }

    if (send)
    {
        send_command();
    }
}

void application_init(void)
{
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_pulse(&led, 2000);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);
    bc_button_set_hold_time(&button, 600);

    bc_gpio_init(PIN_DATA);
    bc_gpio_set_mode(PIN_DATA, BC_GPIO_MODE_OUTPUT);

    bc_gpio_init(PIN_VCC);
    bc_gpio_set_mode(PIN_VCC, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(PIN_VCC, 1);

    bc_gpio_init(PIN_GND);
    bc_gpio_set_mode(PIN_GND, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(PIN_GND, 0);
}
