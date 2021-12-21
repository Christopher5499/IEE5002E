/*
 * kypd_interrupt.c
 *
 *  Created on: 	21-12-2021
 *      Author: 	Christopher León
 *     Version:		2.0
 *     Recycled from Zynq Book tutorial 2
 */
/********************************************************************************************
* VERSION HISTORY
********************************************************************************************
*	v1.0 - 31-03-2021
*	First version created, modified from Zynq Book tutorial
*******************************************************************************************/

/********************************************************************************************
 * This file contains an example of using the GPIO driver to scan a keypad in JE Pmod
 * using interruptions in the Zybo Z7-20 Board. The system connects to the keypad by using AXI GPIO.
 * The AXI GPIO is connected to the LEDs (CH1) and Keypad in (CH2) on the Zybo.
 *
 * The provided code scans the keypad and show the binary value in the leds
 ********************************************************************************************/

#include "xparameters.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "xscugic.h"
#include "xil_exception.h"


/* Definitions */
#define GPIO_DEVICE_ID  XPAR_AXI_GPIO_0_DEVICE_ID		
#define LED_CHANNEL 1						
#define KEYPAD_CHANNEL 2					
#define printf xil_printf					

/* Interruptions definitions */
#define INTC_DEVICE_ID	XPAR_PS7_SCUGIC_0_DEVICE_ID 					
#define INTC_GPIO_INTERRUPT_ID XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR 	
#define KYPD_INT_MASK	XGPIO_IR_CH2_MASK 					


// GPIO device ID one AXI GPIO Block with two channels
//#define BTNS_LEDS_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID


/* Instance of the AXI GPIO */
XGpio Gpio;

/* Instance of the Interrupt Controller */
XScuGic IntC;

static int key;		

/****************************************************
 *
 * Function Prototypes
 *
 * **************************************************/

/* keypad scan routine */
int KYPD_Scan(void);

/* Interrupt handler for the keypad */
static void KYPD_Intr_Handler(void *InstancePtr);

/* Interrupt handler configuration routine */
static int IntCInitFunction(u16 DeviceId, XGpio *GpioInstancePtr);


/******************************************************
 *
 * Main Function
 *
 * ****************************************************/

int main (void)
{
  int status;


  status = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;

   /* Set the direction for the LEDs to output. */
   XGpio_SetDataDirection(&Gpio, LED_CHANNEL, 0x0);

   /* Set the direction for the KEYPAD
    * 4 highest bits for rows (inputs)
    * 4 lowest bits for columns (outputs)*/
   XGpio_SetDataDirection(&Gpio, KEYPAD_CHANNEL, 0xf0);

   /* Setup interrupt controller and handler connection */
  status = IntCInitFunction(INTC_DEVICE_ID, &Gpio);
  if(status != XST_SUCCESS) return XST_FAILURE;

  //infinite loop waiting for interruption
  while(1);

  return 0;
}

/**********************************************************
 *
 * Function Implementations
 *
 * *********************************************************/

/* Interrupt setup and handler connection */
int IntCInitFunction(u16 DeviceId, XGpio *GpioInstancePtr)
{
	XScuGic_Config *IntCConfig;
	int status;

	/* Interrupt controller initialization and success check */
	IntCConfig = XScuGic_LookupConfig(DeviceId);
	status = XScuGic_CfgInitialize(&IntC, IntCConfig, IntCConfig->CpuBaseAddress);
	if(status != XST_SUCCESS) return XST_FAILURE;


	/* Enable Exception handlers */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 	 	 	 	 	 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
								 &IntC);
	Xil_ExceptionEnable();


	/* Connect GPIO interrupt to handler and check for success */
	status = XScuGic_Connect(&IntC,
					  	  	 INTC_GPIO_INTERRUPT_ID,
					  	  	 (Xil_ExceptionHandler)KYPD_Intr_Handler,
					  	  	 (void *)GpioInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;


	/* Enable GPIO interrupts */
	XGpio_InterruptEnable(GpioInstancePtr, KYPD_INT_MASK); //switched 1 with KYPD_INT_MASK
	XGpio_InterruptGlobalEnable(GpioInstancePtr);


	/* Enable GIC */
	XScuGic_Enable(&IntC, INTC_GPIO_INTERRUPT_ID);

	return XST_SUCCESS;
}

/* Interrupt handler implementation */
void KYPD_Intr_Handler(void *InstancePtr)
{
	/******************************************************************
	 * The interrupt handler performs the following actions:
	 *
	 * STEP 1: Disable the interrupt
	 * STEP 2: Service the interrupt
	 * STEP 3: Clear the interrupt
	 * STEP 4: Enable the interrupt
	 *
	 *******************************************************************/

	int led_data;


	/* STEP 1: Disable Gpio Ch2 interrupts */
	XGpio_InterruptDisable(&Gpio, KYPD_INT_MASK);

	/* Ignore additional button presses in Ch2 */
	if ((XGpio_InterruptGetStatus(&Gpio) & KYPD_INT_MASK) !=
			KYPD_INT_MASK) {
			return;
		}

	/* STEP 2: Scan all columns, identify key pressed, and light the leds */
	led_data = KYPD_Scan();
	XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, led_data);


    /* STEP 3: Clear the interrupt flag in Gpio Ch2*/
    (void)XGpio_InterruptClear(&Gpio, KYPD_INT_MASK);

    /* STEP 4: Enable GPIO interrupts in Gpio Ch2 */
    XGpio_InterruptEnable(&Gpio, KYPD_INT_MASK);
}

/* Keyboard scan function implementation*/
int KYPD_Scan(void)
{
	int cols = 0xe; 	/* Create variable to sweep columns */
	int rows = 0x0;   /* Create a variable to scan rows */
	int cols_msb = 0x0;

	int i;
	int key = 0x0;

	for(i=4;i>0;i--){
		/* Write output to the Columns */
		XGpio_DiscreteWrite(&Gpio, KEYPAD_CHANNEL, cols);

		// Read the rows
		rows = XGpio_DiscreteRead(&Gpio, KEYPAD_CHANNEL);

		// flush the 4 least significant bits
		rows = rows >> 4;
	cols = cols & 0x0000000f;
	switch((cols)){
				case 0xe:
						switch(rows){
							case 0xf:
								key = 0x10;
								break;
							case 0xe:
								key = 0xd;
								break;
							case 0xd:
								key = 0xc;
								break;
							case 0xb:
								key = 0xb;
								break;
							case 0x7:
								key = 0xa;
								break;
							} break;

						case 0xd:
							switch(rows){
							case 0xf:
								key = 0x10;
								break;
							case 0xe:
								key = 0xe;
								break;
							case 0xd:
								key = 0x9;
								break;
							case 0xb:
								key = 0x6;
								break;
							case 0x7:
								key = 0x3;
								break;
							} break;
						case 0xb:
							switch(rows){
							case 0xf:
								key = 0x10;
								break;
							case 0xe:
								key = 0xf;
								break;
							case 0xd:
								key = 0x8;
								break;
							case 0xb:
								key = 0x5;
								break;
							case 0x7:
								key = 0x2;
								break;
							} break;
						case 0x7:
							switch(rows){
							case 0xf:
								key = 0x10;
								break;
							case 0xe:
								key = 0x0;
								break;
							case 0xd:
								key = 0x7;
								break;
							case 0xb:
								key = 0x4;
								break;
							case 0x7:
								key = 0x1;
								break;
							} break;
				}

		if(key!=0x10){
			return key;
		}
		/* Shift the '0' in the cols to the left */
		cols_msb = (cols >> 3) & 1;  // Saving the msb of cols
		cols = (cols << 1) | cols_msb; // rotate the 4 bit so cols to the left
	} /* end of for loop*/
} /* end of KYPD_Scan */
void Delay(void){
	int counter = 50000;
	while(counter>0)
		counter -= counter;
}
