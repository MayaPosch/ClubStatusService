/*
	wiringPiI2C.h - Implementation for the mock wiringPi I2C implementation.
	
	Revision 0
	
	Features:
			- 
			
	Notes:
			-
			
	2018/06/04, Maya Posch
*/


#include "wiringPiI2C.h"


int wiringPiI2CSetup(const int devId) {
	// Device ID is ignored as the test hard-codes the device.
	
	return 0;
}


int wiringPiI2CWriteReg8(int fd, int reg, int data) {
	// Write the provided data to the specified register of the simulated device.
	
	// TODO:
	
	return 0;
}
