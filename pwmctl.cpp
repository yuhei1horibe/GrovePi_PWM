/**
 * @author Y.Horibe
 * PWM controller via GrovePi
 */
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <iostream>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "Packet.h"
#include "i2c-dev.h"

#define	INT_INTERVAL	2	// Interrupt interval[ms]
#define	RESOL			255	// Resolution

// File handle for I2C
const char		i2c_dev_name[]	= "/dev/i2c-1";
const uint32_t	addr_grvPi	= 0x04;
const uint32_t	addr_disp	= 0x03;
int32_t			fd_grvPi;	// for GrovePi
int32_t			fd_disp;	// for LCD Display

// For signal handling
struct sigaction	sa_tim, def_tim_hdr;

// Timer settings
timer_t				tid;
struct itimerspec	itval;

// PWM output strength
int32_t		pwmCnt1		= 0;
int32_t		pwmCnt2		= 0;

// Input to determine output strength
int32_t		Notch1		= 0;
int32_t		Notch2		= 0;

// Destination
int32_t		grvpi_ch5	= 5;	// GrovePi pin5
int32_t		grvpi_ch6	= 6;	// GrovePi pin6

// Selecter for controller
int32_t		ctl_sel		= 1;	// By defaut, select controller 1


int32_t pin_mode(const uint8_t pin, const bool bOutput);
int32_t InitGrovePi();	// Initialization for GrovePi (I2C init)
int32_t pwm_out(const uint8_t pin, const uint8_t output);
int32_t lcd_out();
void tim_main_handler(int32_t signum);
int32_t TaskStart();
int32_t mainLoop();
void ReleaseAll();


// Entry point
int32_t main(int32_t argc, char *argv[])
{
	// GrovePi initialization (I2C init)
	if(InitGrovePi() != 0)
		return -1;

	// Start timer handler
	if(TaskStart() != 0)
		return -1;

	// LCD test
	lcd_out();

	// Controller
	mainLoop();

	ReleaseAll();

	return 0;
}

// Initialization for GrovePi (I2C init)
int32_t InitGrovePi()
{
	//I2C initialization for PWM channel
	if((fd_grvPi = open(i2c_dev_name, O_RDWR)) < 0){
		std::cout << "Failed to open i2c device." << std::endl;
		return -1;
	}

	if(ioctl(fd_grvPi, I2C_SLAVE, addr_grvPi) < 0){
		std::cout << "Failed to initialize GrovePi." << std::endl;
		close(fd_grvPi);
		return -1;
	}

	//I2C initialization for LCD
	if((fd_disp = open(i2c_dev_name, O_RDWR)) < 0){
		std::cout << "Failed to open i2c device." << std::endl;
		return -1;
	}

	if(ioctl(fd_disp, I2C_SLAVE, addr_disp) < 0){
		std::cout << "Failed to initialize LCD." << std::endl;
		close(fd_grvPi);
		close(fd_disp);
		return -1;
	}

	// Setup pin Mode
	pin_mode(5, true);
	pin_mode(6, true);
	
	return 0;
}

// Pin mode setup
int32_t pin_mode(const uint8_t pin, const bool bOutput)
{
	const uint32_t	PIN_MODE	= 5;
	Packet_t		packet;

	packet.field.cmd	= PIN_MODE;
	packet.field.pin	= pin;
	packet.field.data1	= bOutput;
	packet.field.data2	= 0;

	if(i2c_smbus_write_block_data(fd_grvPi, 1, sizeof(packet.data), packet.data) < 0){
		std::cout << "Write data to GrovePi failed." << std::endl;
		return -1;
	}

	return 0;
}

// PWM output
int32_t pwm_out(const uint8_t pin, const uint8_t output)
{
	const uint32_t	ANALOG_WRITE	= 4;
	Packet_t		packet;

	packet.field.cmd	= ANALOG_WRITE;
	packet.field.pin	= pin;
	packet.field.data1	= output;
	packet.field.data2	= 0;

	if(i2c_smbus_write_block_data(fd_grvPi, 1, sizeof(packet.data), packet.data) < 0){
		std::cout << "Write data to GrovePi failed." << std::endl;
		return -1;
	}

	return 0;
}

// LCD output
int32_t lcd_out()
{
	Packet_t		packet;

	packet.field.cmd	= 'a';
	packet.field.pin	= 'b';
	packet.field.data1	= 'c';
	packet.field.data2	= '\0';

	if(i2c_smbus_write_block_data(fd_grvPi, 1, sizeof(packet.data), packet.data) < 0){
		std::cout << "Write data to GrovePi failed." << std::endl;
		return -1;
	}
	return 0;
}

// Timer handler
void tim_main_handler(int32_t signum)
{
	static int32_t	ChgTim	= 0;	// Change value in every 5 intrrupts

	ChgTim++;
	ChgTim	= ChgTim % (10 / INT_INTERVAL);
	if(ChgTim != 0)
		return ;

	// Change output
	pwm_out(5, pwmCnt1);
	pwm_out(6, pwmCnt2);

	// Calculate PWM output for both controller 1 and 2
	if((Notch1 <= 5) && (Notch1 > -6)){
		pwmCnt1	= (pwmCnt1 + Notch1 * 1 > RESOL) ? RESOL : (pwmCnt1 + Notch1 * 1 < 0 ? 0 : pwmCnt1 + Notch1 * 1);
	}

	if((Notch2 <= 5) && (Notch2 > -6)){
		pwmCnt2	= (pwmCnt2 + Notch2 * 1 > RESOL) ? RESOL : (pwmCnt2 + Notch2 * 1 < 0 ? 0 :  pwmCnt2 + Notch2 * 1);
	}

	return ;
}

// Start timer handler
int32_t TaskStart()
{
	// Timer task for controlling PWM output
	memset(&sa_tim, 0, sizeof(struct sigaction));
	memset(&def_tim_hdr, 0, sizeof(struct sigaction));
	sa_tim.sa_handler	= tim_main_handler;
	sa_tim.sa_flags		|= SA_RESTART;

	if(sigaction(SIGALRM, &sa_tim, &def_tim_hdr) != 0){
		printf("Registration of timer handler failed!\n");
		ReleaseAll();
		return -1;
	}

	// Timer settings
	itval.it_value.tv_sec	= 1;
	itval.it_value.tv_nsec	= 0;

	itval.it_interval.tv_sec	= 0;
	itval.it_interval.tv_nsec	= INT_INTERVAL * 10000000;	//割り込み周期

	// Create timer handler
	if(timer_create(CLOCK_REALTIME, NULL, &tid) != 0){
		printf("Timer creation failed.\n");
		ReleaseAll();
		return -1;
	}

	// Settings for interval
	if(timer_settime(tid, 0, &itval, NULL) != 0){
		printf("Set timer error.\n");
		ReleaseAll();
		return -1;
	}

	return 0;
}

// Main routine for controller
int32_t mainLoop()
{
	std::string	buff;
	int			tmp_input;

	// Get input from standard IO
	while(1){
		printf("Waiting for inputs...\n");
		std::getline(std::cin, buff);

		if(buff == "exit"){
			printf("Exitting program...\n");
			break;
		}

		//else if(strcmp(buff, "F") == 0){
		//	if((ctl_sel == 1) && (pwmCnt1 == 0)){
		//		gpio_ch1	= 18;
		//		printf("ch:%d\n", gpio_ch1);
		//	}

		//	else if((ctl_sel == 2) && (pwmCnt2 == 0)){
		//		gpio_ch2	= 24;
		//		printf("ch:%d\n", gpio_ch2);
		//	}
		//}

		//else if(strcmp(buff, "R") == 0){
		//	if((ctl_sel == 1) && (pwmCnt1 == 0)){
		//		gpio_ch1	= 23;
		//		printf("ch:%d\n", gpio_ch1);
		//	}

		//	else if((ctl_sel == 2) && (pwmCnt2 == 0)){
		//		gpio_ch2	= 25;
		//		printf("ch:%d\n", gpio_ch2);
		//	}
		//}

		else if(buff == "CTL1")
			ctl_sel	= 1;

		else if(buff == "CTL2")
			ctl_sel	= 2;

		// Get acceleration
		tmp_input	= atoi(buff.c_str());

		if(tmp_input < -5 || tmp_input > 5)
			tmp_input	= 0;

		// Update acceleration
		if(ctl_sel == 1)
			Notch1	= tmp_input;

		else if(ctl_sel == 2)
			Notch2	= tmp_input;
	}
	return 0;
}

// Release
void ReleaseAll()
{
	// All output must be turned off
	pwm_out(5, 0);
	pwm_out(6, 0);

	// Release timer handler
	timer_delete(tid);

	// Close I2C
	close(fd_grvPi);
	close(fd_disp);

	// Release signal handler
	sigaction(SIGALRM, &def_tim_hdr, NULL);
}

