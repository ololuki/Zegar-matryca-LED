/*
	Zegar z matryc� LED + gra SNAKE
    Attiny2313 8MHz wewn�trzny oscylator
	Zegar PCF8563 na programowej magistrali I2c PORTA0 i PORTA1
	Matryca LED PortB katody diodprzez rezystory - wy�wietla kolejne linie
				PortD0,1,2 - anody przez dekoder i tranzystory - zapala kolejne linie do wy�wietlenia
	Przyciski - PortD3,4,5,6 - switche zwieraj� do masy, wewnetrzne rezystory podci�gaj�ce
    Autor: Ololuki
	www.ololuki.elektroda.eu
*/

#include <avr/io.h>
//#include <stdlib.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "i2c.h"

//adresy PCF8563
#define ADR_WRITE 0xA2
#define ADR_READ 0xA3
#define ADR_SEK 0x02
#define ADR_MIN 0x03
#define ADR_GODZ 0x04

// Porty steruj�ce matryc� LED
#define PORT_X PORTB
#define PORT_X_DDR DDRB
#define PORT_Y PORTD
#define PORT_Y_DDR DDRD

//klawiatura
#define PORT_BUTTON PIND
#define PIN_UP 3
#define PIN_DOWN 4
#define PIN_LEFT 6
#define PIN_RIGHT 5

#define Timer0_Start_64 TCCR0B = (1 << CS01) | (1 << CS00); //start timera0 z preskalerem /64
#define Timer0_Start_256 TCCR0B = (1 << CS02); //start timera0 z preskalerem /256
#define Enable_Timer0_OVF TIMSK |= (1 << TOIE0); //w��czenie przerwania od przepe�nienia timera0
#define Timer0_Stop TCCR0 =0; //zatrzymanie timera0



uint8_t cyfra[10][5] EEMEM =  { //czcionka cyfr
                                                    {0x3E,0x51,0x49,0x45,0x3E},		//0
                                                    {0x00,0x42,0x7F,0x40,0x00},		//1
                                                    {0x62,0x51,0x49,0x49,0x46},		//2
                                                    {0x22,0x49,0x49,0x49,0x36},		//3
                                                    {0x18,0x14,0x12,0x7F,0x10},		//4
                                                    {0x2F,0x49,0x49,0x49,0x31},		//5
                                                    {0x3C,0x4A,0x49,0x49,0x30},		//6
                                                    {0x01,0x71,0x09,0x05,0x03},		//7
                                                    {0x36,0x49,0x49,0x49,0x36},		//8
                                                    {0x06,0x49,0x49,0x29,0x1E},		//9
							};


#define CZAS_KLATKI 112 //czas wy�wietlania jednej klatki obrazu przy przesuwaniu
#define CZAS_CYFRY 225 // czas wyswietlania jednej cyfry przy ustawianiu zegara
#define LICZBA_GET_KBD 4 // liczba powt�rze� wywowo�ania get_kbd na jedno przej�cie g��wnej p�tli

uint8_t mode; //zmienna wyboru trybu
#define ZEGAR 0 //tryb zegara
#define SNAKE 1 //tryb snake
uint8_t mode_wait; //zmienna do odmierzania d�ugiego przycisniecia klawisza zmiany trybu (liczy kolejne wywo�ania get_kbd())
uint8_t kierunek = 0; //zmienna przechowuj�ca kierunek(zwi�ksz/zmniejsz) ustawiania cyfry, przechowuje kierunek poruszania sie g�owy w�a 0=stop
#define W_LEWO 1
#define W_PRAWO 2
#define W_GORE 3
#define W_DOL 4

volatile uint8_t linia_x[7]; // przechowuje kolejne linie wyswietlanego ekranu (bufor ekranu)
volatile uint8_t i; //iterator do wy�wietlania kolejnych linii w przerwaniu
unsigned char godzina_bcd[4]; //przechowuje kolejne cyfry czasu dzG jG dzM jM (bufor czasu)
unsigned char x,y; //x - numer kolejnej wy�wietlanej cyfry; y - numer kolejnej wyswietlanej kolumny w cyfrze
uint8_t ustaw = 0xFF; //zmienna przechowuj�ca numer ustawianej cyfry(godzina_bcd[0] = 1,godzina_bcd[1] = 2,godzina_bcd[2] = 3,godzina_bcd[3] = 4, normalny tryb = 0xFF)
uint8_t flaga; // do sprawdzania klawiatury


/////////////////Zmienne i sta�e dla gry Snake////////////////////////
uint8_t X[7];// = {0x06,0x07,0x09,0x09,0x09,0x09,0x09};  //tablaca wsp�rz�dnych X (X[0],Y[0] - wsp�rz�dne g�owy)
uint8_t Y[7];// = {0x03,0x03,0x09,0x09,0x09,0x09,0x09};  //tablica wsp�rz�dnych Y
uint8_t dlugosc=2; //d�ugo�� snake'a - mniejsza lub r�wna liczbie element�w tablic X oraz Y
#define MAX_DLUGOSC 7 //maksymalna dlugosc snake'a r�wna liczbie element�w tablic X oraz Y
uint8_t food_x=5;  //wsp�rz�dna x food
uint8_t food_y=2;  //wsp�rz�dna y food
uint8_t losowa; //losowa liczba, zwi�kszana w p�tli g��wnej, do ustawiania food
#define WIDTH 8 //szeroko�� wy�wietlacza w px
#define HEIGHT 7 //wysoko��wy�wietlacza w px

//pobranie czasu z RTC do bufora czasu, wykona sie po ka�dym przej�ciu wy�wietlanego ekranu
void get_time(void) 
{
		uint8_t minuty, godziny; //tymczasowe zmienne lokalne przechowuj�ce odczytan� z PCFa godzine w kodzie BCD
		
		i2c_start();
		i2c_send(ADR_WRITE);
		i2c_send(ADR_MIN);
		//i2c_stop();
		
		i2c_start();
		i2c_send(ADR_READ);
		//sekundy = (i2c_get(I2C_ACK));
		minuty = (i2c_get(I2C_ACK));
		godziny = (i2c_get(I2C_NACK));
		i2c_stop();

		godzina_bcd[0] = ((godziny >> 4) & 0x03);	//dziesi�tki godzin - starsza tetrada
		godzina_bcd[1] = (godziny & 0x0F);			//jedno�ci godzin - m�odsza tetrada

		godzina_bcd[2] = ((minuty >> 4) & 0x07);	//dziesi�tki minut - starsza tetrada
		godzina_bcd[3] = (minuty & 0x0F);			//jedno�ci minut - m�odsza tetrada
}     

//zapisywanie czasu do pcf8563
void save_time(void)
{
	i2c_start();
	i2c_send(ADR_WRITE);
	i2c_send(ADR_MIN);
	i2c_send((godzina_bcd[2] << 4) | godzina_bcd[3]);	//minuty
	i2c_send((godzina_bcd[0] << 4) | godzina_bcd[1]);	//godziny
	i2c_stop();	
}

//funkcja czyta klawiature i ustawia odpowiednio "ustaw" i "kierunek"
void get_kbd(void)
{
	if (mode == ZEGAR)
	{
/*
			if (bit_is_clear(PORT_BUTTON, PIN_LEFT))
			{
				if (++mode_wait == 60) //d�ugie przyci�ni�cie trwa 60*28ms = 1,6s
				{
					mode = SNAKE;
				}
			}else{
				mode_wait = 0;
			}
*/
		if (bit_is_clear(PORT_BUTTON, PIN_LEFT))
		{
			// aby ustawi� pierwsz� cyfr� przytrzyma� przycisk gdy ta zaczyna si� pojawia�
			// je�li godzina jest przewijana nie mo�na ustawia� godziny - mo�liwe jest to tylko wtedy, kiedy pierwsza cyfra "wsuwa si�" na ekran
			if (--flaga == 255) flaga = 0; //sprawdzenie czy funkcja zosta�a wywo�ana odpowiedni� ilo�� razy
			if (flaga == 0) { //je�li tak
				flaga = LICZBA_GET_KBD; //ponowne ustawienie flagi na ilo�� potrzebnych sprawdze� klawiatury przed zmian� ustawianej cyfry
				if (y == 0) ustaw++; //ustawianie kolejnej cyfry mozna w��czy� tylko gdy poprzednia jest statycznie wyswietlana
				if (x > 3) ustaw = 0xFF; // po przej�ciu ekranu naci�niecie przycisku zeruje mozliwo�� ustawiania
				if (x == 0 ) ustaw = 1;
				//if(ustaw == 5) ustaw = 0xFF;
				if(ustaw == 0 ) ustaw = 1;
			}
		}
	}else	
	//if (mode == SNAKE)
	{

				//_delay_ms(28); // *60
				//odczyt klawiatury
				//kierunek = 0; //sztop

	}
	//zmiana trybu zegar/snake po przytrzymaniu przycisku ok 1,6s
	if (bit_is_clear(PORT_BUTTON, PIN_RIGHT))
	{
		if (++mode_wait == 60) //d�ugie przyci�ni�cie trwa 60*28ms = 1,6s
		{
			mode = !mode;
			/////////////////////////////////////////////////////////////////////////
			//X[] = {0x06,0x07,0x09,0x09,0x09,0x09,0x09};  //tablaca wsp�rz�dnych X
			X[0] = 0x06;
			X[1] = 0x06;
			Y[0] = 0x02;
			Y[1] = 0x03;
			for (uint8_t pip=2; pip<7; ++pip)
			{
				X[pip] = 0x09;
				Y[pip] = 0x09;
			}
			kierunek = W_PRAWO;
			dlugosc=2;
			/*X[2] = 0x09;
			X[3] = 0x09;
			X[4] = 0x09;
			X[5] = 0x09;
			X[6] = 0x09;
			//Y[] = {0x03,0x03,0x09,0x09,0x09,0x09,0x09};  //tablica wsp�rz�dnych Y
			Y[2] = 0x09;
			Y[3] = 0x09;
			Y[4] = 0x09;
			Y[5] = 0x09;
			Y[6] = 0x09;*/
			/////////////////////////////////////////////////////////////////////////
		}
	}else{
		mode_wait = 0;
	}
	
	// pobieranie kierunku z klawiatury dla snake'a i dla zegara- g�ra d� przy ustawianiu
	if (bit_is_clear(PORT_BUTTON, PIN_UP)) {kierunek = W_GORE;} 
	if (bit_is_clear(PORT_BUTTON, PIN_DOWN)) {kierunek = W_DOL;}
	if (bit_is_clear(PORT_BUTTON, PIN_LEFT)) {kierunek = W_LEWO;}
	if (bit_is_clear(PORT_BUTTON, PIN_RIGHT)) {kierunek = W_PRAWO;}
	
	_delay_ms(CZAS_KLATKI/LICZBA_GET_KBD); // wait - czas wy�wietlania jedenj klatki 28ms
		
		
}

//przerwanie timera0 wyswietlaj�ce kolejne liniie z bufora ekranu na matryce LED
ISR (TIMER0_OVF_vect) {
		PORT_X = ~linia_x[i];
		//PORT_Y = i;
		//PORT_Y |=0xF8; //ustawienie spowrotem pullup�w
		PORT_Y = 0xF8 | i; //ustawienie pullup�w na nieu�ywanych pinach
		//PORT_Y |= i;
		i++;
		if (i>6) i=0;
	}

//funkcja przekazuj�ca wsp�rzedne z tablic X[] oraz Y[] do tablicy byfora matrycy LED (linia_x[])
void COORDtoLEDmatrix(void) {
	for (uint8_t n=0;n<7;n++) { //dla kolejnych linii wyswietlacza
		linia_x[n] = 0;	 //wyzeruj linie
		for (uint8_t k=0;k<dlugosc;k++) { //dla kolejnej pary zapisanych wsp�rz�dnych w tablicach X[] oraz Y[]
			if (Y[k] == n) {  //jesli wsp�lrz�dna Y(numer linii) = numer linii
				linia_x[n] |= (1 << X[k]);  //zapis do linii odpowiedniego bitu odpowiadaj�cemu wsp�rz�dnej x zapisanej w tablicy Y[]
			}
		}		
	}
	linia_x[food_y] |= (1 << food_x); //wprowadzenie punktu food na matryce led
}

inline void game_over(void) {
	/*
	for (uint8_t n=0;n<7;++n) { //dla kolejnych linii wyswietlacza
		linia_x[n] = 0;	 //wyzeruj linie
	}
	*/
	//////////////////////////////////////////////////////////////////////////////////
	mode = ZEGAR; //przejscie do trybu zegara
}

//funkcja przesuwaj�ca tu��w w�a opr�cz g�owy (X[0] Y[0])
//wprowadzi� case'y w zale�no�ci od d�ugosci
void przesun(void){
	switch (dlugosc) {
		case 7:
			X[6]=X[5];		Y[6]=Y[5];	
		case 6:
			X[5]=X[4];		Y[5]=Y[4];
		case 5:
			X[4]=X[3];		Y[4]=Y[3];
		case 4:
			X[3]=X[2]; 	Y[3]=Y[2];
		case 3:
			X[2]=X[1]; 	Y[2]=Y[1];
		case 2:
			X[1]=X[0]; 	Y[1]=Y[0];
		break;
	}
}

//funkcja ustawia nowy food
void new_food(void) {
	food_x = losowa % 8;
	food_y = (losowa+2) - godzina_bcd[3];
	//food_y >>= 5; // przesuni�cie w prawo pozostawia tylko 3 najstarsze bity czyli liczby od 0 do 7; sprawdzi�, czy od lewej zostanie uzupe�nione zerami
	food_y &= 0b00000111; //pozostawia tylko 3 najstarsze bity czyli liczby od 0 do 7
	if (food_y == 7) {food_y -=1;} // zapobiega ustawieniu fooda poza ekranem (ekran od 0 do 6)
}

                   
int main(void)
{
	i2c_init(); //inicjalizacja port�w I2C (podci�ganie i kierunki)
	sei();
	Enable_Timer0_OVF; //w��czenie przerwa� timera
	Timer0_Start_64;
	PORT_X_DDR = 0xff;
	PORT_Y_DDR = 0x07;
	
    get_time();
     
    for(;;){
		if(mode == ZEGAR)
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////if mode == ZEGAR////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		{
		kierunek = 0; //zresetowanie zmiennej kierunek, wykorzystywanej w trybie SNAKE
			for (uint8_t xx = LICZBA_GET_KBD ; xx ; xx--)
			{
				get_kbd(); 
				//_delay_ms(CZAS_KLATKI/LICZBA_GET_KBD); // wait - czas wy�wietlania jedenj klatki
			}
			if (!(x == ustaw))
			{ /***********************Tryb normalnej pracy - przesuwanie aktualnej godziny*************************/
				//przesuwa    
				for(uint8_t b=0;b<7;b++){    
						linia_x[b] = linia_x[b] << 1;    
				}    
				if (x<4) 
				{  
					   if (y<5) 
					   {   
							 for(uint8_t b=0;b<7;b++)  //do kolejnych linii bufora wyswietlacza wpisz z prawej strony odpowiedni bit
							 {     
									   if (eeprom_read_byte(&cyfra[(godzina_bcd[x])][y]) & (1<<b)) 
									   {
											linia_x[b] |=1;  //wpisuje bit po prawej stronie linii bufora wyswietlacza
									   }                        
							 }
							 y++; //zwi�kszanie zmiennej y �eby w nastepnym przejsciu rysowa� kolejn� kolumn� wyswietlanej cyfry
					   }else{
							 y++; //zwi�kszanie zmiennej y (do 6) �eby w nastepnym kroku nie nast�powa�o wpisywanie kolejnej kolumny do bufora wyswietlacza, co skutkuje wyswietleniem pustej kolumny czyli przerwy mi�dzy znakami
							 if (x == 1)  //po drugiej cyfrze (jedno�ci godzin) trzeba wy�wietli� dwukropek
							 {
								 if (y == 7) //pozycja dwukropka
								 {
									linia_x[2] |=1; //ustawianie bit�w tworz�cych dwukropek
									linia_x[5] |=1; 
								 }
								 if (y == 8) //po wyswietleniu przerwy za dwukropkiem
								 {
									 y=0;  //umozliwia wy�wietlanie kolejnej (trzeciej) cyfry
									 x++;  
								 }
							 }else{  //gdy dwukropek niepotrzebny
								 y=0;  //umozliwia wy�wietlanie kolejnej (trzeciej) cyfry
								 x++;
							 }
					   }   
				}else{ //gdy 4 cyfry ju� zosta�y wy�wietlone
					 if(++x > 12) //poczekaj kilka cykli, zeby poczatek kolejnej godziny nie goni� ko�ca poprzedniej
					 {
							x=0;
							get_time();
					 }
				}           
			}else{ //(!(x == ustaw)) 
			   /*****************Tryb ustawiania zegara - rysowana jest stoj�ca, ustawiana cyfra**********************/
				if (kierunek == W_GORE)
				{
				   if (++godzina_bcd[x-1] == 10) godzina_bcd[x-1]=0;
				} 
				if (kierunek == W_DOL)
				{
				   if (--godzina_bcd[x-1] == 255) godzina_bcd[x-1]=9;
				}
				for (uint8_t p=0;p<5;p++) //5 to szeroko�� cyfry w px - p�tla rysuje kolejne kolumny cyfry
				{   
				   for(uint8_t b=0;b<7;++b) // 7 to wysoko�� cyfry w px - p�tla rysuje kolejne piksele w kolumnie
				   {     
						 if (eeprom_read_byte(&cyfra[(godzina_bcd[x-1])][p]) & (1<<b)) // je�li pixel == 1 to
						 {
							if (x == 2) //druga cyfra jest podczas ustawiania wy�wietlana w innym miejscu
							{
								linia_x[b] |=(128 >> p); // ustaw piksel w odpowiedniej linii (b) i odpowiedniej kolumnie (p)
							}else{
								linia_x[b] |=(32 >> p); // ustaw piksel w odpowiedniej linii (b) i odpowiedniej kolumnie (p)
							}
						 }else{ // je�li pixel == 0 to
							if (x == 2) //druga cyfra jest podczas ustawiania wy�wietlana w innym miejscu
							{
								linia_x[b] &=~(128 >> p); // skasuj piksel w odpowiedniej linii (b) i odpowiedniej kolumnie (p)
							}else{
								linia_x[b] &=~(32 >> p); // skasuj piksel w odpowiedniej linii (b) i odpowiedniej kolumnie (p)
							}
						 }                       
				   }
				}
				if (kierunek)
				{
					_delay_ms(CZAS_CYFRY);
					kierunek = 0;
				}
				//zapis i2c
				save_time();
				//wait czas wy�wietlania jednej cyfry przy przytrzymywaniu przycisku
				//_delay_ms(CZAS_CYFRY);
			}
		}else{ 
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////if mode == SNAKE///////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			for(uint8_t xx = 20 ; xx ; xx--) // 20 wywa�a� get kbd (w ka�dym get kbd 28 ms) daje 560ms na klatke obrazu w�a
			{
				get_kbd();
			}
			zly_kierunek: //etykieta, do kt�rej skaczemy, gdy kierunek odczytany z klawiszy jest przeciwny do kierunku w�a
			switch (kierunek) {
				case W_LEWO:
					if (X[0]<X[1]) {
						kierunek = W_PRAWO;
						goto zly_kierunek;
					}
					przesun(); //przesuni�cie cia�a w�a
					X[0]++; //przesuni�cie g�owy w�a o jeden pixel
				break;
				case W_PRAWO:
					if (X[0]>X[1]) {
						kierunek = W_LEWO;
						goto zly_kierunek;
					}
					przesun();	
					X[0]--;
				break;
				case W_GORE:
					if (Y[0]>Y[1]) {
						kierunek = W_DOL;
						goto zly_kierunek;
					}
					przesun();
					Y[0]--;
				break;
				case W_DOL:
					if (Y[0]<Y[1]) {
						kierunek = W_GORE;
						goto zly_kierunek;	
					}
					przesun();
					Y[0]++;
				break;
			}
			

			//sprawdzanie opuszczenia planszy - mo�na da� spowrotem do switcha(kierunek) jesli bedzie ok
			if (X[0]==255 || X[0]==WIDTH || Y[0]==255 || Y[0]==HEIGHT) {
				game_over();
			}
			
			//sprawdzanie przeci�cia (zjedzenie w�a przez samego siebie)
			for (uint8_t p=1;p<dlugosc;++p) {
				if (X[0] == X[p] && Y[0] == Y[p]) {
					game_over();
				}
			}
			
			//kiedy g�owa w�a le�y w tym samym miejscu co food
			if (X[0] == food_x && Y[0] == food_y) { 
				if (dlugosc < MAX_DLUGOSC) { //zwieksz d�ugo��, dopuki nie osi�gnie maksymalnej d�ugo�ci
					dlugosc++;
					//new_food();
				} else {
					//winner();
				}
				new_food();//ustawia nowy food - mozna grac w niesko�czono��, ale w�� sie nie wyd�u�a ju�
			}
			
			losowa++; //zwi�kszenie losowej liczby wykorzystywanej w generowaniu po�o�enia food
			
			COORDtoLEDmatrix(); //przepisanie wsp�rz�dnych punkt�w w�a(X[];Y[]) i wsp�rz�dnych food do bufora ekranu
		}
	}
    return 0;
}
