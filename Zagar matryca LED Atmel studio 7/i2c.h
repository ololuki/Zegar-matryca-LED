/*Biblioteka programowej obs³ugi uproszczonego interfejsu I2C
  Procesor w roli jedynego uk³adu Master
  Wykorzystywane wewnêtrzne rezystory podci¹gaj¹ce
	I2C_EXTERNAL_PULLUP 0
  Przy zastosowaniu zewnêtrznych pull-upów mozliwa optymalizacja kodu 
	I2C_EXTERNAL_PULLUP 1
  Na pocz¹tku programu wywo³aæ i2c_init()
  Kompilator: AVR-GCC
  Autor: Ololuki
  Na podstawie: Elektronika dla wszystkich grudzieñ 2005
  Wersja: 20110109
*/

#ifndef I2C_H_INCLUDED
#define I2C_H_INCLUDED

//makra konfiguracji sprzêtowej interfejsu
#define I2C_SDAPORT A
#define I2C_SDA 0
#define I2C_SCLPORT A
#define I2C_SCL 1
// Prêdkoœæ transmisji w b/s
#define I2C_SPEED 400000
//zewnetrzne pullupy - u¿ycie pozwala zmniejszyæ kod; 1 - w³¹czone 0 - wy³¹czone, u¿yte wewnêtrzne
#define I2C_EXTERNAL_PULLUP 1
#define I2C_ACK 1
#define I2C_NACK 0

//makra upraszczaj¹ce dostêp do portów
#define PORT(x) XPORT(x)
#define XPORT(x) (PORT##x)
#define PIN(x) XPIN(x)
#define XPIN(x) (PIN##x)
#define DDR(x) XDDR(x)
#define XDDR(x) (DDR##x)

//makro upraszczaj¹ce wywo³anie instrukcji nop
#define NOP() {asm volatile("nop"::);}

//inicjalizacja portów i2c - na pocz¹tku programu - w³¹czanie podciagania, ustawienie kierunku portów
inline void i2c_init(void)
{
	//DDR(I2C_SDAPORT) |= (1 << I2C_SDA);
	PORT(I2C_SDAPORT) &= ~(1 << I2C_SDA);
	DDR(I2C_SCLPORT) |= (1 << I2C_SCL);
	//PORT(I2C_SCLPORT) &= ~(1 << I2C_SCL);
}
void i2c_start(void);  //rozpoczyna transmisjê I2C
void i2c_stop(void);  //koñczy transmisjê I2C
uint8_t i2c_send(uint8_t data);  //wysy³a bajt danych, zwraca ack
uint8_t i2c_get(uint8_t ack);  //odbiera i zwraca bajt danych, wysy³a ack

#endif //I2C_H_INCLUDED