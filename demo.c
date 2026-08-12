#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "library.h"
#include "wiringPi.h"
#include <signal.h>
#include <pthread.h>

#define LEDPort 0x3A
#define KbdPort 0x3C
#define LCDPort 0x3B
#define SMPort  0x39

#define Col7Lo 0xF7            // column 7 scan
#define Col6Lo 0xFB            // column 6 scan
#define Col5Lo 0xFD            // column 5 scan
#define Col4Lo 0xFE            // column 4 scan

int i,reading,h;
char inp;
int full_seq_drive[4] = {0x08, 0x04, 0x02, 0x01};

int anti_clockwise[4][4] = {
	{0,0,0,1},
	{0,0,1,0},
	{0,1,0,0},
	{1,0,0,0}
};

const unsigned char Bin2LED[] =
{ 
	/* 0     1     2    3 */
	0x40, 0x79, 0x24, 0x30,
	/*  4     5     6    7*/
	0x19, 0x12, 0x02, 0x78,
	/*  8     9     A    B*/
	0x00, 0x18, 0x08, 0x03,
	/*  C     D     E    F*/
	0x46, 0x21, 0x06, 0x0E
};

const unsigned char ScanTable [12] =
{
  // 0     1     2     3	
	0xB7, 0x7E, 0xBE, 0xDE,
  // 4     5     6     7	
	0x7D, 0xBD, 0xDD, 0x7B,
  // 8     9     *     #
	0xBB, 0xDB, 0x77, 0xD7
};

unsigned char dac_start[] = {"Running DAC"};
unsigned char dac_stop[] = {"Stopping DAC"};
unsigned char motor_start[] ={"Starting motor"};
unsigned char motor_stop[] ={"Stopping motor"};
unsigned char stop[] ={"Exiting Lab"};
unsigned char image[]={"Image Slideshow"};


// Declaration of thread condition variable 
pthread_t id[3];
//unsigned int start_motor = 0;
//unsigned int stop_motor = 0;
//unsigned int start_dac = 0;
//unsigned int stop_adc = 0;

pthread_t motor_id;
pthread_t dac_id;
// declaring mutex 
pthread_mutex_t bus_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t motorlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t daclock = PTHREAD_MUTEX_INITIALIZER;  

pthread_cond_t t2 = PTHREAD_COND_INITIALIZER; 
pthread_cond_t t3 = PTHREAD_COND_INITIALIZER;

void initlcd();
void lcd_writecmd(char cmd);
void LCDprint(char *sptr);
void lcddata(unsigned char cmd);

unsigned char ProcKey();
unsigned char ScanKey();
unsigned char ScanCode;

/* void* thread_dac(void* value)
{
	dac_id = pthread_self();

	unsigned char data[2];
	unsigned short fn[100];
	float gain= 255.0f;
	float phase = 0.0f;
	float bias = 255.0f;//1024.0f;
	float freq = 2.0* (3.14159f) /4.0;
	unsigned char buffer[1];
	int fileend;
	FILE *ptr;

	CM3DeviceInit();															
	CM3PortInit(5);																// initialise DAC
	printf("Connect Pin 1 and 2 of selection jumper Connector J3\n");

    if (( ptr = fopen(AUDIOFILE, "r")) == NULL)
	{
		perror (AUDIOFILE);
		printf ("File cannot be found \n");
		return (0);
	}

    while ( (fileend = fgetc(ptr) ) != EOF) 
	{
	    fread(buffer,sizeof(buffer),2,ptr);
	    for(int i=0; i<1 ; i++) {
			CM3PortWrite(3, buffer[i]);
			//usleep(1);
	    }


			//}
		//}
		pthread_mutex_unlock(&daclock);
		//usleep(100);
	}

} */

void* thread_motor(void* value)
{   

	int j;    

	motor_id = pthread_self();

	while(1)
	{ 
		pthread_mutex_lock(&motorlock);
		{
			for(i=0;i<4;i++)
			{
				pthread_mutex_lock(&bus_lock);
				CM3_outport(SMPort, full_seq_drive[i]);
				pthread_mutex_unlock(&bus_lock);
				usleep(8000);
			}
		}	
		pthread_mutex_unlock(&motorlock); 
		usleep(100);
	}	
}

int paywave(char *drink, char *price)
{
    unsigned char key;
    char line1[18];

    lcd_writecmd(0x01);
    usleep(2000);

    snprintf(line1, sizeof(line1), "%s:$%s", drink, price);
    lcd_writecmd(0x80);
    LCDprint(line1);

    lcd_writecmd(0xC0);
    LCDprint("Tap Card (#)");

    while (1)
    {
        key = ScanKey();

        if (key == 0xFF)
        {
            usleep(20000);
            continue;
        }

        while (ScanKey() != 0xFF)
        {
            usleep(10000);
        }

        if (key == 'B')     
        {
            lcd_writecmd(0x01);
            usleep(2000);

            lcd_writecmd(0x80);
            LCDprint("Payment OK");

            return 1;
        }

        else if (key == 'A')
        {
            usleep(2000);

            return 0;
        }
    }
}

int paynow(char *drink, char *price)
{
    unsigned char key;
    char line1[18];

    lcd_writecmd(0x01);
    usleep(2000);

    snprintf(line1, sizeof(line1), "%s:$%s", drink, price);
    lcd_writecmd(0x80);
    LCDprint(line1);

    lcd_writecmd(0xC0);
    LCDprint("Scan QR");

    while (1)
    {
        key = ScanKey();

        if (key == 0xFF)
        {
            usleep(20000);
            continue;
        }

        while (ScanKey() != 0xFF)
        {
            usleep(10000);
        }

        if (key == 'B')     
        {
            lcd_writecmd(0x01);
            usleep(2000);

            lcd_writecmd(0x80);
            LCDprint("Payment OK");

            return 1;
        }

        else if (key == 'A')
        {
            usleep(2000);

            return 0;
        }
    }
}

int input_amount(char *drink, char *price)
{
	unsigned int price_dollars = 0;
	unsigned int price_cents = 0;
	unsigned int price_frac = 0;
	unsigned int change = 0;
    unsigned int cents = 0;

    unsigned char key;
    char display[18];
    char line1[18];

    lcd_writecmd(0x01);     
    usleep(2000);

    snprintf(line1, sizeof(line1), "%s:$%s", drink, price);
    lcd_writecmd(0x80);
    LCDprint(line1);

    sprintf(display, "%u.%02u", cents / 100, cents % 100);
    lcd_writecmd(0xC0);
    LCDprint("                ");
    lcd_writecmd(0xC0);
    LCDprint(display);

    while (1)
    {
        key = ScanKey();

        if (key == 0xFF)
        {
            usleep(20000);
            continue;
        }

        while (ScanKey() != 0xFF)
        {
            usleep(10000);
        }

        if (key == 'B')
        {
            sscanf(price, "%u.%u", &price_dollars, &price_frac);
            price_cents = price_dollars * 100 + price_frac;

            lcd_writecmd(0x01);
            usleep(2000);   

            if (cents < price_cents)
            {
				pthread_t insufficient;
                lcd_writecmd(0x80);            
                LCDprint("Insufficient");

                lcd_writecmd(0xC0);              
                LCDprint("Returning Cash");

				sleep(4);

                return 0;
            }

            change = cents - price_cents;

            sprintf(display, "Change: $%u.%02u", change / 100, change % 100);

            lcd_writecmd(0x80);                  
            LCDprint(display);

            return 1;
        }

        if (key == 'A')
        {
            cents /= 10;
        }

        else if (key >= '0' && key <= '9')
        {
            if (cents <= 9999999)
            {
                cents = cents * 10 + (key - '0');
            }
        }

        sprintf(display, "%u.%02u", cents / 100, cents % 100);

        lcd_writecmd(0xC0);             
        LCDprint("                ");    
        lcd_writecmd(0xC0);
        LCDprint(display);
    }
}

int payment_option(char *drink, char *price)
{
    unsigned char key;
	int paid;

    lcd_writecmd(0x01);
    usleep(2000);

    lcd_writecmd(0x80);
    LCDprint("1Cash 2Card");

    lcd_writecmd(0xC0);
    LCDprint("3PayNow 4Back");

    while (1)
    {
        key = ScanKey();

        if (key == 0xFF)
            continue;

        while (ScanKey() != 0xFF)
            usleep(10000);

        if (key == '1')
        {
            paid = input_amount(drink, price);
            return paid;
        }

        if (key == '2')
        {
            paid = paywave(drink, price);
            return paid;
        }

        if (key == '3')
        {
            paid = paynow(drink, price);
            return paid;
        }

        if (key == '4')
        {
            return 0;
        }
    }
}

void *sd_image_thread(void *arg)
{
	system("DISPLAY=:0.0 pqiv -f /tmp/sddispense1.jpg &");
	sleep(4);
	system("DISPLAY=:0.0 pqiv -f /tmp/sddispense2.jpg &");
	sleep(2);
	system("DISPLAY=:0.0 pqiv -f /tmp/sddispense3.jpg &");
	return NULL;
}

void *pouring_nm_image_thread(void *arg)
{
	system("DISPLAY=:0.0 pqiv -f /tmp/coffeeteadispense.jpg &");
	sleep(8);
	system("DISPLAY=:0.0 pqiv -f /tmp/coffeeteafinish.jpg &");
	return NULL;
}

void *pouring_m_image_thread(void *arg)
{
	system("DISPLAY=:0.0 pqiv -f /tmp/coffeeteadispense.jpg &");
	sleep(3);
	system("DISPLAY=:0.0 pqiv -f /tmp/milkdispense.jpg &");
	sleep(5);
	system("DISPLAY=:0.0 pqiv -f /tmp/coffeeteafinish.jpg &");
	return NULL;
}

void dispense_sd(void) {
	pthread_t sid;
	pthread_create(&sid, NULL, sd_image_thread, NULL);
	usleep(2000);
	{
		FILE *clankptr;
		unsigned char clankbuf[8];
		size_t bytes_read;

		clankptr = fopen("/tmp/vending.raw", "r");

		if (clankptr == NULL)
		{
			perror("/tmp/vending.raw");
			printf("File cannot be found\n");
		}
		else
		{
			while ((bytes_read = fread(clankbuf, 1, sizeof(clankbuf), clankptr)) > 0)
			{
				for (size_t k = 0; k < bytes_read; k++)
				{
					CM3PortWrite(3, clankbuf[k]);
				}
			}
			fclose(clankptr);
		}
	}
	pthread_join(sid, NULL);
}

void dispense_coffee_tea_no_milk(void) {
	pthread_t ctnmid;
	pthread_create(&ctnmid, NULL, pouring_nm_image_thread, NULL);
	usleep(2000);
	{
		FILE *clankptr;
		unsigned char clankbuf[8];
		size_t bytes_read;

		clankptr = fopen("/tmp/pouring.raw", "r");

		if (clankptr == NULL)
		{
			perror("/tmp/pouring.raw");
			printf("File cannot be found\n");
		}
		else
		{
			while ((bytes_read = fread(clankbuf, 1, sizeof(clankbuf), clankptr)) > 0)
			{
				for (size_t k = 0; k < bytes_read; k++)
				{
					CM3PortWrite(3, clankbuf[k]);
				}
			}
			fclose(clankptr);
		}
	}
	usleep(2000);
	{
		FILE *clankptr;
		unsigned char clankbuf[8];
		size_t bytes_read;

		clankptr = fopen("/tmp/beepindicator.raw", "r");

		if (clankptr == NULL)
		{
			perror("/tmp/beepindicator.raw");
			printf("File cannot be found\n");
		}
		else
		{
			while ((bytes_read = fread(clankbuf, 1, sizeof(clankbuf), clankptr)) > 0)
			{
				for (size_t k = 0; k < bytes_read; k++)
				{
					CM3PortWrite(3, clankbuf[k]);
				}
			}
			fclose(clankptr);
		}
	}
	pthread_join(ctnmid, NULL);
}

void dispense_coffee_tea_milk(void) {
	pthread_t ctmid;
	pthread_create(&ctmid, NULL, pouring_m_image_thread, NULL);
	usleep(2000);
	{
		FILE *clankptr;
		unsigned char clankbuf[8];
		size_t bytes_read;

		clankptr = fopen("/tmp/pouring.raw", "r");

		if (clankptr == NULL)
		{
			perror("/tmp/pouring.raw");
			printf("File cannot be found\n");
		}
		else
		{
			while ((bytes_read = fread(clankbuf, 1, sizeof(clankbuf), clankptr)) > 0)
			{
				for (size_t k = 0; k < bytes_read; k++)
				{
					CM3PortWrite(3, clankbuf[k]);
				}
			}
			fclose(clankptr);
		}
	}
	usleep(2000);
	{
		FILE *clankptr;
		unsigned char clankbuf[8];
		size_t bytes_read;

		clankptr = fopen("/tmp/beepindicator.raw", "r");

		if (clankptr == NULL)
		{
			perror("/tmp/beepindicator.raw");
			printf("File cannot be found\n");
		}
		else
		{
			while ((bytes_read = fread(clankbuf, 1, sizeof(clankbuf), clankptr)) > 0)
			{
				for (size_t k = 0; k < bytes_read; k++)
				{
					CM3PortWrite(3, clankbuf[k]);
				}
			}
			fclose(clankptr);
		}
	}
	pthread_join(ctmid, NULL);
}

void* thread_keypad(void* value)
{
	unsigned char i;
	int mutex_check;
	int mutex_check_dac;

	pthread_mutex_lock(&motorlock);
	pthread_mutex_lock(&daclock);

	while(1)
	{
		pthread_mutex_lock(&bus_lock);
		i = ScanKey();
		pthread_mutex_unlock(&bus_lock);
		usleep(100000);

		if (i == '1')
		{
			printf("Pressed Key: %c\n",i);
			initlcd();
			int mj = 0;

			system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/sdselect.jpg &");
				
			pthread_mutex_lock(&bus_lock);
			LCDprint("Soft Drink Select");
			pthread_mutex_unlock(&bus_lock);
				
			usleep(1000);
			pthread_mutex_lock(&bus_lock);
			CM3_outport(LEDPort,Bin2LED[0]);
			pthread_mutex_unlock(&bus_lock);

			usleep(10000);
			printf("Drink Select\n");

			int verified = 0;

			while (1) {
				pthread_mutex_lock(&bus_lock);
				i = ScanKey();
				pthread_mutex_unlock(&bus_lock);

				if (i == '1')
				{
					verified = payment_option("Fanta", "1.70");
					if (verified == 0) {
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/sdselect.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Soft Drink Select");
						continue;
					}
					else if (verified == 1) {
						dispense_sd();
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
					}
				}

				if (i == '2')
				{
					verified = payment_option("Coke", "1.80");
					if (verified == 0) {
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/sdselect.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Soft Drink Select");
						continue;
					}
					else if (verified == 1) {
						dispense_sd();
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
					}
				}

				if (i == '3')
				{
					verified = payment_option("Sprite", "1.60");
					if (verified == 0) {
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/sdselect.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Soft Drink Select");
						continue;
					}
					else if (verified == 1) {
						dispense_sd();
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
					}
				}

				if (i == '4')
				{
					verified = payment_option("Pepsi", "1.90");
					if (verified == 0) {
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/sdselect.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Soft Drink Select");
						continue;
					}
					else if (verified == 1) {
						dispense_sd();
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
					}
				}
				if (i == 'A') {
					system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
					lcd_writecmd(0x01);
					LCDprint("Welcome");
					break;
				}
			}
		}

		if(i == '2')
		{
			printf("Pressed Key: %c\n",i);
			initlcd();
			int mj = 0;

			system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/coffeeselect.jpg &");
				
			pthread_mutex_lock(&bus_lock);
			LCDprint("Coffee Mods");
			pthread_mutex_unlock(&bus_lock);
				
			usleep(1000);
			pthread_mutex_lock(&bus_lock);
			CM3_outport(LEDPort,Bin2LED[1]);
			pthread_mutex_unlock(&bus_lock);

			usleep(10000);
			printf("Config Select\n");

			int verified = 0;

			while(1) {
				pthread_mutex_lock(&bus_lock);
				i = ScanKey();
				pthread_mutex_unlock(&bus_lock);

				if (i == '1')
				{
					verified = payment_option("Coffee", "2.50");
					if (verified == 0) {
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
						break;
					}
					else if (verified == 1) {
						dispense_coffee_tea_no_milk();
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
					}
				}

				if (i == '2')
				{
					verified = payment_option("Milk Coffee", "2.50");
					if (verified == 0) {
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
						break;
					}
					else if (verified == 1) {
						dispense_coffee_tea_milk();
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
						break;
					}
				}

				if (i == 'A')
				{
					system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
					lcd_writecmd(0x01);
					LCDprint("Welcome");
					break;
				}
			}
		}

		if(i == '3')
		{
			printf("Pressed Key: %c\n",i);
			initlcd();
			int mj = 0;

			system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/teaselect.jpg &");
				
			pthread_mutex_lock(&bus_lock);
			LCDprint("Tea Mods");
			pthread_mutex_unlock(&bus_lock);
				
			usleep(1000);
			pthread_mutex_lock(&bus_lock);
			CM3_outport(LEDPort,Bin2LED[2]);
			pthread_mutex_unlock(&bus_lock);

			usleep(10000);
			printf("Config Select\n");

			int verified = 0;

			while(1) {
				pthread_mutex_lock(&bus_lock);
				i = ScanKey();
				pthread_mutex_unlock(&bus_lock);

				if (i == '1')
				{
					verified = payment_option("Tea", "2.30");
					if (verified == 0) {
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
						break;
					}
					else if (verified == 1) {
						dispense_coffee_tea_no_milk();
						usleep(2000);
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						lcd_writecmd(0x01);
						LCDprint("Welcome");
						break;
					}
				}

				if (i == '2')
				{
					verified = payment_option("Milk Tea", "2.30");
					if (verified == 0) {
						lcd_writecmd(0x01);
						LCDprint("Welcome");
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						break;
					}
					else if (verified == 1) {
						dispense_coffee_tea_milk();
						usleep(2000);
						lcd_writecmd(0x01);
						LCDprint("Welcome");
						system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
						break;
					}
				}

				if (i == 'A')
				{
					system("killall -q pqiv; DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
					lcd_writecmd(0x01);
					LCDprint("Welcome");
					break;
				}
			}
		}
	}
}



int main(int agrv,char* argc[])
{
	int* ptr;
    system("killall pqiv");
	system("DISPLAY=:0.0 pqiv -f /tmp/menu.jpg &");
	sleep(2);
	CM3DeviceInit();
	CM3DeviceSpiInit(0);

	lcd_writecmd(0x01);
	LCDprint("Welcome");

	pthread_create(&id[0],NULL,thread_keypad,NULL);
	pthread_create(&id[1],NULL,thread_motor,NULL);

	//     }


	//CM3DeviceDeInit();
	pthread_join(id[0], (void**)&ptr);
	pthread_join(id[1], (void**)&ptr);
	return 0;
}

//----------- LCD Functions ----------------

void initlcd(void)
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
	lcd_writecmd(0x80);
}

void lcd_writecmd(char cmd)
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

void LCDprint(char *sptr)
{
	while (*sptr != 0)
	{
		int i=1;
        lcddata(*sptr);
		++sptr;
	}
}

void lcddata(unsigned char cmd)
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

//----------- Keypad Functions ----------------

unsigned char ScanKey()
{
	CM3_outport(KbdPort, Col7Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col7Lo;
	if (ScanCode != Col7Lo)
	{
	    return ProcKey();
	}

	CM3_outport(KbdPort, Col6Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col6Lo;
	if (ScanCode != Col6Lo)
	{
	    return ProcKey();
	}

	CM3_outport(KbdPort, Col5Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col5Lo;
	if (ScanCode != Col5Lo)
	{
	    return ProcKey();
	}

	CM3_outport(KbdPort, Col4Lo);
	ScanCode = CM3_inport(KbdPort);
	ScanCode |= 0x0F;
	ScanCode &= Col4Lo;
	if (ScanCode != Col4Lo)
	{
	    return ProcKey();
	}

	return 0xFF;
}

unsigned char ProcKey()
{
	unsigned char j;
	for (j = 0 ; j <= 12 ; j++)
	if (ScanCode == ScanTable [j])
	{
	   if(j > 9) {
		   j = j + 0x37;
	   } else {
		   j = j + 0x30;
	   }
	   return j;
	}

	if (j == 12)
	{
		return 0xFF;
	}

	return (0);
}