// DS18x20 1-Wire Digital Thermometer
// 
//#define OneWirePort	PORTB
//#define OneWireDDR	DDRB
//#define OneWireIN		PINB
//#define OneWire		(1<<PORTB0)
#define OneWireRead		0xFF

uint8_t OneWire_CheckSumBuf;

void Delay100us(uint8_t ms)
{
	while(ms-- > 0) { _delay_us(100); }
}

uint8_t OneWire_Reset(void) // 0 - Ok
{
	OneWireDDR |= OneWire;   // Out
	OneWirePort &= ~OneWire; // Reset (Low)
    Delay100us(5);
    cli();
    OneWireDDR &= ~OneWire;  // In
    Delay100us(1);
    uint8_t i = OneWireIN & OneWire;
    sei();
    Delay100us(4);
	return i;
}

// Calc check sum. Before run clear global var: OneWire_CheckSumBuf
// If all ok OneWire_CheckSumBuf == 0 after precessed all bytes + CRC.
void OneWire_CheckSum(uint8_t data)
{
	uint8_t tmp;
		
	for(uint8_t i = 0; i < 8; i++)
	{
		if ((data ^ OneWire_CheckSumBuf) & 1) {
			OneWire_CheckSumBuf ^= 0x18;
			tmp = 0x80;
		} else
			tmp = 0;
		OneWire_CheckSumBuf >>= 1;
		OneWire_CheckSumBuf |= tmp;
		data >>= 1;
	}
}

// Read/Write 1Wire. If Out == 0xFF - Reading.
uint8_t OneWire_ByteIO(uint8_t OutB)
{
	uint8_t InB = 0;

	for (uint8_t i = 0; i < 8; i++)
	{
		InB >>= 1;
		cli();
		OneWireDDR |= OneWire;                // Out, Low
		_delay_us(5);
		if(OutB & 1) {
			// Write 1 or read
			OneWireDDR &= ~OneWire;            // In, Hi
			_delay_us(10);
			
			if(OneWireIN & OneWire) InB |= 0x80;  // Bit == 1
			sei();
		}		
		_delay_us(45);
		if(!(OutB & 1)) {
			// Write 0
			OneWireDDR &= ~OneWire;            // In, Hi
			sei();
			_delay_us(10);
		}
		OutB >>= 1;
	}
	OneWire_CheckSum(InB);
	return InB;
}


int16_t DS18X20_ReadTempSingle(void) // Return 2 BYTE: -550..1250 C (-55..125 * 10), Error: 0x8000..0x8004
{
	int16_t T;
	
	if(OneWire_Reset()) return 0x8001;
	OneWire_ByteIO(0xCC);  // SKIP ROM - SingeDevice
	OneWire_ByteIO(0x44);  // Convert temp
	uint8_t i = 0;
	while(!(OneWire_ByteIO(OneWireRead))) // Wait while 0
	{  
		if(++i == 0) return 0x8002; // Error timeout
		Delay100us(100);
		wdt_reset();
	}
	if(OneWire_Reset()) return 0x8003; // Error - line busy
	OneWire_ByteIO(0xCC);  // SKIP ROM - SingeDevice
	OneWire_ByteIO(0xBE);  // Read SCRATCHPAD
	OneWire_CheckSumBuf = 0;
	T = OneWire_ByteIO(OneWireRead) | (OneWire_ByteIO(OneWireRead) * 256);
	for(i = 0; i < 7; i++) OneWire_ByteIO(OneWireRead); // Skip 7 bytes
	if(OneWire_CheckSumBuf) 
		return 0x8004;  // Error - incorrect checksum
	else 
		return (T / 2 + T / 8);
}

int8_t DS18X20_ReadSerialSingle(uint8_t buf[]) // Read 8 bytes (last CRC), Return 0 - Ok, Error: 0x01, 0x04
{
	if(OneWire_Reset()) return 0x01; // Error busy
	OneWire_ByteIO(0xCC);  // SKIP ROM - SingeDevice
	OneWire_ByteIO(0x33);  // Read ROM
	OneWire_CheckSumBuf = 0;
	for(uint8_t i = 0; i < 8; i++) buf[i] = OneWire_ByteIO(OneWireRead);
	if(OneWire_CheckSumBuf) return 0x04; else return 0;
}

int8_t DS18X20_ReadMemSingle(uint8_t buf[]) // Read 9 bytes (last CRC), Return 0 - Ok, Error: 0x01, 0x04
{
	if(OneWire_Reset()) return 0x01; // Error busy
	OneWire_ByteIO(0xCC);  // SKIP ROM - SingeDevice
	OneWire_ByteIO(0xBE);  // Read SCRATCHPAD
	OneWire_CheckSumBuf = 0;
	for(uint8_t i = 0; i < 9; i++) buf[i] = OneWire_ByteIO(OneWireRead);
	if(OneWire_CheckSumBuf) return 0x04; else return 0;
}