/*
 * SIM900TCPClient.c
 * http://www.electronicwings.com
 */

#include "SIM900TCPClient.h"		/* Include TCP Client header file */

int8_t Response_Status, CRLF_COUNT = 0;
volatile int16_t Counter = 0, pointer = 0;
uint32_t TimeOut = 0;

void Read_Response()
{
	char CRLF_BUF[2];
	char CRLF_FOUND;
	uint32_t TimeCount = 0, ResponseBufferLength;
	while(1)
	{
		if(TimeCount >= (DEFAULT_TIMEOUT+TimeOut))
		{
			CRLF_COUNT = 0; TimeOut = 0;
			Response_Status = SIM900_RESPONSE_TIMEOUT;
			return;
		}

		if(Response_Status == SIM900_RESPONSE_STARTING)
		{
			CRLF_FOUND = 0;
			memset(CRLF_BUF, 0, 2);
			Response_Status = SIM900_RESPONSE_WAITING;
		}
		ResponseBufferLength = strlen(RESPONSE_BUFFER);
		if (ResponseBufferLength)
		{
			_delay_ms(1);
			TimeCount++;
			if (ResponseBufferLength==strlen(RESPONSE_BUFFER))
			{
				for (uint16_t i=0;i<ResponseBufferLength;i++)
				{
					memmove(CRLF_BUF, CRLF_BUF + 1, 1);
					CRLF_BUF[1] = RESPONSE_BUFFER[i];
					if(!strncmp(CRLF_BUF, "\r\n", 2))
					{
						if(++CRLF_FOUND == (DEFAULT_CRLF_COUNT+CRLF_COUNT))
						{
							CRLF_COUNT = 0; TimeOut = 0;
							Response_Status = SIM900_RESPONSE_FINISHED;
							return;
						}
					}
				}
				CRLF_FOUND = 0;
			}
		}
		_delay_ms(1);
		TimeCount++;
	}
}

void TCPClient_Clear()
{
	memset(RESPONSE_BUFFER,0,DEFAULT_BUFFER_SIZE);
	Counter = 0;	pointer = 0;
}

void Start_Read_Response()
{
	Response_Status = SIM900_RESPONSE_STARTING;
	do {
		Read_Response();
	} while(Response_Status == SIM900_RESPONSE_WAITING);

}

void GetResponseBody(char* Response, uint16_t ResponseLength)
{

	uint16_t i = 12;
	char buffer[5];
	while(Response[i] != '\r')
	++i;

	strncpy(buffer, Response + 12, (i - 12));
	ResponseLength = atoi(buffer);

	i += 2;
	uint16_t tmp = strlen(Response) - i;
	memcpy(Response, Response + i, tmp);

	if(!strncmp(Response + tmp - 6, "\r\nOK\r\n", 6))
	memset(Response + tmp - 6, 0, i + 6);
}

bool WaitForExpectedResponse(char* ExpectedResponse)
{
	TCPClient_Clear();
	_delay_ms(200);
	Start_Read_Response();						/* First read response */
	if((Response_Status != SIM900_RESPONSE_TIMEOUT) && (strstr(RESPONSE_BUFFER, ExpectedResponse) != NULL))
	return true;							/* Return true for success */
	return false;								/* Else return false */
}

bool SendATandExpectResponse(char* ATCommand, char* ExpectedResponse)
{
	USART_SendString(ATCommand);				/* Send AT command to SIM900 */
	USART_TxChar('\r');
	return WaitForExpectedResponse(ExpectedResponse);
}

bool TCPClient_ApplicationMode(uint8_t Mode)
{
	char _buffer[20];
	sprintf(_buffer, "AT+CIPMODE=%d\r", Mode);
	_buffer[19] = 0;
	USART_SendString(_buffer);
	return WaitForExpectedResponse("OK");
}

bool TCPClient_ConnectionMode(uint8_t Mode)
{
	char _buffer[20];
	sprintf(_buffer, "AT+CIPMUX=%d\r", Mode);
	_buffer[19] = 0;
	USART_SendString(_buffer);
	return WaitForExpectedResponse("OK");
}

bool AttachGPRS()
{
	USART_SendString("AT+CGATT=1\r");
	return WaitForExpectedResponse("OK");
}

bool SIM900_Start()
{
	for (uint8_t i=0;i<5;i++)
	{
		if(SendATandExpectResponse("ATE0","OK")||SendATandExpectResponse("AT","OK"))
		return true;
	}
	return false;
}

bool TCPClient_Shut()
{
	USART_SendString("AT+CIPSHUT\r");
	return WaitForExpectedResponse("OK");
}

bool TCPClient_Close()
{
	USART_SendString("AT+CIPCLOSE=1\r");
	return WaitForExpectedResponse("OK");
}

bool TCPClient_Connect(char* _APN, char* _USERNAME, char* _PASSWORD)
{

	USART_SendString("AT+CREG?\r");
	if(!WaitForExpectedResponse("+CREG: 0,1"))
	return false;

	USART_SendString("AT+CGATT?\r");
	if(!WaitForExpectedResponse("+CGATT: 1"))
	return false;

	USART_SendString("AT+CSTT=\"");
	USART_SendString(_APN);
	USART_SendString("\",\"");
	USART_SendString(_USERNAME);
	USART_SendString("\",\"");
	USART_SendString(_PASSWORD);
	USART_SendString("\"\r");
	if(!WaitForExpectedResponse("OK"))
	return false;

	USART_SendString("AT+CIICR\r");
	if(!WaitForExpectedResponse("OK"))
	return false;

	USART_SendString("AT+CIFSR\r");
	if(!WaitForExpectedResponse("."))
	return false;

	USART_SendString("AT+CIPSPRT=1\r");
	return WaitForExpectedResponse("OK");
}

bool TCPClient_connected() {
	USART_SendString("AT+CIPSTATUS\r");
	CRLF_COUNT = 2;
	return WaitForExpectedResponse("CONNECT OK");
}

uint8_t TCPClient_Start(uint8_t _ConnectionNumber, char* Domain, char* Port)
{
	char _buffer[25];
	USART_SendString("AT+CIPMUX?\r");
	if(WaitForExpectedResponse("+CIPMUX: 0"))
	USART_SendString("AT+CIPSTART=\"TCP\",\"");
	else
	{
		sprintf(_buffer, "AT+CIPSTART=\"%d\",\"TCP\",\"", _ConnectionNumber);
		USART_SendString(_buffer);
	}
	
	USART_SendString(Domain);
	USART_SendString("\",\"");
	USART_SendString(Port);
	USART_SendString("\"\r");

	CRLF_COUNT = 2;
	if(!WaitForExpectedResponse("CONNECT OK"))
	{
		if(Response_Status == SIM900_RESPONSE_TIMEOUT)
		return SIM900_RESPONSE_TIMEOUT;
		return SIM900_RESPONSE_ERROR;
	}
	return SIM900_RESPONSE_FINISHED;
}

uint8_t TCPClient_Send(char* Data, uint16_t _length)
{
	USART_SendString("AT+CIPSEND\r");
	CRLF_COUNT = -1;
	WaitForExpectedResponse(">");
	
	for (uint16_t i = 0; i < _length; i++)
	USART_TxChar(Data[i]);
	USART_TxChar(0x1A);

	if(!WaitForExpectedResponse("SEND OK"))
	{
		if(Response_Status == SIM900_RESPONSE_TIMEOUT)
		return SIM900_RESPONSE_TIMEOUT;
		return SIM900_RESPONSE_ERROR;
	}
	return SIM900_RESPONSE_FINISHED;
}

int16_t TCPClient_DataAvailable()
{
	return (Counter - pointer);
}

uint8_t TCPClient_DataRead()
{
	if(pointer<Counter)
	return RESPONSE_BUFFER[pointer++];
	else{
		TCPClient_Clear();
		return 0;
	}
}

ISR (USART1_RX_vect)
{
	uint8_t oldsrg = SREG;
	RESPONSE_BUFFER[Counter] = UDR1;
	Counter++;
	if(Counter == DEFAULT_BUFFER_SIZE){
		Counter = 0; pointer = 0;
	}
	SREG = oldsrg;
}
