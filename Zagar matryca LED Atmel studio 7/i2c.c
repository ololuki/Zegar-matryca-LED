/*Biblioteka programowej obs�ugi uproszczonego interfejsu I2C
  Procesor w roli jedynego uk�adu Master
  Wykorzystywane wewn�trzne rezystory podci�gaj�ce
	I2C_EXTERNAL_PULLUP 0
  Przy zastosowaniu zewn�trznych pull-up�w mozliwa optymalizacja kodu 
	I2C_EXTERNAL_PULLUP 1
  Na pocz�tku programu wywo�a� i2c_init()
  Kompilator: AVR-GCC
  Autor: Ololuki
  Na podstawie: Elektronika dla wszystkich grudzie� 2005
  Wersja: 20110109
*/

#include <avr/io.h>
#include "i2c.h"

//--------------------------------------------------------------------------------------------------------
//
//   Funkcje op�nie� po��wkowych
//
//--------------------------------------------------------------------------------------------------------

#define I2C_nhalf (F_CPU/I2C_SPEED/2)

// Funkcja d�u�szych op�nie�
#if I2C_nhalf < 3
	//nic
#elif I2C_nhalf < 8
	static void i2c_xdelay(void)
	{
		NOP();
	}
#else //I2C_nhalf >= 8
	#define I2C_delayloops (1+(I2C_nhalf-8)/3)
	#if I2C_delayloops > 255
		#error Zwi�ksz pr�dko�� transmisji I2C lub zmiejsz F_CPU
	#endif
	static void i2c_xdelay(void)
	{
		asm volatile( \
			"delayus8_loop%=: \n\t"\
			"dec %[ticks] \n\t"\
			"brne delayus8_loop%= \n\t"\
		: :[ticks]"r"(I2C_delayloops) );
	}
#endif //I2C_nhalf

//Op�nienia dla I2C
static inline void i2c_hdelay(void)
{
#if I2C_nhalf < 1
	return;
#elif I2C_nhalf < 2
	NOP();
#elif I2C_nhalf < 3
	asm volatile(
		"rjmp exit%=\n\t"
		"exit%=:\n\t"::);
#else //I2C_nhalf >= 3
	i2c_xdelay();
#endif
}

//--------------------------------------------------------------------------------------------------------
//
//   Funkcje zmieniaj�ce stan na wyprowadzeniach SDA i SCL
//
//--------------------------------------------------------------------------------------------------------

// Ustawienie i zerowanie wyj�cia danych(SDA)
static inline void i2c_sdaset(void)
{
	DDR(I2C_SDAPORT) &= ~(1<<I2C_SDA); 	//wej�cie
	#if I2C_EXTERNAL_PULLUP < 1 			//tylko przy wewn�trznym pullupie
	PORT(I2C_SDAPORT) |= (1<<I2C_SDA); 	//zewn�trzny pullup zadba o stan wysoki
	#endif
}

static inline void i2c_sdaclear(void)
{
	#if I2C_EXTERNAL_PULLUP < 1 			//tylko przy wewn�trznym pullupie
	PORT(I2C_SDAPORT) &= ~(1<<I2C_SDA); 	//u�ywaj�c zewnetrznego pullupa zawsze PORT = 0
	#endif
	DDR(I2C_SDAPORT) |= (1<<I2C_SDA);		//wyj�cie
}

// Pobieranie danej z wyprowadzenia portu
static inline uint8_t i2c_sdaget(void)
{
	return PIN(I2C_SDAPORT) & (1<<I2C_SDA);
}

// Zerowanie i ustawianie wyj�cia zegara(SCL)
static inline void i2c_sclset(void)
{
	PORT(I2C_SCLPORT) |= (1<<I2C_SCL);
}
static inline void i2c_sclclear(void)
{
	PORT(I2C_SCLPORT) &= ~(1<<I2C_SCL);
}

//--------------------------------------------------------------------------------------------------------
//
//   Funkcje zewn�trzne - inicjuj�ce i ko�cz�ce transmisj� oraz wysy�aj�ce i odbieraj�ce bajt danych
//
//--------------------------------------------------------------------------------------------------------

void i2c_start(void)
{
	#if I2C_EXTERNAL_PULLUP > 0				//tylko przy zewn�trznym pullupie
	PORT(I2C_SDAPORT) &= ~(1<<I2C_SDA); 	//u�ywaj�c zewnetrznego pullupa zawsze PORT = 0
	#endif
	// Je�li start bez stop
	i2c_sdaset();
	i2c_hdelay();
	i2c_sclset();
	i2c_hdelay();
	// Normalna sekwencja startu
	i2c_sdaclear();
	i2c_hdelay();
	i2c_sclclear();
}

void i2c_stop(void)
{
	i2c_sdaclear();
	i2c_hdelay();
	i2c_sclset();
	i2c_hdelay();
	i2c_sdaset();
	i2c_hdelay();
}

// Wys�anie znaku - zwraca ack
uint8_t i2c_send(uint8_t data)
{
	uint8_t n;
	
	for(n=8; n>0; --n)
	{
		if(data & 0x80)
			i2c_sdaset();
		else
			i2c_sdaclear();
		data <<= 1;
		i2c_hdelay();
		i2c_sclset();
		i2c_hdelay();
		i2c_sclclear();
	}
	// ack
	i2c_sdaset();
	i2c_hdelay();
	i2c_sclset();
	i2c_hdelay();
	n = i2c_sdaget();
	i2c_sclclear();
	
	return n;
}

//odebranie znaku
uint8_t i2c_get(uint8_t ack)
{
	uint8_t n, temp = 0;
	
	i2c_sdaset();
	for(n=8; n>0; --n)
	{
		i2c_hdelay();
		i2c_sclset();
		i2c_hdelay();
		temp <<= 1;
		if(i2c_sdaget())
			temp++;
		i2c_sclclear();
	}
	// ack
	if(ack == I2C_ACK)
		i2c_sdaclear();
	else
		i2c_sdaset();
	i2c_hdelay();
	i2c_sclset();
	i2c_hdelay();
	i2c_sclclear();
	
	return temp;
}

