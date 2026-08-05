/*

Updated date 
- 25/01/2021 library from vendor
- 29/01/2021 CM3_outport, CM3_inport (pval = port & 0x3F); -- port definitions moved
- 10/02/2021 Added GPIO pin setup to "void CM3DeviceInit(void)"

*/

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>


#include "library.h"

unsigned char buffer[2];
int fd;

const static char *spiDev0  = "/dev/spidev1.0" ;
const static char *spiDev1  = "/dev/spidev1.1" ;
const static uint8_t spiBPW   = 8 ;
const static uint16_t spiDelay = 0 ;
static uint32_t spiSpeeds [2] ;
static int spiFds [2] ;

static int input_gpio(int gpio);




typedef struct {
	uint32_t funcsel[6];
	uint32_t rsvd;
	uint32_t pinoutset[2];
	uint32_t rsvd1;
	uint32_t pinoutclr[2];
	uint32_t rsvd2;
	uint32_t pinlevel[2];
	uint32_t rsvd3;
	uint32_t pinevtdet[2];
	uint32_t rsvd4;
	uint32_t pinrisedet[2];
	uint32_t rsvd5;
	uint32_t pinfalldet[2];
	uint32_t rsvd6;
	uint32_t highdeten[2];
	uint32_t rsvd7;
	uint32_t lowdeten[2];
	uint32_t rsvd8;
	uint32_t asyncrisedet[2];
	uint32_t rsvd9;
	uint32_t asyncfalldet[2];
	uint32_t rsvd10;
	uint32_t gppud;
	uint32_t gppudclk[2];
	uint32_t rsvd11;
}rpi_gpio_t;

typedef struct {
	uint32_t aux_irq;
	uint32_t aux_enb; 
	uint32_t mini_u_io;
	uint32_t muie;
	uint32_t muii;
	uint32_t mulc;
	uint32_t mumc;
	uint32_t muls;
	uint32_t mums; 
	uint32_t mus;
	uint32_t muec;
	uint32_t mues;
	uint32_t mub; 
	uint32_t spic0;
	uint32_t spic1;
	uint32_t spis;
	uint32_t spid;
	uint32_t spip;
	uint32_t spi1c0;
	uint32_t spi1c1;
        uint32_t status;
	uint32_t data;
        uint32_t peek;	
}rpi_spi_t;  


/************************ New Library ************************************/
void CM3_SpiDeviceInit(void)
{
	volatile rpi_gpio_t *gpiobase = NULL;
	uint8_t *gpiomem;

	fd = open("/dev/gpiomem", O_RDWR|O_SYNC);
	if(fd < 0) 
	{
		fprintf(stderr, "Unable to open /dev/mem\n");
		//goto err;
	}

	gpiobase = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(gpiobase == NULL) 
	{
		fprintf(stderr, "Unable to map GPIO memory /dev/mem\n");
		close(fd);
		//goto err;
	}
	gpio_map = (volatile uint32_t *)gpiobase;
	
}

void CM3_Devicespi(int port)

{
	int fdd;
	volatile rpi_spi_t *spibase = NULL;
	uint32_t *spidev0;
	if(port==0) {              //dac
	fdd = open("/dev/spidev1.1", O_RDWR|O_SYNC);
        }
	else{
	fdd = open("/dev/spidev1.0", O_RDWR|O_SYNC);    //adc
	}
	if(fdd < 0) 
	{
		fprintf(stderr, "Unable to open /dev/dpi\n");
	//	goto err;
	}

	spibase = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fdd, SPIBASE);
	if(spibase == NULL) 
	{
		fprintf(stderr, "Unable to map spi memory /dev/spi\n");
		close(fdd);
	//	goto err;
	}
}



void CM3DeviceInit(void)
{
	volatile rpi_gpio_t *gpiobase = NULL;
	uint8_t *gpiomem;
	int i;
	
	fd = open("/dev/gpiomem", O_RDWR|O_SYNC);
	if(fd < 0) 
	{
		fprintf(stderr, "Unable to open /dev/mem\n");
		//goto err;
	}

	gpiobase = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(gpiobase == NULL) 
	{
		fprintf(stderr, "Unable to map GPIO memory /dev/mem\n");
		close(fd);
		//goto err;
	}
	gpio_map = (volatile uint32_t *)gpiobase;
	
	
	for(int i=0;i<16;i++){
		setup_gpio(i,1,1);
	}

	write_7seg_setup();      // GPIO pin setup
	
}

int CM3DeviceDeInit(void)
{
	volatile rpi_gpio_t *gpiobase = NULL;
	munmap((void *)gpiobase, 4096);
	close(fd);
	return 0;
err:
	return -1;
}

void CM3_outport(unsigned char port, unsigned char data)
{
	unsigned char pval;
	int i;
	/* Write Port on GPIO */
	//pval = port | 0x38;                        //0x38
	pval = port & 0x3F;
	
	for(i = 0; i <= 5; i++) {
		output_gpio(5-i, (pval >> i) & 0x01);
	}
	
	usleep(10);
	
	for(i=0; i < 8; i++)
	{
		if(data & (1<<i))
		{

			output_gpio(i+8,1);
			
		}
		else
		{
			output_gpio(i+8,0);
		}
	}
	
	/* Generate WE_N */
	output_gpio(7, 1);
	usleep(10);
	output_gpio(7, 0);
	usleep(100);
	output_gpio(7, 1);
	usleep(10);

	for(i = 0; i <= 5; i++) {
		output_gpio(5-i, (port >> i) & 0x01);
	}	
}

unsigned char CM3_inport(unsigned char port)
{
	unsigned char pval;
	unsigned int res;
	unsigned char data = 0;
	
	int i;
	/* Write Port on GPIO */
	//pval = port | 0x38;
	pval = port & 0x3F;
	
	/*output_gpio(12, 1);
	output_gpio(13, 1);
	output_gpio(14, 1);
	output_gpio(15, 1);
	setup_gpio(12,0,1);
	setup_gpio(13,0,1);
	setup_gpio(14,0,1);
	setup_gpio(15,0,1);
	usleep(1000);*/

    output_gpio(8, 1);	// set to 1
	output_gpio(9, 1);
	output_gpio(10, 1);
	output_gpio(11, 1);
	output_gpio(12, 1);
	output_gpio(13, 1);
	output_gpio(14, 1);
	output_gpio(15, 1);
	setup_gpio(8,0,1); // set to i/p
	setup_gpio(9,0,1);
	setup_gpio(10,0,1);
	setup_gpio(11,0,1);
	setup_gpio(12,0,1);
	setup_gpio(13,0,1);
	setup_gpio(14,0,1);
	setup_gpio(15,0,1);
	usleep(1000);
	
	for(i = 0; i <= 5; i++) {
		output_gpio(5-i, (pval >> i) & 0x01);
	}
	
	usleep(10);
	output_gpio(6,1);
	usleep(10);
	output_gpio(6,0);
	usleep(1000);
	
	for(i=0; i < 8; i++) {                                                                 // mod 4 to 8

		res = 	input_gpio(8+i);                                                     // 12 to 8
		if(res & (1 << (8+i))) {                                                         //12 to 0
			data = data | (1 << (i));                                                    //mod 4+i
		}
	}
	
	for(i = 0; i <= 5; i++) {
		output_gpio(5-i, (port >> i) & 0x01);
	}

	output_gpio(6,1);
	usleep(10);

    setup_gpio(8,1,1);
	setup_gpio(9,1,1);
	setup_gpio(10,1,1);
	setup_gpio(11,1,1);
	setup_gpio(12,1,1);
	setup_gpio(13,1,1);
	setup_gpio(14,1,1);
	setup_gpio(15,1,1);	

	return data;
}
	
/*void CM3WRITE_motor(int arr[4])
{

	int i;
	for(i=0;i<4;i++)
	{
		CM3_outport(MOTORdrive, arr[i]);
		//usleep(50000);
		//output_gpio(8,arr[i][0]);
		//output_gpio(10,arr[i][1]);
		//output_gpio(9,arr[i][2]);
		//output_gpio(11,arr[i][3]);
		usleep(1000);


	}
}
*/

/*
void write_lcd(unsigned char cmd)
{

	char data;

	data = (cmd & 0xf0);
	CM3_outport(LCDPort, data | 0x05);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(200);

	data = (cmd & 0x0f) << 4;
	CM3_outport(LCDPort, data | 0x05);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(2000);
}

static void lcd_writecmd(char cmd)
{
	char data;

	data = (cmd & 0xf0);
	CM3_outport(LCDPort, data | 0x04);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(200);

	data = (cmd & 0x0f) << 4;
	CM3_outport(LCDPort, data | 0x04);
	usleep(10);
	CM3_outport(LCDPort, data);

	usleep(2000);
}

void init_lcd(void)
{
        usleep(20000);
	lcd_writecmd(0x30);
        usleep(20000);
	lcd_writecmd(0x30);   
  	usleep(20000);
	lcd_writecmd(0x30);

	lcd_writecmd(0x02);  // 4 bit mode 
	lcd_writecmd(0x28);  // 2 line  5*7 dots
	lcd_writecmd(0x01);  //clear screen
	lcd_writecmd(0x0c);  //dis on cur off
	lcd_writecmd(0x06);  //inc cur
      //  lcd_writecmd(0xc0);  //1 line
	lcd_writecmd(0x80);
}
*/

/*
void init_lcd2(void)
{
	lcd_writecmd(0xc0);
}
*/

/*
unsigned char CM3_scankey(void)
{
	unsigned char ScanCode;

	CM3_outport(KbdPort, 0xF7);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= 0xF7;
	if (ScanCode != 0xF7)
	{
	    if (ScanCode == 0x77)
			return '*';
		if (ScanCode == 0xB7)
		    return '0';
		if (ScanCode == 0xD7)
			return '#';	
	}

	CM3_outport(KbdPort, 0xFB);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= 0xFB;
	if (ScanCode != 0xFB)
	{
	    if (ScanCode == 0x7B)
			return '7';
		if (ScanCode == 0xBB)
		    return '8';
		if (ScanCode == 0xDB)
			return '9';	
	}

	CM3_outport(KbdPort, 0xFD);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= 0xFD;
	if (ScanCode != 0xFD)
	{
	    if (ScanCode == 0x7D)
			return '4';
		if (ScanCode == 0xBD)
		    return '5';
		if (ScanCode == 0xDD)
			return '6';	
	}

	CM3_outport(KbdPort, 0xFE);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= 0xFE;
	if (ScanCode != 0xFE)
	{
	    if (ScanCode == 0x7E)
			return '1';
		if (ScanCode == 0xBE)
		    return '2';
		if (ScanCode == 0xDE)
			return '3';	
	}

	return 0xFF;
}

unsigned char cm3_scan_keypad(void)
{
	unsigned char data;

	CM3_outport(KbdPort, 0xFE);
	usleep(1000);
	data = CM3_inport(KbdPort);
	if((data & 0x0F) != 0x0F) {	
		if((data & 0x02) == 0)
			return '3';
		if((data & 0x04) == 0)
			return '2';
		if((data & 0x08) == 0)
			return '1';
	}
      
	CM3_outport(KbdPort, 0xfd);
	usleep(1000);
	data = CM3_inport(KbdPort);
	if((data & 0x0f) != 0x0F){
		if((data & 0x02) == 0)
			return '6';
		if((data & 0x04) == 0)
			return '5';
		if((data & 0x08) == 0)
			return '4';
	}

	CM3_outport(KbdPort, 0xfb);
	usleep(1000);
	data = CM3_inport(KbdPort);
	if((data & 0x0f) != 0x0f){
		if((data & 0x02) == 0)
			return '9';
		if((data & 0x04) == 0)
			return '8';
		if((data & 0x08) == 0)
			return '7';
	}

	CM3_outport(KbdPort, 0xf7);
	usleep(1000);
	data = CM3_inport(KbdPort);
	if((data & 0x0f) != 0x0f){
		if((data & 0x02) == 0)
			return '#';
		if((data & 0x04) == 0)
			return '0';
		if((data & 0x08) == 0)
			return '*';
	}

	return 0xff;
}
*/

// `short_wait` waits 150 cycles
static void short_wait(void)
{
	int i;
	for (i=0; i<150; i++) {
		asm volatile("nop");
	}
}
// Sets a pullup or -down resistor on a GPIO
static void set_pullupdn(int gpio, int pud)
{
	int clk_offset = OFFSET_PULLUPDNCLK + (gpio/32);
	int shift = (gpio%32);

	if (pud == PUD_DOWNS)
		*(gpio_map+OFFSET_PULLUPDN) = (*(gpio_map+OFFSET_PULLUPDN) & ~3) | PUD_DOWNS;
	else if (pud == PUD_UPS)
		*(gpio_map+OFFSET_PULLUPDN) = (*(gpio_map+OFFSET_PULLUPDN) & ~3) | PUD_UPS;
	else  // pud == PUD_OFF
		*(gpio_map+OFFSET_PULLUPDN) &= ~3;

	short_wait();
	*(gpio_map+clk_offset) = 1 << shift;
	short_wait();
	*(gpio_map+OFFSET_PULLUPDN) &= ~3;
	*(gpio_map+clk_offset) = 0;
}

// Sets a GPIO to either output or input (input can have an optional pullup
// or -down resistor).
static void setup_gpio(int gpio, int direction, int pud)
{
	int offset = OFFSET_FSEL + (gpio/10);
	int shift = (gpio%10)*3;

	set_pullupdn(gpio, pud);
	if (direction == OUTPUT)
		*(gpio_map+offset) = (*(gpio_map+offset) & ~(7<<shift)) | (1<<shift);
	else if (direction == ALTERNATE)
		*(gpio_map+offset) = (*(gpio_map+offset) & ~(7<<shift)) | (4<<shift);
	else  // direction == INPUT
		*(gpio_map+offset) = (*(gpio_map+offset) & ~(7<<shift));
	set_pullupdn(gpio, pud);
}

// Returns the function of a GPIO: 0=input, 1=output, 4=alt0
// Contribution by Eric Ptak <trouch@trouch.com>
static int gpio_function(int gpio)
{
	int offset = OFFSET_FSEL + (gpio/10);
	int shift = (gpio%10)*3;
	int value = *(gpio_map+offset);
	value >>= shift;
	value &= 7;
	return value;
}

// Sets a GPIO output to 1 or 0

static void output_gpio(int gpio, int value)
{
	int offset;
	if (value) // value == HIGH
		offset = OFFSET_SET + (gpio / 32);
	else       // value == LOW
		offset = OFFSET_CLR + (gpio / 32);
	*((volatile uint32_t *)(gpio_map+offset)) = 1 << gpio % 32;
}

// Returns the value of a GPIO input (1 or 0)
static int input_gpio(int gpio)
{
	int offset, value, mask;
	offset = OFFSET_PINLEVEL + (gpio/32);
	mask = (1 << gpio%32);
	value = *((volatile uint32_t *)(gpio_map+offset)) & mask;
	return value;
}


static void cleanup(void)
{
	// fixme - set all gpios back to input
	munmap((void *)gpio_map, BLOCK_SIZE);

}

/*************************************************************************/

static void device_cycle_led(void)
{
	output_gpio(3,0);
	output_gpio(4,1);
	output_gpio(5,0);
	usleep(100000);

	output_gpio(3,1);
	output_gpio(4,1);
	output_gpio(5,1);
	usleep(100000);
}

static void write_7seg_setup(void)
{
	/* GPIO SETUP*/

	for(int i=3;i<16;i++){
		setup_gpio(i,1,1);
	}
	sleep(1);

	/* GPIO OUTPUT */
	output_gpio(0,1);
	output_gpio(1,1);
	output_gpio(2,1);
	output_gpio(3,0);
	output_gpio(4,0);
	output_gpio(5,1);
	output_gpio(6,1);
	output_gpio(7,0);

	sleep(1);

}


static void write_7seg_led(uint8_t data)
{
	int i;
	for(i=0; i < 8; i++)
	{
		if(data & (1<<i))
		{

			output_gpio(i+8,1);

		}
		else
		{
			output_gpio(i+8,0);
		}
	}
	usleep(100000);
	device_cycle_led();
}

static void device_cycle_lcd(void)
{
	output_gpio(3,0);
	output_gpio(4,1);
	output_gpio(5,1);
	sleep(1);

	output_gpio(3,1);
	output_gpio(4,1);
	output_gpio(5,1);
}

static void write_lcd_setup(void)
{


	/* GPIO SETUP*/

	for(int i=3;i<16;i++){
		setup_gpio(i,1,1);
	}
	sleep(1);

	/* GPIO OUTPUT */
	output_gpio(0,1);
	output_gpio(1,1);
	output_gpio(2,1);
	output_gpio(3,0);
	output_gpio(4,0);
	output_gpio(5,1);
	output_gpio(6,1);
	output_gpio(7,0);

	sleep(1);

}

static void pulse_enable(void)
{
	output_gpio(8,1);
	output_gpio(9,0);
	output_gpio(10,1);
	usleep(1);

	output_gpio(10,0);
}

static void cmd_pulse(void)
{
	output_gpio(8,0);   //rs
	output_gpio(9,0);   //r/w 
	output_gpio(10,1);  //enable bit
	usleep(1);
	output_gpio(10,0);
}

/*
static void lcd_writedata(unsigned char dat)

{     

	unsigned char data;

	data = (dat & 0xf0);
	output_gpio(12,(data & 0x10));
	output_gpio(13,(data & 0x20));
	output_gpio(14,(data & 0x40));
	output_gpio(15,(data & 0x80));

	pulse_enable();
	usleep(200);

	data = ((dat<<4)&0xf0); 

	output_gpio(12,(data & 0x10));
	output_gpio(13,(data & 0x20));
	output_gpio(14,(data & 0x40));
	output_gpio(15,(data & 0x80));

	pulse_enable();
	usleep(2000);  
}
* /
/*
static void lcd_writecmd(char cmd)
{

	char data;

	data = (cmd & 0xf0);;

	output_gpio(12,(data & 0x10));
	output_gpio(13,(data & 0x20));
	output_gpio(14,(data & 0x40));
	output_gpio(15,(data & 0x80));

	cmd_pulse();
	usleep(200);

	data = ((cmd<<4) & 0xf0);


	output_gpio(12,(data & 0x10));
	output_gpio(13,(data & 0x20));
	output_gpio(14,(data & 0x40));
	output_gpio(15,(data & 0x80));

	cmd_pulse();
	usleep(2000);

} 

*/

/*


void position_set_lcd(int row)
{

	if(row==1)
	{
	lcd_writecmd(0x80);
	}
	else
	{
	lcd_writecmd(0xc0);
	}


}
*/


/*
void clear_display(void)
{
	lcd_writecmd(0x01);
	sleep(1);


}
*/

static void device_cycle_sm(void)
{
	output_gpio(3,0);
	output_gpio(4,0);
	output_gpio(5,1);
	sleep(1);

	output_gpio(3,1);
	output_gpio(4,1);
	output_gpio(5,1);
//	printf("device_cycle done \n");
}
static void setup_pin_sm(void)
{
	for(int i=0;i<9;i++)
	{
		setup_gpio(i+3,1,1);

	}
	usleep(50000);
}
static void  device_cycle_keypad(void)
{
	output_gpio(3,1);
	output_gpio(4,0);
	output_gpio(5,0);
	usleep(100000);
}
static void keypad_setup(void)
{
	/* GPIO SETUP*/
	setup_gpio(0,1,1);
	setup_gpio(1,1,1);
	setup_gpio(2,1,1);

	setup_gpio(3,1,1);
	setup_gpio(4,1,1);
	setup_gpio(5,1,1);
	setup_gpio(6,1,1);
	setup_gpio(7,1,1);
	setup_gpio(8,1,1);
	setup_gpio(9,1,1);
	setup_gpio(10,1,1);
	setup_gpio(11,1,1);
	setup_gpio(12,0,1);
	setup_gpio(13,0,1);
	setup_gpio(14,0,1);
	setup_gpio(15,0,1);
	usleep(100000);

	/* GPIO OUTPUT */
	output_gpio(0,1);
	output_gpio(1,1);
	output_gpio(2,1);
	output_gpio(3,0);
	output_gpio(4,0);
	output_gpio(5,1);
	output_gpio(6,1);
	output_gpio(7,0);

	usleep(100000);
}

static void write_enable_keypad(void)
{
	output_gpio(7,0);
	output_gpio(6,1);
	usleep(5000); 
	output_gpio(7,1);
	usleep(5000); 
	//output_gpio(7,1);
}
static void read_enable_keypad(void)
{
	//output_gpio(6,0);
	output_gpio(6,0);
	//output_gpio(7,1);
	usleep(5000); 
	//usleep(500000);
}
static void scanme(int x)
{
	static int clk = 0;
	usleep(5000);
	output_gpio(8,1);
	output_gpio(9,1);
	output_gpio(10,1);
	output_gpio(11,1);
	output_gpio(x,0);
//	printf(" x= %d\n",x);

	//if(clk == 0) {
/*	usleep(500000);
	write_enable_keypad();
	usleep(250000);
	device_cycle_keypad();
	usleep(500000);
	read_enable_keypad();
	usleep(200000);
	clk = 1; */
	//}
//	printf("keypad_oe_rw \n");


	usleep(5000);
	write_enable_keypad();
	//usleep(100000);
	if(clk == 0) {
		device_cycle_keypad();
		usleep(100000);
		clk=1;
	}
	read_enable_keypad();
	//usleep(100000);

}
static unsigned char scan_keypad(void)
{
	int i;
	//printf("Testing_Keyapd_Row_1\n");
	scanme(8);
	i=0;
	do 
	{
		if(input_gpio(15)==0)
		{
			printf("1 is pressed\n");
			return 1;
		}
		if(input_gpio(14)==0)
		{
			printf("2 is pressed\n");
			return 2;
		}
		if(input_gpio(13)==0)
		{
			printf("3 is pressed\n");
			return 3;
		}
		usleep(5000);
		i++;
	}while(i < 2);
	output_gpio(8,1);
	//usleep(200000);

        //printf("Testing_Keyapd_Row_2\n");
	scanme(9);
	i=0;
	do
	{
		if(input_gpio(15)==0)
		{
			printf("4 is pressed\n");
			return 4;
		}
		if(input_gpio(14)==0)
		{
			printf("5 is pressed\n");
			return 5;
		}
		if(input_gpio(13)==0)
		{
			printf("6 is pressed\n");
			return 6;
		}
		usleep(5000);
		i++;
	}while(i < 2);
	output_gpio(9,1);
	//usleep(200000);

	//printf("Testing_Keyapd_Row_3\n");
	scanme(10);
        i=0;
	do
	{
		if(input_gpio(15)==0)
		{
			printf("7 is pressed\n");
			return 7;
		}
		if(input_gpio(14)==0)
		{
			printf("8 is pressed\n");
			return 8;
		}
		if(input_gpio(13)==0)
		{
			printf("9 is pressed\n");
			return 9;
		}
		usleep(5000);
		i++;
	}while(i < 2);
	output_gpio(10,1);
	//usleep(200000);

	//printf("Testing_Keyapd_Row_4\n");
	scanme(11);
	i=0;
	do
	{
		if(input_gpio(15)==0)
		{
			printf("* is pressed\n");
			return 10;
		}
		if(input_gpio(14)==0)
		{
			printf("0 is pressed\n");
			return 0;
		}
		if(input_gpio(13)==0)
		{
			printf("# is pressed\n");
			return 11;
		}
		usleep(5000);
		i++;
	}while(i < 2);
	output_gpio(11,1);
	//usleep(5000);	
	return 0xff;
}

static void spi_cycle(void)
{
	output_gpio(3,0);
	output_gpio(4,0);
	output_gpio(5,0);
	usleep(500000);
	output_gpio(3,1);
	output_gpio(4,1);
	output_gpio(5,1);
}
static void spi_setup(void)
{

	/* GPIO SETUP*/

	for(int i=0;i<16;i++){
		setup_gpio(i,1,1);
	}
        	sleep(1);

	/* GPIO OUTPUT */
	output_gpio(0,1);
	output_gpio(1,1);
	output_gpio(2,1);
	output_gpio(3,0);
	output_gpio(4,0);
	output_gpio(5,1);
	output_gpio(6,1);
	output_gpio(7,0);

	sleep(1);

}

static int wiringPiSPIDataRW (int channel, unsigned char *data, int len)

{

	struct spi_ioc_transfer spi ;

	channel &= 1 ;
	memset (&spi, 0, sizeof (spi)) ;

	spi.tx_buf        = (unsigned long)data ;

	spi.rx_buf        = (unsigned long)data ;

	spi.len           = len ;

	spi.delay_usecs   = spiDelay ;

	spi.speed_hz      = spiSpeeds [channel] ;

	spi.bits_per_word = spiBPW ;

	return ioctl (spiFds [channel], SPI_IOC_MESSAGE(1), &spi) ;

}
static int wiringPiSPISetupMode (int channel, int speed, int mode)

{

	int fd ;
	mode    &= 3 ;	
	channel &= 1 ;	

	if ((fd = open (channel == 0 ? spiDev0 : spiDev1, O_RDWR)) < 0){

		return wiringPiFailure (WPI_ALMOST, "Unable to open SPI device: %s\n", strerror (errno)) ;

	}

	spiSpeeds [channel] = speed ;

	spiFds    [channel] = fd ;

	if (ioctl (fd, SPI_IOC_WR_MODE, &mode)   < 0){

		return wiringPiFailure (WPI_ALMOST, "SPI Mode Change failure: %s\n", strerror (errno)) ;
	}


	if (ioctl (fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0){

		return wiringPiFailure (WPI_ALMOST, "SPI BPW Change failure: %s\n", strerror (errno)) ;
	}


	if (ioctl (fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed)   < 0){

		return wiringPiFailure (WPI_ALMOST, "SPI Speed Change failure: %s\n", strerror (errno)) ;
	}


	return fd ;

}

static int wiringPiSPISetup (int channel, int speed)

{

	  return wiringPiSPISetupMode (channel, speed, 0) ;

}

static int read_mcp3001(void)
{
	int channel = 0;
	int reading; 
	int hi,lo,vin;

	output_gpio(18,true);
	usleep(100000);
	output_gpio(18,false);
	usleep(200000);

	reading =  wiringPiSPIDataRW(channel,buffer,2);
	printf("[%d %d]\n",buffer[0],buffer[1]);

	hi = buffer[0] & 0x1f;

	lo = buffer[1] & 0xf8;

	vin = (hi<<5) + (lo>>3);
	printf("%d \n",vin);
	vin = vin * 5/1024;
	printf("vin= %dv \n",vin);
	usleep(100000);

	return vin;


}
void CM3_DeviceInit(void)
{
	volatile rpi_gpio_t *gpiobase = NULL;
	uint8_t *gpiomem;

	fd = open("/dev/gpiomem", O_RDWR|O_SYNC);
	if(fd < 0) 
	{
		fprintf(stderr, "Unable to open /dev/mem\n");
		//goto err;
	}

	gpiobase = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(gpiobase == NULL) 
	{
		fprintf(stderr, "Unable to map GPIO memory /dev/mem\n");
		close(fd);
		//goto err;
	}
	gpio_map = (volatile uint32_t *)gpiobase;
}

void CM3DeviceSpiInit(int port)
{
	int fdd;
	volatile rpi_spi_t *spibase = NULL;
	uint32_t *spidev0;

	if(port==0) {              //dac
		fdd = open("/dev/spidev1.1", O_RDWR|O_SYNC);
	}
	else{
		fdd = open("/dev/spidev1.0", O_RDWR|O_SYNC);    //adc
	}
	if(fdd < 0) 
	{
		fprintf(stderr, "Unable to open /dev/dpi\n");
		goto err;
	}

	spibase = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fdd, SPIBASE);
	if(spibase == NULL) 
	{
		fprintf(stderr, "Unable to map spi memory /dev/spi\n");
		close(fdd);
		goto err;
	}

	setup_gpio(17,1,1);
	output_gpio(17,true);
	// 	wiringPiSPISetup(1,4000);  //channel 1 ;spi_speed_hz 4000;
	wiringPiSPISetupMode(1,4000,0);

err:
	return;
}

void CM3DeviceSpiWrite(uint8_t value)
{		
	unsigned char send[2];
	uint8_t base = 0x50;

	output_gpio(17,false);
	usleep(10);
	send[0] = (base | value>>8);
	send[1] = value & 0x00FF;
	wiringPiSPIDataRW(1, send, 2);
	output_gpio(17,true);
}


void CM3PortInit(int port)
{ 
	
	if(port == 0)     //led
	{
		device_cycle_led();
		write_7seg_setup();
	}
	if(port == 1)      //lcd
	{
		device_cycle_lcd();
		write_lcd_setup();
		//lcd_init();
		//lcd_writecmd(0x80);
	//	lcd_writecmd(0xc0);

	}
        if(port == 2)
	{
        	//lcd_writecmd(0xc0);
	}
	if(port == 3)      //steppermotor
	{
		setup_pin_sm();
         	output_gpio(6,1);
        	output_gpio(7,0);
	        usleep(50000);
		

	}
	if(port == 4)      //keypad
	{
		keypad_setup();

	}
	

	if(port == 5)      //dac
	{
		spi_cycle();
		spi_setup();
		setup_gpio(17,1,1);
		output_gpio(17,true);
       // 	wiringPiSPISetup(1,4000);  //channel 1 ;spi_speed_hz 4000;                      
		wiringPiSPISetupMode(1,4000000,0);		//mod from 4000
          	printf("init_result\n");
	}
	if(port == 6)      //adc
	{
		spi_cycle();
		spi_setup();
		setup_gpio(18,1,1);
	//	wiringPiSPISetup(0,1000);  //channel =0 ; spi_speed_hz = 1000 ;
         if(wiringPiSPISetupMode(0,1000,0)<0){
		  fprintf (stderr, "Unable to open SPI device 0: %s\n", strerror (errno)) ;
		      exit (1) ;
	 }
		printf("init_result\n");


	}
}


void CM3PortWrite(int port,uint8_t value)
{
	if(port == 0)
	{
		write_7seg_led(value);      //led
	}
	if(port == 1)
	{
		//lcd_writedata(value);      //lcd
	}

	if(port == 2)
	{
		output_gpio(18,value);     
	}
	if(port == 3)                        //dac
	{ 
		
		/*if(value == 0)
		{
	        output_gpio(17,false);
		usleep(100000);
		buffer[0] = 0x3f;
		buffer[1] = 0xfc;
		wiringPiSPIDataRW(1,buffer,2);     //channel 1, length 2
		usleep(100000);
          	output_gpio(17,true);
		}
		if(value == 1)
		{   
		output_gpio(17,false);
		usleep(100000);
		buffer[0] = 0x00;
		buffer[1] = 0x00;
		wiringPiSPIDataRW(1,buffer,2);     //channel 1, length 2 
		usleep(100000);
          	output_gpio(17,true);

		}*/

		{ 
			unsigned char send[2];
			uint8_t base = 0x50;
	
			output_gpio(17,false);
			usleep(10);
			send[0] = (base | value>>8);
			send[1] = value & 0x00FF;
			wiringPiSPIDataRW(1, send, 2);
			output_gpio(17,true);
				
	    }
	}
}
int CM3PortRead(int port)
{
	if(port == 4)   //keypad
	{
		scan_keypad();

	}
	if(port == 3)     //adc
	{
	  read_mcp3001();
	}
}
void CM3WRITEPortArray(int arr[4][4])
{

        int i;
	for(i=0;i<4;i++)
	{
            	device_cycle_sm();
		usleep(50000);
		output_gpio(8,arr[i][0]);
		output_gpio(10,arr[i][1]);
		output_gpio(9,arr[i][2]);
		output_gpio(11,arr[i][3]);
		usleep(1000);


	}
}
