#include <application.h>

// LED instance
bc_led_t led;

uint8_t frame[7];
volatile uint8_t sync;
volatile bool is_busy;
volatile state_type state;
volatile uint8_t state_i;
volatile uint8_t state_j;

void build_frame(uint32_t shutter_id, uint32_t code, uint8_t button)
{
    frame[0] = 0xA7;                    // Encryption key. Doesn't matter much
    frame[1] = button << 4;             // Which button did  you press? The 4 LSB will be the checksum
    frame[2] = code >> 8;               // Rolling code (big endian)
    frame[3] = code & 0xFF;             // Rolling code
    frame[4] = shutter_id >> 16;        // Remote address
    frame[5] = (shutter_id >> 8) & 0xFF;// Remote address
    frame[6] = shutter_id & 0xFF;       // Remote address

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

    // Obfuscation: a XOR of all the bytes
    for (uint8_t i = 1; i < 7; i++)
    {
        frame[i] ^= frame[i - 1];
    }
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

void somfy_cmd(uint64_t *id, const char *topic, void *value, void *param)
{
    bc_led_pulse(&led, 100);

    uint32_t shutter_id = (uint32_t) param;
    uint32_t code = *((uint32_t *) value);
    uint8_t cmd = 0;
    size_t topic_len = strlen(topic);

    if (topic[topic_len - 2] == 'u' && topic[topic_len - 1] == 'p')
    {
        cmd = CMD_UP;
    }
    else if (topic[topic_len - 1] == 'p')
    {
        cmd = CMD_STOP;
    }
    else if (topic[topic_len - 1] == 'n')
    {
        cmd = CMD_DOWN;
    }
    else if (topic[topic_len - 1] == 'g')
    {
        cmd = CMD_PROG;
    }

    if (shutter_id && code && cmd)
    {
        build_frame(shutter_id, code, cmd);
        send_command();
    }
}

void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_pulse(&led, 2000);

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_LISTENING);
    bc_radio_set_subs((bc_radio_sub_t *) subs, sizeof(subs)/sizeof(bc_radio_sub_t));

    bc_gpio_init(PIN_DATA);
    bc_gpio_set_mode(PIN_DATA, BC_GPIO_MODE_OUTPUT);

    bc_gpio_init(PIN_VCC);
    bc_gpio_set_mode(PIN_VCC, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(PIN_VCC, 1);

    bc_gpio_init(PIN_GND);
    bc_gpio_set_mode(PIN_GND, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(PIN_GND, 0);

    bc_radio_pairing_request("bcf-somfy-controller", VERSION);

    bc_led_pulse(&led, 2000);
}
