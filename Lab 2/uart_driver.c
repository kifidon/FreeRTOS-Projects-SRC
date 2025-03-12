/*
 * uart_driver.c
 * Created on: July 27, 2021
 * Author: Shyama M. Gandhi
 * Modified by : Antonio Andara
 * Modified on : January 24, 2023
 * Modified on : January 17, 2025
 * Implementation of UART driver functions declared in uart_driver.h
 */

#include "uart_driver.h"
#include "task.h"

/*
 * Be sure to protect any critical sections into these four functions by using the FreeRTOS macros
 * taskENTER_CRITICAL() and taskEXIT_CRITICAL().
 */

// Function definitions
void interruptHandler(void *CallBackRef, u32 event, unsigned int EventData)
{
	if (event == RECEIVED_DATA){
    	handleReceiveEvent();
    } else if (event == SENT_DATA){
    	handleSentEvent();
    } else {
    	xil_printf("Neither a RECEIVE event nor a SEND event\n");
    }
}


void handleReceiveEvent()
{
    u8 receive_buffer;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		countRxIrq ++;
		while (XUartPs_IsReceiveData(UART_BASEADDR)){
			receive_buffer = XUartPs_ReadReg( UART_BASEADDR, UART_FIFO_OFFSET);
			xQueueSendFromISR( xRxQueue, &receive_buffer, &xHigherPriorityTaskWoken);
		}
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void handleSentEvent()
{
    u8 transmit_data;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	countTxIrq++;
    transmitDataFromQueue(&transmit_data, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    disableTxEmpty();
}


void transmitDataFromQueue(u8 *data, BaseType_t *taskToSwitch)
{
	int counter = 0;
	if (XUartPs_IsTransmitEmpty(&UART)){
	    while (uxQueueMessagesWaitingFromISR(xTxQueue) > 0 ){
/*************************** Enter your code here ****************************/
	// TODO: receive data from the queue and send it through the UART interface
	    	 if (xQueueReceiveFromISR(xTxQueue, data, taskToSwitch) == pdTRUE) {
				XUartPs_WriteReg(UART_BASEADDR, UART_FIFO_OFFSET, *data);
				counter++;
			}
/*****************************************************************************/
	    }
	}
}


void disableTxEmpty()
{
    u32 mask;
    if (uxQueueMessagesWaitingFromISR(xTxQueue) <= 0){
        mask = XUartPs_GetInterruptMask(&UART);
/*************************** Enter your code here ****************************/
		// TODO: set the interrupt mask
        mask |= XUARTPS_IXR_TXEMPTY;
        XUartPs_WriteReg(UART_BASEADDR, XUARTPS_IDR_OFFSET, mask);
        mask &= ~XUARTPS_IXR_TXEMPTY;
		XUartPs_WriteReg(UART_BASEADDR, XUARTPS_IER_OFFSET, mask);
/*****************************************************************************/
    }
}


void enableTxEmpty()
{
    u32 mask;
/*************************** Enter your code here ****************************/
    // TODO: set the enable mask
		mask |= XUARTPS_IXR_TXEMPTY;
		XUartPs_WriteReg(UART_BASEADDR, XUARTPS_IER_OFFSET, mask);
		mask &= ~XUARTPS_IXR_TXEMPTY;
		XUartPs_WriteReg(UART_BASEADDR, XUARTPS_IDR_OFFSET, mask);
/*****************************************************************************/
}


BaseType_t myReceiveData(void)
{
	if (uxQueueMessagesWaiting( xRxQueue ) > 0){
		return pdTRUE;
	} else {
		return pdFALSE;
	}
}


u8 myReceiveByte(void)
{
	u8 recv = 0;
/*************************** Enter your code here ****************************/
	// TODO: receive a byte from the queue
	if (uxQueueMessagesWaiting(xRxQueue) > 0) {
		// Dequeue data from the RX queue
		xQueueReceive(xRxQueue, &recv, portMAX_DELAY);
	}
/*****************************************************************************/
	return recv;
}


BaseType_t myTransmitFull(void)
{
	if (uxQueueSpacesAvailable(xTxQueue) <= 0){
		return pdTRUE;
	} else {
		return pdFALSE;
	}
}


void mySendByte(u8 data)
{
/*************************** Enter your code here ****************************/
	// TODO: send data through UART
    taskENTER_CRITICAL();
	disableTxEmpty();
	XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, data);
		if(data=='\r'){
			enableTxEmpty();
		}
    taskEXIT_CRITICAL();
/*****************************************************************************/
}


void mySendString(const char pString[])
{
	int i = 0;
    taskENTER_CRITICAL();
	disableTxEmpty();
    while (pString[i] != '\0'){
		XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, pString[i]);
		i++;
	}
	enableTxEmpty();
    taskEXIT_CRITICAL();
}


int initializeUART(void)
{
	int status;

	Config = XUartPs_LookupConfig(UART_DEVICE_ID);
	if (NULL == Config){
		return XST_FAILURE;
		xil_printf("UART PS Config failed\n");
	}

	//Initialize UART
	status = XUartPs_CfgInitialize( &UART, Config, Config->BaseAddress);

	if (status != XST_SUCCESS){
		return XST_FAILURE;
		xil_printf("UART PS init failed\n");
	}
	return XST_SUCCESS;
}


int setupInterruptSystem(INTC *IntcInstancePtr, XUartPs *UartInstancePtr, u16 UartIntrId)
{
	int status;
	XScuGic_Config *IntcConfig; // Config pointer for interrupt controller

	//Lookup the config information for interrupt controller
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig){
		return XST_FAILURE;
	}

	//Initialize interrupt controller
	status = XScuGic_CfgInitialize( IntcInstancePtr, IntcConfig, IntcConfig->CpuBaseAddress);

	if (status != XST_SUCCESS){
		return XST_FAILURE;
	}

	//Connect the interrupt controller interrupt handler
	Xil_ExceptionRegisterHandler( XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler) XScuGic_InterruptHandler, IntcInstancePtr);

	//Connect the PS UART interrupt handler
	//The interrupt handler which handles the interrupts for the UART peripheral is connected to it's unique ID number (82 in this case)
	status = XScuGic_Connect( IntcInstancePtr, UartIntrId, (Xil_ExceptionHandler) XUartPs_InterruptHandler, (void *) UartInstancePtr);

	if (status != XST_SUCCESS){
		return XST_FAILURE;
	}

	//Enable the UART interrupt input on the interrupt controller
	XScuGic_Enable(IntcInstancePtr, UartIntrId);

	//Enable the processor interrupt handling on the ARM processor
	Xil_ExceptionEnable();


	//Setup the UART Interrupt handler function
	XUartPs_SetHandler( UartInstancePtr, (XUartPs_Handler)interruptHandler, UartInstancePtr);

	//Create mask for UART interrupt, Enable the interrupt when the receive buffer has reached a particular threshold
	IntrMask = XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY  | XUARTPS_IXR_FRAMING |
	           XUARTPS_IXR_OVER | XUARTPS_IXR_TXEMPTY | XUARTPS_IXR_RXFULL  |
	           XUARTPS_IXR_RXOVR;

	//Setup the UART interrupt Mask
	XUartPs_SetInterruptMask(UartInstancePtr, IntrMask);

	//Setup the PS UART to Work in Normal Mode
	XUartPs_SetOperMode(UartInstancePtr, XUARTPS_OPER_MODE_NORMAL);

	return XST_SUCCESS;
}
