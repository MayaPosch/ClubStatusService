/*
	wiringPi.cpp - Mock implementation of the wiringPi header for integration testing.
	
	Revision 0
	
	Features:
			- 
			
	Notes:
			-
			
	2018/06/04, Maya Posch	
*/


#include "wiringPi.h"


#include <fstream>
#include <memory>
#include <iostream>


WiringTimer::WiringTimer() {
	// Defaults.
	triggerCnt = 0;
	isrcb_lock = 0;
	isrcb_status = 0;
	isr_lock_set = false;
	isr_status_set = false;
	
	// Start a timer which regularly triggers the interrupts for the two switch GPIO pins.
	wiringTimer = new Poco::Timer(10 * 1000, 10 * 1000); // 10 s start delay & interval.
	cb = new Poco::TimerCallback<WiringTimer>(*this, &WiringTimer::trigger);
}
	
WiringTimer::~WiringTimer() {
	delete wiringTimer;
	delete cb;
}

void WiringTimer::start() {
	wiringTimer->start(*cb);
}
	
void WiringTimer::trigger(Poco::Timer &t) {
	// Pin 4: Lock.
	// Pin 25: Status. 
	// 
	// Timing:
	// The timer has a 10 second start delay.
	// 0. Trigger the lock pin.
	// 1. followed by the status pin 10 seconds later. 
	// 2. Wait ten seconds, close status.
	// 3. wait then, close lock. Wait 10s, repeat.
	if (triggerCnt == 0) {
		// Write pin value.
		char val = 0x00; // locked false.
		std::ofstream LOCKVAL;
		LOCKVAL.open("lockval", std::ios_base::binary | std::ios_base::trunc);
		LOCKVAL.put(val);
		LOCKVAL.close();
		
		// Call callback.
		isrcb_lock();
		
		++triggerCnt;
	}
	else if (triggerCnt == 1) {
		// Write pin value.
		char val = 0x01; // off false.
		std::ofstream STATUSVAL;
		STATUSVAL.open("statusval", std::ios_base::binary | std::ios_base::trunc);
		STATUSVAL.put(val);
		STATUSVAL.close();
		
		// Call callback.
		isrcb_status();
		
		++triggerCnt;
	}
	else if (triggerCnt == 2) {
		// Write pin value.
		char val = 0x00; // off true.
		std::ofstream STATUSVAL;
		STATUSVAL.open("statusval", std::ios_base::binary | std::ios_base::trunc);
		STATUSVAL.put(val);
		STATUSVAL.close();
		
		// Call callback.
		isrcb_status();
		
		++triggerCnt;
	}
	else if (triggerCnt == 3) {
		// Write pin value.
		char val = 0x01; // locked true.
		std::ofstream LOCKVAL;
		LOCKVAL.open("lockval", std::ios_base::binary | std::ios_base::trunc);
		LOCKVAL.put(val);
		LOCKVAL.close();
		
		// Call callback.
		isrcb_lock();
		
		triggerCnt = 0; // reset
	}
}


// Globals
namespace Wiring {
	std::unique_ptr<WiringTimer> wt;
	bool initialized = false;
}


int wiringPiSetup() {
	// Set the pin's file to the default value.
	char val = 0x01;
	std::ofstream LOCKVAL;
	std::ofstream STATUSVAL;
	LOCKVAL.open("lockval", std::ios_base::binary | std::ios_base::trunc);
	STATUSVAL.open("statusval", std::ios_base::binary | std::ios_base::trunc);
	LOCKVAL.put(val);
	val = 0x00;
	STATUSVAL.put(val);
	LOCKVAL.close();
	STATUSVAL.close();
	
	Wiring::wt = std::make_unique<WiringTimer>();
	Wiring::initialized = true;
	 
	return 0;
}

 
void pinMode(int pin, int mode) {
	// We can ignore any calls this function in the mock, because the behaviour of the virtual pins
	// is hard-coded in the test.
	
	return;
}

 
void pullUpDnControl(int pin, int pud) {
	// As these are virtual pins and behaviour set by the test logic, we can ignore calls to this
	// function as well.
	
	return;
}

 
int digitalRead(int pin) {
	// Return the current value for the virtual pin.	
	// Open the respective pin's file and read out its value (first character).
	// Return either a 0 or 1.
	if (pin == 4) {
		std::ifstream LOCKVAL;
		LOCKVAL.open("lockval", std::ios_base::binary);
		int val = LOCKVAL.get();
		LOCKVAL.close();
		
		//std::cout << "TEST: Reading pin 0: " << val << std::endl;
		
		return val;
	}
	else if (pin == 25) {
		std::ifstream STATUSVAL;
		STATUSVAL.open("statusval", std::ios_base::binary);
		int val = STATUSVAL.get();
		STATUSVAL.close();
		
		//std::cout << "TEST: Reading pin 7: " << val << std::endl;
		
		return val;
	}
	
	return 0;
}

 
int wiringPiISR(int pin, int mode, void (*function)(void)) {
	if (!Wiring::initialized) { 
		return 1;
	}
	
	// Registers the function pointer to call when the interrupt for the virtual pin gets triggered.
	if (pin == 4) { 
		Wiring::wt->isrcb_lock = function;
		Wiring::wt->isr_lock_set = true;
	}
	else if (pin == 25) {
		Wiring::wt->isrcb_status = function;
		Wiring::wt->isr_status_set = true;
	}
	
	if (Wiring::wt->isr_lock_set && Wiring::wt->isr_status_set) {
		// Both interrupts have a callback set.
		
		// Start the interrupt timer.
		Wiring::wt->start();
	}
	
	return 0;
}
