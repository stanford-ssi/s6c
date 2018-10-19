#include "SerialHeader.h"

Uart SerialHeader(&sercom0, HEADER_RX, HEADER_TX, SERCOM_RX_PAD_2, UART_TX_PAD_0);

void SERCOM0_Handler()
{
  SerialHeader.IrqHandler();
}
