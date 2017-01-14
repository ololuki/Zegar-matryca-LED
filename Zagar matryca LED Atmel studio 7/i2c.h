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

#ifndef I2C_H_INCLUDED
#define I2C_H_INCLUDED

//makra konfiguracji sprz�towej interfejsu
#define I2C_SDAPORT A
#define I2C_SDA 0
#define I2C_SCLPORT A
#define I2C_SCL 1
// Pr�dko�� transmisji w b/s
#define I2C_SPEED 400000
//zewnetrzne pullupy - u�ycie pozwala zmniejszy� kod; 1 - w��czone 0 - wy��czone, u�yte wewn�trzne
#define I2C_EXTERNAL_PULLUP 1
#define I2C_ACK 1
#define I2C_NACK 0

//makra upraszczaj�ce dost�p do port�w
#define PORT(x) XPORT(x)
#define XPORT(x) (PORT##x)
#define PIN(x) XPIN(x)
#define XPIN(x) (PIN##x)
#define DDR(x) XDDR(x)
#define XDDR(x) (DDR##x)

//makro upraszczaj�ce wywo�anie instrukcji nop
#define NOP() {asm volatile("nop"::);}

//inicjalizacja port�w i2c - na pocz�tku programu - w��czanie podciagania, ustawienie kierunku port�w
inline void i2c_init(void)
{
	//DDR(I2C_SDAPORT) |= (1 << I2C_SDA);
	PORT(I2C_SDAPORT) &= ~(1 << I2C_SDA);
	DDR(I2C_SCLPORT) |= (1 << I2C_SCL);
	//PORT(I2C_SCLPORT) &= ~(1 << I2C_SCL);
}
void i2c_start(void);  //rozpoczyna transmisj� I2C
void i2c_stop(void);  //ko�czy transmisj� I2C
uint8_t i2c_send(uint8_t data);  //wysy�a bajt danych, zwraca ack
uint8_t i2c_get(uint8_t ack);  //odbiera i zwraca bajt danych, wysy�a ack

#endif //I2C_H_INCLUDED