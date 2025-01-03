
#include "main.h"
#include "notecard.h"

// SWAN LED
#define LED_Pin                 GPIO_PIN_2
#define LED_GPIO_Port           GPIOE

void SystemClock_Config(void);
bool i2cTransmit(void *port, uint16_t deviceAddr, uint8_t *buf, uint16_t buflen);
bool i2cReceive(void *port, uint16_t deviceAddr, uint8_t *buf, uint16_t buflen);
void i2cDelayMs(uint32_t ms);

int main(void)
{

    // Init HAL
    HAL_Init();

    // Init clocks
    SystemClock_Config();

    // Init GPIO
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    // Init busy indicator GPIO
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pin = LED_Pin;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

    // Init I2C1
    I2C_HandleTypeDef hi2c1 = {0};
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00707CBB;     // note-stm32l4
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
        Error_Handler();
    }

    // Configure I2C to speak to the notecard using our implementation
    notecardContext_t notecard = {0};
    notecard.port = &hi2c1;
    notecard.tx = i2cTransmit;
    notecard.rx = i2cReceive;
    notecard.delay = i2cDelayMs;

    // Reset I2C so that it's clean from any prior reboot
    notecardReset(&notecard);

    // Test
    while (true) {
        uint32_t rsplen;
        uint8_t *rsp;
        uint8_t req[512];
        jsonbContext jsonb;
        notecardStatus_t status;

        // Format a JSONB request to be sent to the notecard
        jsonbObjectBegin(&jsonb, req, sizeof(req), NULL);
        jsonbAddStringToObject(&jsonb, "req", "card.version");
        if (!jsonbObjectEnd(&jsonb)) {
            Error_Handler();
        }

        // Send the JSONB request over I2C, and receive the response in the same buffer
        status = notecardRequestResponse(&notecard, req, sizeof(req));
        if (status) {
            for (int i=0; i<status; i++) {
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
                HAL_Delay(250);
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
                HAL_Delay(250);
            }
            HAL_Delay(2500);
            continue;
        }

        // Parse the response
        rsplen = notecardBuf(&notecard, &rsp, NULL);
        if (!jsonbParse(&jsonb, rsp, rsplen)) {
            for (int i=0; i<3; i++) {
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
                HAL_Delay(500);
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
                HAL_Delay(150);
            }
            HAL_Delay(2500);
            continue;
        }

        // Look for an error
        char *deviceUID = jsonbGetString(&jsonb, "device");
        if (deviceUID[0] == '\0') {
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
            HAL_Delay(1000);
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        }

        // Test again
        HAL_Delay(5000);

    }

}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
        Error_Handler();
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

// Error handling
void Error_Handler(void)
{
    bool on = false;
    while (true) {
        on = !on;
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_Delay(75);
    }
}

// Transmits in master mode an amount of data, in blocking mode. The address
// is the actual address; the caller should have shifted it right so that the
// low bit is NOT the read/write bit.  True is returned if success
bool i2cTransmit(void *port, uint16_t deviceAddr, uint8_t *buf, uint16_t buflen)
{
    return (HAL_I2C_Master_Transmit((I2C_HandleTypeDef *) port, deviceAddr<<1, buf, buflen, 250) == HAL_OK);
}

// Receives in master mode an amount of data in blocking mode, returning success
bool i2cReceive(void *port, uint16_t deviceAddr, uint8_t *buf, uint16_t buflen)
{
    return (HAL_I2C_Master_Receive((I2C_HandleTypeDef *) port, deviceAddr<<1, buf, buflen, 250) == HAL_OK);
}

// Delays the specified number of milliseconds
void i2cDelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}
