// SPDX-License-Identifier: MIT
// STM32F103x8 registers: ST RM0008, GPIO section 9, TIM2 section 15, USART section 27.
// HSI 8 MHz is intentional: no assumptions about an unknown learning board crystal.
#include "board.h"

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define RCC_BASE 0x40021000u
#define RCC_CR REG(RCC_BASE + 0x00)
#define RCC_CFGR REG(RCC_BASE + 0x04)
#define RCC_APB2RSTR REG(RCC_BASE + 0x0c)
#define RCC_APB1RSTR REG(RCC_BASE + 0x10)
#define RCC_APB2ENR REG(RCC_BASE + 0x18)
#define RCC_APB1ENR REG(RCC_BASE + 0x1c)
#define GPIO_A 0x40010800u
#define GPIO_B 0x40010c00u
#define GPIO_C 0x40011000u
#define USART_BASE 0x40013800u
#define USART_SR REG(USART_BASE + 0x00)
#define USART_DR REG(USART_BASE + 0x04)
#define USART_BRR REG(USART_BASE + 0x08)
#define USART_CR1 REG(USART_BASE + 0x0c)
#define TIM2_BASE 0x40000000u
#define RX_CAPACITY 512u

static volatile uint32_t milliseconds;
static volatile uint16_t rx_head, rx_tail;
static volatile bool rx_lost;
static volatile char rx_ring[RX_CAPACITY];

static void gpio_config(uint32_t port, unsigned pin, uint32_t nibble)
{
    const uint32_t register_address = port + (pin < 8 ? 0 : 4);
    const unsigned shift = (pin % 8) * 4;
    REG(register_address) = (REG(register_address) & ~(15u << shift)) | (nibble << shift);
}
static void gpio_write(uint32_t port, unsigned pin, bool high)
{
    REG(port + 0x10) = 1u << (pin + (high ? 0 : 16));
}
static bool gpio_read(uint32_t port, unsigned pin) { return (REG(port + 0x08) & (1u << pin)) != 0; }
static uint16_t micros(void) { return (uint16_t)REG(TIM2_BASE + 0x24); }
static void delay_us(uint16_t us)
{
    const uint16_t start = micros();
    while ((uint16_t)(micros() - start) < us) { }
}

void SysTick_Handler(void) { ++milliseconds; }
uint32_t board_millis(void) { return milliseconds; }
void board_delay_ms(uint32_t ms)
{
    const uint32_t start = board_millis();
    while ((uint32_t)(board_millis() - start) < ms) { __asm volatile ("nop"); }
}

void USART1_IRQHandler(void)
{
    const uint32_t status = USART_SR;
    if (status & ((1u << 5) | 15u)) {
        const char byte = (char)USART_DR; // SR then DR clears ORE/NE/FE/PE.
        if (status & 15u) { rx_lost = true; return; }
        const uint16_t next = (uint16_t)((rx_head + 1u) & (RX_CAPACITY - 1u));
        if (next == rx_tail) rx_lost = true;
        else { rx_ring[rx_head] = byte; rx_head = next; }
    }
}

bool board_uart_read(char *byte)
{
    if (rx_tail == rx_head) return false;
    *byte = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & (RX_CAPACITY - 1u));
    return true;
}
bool board_uart_lost_bytes(void)
{
    __asm volatile ("cpsid i" ::: "memory");
    const bool lost = rx_lost;
    if (lost) { rx_tail = rx_head; rx_lost = false; }
    __asm volatile ("cpsie i" ::: "memory");
    return lost;
}
void board_uart_write(const char *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        const uint32_t start = board_millis();
        while (!(USART_SR & (1u << 7)))
            if ((uint32_t)(board_millis() - start) >= 100) return;
        USART_DR = (uint8_t)bytes[i];
    }
}

// Software I2C keeps the small driver inspectable and avoids the F1 I2C peripheral
// state machine. PB6/PB7 are GPIO open drain; external pull-ups to 3.3 V REQUIRED.
// Approx. <=100 kHz, bounded clock stretch, NACK/error propagation, bus recovery.
static void sda(bool high) { gpio_write(GPIO_B, 7, high); }
static void scl_low(void) { gpio_write(GPIO_B, 6, false); }
static bool scl_high(void)
{
    gpio_write(GPIO_B, 6, true);
    const uint16_t start = micros();
    while (!gpio_read(GPIO_B, 6))
        if ((uint16_t)(micros() - start) >= 2000) return false;
    return true;
}
static void i2c_stop(void)
{
    scl_low(); sda(false); delay_us(5);
    (void)scl_high(); delay_us(5); sda(true); delay_us(5);
}
static bool i2c_start(void)
{
    sda(true);
    if (!scl_high()) return false;
    delay_us(5);
    if (!gpio_read(GPIO_B, 7)) {
        // Nine release clocks recover an interrupted slave transfer, never drive high.
        for (unsigned i = 0; i < 9; ++i) {
            scl_low(); delay_us(5);
            if (!scl_high()) return false;
            delay_us(5);
        }
        i2c_stop();
        if (!gpio_read(GPIO_B, 7)) return false;
    }
    sda(false); delay_us(5); scl_low();
    return true;
}
static bool i2c_byte_write(uint8_t byte)
{
    for (unsigned i = 0; i < 8; ++i) {
        sda((byte & 0x80u) != 0); byte <<= 1; delay_us(5);
        if (!scl_high()) return false;
        delay_us(5); scl_low();
    }
    sda(true); delay_us(5);
    if (!scl_high()) return false;
    delay_us(5); const bool ack = !gpio_read(GPIO_B, 7);
    scl_low(); delay_us(5);
    return ack;
}
static bool i2c_byte_read(uint8_t *byte, bool ack)
{
    *byte = 0; sda(true);
    for (unsigned i = 0; i < 8; ++i) {
        delay_us(5); if (!scl_high()) return false;
        delay_us(5); *byte = (uint8_t)((*byte << 1) | (gpio_read(GPIO_B, 7) ? 1u : 0u));
        scl_low();
    }
    sda(!ack); delay_us(5);
    if (!scl_high()) return false;
    delay_us(5); scl_low(); sda(true);
    return true;
}
bool board_i2c_write(uint8_t address, const uint8_t *bytes, size_t length)
{
    bool ok = i2c_start();
    if (ok) ok = i2c_byte_write((uint8_t)(address << 1));
    for (size_t i = 0; ok && i < length; ++i) ok = i2c_byte_write(bytes[i]);
    i2c_stop(); return ok;
}
bool board_i2c_read(uint8_t address, uint8_t *bytes, size_t length)
{
    bool ok = i2c_start();
    if (ok) ok = i2c_byte_write((uint8_t)((address << 1) | 1u));
    for (size_t i = 0; ok && i < length; ++i) ok = i2c_byte_read(bytes + i, i + 1 < length);
    i2c_stop(); return ok;
}

void board_outputs(bool light_on, bool fan_on)
{
#if STAGE == 2 || STAGE == 3
    gpio_write(LED_GPIO_BASE, LED_PIN, LED_ACTIVE_LOW ? !light_on : light_on);
    (void)fan_on;
#elif STAGE == 4
    gpio_write(GPIO_B, 0, light_on); gpio_write(GPIO_B, 1, fan_on);
#else
    (void)light_on; (void)fan_on;
#endif
}

void board_init(void)
{
    RCC_CR |= 1u; while (!(RCC_CR & 2u)) { }
    RCC_CFGR = 0; while (RCC_CFGR & (3u << 2)) { }
    RCC_CR &= ~((1u << 24) | (1u << 16) | (1u << 18) | (1u << 19));
    REG(0xe000ed08u) = 0x08000000u; // VTOR: boot from the provided Flash vector table.
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 14);
    RCC_APB1ENR |= 1u;
    RCC_APB2RSTR |= (1u << 2) | (1u << 3) | (1u << 4) | (1u << 14);
    RCC_APB2RSTR &= ~((1u << 2) | (1u << 3) | (1u << 4) | (1u << 14));
    RCC_APB1RSTR |= 1u; RCC_APB1RSTR &= ~1u;
    REG(TIM2_BASE + 0x28) = 7; // PSC: 8 MHz / 8 => 1 MHz, 16-bit counter.
    REG(TIM2_BASE + 0x2c) = 65535;
    REG(TIM2_BASE + 0x14) = 1; REG(TIM2_BASE + 0x00) = 1;
    gpio_config(GPIO_A, 9, 0xbu); // USART1 TX alternate push-pull, 50 MHz.
    gpio_config(GPIO_A, 10, 0x4u); // USART1 RX floating input, driven by bridge TXD.
    USART_BRR = 69u; // 8 MHz / 69 = 115942 baud (+0.64%, excluding HSI tolerance).
    USART_CR1 = (1u << 13) | (1u << 5) | (1u << 3) | (1u << 2);
    REG(0xe000e104u) = 1u << 5; // NVIC ISER1, USART1 IRQ 37.
#if STAGE >= 3
    sda(true); gpio_write(GPIO_B, 6, true);
    gpio_config(GPIO_B, 6, 0x6u); gpio_config(GPIO_B, 7, 0x6u); // 2 MHz OD.
#endif
#if STAGE == 2 || STAGE == 3
    gpio_write(LED_GPIO_BASE, LED_PIN, LED_ACTIVE_LOW != 0); // OFF before output mode.
    gpio_config(LED_GPIO_BASE, LED_PIN, 0x2u); // 2 MHz push-pull, low-current LED only.
#elif STAGE == 4
    gpio_write(GPIO_B, 0, false); gpio_write(GPIO_B, 1, false);
    gpio_config(GPIO_B, 0, 0x2u); gpio_config(GPIO_B, 1, 0x2u);
#endif
    REG(0xe000e014u) = 7999u; REG(0xe000e018u) = 0; REG(0xe000e010u) = 7u;
    __asm volatile ("cpsie i" ::: "memory");
}
