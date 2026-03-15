#include "stm32f446xx.h"

// --- Global Debug Variables ---
volatile uint8_t flash_id = 0;
volatile int16_t accel_x = 0;

// ========================================================
// 1. I2C DRIVER (MPU6050 on PB8, PB9)
// ========================================================
void I2C1_Init(void) {
    RCC->AHB1ENR |= (1 << 1);
    RCC->APB1ENR |= (1 << 21);

    GPIOB->MODER &= ~((3 << 16) | (3 << 18));
    GPIOB->MODER |= ((2 << 16) | (2 << 18));
    GPIOB->OTYPER |= (1 << 8) | (1 << 9);
    GPIOB->PUPDR &= ~((3 << 16) | (3 << 18));
    GPIOB->PUPDR |= ((1 << 16) | (1 << 18));
    GPIOB->AFR[1] &= ~((0xF << 0) | (0xF << 4));
    GPIOB->AFR[1] |= ((4 << 0) | (4 << 4));

    I2C1->CR1 &= ~(1 << 0);
    I2C1->CR2 = 16;
    I2C1->CCR = 80;
    I2C1->TRISE = 17;
    I2C1->CR1 |= (1 << 0);
}

void MPU6050_Write_Reg(uint8_t reg_addr, uint8_t data) {
    I2C1->CR1 |= (1 << 8);
    while (!(I2C1->SR1 & (1 << 0)));

    I2C1->DR = 0xD0;
    while (!(I2C1->SR1 & (1 << 1)));
    (void)I2C1->SR1; (void)I2C1->SR2;

    while (!(I2C1->SR1 & (1 << 7)));
    I2C1->DR = reg_addr;

    while (!(I2C1->SR1 & (1 << 7)));
    I2C1->DR = data;

    while (!(I2C1->SR1 & (1 << 2)));
    I2C1->CR1 |= (1 << 9);
}

uint8_t MPU6050_Read_Reg(uint8_t reg_addr) {
    uint8_t data;
    I2C1->CR1 |= (1 << 8);
    while (!(I2C1->SR1 & (1 << 0)));

    I2C1->DR = 0xD0;
    while (!(I2C1->SR1 & (1 << 1)));
    (void)I2C1->SR1; (void)I2C1->SR2;

    while (!(I2C1->SR1 & (1 << 7)));
    I2C1->DR = reg_addr;

    while (!(I2C1->SR1 & (1 << 7)));

    I2C1->CR1 |= (1 << 8);
    while (!(I2C1->SR1 & (1 << 0)));

    I2C1->DR = 0xD1;
    while (!(I2C1->SR1 & (1 << 1)));

    I2C1->CR1 &= ~(1 << 10);
    (void)I2C1->SR1; (void)I2C1->SR2;

    I2C1->CR1 |= (1 << 9);

    while (!(I2C1->SR1 & (1 << 6)));
    data = I2C1->DR;
    return data;
}

// ========================================================
// 2. SPI DRIVER (W25Q128 on PA4, PA5, PA6, PA7)
// ========================================================
void SPI1_Init(void) {
    RCC->AHB1ENR |= (1 << 0);
    RCC->APB2ENR |= (1 << 12);

    GPIOA->MODER &= ~((3 << 10) | (3 << 12) | (3 << 14));
    GPIOA->MODER |= ((2 << 10) | (2 << 12) | (2 << 14));
    GPIOA->AFR[0] &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));
    GPIOA->AFR[0] |= ((5 << 20) | (5 << 24) | (5 << 28));

    GPIOA->MODER &= ~(3 << 8);
    GPIOA->MODER |= (1 << 8);
    GPIOA->ODR |= (1 << 4); // CS HIGH

    SPI1->CR1 = 0;
    SPI1->CR1 |= (1 << 2);
    SPI1->CR1 |= (3 << 3);
    SPI1->CR1 |= (1 << 9) | (1 << 8);
    SPI1->CR1 |= (1 << 6);
}

uint8_t SPI_TxRx(uint8_t data) {
    while (!(SPI1->SR & (1 << 1)));
    SPI1->DR = data;
    while (!(SPI1->SR & (1 << 0)));
    return SPI1->DR;
}

// ========================================================
// 2.5 FLASH MEMORY COMMANDS (Put these above main!)
// ========================================================
void W25Q_WriteEnable(void) {
    GPIOA->ODR &= ~(1 << 4);
    SPI_TxRx(0x06);
    GPIOA->ODR |= (1 << 4);
}

void W25Q_Wait(void) {
    GPIOA->ODR &= ~(1 << 4);
    SPI_TxRx(0x05);
    while ((SPI_TxRx(0x00) & 0x01) == 0x01);
    GPIOA->ODR |= (1 << 4);
}

void W25Q_SectorErase(uint32_t address) {
    W25Q_WriteEnable();
    GPIOA->ODR &= ~(1 << 4);
    SPI_TxRx(0x20);
    SPI_TxRx((address >> 16) & 0xFF);
    SPI_TxRx((address >> 8) & 0xFF);
    SPI_TxRx(address & 0xFF);
    GPIOA->ODR |= (1 << 4);
    W25Q_Wait();
}

void W25Q_WriteByte(uint32_t address, uint8_t data) {
    W25Q_WriteEnable();
    GPIOA->ODR &= ~(1 << 4);
    SPI_TxRx(0x02);
    SPI_TxRx((address >> 16) & 0xFF);
    SPI_TxRx((address >> 8) & 0xFF);
    SPI_TxRx(address & 0xFF);
    SPI_TxRx(data);
    GPIOA->ODR |= (1 << 4);
    W25Q_Wait();
}

uint8_t W25Q_ReadByte(uint32_t address) {
    uint8_t data;
    GPIOA->ODR &= ~(1 << 4);
    SPI_TxRx(0x03);
    SPI_TxRx((address >> 16) & 0xFF);
    SPI_TxRx((address >> 8) & 0xFF);
    SPI_TxRx(address & 0xFF);
    data = SPI_TxRx(0x00);
    GPIOA->ODR |= (1 << 4);
    return data;
}

// ========================================================
// 3. THE BLACK BOX LOGGER (Main Program)
// ========================================================
// Global debug variables

volatile int16_t current_accel_x = 0;
volatile int16_t saved_accel_x = 0;

int main(void) {
    I2C1_Init();
    SPI1_Init();

    // 1. Wake up MPU6050
    MPU6050_Write_Reg(0x6B, 0x00);

    // 2. Read live gravity data (X-Axis)
    uint8_t x_high = MPU6050_Read_Reg(0x3B);
    uint8_t x_low = MPU6050_Read_Reg(0x3C);
    current_accel_x = (int16_t)((x_high << 8) | x_low);

    // 3. Prepare the Flash Memory (Address 0x000000)
    uint32_t start_address = 0x000000;
    W25Q_SectorErase(start_address); // Always erase before writing!

    // 4. THE LOGGING EVENT: Save High Byte, then Low Byte
    W25Q_WriteByte(start_address, x_high);
    W25Q_WriteByte(start_address + 1, x_low);

    // 5. Read it back out of the physical flash memory!
    uint8_t read_high = W25Q_ReadByte(start_address);
    uint8_t read_low = W25Q_ReadByte(start_address + 1);

    // 6. Stitch it back together to prove it matches
    saved_accel_x = (int16_t)((read_high << 8) | read_low);

    while(1) {
        // Breakpoint goes here! We successfully logged the data and halted.
    }
}
