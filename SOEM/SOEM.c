#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "stm32f7xx_hal.h"

// local RX buffer
#define FRAME_LENGTH_MAX 1536
static unsigned char ethernetFrameBuffer[FRAME_LENGTH_MAX];

// Ethernet peripheral handle
//static ETH_HandleTypeDef EthHandle;
extern ETH_HandleTypeDef heth;
// Ethernet Rx DMA Descriptor
extern ETH_DMADescTypeDef DMARxDscrTab[ETH_RXBUFNB];
// Ethernet Tx DMA Descriptor
extern ETH_DMADescTypeDef DMATxDscrTab[ETH_TXBUFNB];
// Ethernet Receive Buffer
extern uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE];
// Ethernet Transmit Buffer
extern uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE];

// (1) open
// return: 0=SUCCESS / 1=FAILED
int hal_ethernet_open(void)
{
	printf("hal_ethernet_open...\n");

	// Disable interrupt
	NVIC_DisableIRQ(ETH_IRQn);
	
	// Initialize Tx Descriptors list: Chain Mode
	HAL_ETH_DMATxDescListInit(&heth, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
	// Initialize Rx Descriptors list: Chain Mode
	HAL_ETH_DMARxDescListInit(&heth, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);
	// Enable MAC and DMA transmission and reception
	HAL_ETH_Start(&heth);

	return 0;
}

// (2) close
void hal_ethernet_close(void)
{
	// Disable interrupt
	NVIC_DisableIRQ(ETH_IRQn);
	// Disable MAC and DMA transmission and reception
	HAL_ETH_Stop(&heth);
}

// (3) send
// return: 0=SUCCESS (!!! not sent size)
int hal_ethernet_send(unsigned char *data, int len)
{
	int ret = 0;
	// Is this buffer available?
	if ((heth.TxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET){
		printf("hal_ethernet_send error: DMA Busy\n");
		ret = -1;
	}
	else{
		// Copy data to Tx buffer
		uint8_t* buffer = (uint8_t*)(heth.TxDesc->Buffer1Addr);
		memcpy (buffer, data, len);

		// Prepare transmit descriptors to give to DMA
		HAL_ETH_TransmitFrame(&heth, len);
	}

	// When Transmit Underflow flag is set,
	// clear it and issue a Transmit Poll Demand to resume transmission
	if ((heth.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET) {
		// Clear TUS ETHERNET DMA flag
		heth.Instance->DMASR = ETH_DMASR_TUS;
		// Resume DMA transmission
		heth.Instance->DMATPDR = 0;
	}
	return ret;
}

// (4) receive
// return: received size
int hal_ethernet_recv(unsigned char **data)
{
	// get received frame
	if (HAL_ETH_GetReceivedFrame(&heth) != HAL_OK) {
		//printf("HAL_ETH_GetReceivedFrame ERROR!\n");
		return 0; // no packet received
	}
	// Obtain the size of the packet
	uint16_t len = heth.RxFrameInfos.length;
	if(len <= 0){
		printf("hal_ethernet_recv: No Data or Error\n");
		return 0;
	}
	if(len > FRAME_LENGTH_MAX){
		printf("hal_ethernet_recv: Too long data! (%d byte)\n", len);
		len = FRAME_LENGTH_MAX;
	}
	//printf("hal_ethernet_recv: received data! (%d byte)\n", len);
	
	// DMA buffer pointer
	uint8_t* buffer = (uint8_t*)heth.RxFrameInfos.buffer;
	// Copy data to my buffer
	memcpy(ethernetFrameBuffer, buffer, len);
	// Release descriptors to DMA
	heth.RxFrameInfos.FSRxDesc->Status |= ETH_DMARXDESC_OWN;
	// Clear Segment_Count
	heth.RxFrameInfos.SegCount = 0;
	// When Rx Buffer unavailable flag is set: clear it and resume reception
	if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET) {
		// Clear RBUS ETHERNET DMA flag
		heth.Instance->DMASR = ETH_DMASR_RBUS;
		// Resume DMA reception
		heth.Instance->DMARPDR = 0;
	}
	// received data
	*data = ethernetFrameBuffer;
	return len;
}

#ifdef __cplusplus
}
#endif
