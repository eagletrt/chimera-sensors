/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2019 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal.h"

/* USER CODE BEGIN Includes */

#define LONG_DELTA 3000
#define SHORT_DELTA 850
#define TIMER_PERIOD 5000

#define MSG_LENGHT 8
#define NUM_DATA 100

#define ID_STEERING_WHEEL 160
#define ID_PEDALS 176
#define ID_FRONT 192
#define ID_CENTER 208
#define ID_BMS_HV 170
#define ID_BMS_LV 255
#define ID_ASK_INV_SX 513
#define ID_ASK_INV_DX 514
#define ID_REQ_INV_SX 385
#define ID_REQ_INV_DX 386
#define ID_ECU 85
#define ID_ASK_STATE 16
#define GYRO 0x4EC
#define ACCEL 0x4ED

#define MAX_POWER 8.0
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
TIM_HandleTypeDef htim10;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

//----------------typedef---------------
typedef uint8_t errorsInt;

// List of SCS
typedef enum
{
	APPS,
	BSE,
	LV,
	MOT_TEMP_SX,
	MOT_TEMP_DX,
	INV_TEMP_SX,
	INV_TEMP_DX,
	INV_CUR_SX,
	INV_CUR_DX,
	NUM_SCS
} scsList;

typedef struct fifoDataType
{
	int idsave;
	uint8_t RxData[MSG_LENGHT];
} fifoDataType;

typedef struct state_global_data_t
{
	bool tractiveSystem;	// Tractive system on=1/off=0
	bool go;				// run=1/setup=0
	bool breakingPedal;		// breaking pedal down=1/up=0
	bool inverterSx;		// inverter Sx enable=1/disable=0
	bool inverterDx;		// inverter Dx enable=1/disable=0
	bool requestOfShutdown; // shutdown request done yes=1/no=0
	bool writeInCan;		// write in can=1/wait next timeout=0

	bool steeringPresence;
	bool pedalsPresence;
	bool frontalPresence;
	bool centralPresence;
	bool bmsLvPresence;
	bool bmsHvPresence;
	bool invDxPresence;
	bool invSxPresence;

	errorsInt errors;
	errorsInt warningsB1;

	uint8_t scs[NUM_SCS];

	fifoDataType fifoData[NUM_DATA];

	uint16_t dataCounterUp;
	uint16_t dataCounterDown;

	uint8_t accelerator; // apps value
	uint16_t motLeftTemp;
	uint16_t motRightTemp;
	uint16_t invLeftTemp;
	uint16_t invRightTemp;
	uint16_t invLeftVol;
	uint16_t invRightVol;
	uint16_t invLeftCur;
	uint16_t invRightCur;
	uint32_t hvVol;
	int16_t hvCur;
	int curRequested;
	int8_t powerRequested; // % of power requested

	uint32_t steeringTimeStamp;
	uint32_t pedalsTimeStamp;
	uint32_t frontalTimeStamp;
	uint32_t centralTimeStamp;
	uint32_t bmsLvTimeStamp;
	uint32_t bmsHvTimeStamp;
	uint32_t invDxTimeStamp;
	uint32_t invSxTimeStamp;
	uint32_t actualTime;
} state_global_data_t;

// List of states
typedef enum
{
	STATE_INIT,
	STATE_IDLE,
	STATE_SETUP,
	STATE_RUN,
	NUM_STATES
} state_t;

// State function and state transition prototypes
typedef state_t state_func_t(state_global_data_t *data);
typedef void transition_func_t(state_global_data_t *data);

/* ---------------- CAN ------------------ */
CAN_FilterTypeDef sFilter;
CAN_RxHeaderTypeDef RxHeader;
HAL_CAN_StateTypeDef state;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);
static void MX_CAN1_Init(void);
static void MX_TIM10_Init(void);
static void MX_NVIC_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

//------------------CAN------------------
int CAN_Send(int id, uint8_t *dataTx, int size);
int CAN_Receive(uint8_t *DataRx);

//-----State functions declarations-----
state_t run_state(state_t cur_state, state_global_data_t *data);
state_func_t *const state_table[NUM_STATES];
transition_func_t *const transition_table[NUM_STATES][NUM_STATES];
state_t do_state_init(state_global_data_t *data);
state_t do_state_idle(state_global_data_t *data);
state_t do_state_setup(state_global_data_t *data);
state_t do_state_run(state_global_data_t *data);

state_t STATO = STATE_INIT;

//--Transition functions declarations--
void to_idle(state_global_data_t *data);
void from_idle_to_setup(state_global_data_t *data);
void from_run_to_setup(state_global_data_t *data);
void to_run(state_global_data_t *data);

//---------operative function---------
void initData(state_global_data_t *data);
void canSendMSGInit(uint8_t *CanSendMSG);
void checkTimeStamp(state_global_data_t *data);
void sendStatus(state_global_data_t *data);
void sendErrors(state_global_data_t *data);
void shutdown(state_global_data_t *data);
void shutdownErrors(state_global_data_t *data, int err);
void checkValues(state_global_data_t *data);
void transmission(state_global_data_t *data);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ---global variables--- */

uint8_t canSendMSG[MSG_LENGHT];
state_global_data_t data;
char mander[50];
char txt[100];

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration----------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	MX_TIM6_Init();
	MX_TIM7_Init();
	MX_CAN1_Init();
	MX_TIM10_Init();

	/* Initialize interrupts */
	MX_NVIC_Init();
	/* USER CODE BEGIN 2 */
	sFilter.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilter.FilterIdLow = 0;
	sFilter.FilterIdHigh = 0;
	sFilter.FilterMaskIdHigh = 0;
	sFilter.FilterMaskIdLow = 0;
	sFilter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	sFilter.FilterBank = 0;
	sFilter.FilterScale = CAN_FILTERSCALE_16BIT;
	sFilter.FilterActivation = ENABLE;
	HAL_CAN_ConfigFilter(&hcan1, &sFilter);

	//HAL_CAN_Init(&hcan1);
	HAL_CAN_Start(&hcan1);

	HAL_TIM_Base_Start(&htim6);
	HAL_TIM_Base_Start(&htim7);
	HAL_TIM_Base_Start_IT(&htim6);
	HAL_TIM_Base_Start_IT(&htim7);
	HAL_TIM_Base_Start(&htim10);

	__HAL_TIM_SetCounter(&htim6, 0);
	__HAL_TIM_SetCounter(&htim7, 0);
	__HAL_TIM_SetCounter(&htim10, 0);

	HAL_CAN_ActivateNotification(&hcan1, CAN1_RX0_IRQn);
	HAL_CAN_ActivateNotification(&hcan1, CAN1_RX1_IRQn);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	uint32_t tick = HAL_GetTick();
	uint32_t previous_millis = tick;
	while (1)
	{

		STATO = run_state(STATO, &data);

		if (previous_millis != tick)
		{
			if (tick % SHORT_DELTA == 0)
			{
				//checkTimeStamp(&data);
				data.errors = 0;
				data.warningsB1 = 0;
			}

			if (tick % 1000 == 0)
			{
				sendStatus(&data);
			}
			previous_millis = tick;
		}

		if (tick % 10 > 0 && data.writeInCan == false)
		{
			data.writeInCan = true;
		}

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;

	/**Configure the main internal regulator output voltage 
    */
	__HAL_RCC_PWR_CLK_ENABLE();

	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/**Initializes the CPU, AHB and APB busses clocks 
    */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 288;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 8;
	RCC_OscInitStruct.PLL.PLLR = 2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	/**Initializes the CPU, AHB and APB busses clocks 
    */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	/**Configure the Systick interrupt time 
    */
	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

	/**Configure the Systick 
    */
	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

	/* SysTick_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
	/* CAN1_RX0_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
	/* CAN1_RX1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
	/* CAN1_SCE_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
	/* TIM6_DAC_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
	/* TIM7_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(TIM7_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

/* CAN1 init function */
static void MX_CAN1_Init(void)
{

	hcan1.Instance = CAN1;
	hcan1.Init.Prescaler = 2;
	hcan1.Init.Mode = CAN_MODE_NORMAL;
	hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
	hcan1.Init.TimeSeg1 = CAN_BS1_12TQ;
	hcan1.Init.TimeSeg2 = CAN_BS2_5TQ;
	hcan1.Init.TimeTriggeredMode = DISABLE;
	hcan1.Init.AutoBusOff = DISABLE;
	hcan1.Init.AutoWakeUp = ENABLE;
	hcan1.Init.AutoRetransmission = ENABLE;
	hcan1.Init.ReceiveFifoLocked = DISABLE;
	hcan1.Init.TransmitFifoPriority = DISABLE;
	if (HAL_CAN_Init(&hcan1) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}
}

/* TIM6 init function */
static void MX_TIM6_Init(void)
{

	TIM_MasterConfigTypeDef sMasterConfig;

	htim6.Instance = TIM6;
	htim6.Init.Prescaler = 719;
	htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim6.Init.Period = 1999;
	if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}
}

/* TIM7 init function */
static void MX_TIM7_Init(void)
{

	TIM_MasterConfigTypeDef sMasterConfig;

	htim7.Instance = TIM7;
	htim7.Init.Prescaler = 7199;
	htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim7.Init.Period = 19999;
	if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}
}

/* TIM10 init function */
static void MX_TIM10_Init(void)
{

	htim10.Instance = TIM10;
	htim10.Init.Prescaler = 7199;
	htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim10.Init.Period = 49999;
	htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}
}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}
}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

	GPIO_InitTypeDef GPIO_InitStruct;

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_5 | GPIO_PIN_8, GPIO_PIN_RESET);

	/*Configure GPIO pin : PA5 */
	GPIO_InitStruct.Pin = GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PB0 PB5 PB8 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_5 | GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

//---------------CAN--------------
int CAN_Send(int id, uint8_t dataTx[], int size)
{
	/* This function is used to send msgs via can.
	 * In the condition all the mailbox are considered to be sure that
	 * you can physically send the msg.
	 * The delay is needed because of the hardware: if you must send more
	 * than one msgs the hardware has to take its time to reload the msg. */
	uint32_t mailbox;
	uint8_t flag = 0;

	CAN_TxHeaderTypeDef TxHeader;
	TxHeader.StdId = id;
	TxHeader.IDE = CAN_ID_STD;
	TxHeader.RTR = CAN_RTR_DATA;
	TxHeader.DLC = size;
	TxHeader.TransmitGlobalTime = DISABLE;

	if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) != 0 && HAL_CAN_IsTxMessagePending(&hcan1, CAN_TX_MAILBOX0 + CAN_TX_MAILBOX1 + CAN_TX_MAILBOX2) == 0)
	{
		HAL_CAN_AddTxMessage(&hcan1, &TxHeader, dataTx, &mailbox);
		flag = 1;
	}

	return flag;
}

int CAN_Receive(uint8_t *DataRx)
{
	/* This function is used to receive the msgs sent by other devices */
	if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) != 0)
	{
		HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, DataRx);
	}

	int id = RxHeader.StdId;

	return id;
}

//-----------Interrupt-----------
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan)
{
	/* This function works in interrupt mode, whenever there is a new msg sent by other
	 * devices this is called back.
	 * The idea is to save the new msg in a circular FIFO so that is possible to process
	 * it during the execution of the state in which you are after the interrupt is ended.
	 * Then it looks at the id of the msg to know which device sent it and saves the time
	 * msg is arrived.
	 * In case of the pedals the variables are immediately set because of the delay could
	 * be introduced during the processing of the FIFO in the states.
	 * After all the CounterUp is incremented to point out a new msg is arrived.
	 * Than the operation mod is done so that prevents to make counter overflow */

	bool insert_in_fifo = false;
	int idsave;
	uint8_t RxData[MSG_LENGHT];

	idsave = CAN_Receive(RxData);

	switch (idsave)
	{
	case ID_STEERING_WHEEL:
		insert_in_fifo = true;
		data.steeringPresence = true;
		data.steeringTimeStamp = HAL_GetTick();
		break;
	case ID_PEDALS:
		insert_in_fifo = true;
		if (RxData[0] == 0x01 && data.go == 1)
		{
			data.accelerator = RxData[1];
		}
		else if (RxData[0] == 0x02 && data.go == 1)
		{
			data.breakingPedal = RxData[1];
		}

		data.pedalsPresence = true;
		data.pedalsTimeStamp = HAL_GetTick();
		break;
	case ID_REQ_INV_SX:
		insert_in_fifo = true;
		data.invSxPresence = true;
		data.invSxTimeStamp = HAL_GetTick();
		break;
	case ID_REQ_INV_DX:
		insert_in_fifo = true;
		data.invDxPresence = true;
		data.invDxTimeStamp = HAL_GetTick();
		break;
	case ID_BMS_HV:
		insert_in_fifo = true;
		data.bmsHvPresence = true;
		data.bmsHvTimeStamp = HAL_GetTick();
		break;
	case ID_BMS_LV:
		insert_in_fifo = true;
		data.bmsLvPresence = true;
		data.bmsLvTimeStamp = HAL_GetTick();
		break;
	default:
		/* Non succede niente ai timestamp */
		break;
	}

	if (insert_in_fifo)
	{
		data.fifoData[data.dataCounterUp].idsave = idsave;
		for (int i = 0; i < MSG_LENGHT; i++)
		{
			data.fifoData[data.dataCounterUp].RxData[i] = RxData[i];
		}

		data.dataCounterUp += 1;
		data.dataCounterUp = data.dataCounterUp % NUM_DATA;

		if (data.dataCounterUp == data.dataCounterDown)
		{
			//shutdownErrors(&data, 0x23);
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	/* This functions works in interrupt mode, there are two conditions based on the
	 * timer.
	 * The first one is used to timing the writing in can of recursived msgs like the
	 * torque request to inverter.
	 * The second one is used to make buzzer sounds for just two seconds like set in
	 * the settings of the timer. */

	/*if (htim->Instance == TIM6){
		data.writeInCan = true;
	}*/

	if (htim->Instance == TIM7)
	{
		if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET)
		{
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
		}
	}
}

//-------do_state functions-------
state_t do_state_init(state_global_data_t *data)
{
	/* This state is the initialization state: it's the first of the Finite State Machine
	 * but it's executed just one time suddenly the car is turned on. */
	initData(data);
	return STATE_IDLE;
}

state_t do_state_idle(state_global_data_t *data)
{
	/* In this state the Tractive System is completely disable. This is the first state
	 * which the driver is put in contact, here it's possible to turning on the car.
	 * The first thing to do is to check if the CounterDown and the CounterUp are different,
	 * if they are it means that new msgs have been saved in the FIFO so it's possible to
	 * process it.
	 * The mod operation is done to be sure the that counter doesn't overflow.
	 * After this a switch structure checks the id of the first msg to process, if it
	 * corresponds to one of the case considered than it executes the request and
	 * increments the CounterDown else it simply increments the counter.
	 * At the end, if the driver asks for the turning on of the car ECU sends a msg
	 * to the BMS_HV, after receiving back the confirm the variable Tractive System
	 * is set to true and there the FSM goes to the SETUP state. */
	if (data->dataCounterUp != data->dataCounterDown)
	{
		switch (data->fifoData[data->dataCounterDown].idsave)
		{
		/* Debug case: just to know if it's online and in which state */
		case ID_ASK_STATE:
			canSendMSGInit(canSendMSG);
			canSendMSG[0] = 0x10;
			canSendMSG[1] = 0x01;		// state
			CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
			break;
		case ID_STEERING_WHEEL:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Check who is online */
			case 0x02:
				sendErrors(data);
				if (data->fifoData[data->dataCounterDown].RxData[1] == 0xEC)
				{
					data->powerRequested = -20;
				}
				else
				{
					data->powerRequested = data->fifoData[data->dataCounterDown].RxData[1];
				}
				break;
			/* Turning on car driver request */
			case 0x03:
				sendErrors(data);
				/* Check for critical errors */
				if (data->errors == 0 || data->errors == 16 || data->errors == 32 || data->errors == 48)
				{
					/* Turning on car request to bms_hv*/
					canSendMSGInit(canSendMSG);
					canSendMSG[0] = 0x0A;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
				}
				break;
			default:
				break;
			}
			break;
		case ID_BMS_HV:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Turning on car confirmed */
			case 0x03:
				data->tractiveSystem = true;
				break;
			/* Turning off car: shouldn't never happen here */
			case 0x04:
				data->tractiveSystem = false;
				break;
			default:
				break;
			}
			break;
		case ID_REQ_INV_SX:
			/* Check for Inverter status */
			if (data->fifoData[data->dataCounterDown].RxData[0] == 0xD8)
			{
				if (data->fifoData[data->dataCounterDown].RxData[2] == 0x0C)
				{
					canSendMSG[0] = 0x08;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					data->inverterSx = true;
				}
				else
				{
					canSendMSG[0] = 0x0C;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					data->inverterSx = false;
				}
			}
			break;
		case ID_REQ_INV_DX:
			/* Check for Inverter status */
			if (data->fifoData[data->dataCounterDown].RxData[0] == 0xD8)
			{
				if (data->fifoData[data->dataCounterDown].RxData[2] == 0x0C)
				{
					canSendMSG[0] = 0x09;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					data->inverterDx = true;
				}
				else
				{
					canSendMSG[0] = 0x0D;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					data->inverterDx = false;
				}
			}
			break;
		default:
			break;
		}
		data->dataCounterDown += 1;
		data->dataCounterDown = data->dataCounterDown % NUM_DATA;
	}

	if (data->steeringPresence == true && data->tractiveSystem == true)
	{
		return STATE_SETUP;
	}
	return STATE_IDLE;
}

state_t do_state_setup(state_global_data_t *data)
{
	/* This state is structured like IDLE.
	 * In this state is possible to:
	 * 	1. enable inverters on driver request. At the moment it's possible to
	 * 	   enable just once a time. Before sending the request it checks the
	 * 	   temperature to avoid issues.
	 *	2. go back to state IDLE on driver request. Please note: to change state
	 *	   confirm by BMS_HV is needed.
	 *	3. go to state RUN on driver request. To go to that state there are
	 *	   some conditions: first of all inverters have to be enabled; then the
	 *	   breaking pedal has to be pressed (by regulation) and no requests of
	 *	   shutdown have to be done. If everything is verified after the request
	 *	   the variable go is set to true.
	 * Since the tractive system is on, all the critical issues have to be checked
	 * so scs signals are analyzed. */
	if (data->dataCounterUp != data->dataCounterDown)
	{
		switch (data->fifoData[data->dataCounterDown].idsave)
		{
		/* Debug case: just to know if it's online and in which state */
		case ID_ASK_STATE:
			canSendMSGInit(canSendMSG);
			canSendMSG[0] = 0x10;
			canSendMSG[1] = 0x02;
			CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
			break;
		case ID_STEERING_WHEEL:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Check who is online */
			case 0x02:
				sendErrors(data);
				// TODO: reinsert
				//if (data->errors != 0 && data->errors != 16 && data->errors != 32 && data->errors != 48){
				/* Turning on car request to bms_hv*/
				//shutdownErrors(data, data->errors);
				//}
				break;
			/* Stop msg from driver */
			case 0x04:
				shutdown(data);
				break;
			/* Going to run mode driver request */
			case 0x05:
				/* If inverter are turned on and break is pressed i can go to run */
				if (data->inverterSx == true && data->inverterDx == true && data->breakingPedal == true && data->requestOfShutdown == false)
				{
					data->go = true;
					/* Map */
					if (data->fifoData[data->dataCounterDown].RxData[1] == 0xEC)
					{
						data->powerRequested = -20;
					}
					else
					{
						data->powerRequested = data->fifoData[data->dataCounterDown].RxData[1];
					}
					HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
					__HAL_TIM_SetCounter(&htim7, 0);
				}
				else
				{
					uint8_t tosend = 0;
					if (data->inverterSx)
						tosend += 8;
					if (data->inverterDx)
						tosend += 16;
					if (data->breakingPedal)
						tosend += 32;
					if (data->requestOfShutdown)
						tosend += 64;

					canSendMSG[0] = 0xFF;
					canSendMSG[1] = tosend;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
				}
				break;
			/* Turning on Inverter sx driver request */
			case 0x08:
				if (data->invLeftTemp < 80)
				{
					canSendMSGInit(canSendMSG);

					/* Enable Inverter msg */
					canSendMSG[0] = 0x51;
					canSendMSG[1] = 0x08;
					CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);

					/* Status Request */
					canSendMSG[0] = 0x3D;
					canSendMSG[1] = 0xD8;
					CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
				}
				break;
			/* Turning on Inverter dx driver request */
			case 0x09:
				if (data->invRightTemp < 80)
				{
					canSendMSGInit(canSendMSG);

					/* Enable Inverter msg */
					canSendMSG[0] = 0x51;
					canSendMSG[1] = 0x08;
					CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

					/* Status Request */
					canSendMSG[0] = 0x3D;
					canSendMSG[1] = 0xD8;
					CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);
				}
				break;
			default:
				break;
			}
			break;
		case ID_BMS_HV:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Shutdown confirmed */
			case 0x04:
				data->tractiveSystem = false;
				break;
			/* Shutdown caused by an error */
			case 0x08:
				data->tractiveSystem = false;
				break;
			default:
				break;
			}
			break;
		case ID_BMS_LV:
			/* Saving SCS */
			if (data->fifoData[data->dataCounterDown].RxData[0] == 0x01)
			{
				data->scs[LV] = data->fifoData[data->dataCounterDown].RxData[5];
			}
			break;
		case ID_PEDALS:
			/* Saving SCS */
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			case 0x01:
				data->scs[APPS] = data->fifoData[data->dataCounterDown].RxData[6];
				break;
			case 0x02:
				data->breakingPedal = data->fifoData[data->dataCounterDown].RxData[1];
				data->scs[BSE] = data->fifoData[data->dataCounterDown].RxData[6];
				break;
			default:
				break;
			}
			break;
		case ID_FRONT:
			break;
		case ID_CENTER:
			break;
		case ID_REQ_INV_SX:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			case 0x4A:
				data->invLeftTemp = (data->fifoData[data->dataCounterDown].RxData[2] * 256 + data->fifoData[data->dataCounterDown].RxData[1] - 15797) / 112.1182;
				break;
			/* Check for Inverter status */
			case 0xD8:
				if (data->fifoData[data->dataCounterDown].RxData[2] == 0x0C && data->requestOfShutdown == false)
				{
					canSendMSG[0] = 0x08;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					data->inverterSx = true;
				}
				else
				{
					canSendMSG[0] = 0x0C;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					//data->inverterSx = false;
				}
				break;
			default:
				break;
			}
			break;
		case ID_REQ_INV_DX:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			case 0x4A:
				data->invRightTemp = (data->fifoData[data->dataCounterDown].RxData[2] * 256 + data->fifoData[data->dataCounterDown].RxData[1] - 15797) / 112.1182;
				break;
			/* Check for Inverter status */
			case 0xD8:
				if (data->fifoData[data->dataCounterDown].RxData[2] == 0x0C && data->requestOfShutdown == false)
				{
					canSendMSG[0] = 0x09;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					data->inverterDx = true;
				}
				else
				{
					canSendMSG[0] = 0x0D;
					CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
					//data->inverterDx = false;
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		data->dataCounterDown += 1;
		data->dataCounterDown = data->dataCounterDown % NUM_DATA;
	}

	/* Check SCS */
	for (int i = 0; i < NUM_SCS; i++)
	{
		if (data->scs[i] != 0)
		{
			shutdownErrors(data, ID_STEERING_WHEEL);
		}
	}

	/* TODO: inserire un messaggio mandato ogni tot, serve per avvisare il BMS_HV che l'ecu � online */
	/*if (data->writeInCan){
		canSendMSGInit(canSendMSG);
		CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);

		data->writeInCan = false;
	}*/

	if (data->tractiveSystem == false)
	{
		return STATE_IDLE;
	}
	if (data->go == true)
	{
		return STATE_RUN;
	}

	return STATE_SETUP;
}

state_t do_state_run(state_global_data_t *data)
{
	/* This state is structured like IDLE. Here the car is completely able to run.
	 * In this state is possible to:
	 * 	1. go back to state SETUP on driver request, please note: from here is
	 * 	   not possible to go to state IDLE except for some error.
	 *	2. change map, so that driver could choose power at anytime.
	 *	3. to execute controls: yaw control and slip control to optimize trajectory
	 *	   and at the end transmit current requests to inverters.
	 * Writing in can is possible just every 10 ms, that's because otherwise it
	 * could be possible to block the can-bus.
	 * Since the tractive system is on, all the critical issues have to be checked
	 * so scs signals are analyzed. */
	if (data->dataCounterDown != data->dataCounterUp)
	{
		switch (data->fifoData[data->dataCounterDown].idsave)
		{
		/* Debug case: just to know if it's online and in which state */
		case ID_ASK_STATE:
			canSendMSGInit(canSendMSG);
			canSendMSG[0] = 0x10;
			canSendMSG[1] = 0x03;
			CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
			break;
		case ID_STEERING_WHEEL:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Check who is online */
			case 0x02:
				sendErrors(data);
				/* map changing */
				if (data->fifoData[data->dataCounterDown].RxData[1] == 0xEC)
				{
					data->powerRequested = -20;
				}
				else
				{
					data->powerRequested = data->fifoData[data->dataCounterDown].RxData[1];
				}
				break;
				// TODO: reinsert
				//if (data->errors != 0 && data->errors != 16 && data->errors != 32 && data->errors != 48){
				/* Turning on car request to bms_hv*/
				//shutdownErrors(data, data->errors);
				//}
				break;
			/* Setup request */
			case 0x06:
				data->go = false;
				break;
			default:
				break;
			}
			break;
		case ID_PEDALS:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Saving SCS */
			case 0x01:
				data->scs[APPS] = data->fifoData[data->dataCounterDown].RxData[6];
				break;
			case 0x02:
				data->scs[BSE] = data->fifoData[data->dataCounterDown].RxData[6];
				break;
			default:
				break;
			}
			break;
		case ID_FRONT:
			break;
		case ID_CENTER:
			break;
		case ID_BMS_LV:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Saving SCS */
			case 0x01:
				data->scs[LV] = data->fifoData[data->dataCounterDown].RxData[5];
				break;
			default:
				break;
			}
			break;
		case ID_BMS_HV:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			/* Volt --> must be converted */
			case 0x01:
				data->hvVol = ((data->fifoData[data->dataCounterDown].RxData[1] << 16) + (data->fifoData[data->dataCounterDown].RxData[2] << 8) + 0x00) / 10000;
				break;
			case 0x05:
				data->hvCur = (*(int16_t *)(data->fifoData[data->dataCounterDown].RxData + 1)) / 10;
				break;
			/* Shutdown caused by an error */
			case 0x08:
				data->tractiveSystem = false;
				data->go = false;
				break;
			default:
				break;
			}
			break;
		case ID_REQ_INV_SX:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			case 0x4A:
				data->invLeftTemp = data->fifoData[data->dataCounterDown].RxData[2] * 256 + data->fifoData[data->dataCounterDown].RxData[1];
				break;
			case 0x49:
				data->motLeftTemp = data->fifoData[data->dataCounterDown].RxData[2] * 256 + data->fifoData[data->dataCounterDown].RxData[1];
				break;
			default:
				break;
			}
			break;
		case ID_REQ_INV_DX:
			switch (data->fifoData[data->dataCounterDown].RxData[0])
			{
			case 0x4A:
				data->invRightTemp = data->fifoData[data->dataCounterDown].RxData[2] * 256 + data->fifoData[data->dataCounterDown].RxData[1];
				break;
			case 0x49:
				data->motRightTemp = data->fifoData[data->dataCounterDown].RxData[2] * 256 + data->fifoData[data->dataCounterDown].RxData[1];
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		data->dataCounterDown += 1;
		data->dataCounterDown = data->dataCounterDown % NUM_DATA;
	}

	//TODO: controllo

	if (data->writeInCan)
	{
		transmission(data);
		data->writeInCan = false;
	}

	if (data->hvCur * data->hvVol > 1500)
	{
		//shutdownErrors(data, 0x99);
	}

	checkValues(data);

	/* Check SCS */
	/*for (int i = 0; i < NUM_SCS; i++){
		if (data->scs[i] != 0){
			shutdownErrors(data, ID_STEERING_WHEEL);
		}
	}*/

	if (data->tractiveSystem == false)
	{
		return STATE_IDLE;
	}
	if (data->go == false)
	{
		return STATE_SETUP;
	}
	return STATE_RUN;
}

//-------to_state functions--------
void to_idle(state_global_data_t *data)
{
	/* This function is used everytime ECU goes to state IDLE.
	 * Here the msg to steering wheel is sent to let it know the change of
	 * state. Errors are sent as well.
	 * Moreover a data request for the temperature is sent to inverters.
	 * All the other requests to the inverters are to stop msgs useless in
	 * this state. */
	canSendMSGInit(canSendMSG);

	data->requestOfShutdown = false;

	/* Sending to steering wheel msg i'm in idle, 3 times so i'm sure it won't be lost */
	canSendMSG[0] = 0x04;
	for (int i = 0; i < 3; i++)
	{
		CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
	}

	canSendMSGInit(canSendMSG);

	/* Sending Errors To Steering Wheel */
	canSendMSG[0] = 0x01;
	canSendMSG[1] = data->errors;
	canSendMSG[2] = data->warningsB1;
	canSendMSG[3] = 0;
	CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);

	canSendMSGInit(canSendMSG);

	/* Inverter Disable */
	canSendMSG[0] = 0x51;
	canSendMSG[1] = 0x04;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Status Request */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xD8;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/*IGBT Temperature: 100 ms*/
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x4A;
	canSendMSG[2] = 0x64;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Actual Speed Value Filtered: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xA8;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Filtered Actual Current: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x5F;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Command Current: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x26;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Motor Temperature: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x49;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* VDC-Bus: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xEB;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Max Motor Current: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x4D;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Motor Continuous Current: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x4E;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);
}

void from_idle_to_setup(state_global_data_t *data)
{
	/* This function is used everytime ECU goes from the state IDLE to state
	 * SETUP.
	 * Here the msg to steering wheel is sent to let it know the change of
	 * state.
	 * An inverter disable request is sent to be sure that they're disable.
	 * After that a status request is sent to let it know to steering wheel.
	 * Moreover some data requests are sent to inverters. Read the
	 * descriptions for every msg. */
	canSendMSGInit(canSendMSG);

	/* Sending to steering wheel msg i'm in setup */
	canSendMSG[0] = 0x03;
	CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);

	/* Inverter Disable */
	canSendMSG[0] = 0x51;
	canSendMSG[1] = 0x04;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Status Request */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xD8;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Motor Temperature: 100 ms */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x49;
	canSendMSG[2] = 0x64;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* VDC-Bus: 100 ms */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xEB;
	canSendMSG[2] = 0x64;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* V-Out: 100 ms */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x8A;
	canSendMSG[2] = 0x64;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Motor Power: 100 ms */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xF6;
	canSendMSG[2] = 0x64;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Torque-Out: 100 ms */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xA0;
	canSendMSG[2] = 0x64;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/*
	//Max Motor Current: 20 ms --> logging
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x4D;
	canSendMSG[2] = 0x14;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	//Motor Continuous Current: 20 ms --> logging
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x4E;
	canSendMSG[2] = 0x14;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);
	*/
}

void from_run_to_setup(state_global_data_t *data)
{
	/* This function is used everytime ECU goes from the state RUN to state
	 * SETUP.
	 * Here the msg to steering wheel is sent to let it know the change of
	 * state.
	 * An inverter disable request is sent to be sure that they're disable.
	 * After that a status request is sent to let it know to steering wheel.
	 * All the other requests to the inverters are to stop msgs useless in
	 * this state. */
	canSendMSGInit(canSendMSG);

	data->accelerator = 0;

	/* Sending to steering wheel msg i'm in setup */
	canSendMSG[0] = 0x06;
	CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);

	canSendMSGInit(canSendMSG);

	/* Inverter Disable */
	canSendMSG[0] = 0x51;
	canSendMSG[1] = 0x04;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Status Request */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xD8;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Actual Speed Value Filtered: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xA8;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Filtered Actual Current: stop sending */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x5F;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Command Current: stop sending */
	/*canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x26;
	canSendMSG[2] = 0xFF;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);*/
}

void to_run(state_global_data_t *data)
{
	/* This function is used everytime ECU goes to state RUN.
	 * First of all timer is set to 0, that's because by regulation a
	 * buzzer has to ring for a time between 1-3 secs everytime the car
	 * is able to run.
	 * Here the msg to steering wheel is sent to let it know the change of
	 * state.
	 * Moreover some data request are sent to inverters. */
	__HAL_TIM_SetCounter(&htim7, 0);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);

	canSendMSGInit(canSendMSG);

	/* Sending to steering wheel msg i'm in run */
	canSendMSG[0] = 0x05;
	CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);

	/* Actual Speed Value Filtered: 50 ms */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0xA8;
	canSendMSG[2] = 0x32;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/* Filtered Actual Current: 100 ms */
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x5F;
	canSendMSG[2] = 0x64;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);

	/*
	//Command Current: 20 ms --> logging
	canSendMSG[0] = 0x3D;
	canSendMSG[1] = 0x26;
	canSendMSG[2] = 0x14;
	CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
	CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);
	*/
}

//-----List of state functions-----
state_func_t *const state_table[NUM_STATES] = {
	/* This list is an array called back from the state manager to
	 * know which state function execute according to the state returned
	 * by the function before. */
	do_state_init, do_state_idle,
	do_state_setup, do_state_run};

//--Table of transition functions--
transition_func_t *const transition_table[NUM_STATES][NUM_STATES] = {
	/* This matrix is called back from the state manager to know
	 * which transition function execute according to present state and
	 * the future state. */
	{NULL, to_idle, NULL, NULL},
	{NULL, NULL, from_idle_to_setup, NULL},
	{NULL, to_idle, NULL, to_run},
	{NULL, to_idle, from_run_to_setup, NULL},
};

//call the current state and returns the next one
state_t run_state(state_t cur_state, state_global_data_t *data)
{
	/* This function is the state manager.
	 * According to the cur_state passed by the function before is chosen the
	 * new_state and so on. */
	state_t new_state = state_table[cur_state](data);
	transition_func_t *transition = transition_table[cur_state][new_state];
	if (transition)
	{
		transition(data);
	}
	return new_state;
}

//--------operative function--------
void initData(state_global_data_t *data)
{
	/* This functions is used to initialize all the variables.
	 * It's called just during the state INIT. */
	data->tractiveSystem = false;	 // Tractive system off
	data->go = false;				 // setup
	data->breakingPedal = false;	 // breaking pedal up
	data->inverterSx = false;		 // inverter Sx disable
	data->inverterDx = false;		 // inverter Dx disable
	data->requestOfShutdown = false; // shutdown not request
	data->writeInCan = false;		 // wait next timeout for writing

	data->steeringPresence = false;
	data->pedalsPresence = false;
	data->frontalPresence = false;
	data->centralPresence = false;
	data->bmsLvPresence = false;
	data->bmsHvPresence = false;
	data->invDxPresence = false;
	data->invSxPresence = false;

	data->errors = 255;		// no devices connected
	data->warningsB1 = 255; // no sensors connected

	for (int i = 0; i < NUM_SCS; i++)
	{
		data->scs[i] = 0;
	}

	data->dataCounterUp = 0x0000;
	data->dataCounterDown = 0x0000;

	data->accelerator = 0x00;
	data->motLeftTemp = 0x0000;
	data->motRightTemp = 0x0000;
	data->invLeftTemp = 0x0000;
	data->invRightTemp = 0x0000;
	data->invLeftVol = 0x0000;
	data->invRightVol = 0x0000;
	data->invLeftCur = 0x0000;
	data->invRightCur = 0x0000;
	data->hvCur = 0x0000;
	
	data->hvVol = 0x00000000;
	data->curRequested = 0.0;
	data->powerRequested = 0x00;

	data->steeringTimeStamp = 0x0000;
	data->pedalsTimeStamp = 0x0000;
	data->frontalTimeStamp = 0x0000;
	data->centralTimeStamp = 0x0000;
	data->bmsLvTimeStamp = 0x0000;
	data->bmsHvTimeStamp = 0x0000;
	data->invDxTimeStamp = 0x0000;
	data->invSxTimeStamp = 0x0000;
}

void canSendMSGInit(uint8_t *CanSendMSG)
{
	/* This function is used to initialize the msg everytime you need to send
	 * something via can. */
	for (int i = 0; i < MSG_LENGHT; i++)
	{
		CanSendMSG[i] = 0;
	}
}

void checkTimeStamp(state_global_data_t *data)
{
	/* This function is used to compare the current time with the time of the last msg
	 * sent by a device, if a difference is greater than the preset delta the device
	 * is considered out of order.
	 * There are case since the timer is linear (from 0 to 49999 and than
	 * starts again):
	 * 	1. the first case is used when the current time is greater than the last
	 * 	   time of the msg.
	 *	2. the second case is used when the current time is lower than the last
	 *	   time of the msg, this happens when the timer has ended its period and it's
	 *	   started again from the beginning.
	 * After the check if a device is disabled it increments the variables errors and
	 * warningB1, these are reading bit for bit from the steering wheel so every bit
	 * corresponds to a device, that's why the increment is done in exponentiation of 2. */
	data->errors = 0;
	data->warningsB1 = 0;

	//data->actualTime = __HAL_TIM_GetCounter(&htim10);
	data->actualTime = HAL_GetTick();

	/* Steering Wheel Timer */
	if (data->steeringPresence == false)
		data->steeringPresence = true;

	/* Pedals Timer */
	if (data->pedalsPresence == true)
	{
		if (data->actualTime - data->pedalsTimeStamp > SHORT_DELTA)
		{
			data->pedalsPresence = false;
			data->errors += 8;
			data->warningsB1 += 192;
		}
	}
	else
	{
		data->errors += 8;
		data->warningsB1 += 192;
	}

	/* Frontal Timer */
	if (data->frontalPresence == true)
	{
		if (data->actualTime - data->frontalTimeStamp > SHORT_DELTA)
		{
			data->frontalPresence = false;
			data->errors += 32;
			data->warningsB1 += 60;
		}
	}
	else
	{
		data->errors += 32;
		data->warningsB1 += 60;
	}

	/* Central Timer */
	if (data->centralPresence == true)
	{
		if (data->actualTime - data->centralTimeStamp > SHORT_DELTA)
		{
			data->centralPresence = false;
			data->errors += 16;
			data->warningsB1 += 3;
		}
	}
	else
	{
		data->errors += 16;
		data->warningsB1 += 3;
	}

	/* BmsLv Timer */
	if (data->bmsLvPresence == true)
	{
		if (data->actualTime - data->bmsLvTimeStamp > LONG_DELTA)
		{
			data->bmsLvPresence = false;
			data->errors += 2;
		}
	}
	else
	{
		data->errors += 2;
	}

	/* BmsHv Timer */
	if (data->bmsHvPresence == true)
	{
		if (data->actualTime - data->bmsHvTimeStamp > LONG_DELTA)
		{
			data->bmsHvPresence = false;
			data->errors += 1;
		}
	}
	else
	{
		data->errors += 1;
	}

	/* TODO: Reintroduce Inverter Dx*/
	/* Inverter Dx Timer */
	if (data->invDxPresence == true)
	{
		if (data->actualTime - data->invDxTimeStamp > SHORT_DELTA)
		{
			data->invDxPresence = false;
			data->errors += 128;
		}
	}
	else
	{
		data->errors += 128;
	}

	/* Inverter Sx Timer */
	if (data->invSxPresence == true)
	{
		if (data->actualTime - data->invSxTimeStamp > SHORT_DELTA)
		{
			data->invSxPresence = false;
			data->errors += 64;
		}
	}
	else
	{
		data->errors += 64;
	}
}

void sendStatus(state_global_data_t *data)
{
	canSendMSGInit(canSendMSG);

	uint8_t map = data->powerRequested;
	uint8_t state = 0;

	if (STATO == STATE_IDLE)
		state = 0;
	if (STATO == STATE_SETUP)
		state = 1;
	if (STATO == STATE_RUN)
		state = 2;

	canSendMSG[0] = 0x01;
	canSendMSG[1] = data->errors;
	canSendMSG[2] = data->warningsB1;
	canSendMSG[3] = map;
	canSendMSG[4] = state;
	CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
}

void sendErrors(state_global_data_t *data)
{
	/* This function is used to send errors and warnings to the steering wheel.
	 * Errors and Warnings are calculated every cycle but sent just on request. */
	canSendMSGInit(canSendMSG);

	uint8_t map = data->powerRequested;
	uint8_t state = 0;

	if (STATO == STATE_IDLE)
		state = 0;
	if (STATO == STATE_SETUP)
		state = 1;
	if (STATO == STATE_RUN)
		state = 2;

	canSendMSG[0] = 0x01;
	canSendMSG[1] = data->errors;
	canSendMSG[2] = data->warningsB1;
	canSendMSG[3] = map;
	canSendMSG[4] = state;
	CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
}

void shutdown(state_global_data_t *data)
{
	/* This function is used to sand the turning off request to the BMS_HV.
	 * This is used just on driver request.
	 * The msg is sent three times to be sure that BMS_HV would received that. */
	canSendMSGInit(canSendMSG);

	char mander[50];
	sprintf(mander, "ShutDown\n\r");
	HAL_UART_Transmit(&huart2, (uint8_t *)mander, strlen(mander), 10);

	canSendMSG[0] = 0x0B;
	canSendMSG[1] = 0x04;
	for (int i = 0; i < 3; i++)
	{
		CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
	}
	//data->requestOfShutdown = 1;
}

void shutdownErrors(state_global_data_t *data, int err)
{
	/* This function is used to sand the turning off request to the BMS_HV.
	 * This is used just in case of an error occurred.
	 * The msg is sent three times to be sure that BMS_HV would received that. */
	canSendMSGInit(canSendMSG);

	char mander[50];
	sprintf(mander, "ShutDown_Errors: %d\n\r", err);
	HAL_UART_Transmit(&huart2, (uint8_t *)mander, strlen(mander), 10);

	canSendMSG[0] = 0x0B;
	canSendMSG[1] = 0x08;
	canSendMSG[2] = err;
	for (int i = 0; i < 3; i++)
	{
		CAN_Send(ID_ECU, canSendMSG, MSG_LENGHT);
	}
	//data->requestOfShutdown = 1;
}

void 
checkValues(state_global_data_t *data)
{
	/* This function is used to check data coming from inverter.
	 * The scs array is used to keep in memory the situation of the errors. */
	if ((data->motLeftTemp - 9393.9) / 55.1 > 115)
	{
		data->scs[MOT_TEMP_SX] = 0x01;
	}
	if ((data->motRightTemp - 9393.9) / 55.1 > 115)
	{
		data->scs[MOT_TEMP_DX] = 0x01;
	}
	if ((data->invLeftTemp - 15797) / 112.12 > 80)
	{
		data->scs[INV_TEMP_SX] = 0x01;
	}
	if ((data->invRightTemp - 15797) / 112.12 > 80)
	{
		data->scs[INV_TEMP_DX] = 0x01;
	}
	if (data->invLeftCur * 200 / 560 > 235)
	{
		data->scs[INV_CUR_SX] = 0x01;
	}
	if (data->invRightCur * 200 / 560 > 235)
	{
		data->scs[INV_CUR_DX] = 0x01;
	}
}

void transmission(state_global_data_t *data)
{
	/* This function is used to send the current request to the inverters.
	 * The current needed is calculated considering the max power (80kW) and the
	 * voltage sent by the BMS_HV.
	 * The condition is a recommendation coming from the code of rules, is to be
	 * sure that the driver isn't pressing breaking pedal and accelerator pedal
	 * both.
	 * Current requested are opposite because of the resolver positioned in
	 * opposite direction (right and left). */
	canSendMSGInit(canSendMSG);

	uint8_t firstByte;
	uint8_t secondByte;
	uint8_t negFirstByte;
	uint8_t negSecondByte;
	int negativeCurrentToInverter;

	/* TODO: check rules */
	//if ((data->breakingPedal == 1 && data->accelerator > 25) || (data->requestOfShutdown == true))
	if ((data->requestOfShutdown == true))
	{
		data->curRequested = 0;

		canSendMSG[0] = 0x90;
		canSendMSG[1] = 0x00;
		canSendMSG[2] = 0x00;

		CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);
		CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);
	}
	else
	{
		/* Check Inverter datasheet */

		int currentToInverter = ((32767 / 424.2) * (120 / 0.8) * 1.414) * (data->accelerator / 100.0) * (data->powerRequested / 100.0);
		//int currentToInverter = ((32767 * data->powerRequested * 800.0) / (424.2 * data->hvVol)) * (1.414 / 2) * (data->accelerator / 100.0);

		/* Convert current to inverter */
		firstByte = currentToInverter % 256;
		secondByte = currentToInverter / 256;
		canSendMSG[0] = 0x90;
		canSendMSG[1] = firstByte;
		canSendMSG[2] = secondByte;
		CAN_Send(ID_ASK_INV_SX, canSendMSG, MSG_LENGHT);

		/* Convert current to inverter must be negative */
		negativeCurrentToInverter = -currentToInverter;
		negFirstByte = negativeCurrentToInverter % 256;
		negSecondByte = negativeCurrentToInverter / 256;
		canSendMSG[0] = 0x90;
		canSendMSG[1] = negFirstByte;
		canSendMSG[2] = negSecondByte;
		CAN_Send(ID_ASK_INV_DX, canSendMSG, MSG_LENGHT);
	}
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
