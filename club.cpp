/*
	club.cpp - Implementation of the Club class.
	
	Revision 0
	
	Notes:
			- 
			
	2018/02/28, Maya Posch
*/


#include "club.h"

#include <iostream>

using namespace std;


#include <Poco/NumberFormatter.h>

using namespace Poco;


#include "listener.h"


#define REG_INPUT_PORT0              0x00
#define REG_INPUT_PORT1              0x01
#define REG_OUTPUT_PORT0             0x02
#define REG_OUTPUT_PORT1             0x03
#define REG_POL_INV_PORT0            0x04
#define REG_POL_INV_PORT1            0x05
#define REG_CONF_PORT0               0x06
#define REG_CONG_PORT1               0x07
#define REG_OUT_DRV_STRENGTH_PORT0_L 0x40
#define REG_OUT_DRV_STRENGTH_PORT0_H 0x41
#define REG_OUT_DRV_STRENGTH_PORT1_L 0x42
#define REG_OUT_DRV_STRENGTH_PORT1_H 0x43
#define REG_INPUT_LATCH_PORT0        0x44
#define REG_INPUT_LATCH_PORT1        0x45
#define REG_PUD_EN_PORT0             0x46
#define REG_PUD_EN_PORT1             0x47
#define REG_PUD_SEL_PORT0            0x48
#define REG_PUD_SEL_PORT1            0x49
#define REG_INT_MASK_PORT0           0x4A
#define REG_INT_MASK_PORT1           0x4B
#define REG_INT_STATUS_PORT0         0x4C
#define REG_INT_STATUS_PORT1         0x4D
#define REG_OUTPUT_PORT_CONF         0x4F

#define RELAY_POWER 0
#define RELAY_GREEN 1
#define RELAY_YELLOW 2
#define RELAY_RED 3


// Static initialisations.
bool Club::clubOff;
bool Club::clubLocked;
bool Club::powerOn;
Thread Club::updateThread;
ClubUpdater Club::updater;
bool Club::relayActive;
uint8_t Club::relayAddress;
string Club::mqttTopic;
Listener* Club::mqtt;

Condition Club::clubCnd;
Mutex Club::clubCndMutex;
Mutex Club::logMutex;
bool Club::clubChanged = false;
bool Club::running = false;
bool Club::clubIsClosed = true;
bool Club::firstRun = true;
bool Club::lockChanged = false;
bool Club::statusChanged = false;
bool Club::previousLockValue = false;
bool Club::previousStatusValue = false;


// === POWER TIMER ===
// --- CONSTRUCTOR ---
/* PowerTimer::PowerTimer() {
	// Setup.
	cb = new TimerCallback<ClubUpdater>(*this, &ClubUpdater::setPowerState);
}


// --- ADD TIMER ---
void PowerTimer::addTimer() {
	// Increase the 
} */


// === CLUB UPDATER ===
// --- RUN ---
void ClubUpdater::run() {
	// Defaults.
	regDir0 = 0x00;
	regOut0 = 0x00;
	Club::powerOn = false;
	powerTimerActive = false;
	powerTimerStarted = false;
	cb = new TimerCallback<ClubUpdater>(*this, &ClubUpdater::setPowerState);
	timer = new Timer(10 * 1000, 0); // 10 second start interval.
	
	if (Club::relayActive) {
		// First pulse the i2c's SCL a few times in order to unlock any frozen devices.
		// TODO:
	
		// Start the i2c bus and check for presence of the relay board.
		// i2c address: 0x20.
		Club::log(LOG_INFO, "ClubUpdater: Starting i2c relay device.");
		i2cHandle = wiringPiI2CSetup(Club::relayAddress);
		if (i2cHandle == -1) {
			Club::log(LOG_FATAL, string("ClubUpdater: error starting i2c relay device."));
			return;
		}
	
		// Configure the GPIO expander.
		// Register is 0x06, the pin direction configuration register.
		// FIXME: Skip this step is the club is already open? i2c device should be configured.
		wiringPiI2CWriteReg8(i2cHandle, REG_CONF_PORT0, 0x00);		// All pins as output.
		wiringPiI2CWriteReg8(i2cHandle, REG_OUTPUT_PORT0, 0x00);	// All pins low.
		
		Club::log(LOG_INFO, "ClubUpdater: Finished configuring the i2c relay device's registers.");
	}
	
	// Perform initial update for club status.
	updateStatus();
	
	Club::log(LOG_INFO, "ClubUpdater: Initial status update complete.");
	Club::log(LOG_INFO, "ClubUpdater: Entering waiting condition.");
	
	// Start waiting using the condition variable until signalled by one of the interrupts.	
	while (Club::running) {
		Club::clubCndMutex.lock();
		if (!Club::clubCnd.tryWait(Club::clubCndMutex, 60 * 1000)) { // Wait for a minute.
			Club::clubCndMutex.unlock();
			if (!Club::clubChanged) { continue; } // Timed out, still no change. Wait again.
		}
		else {
			Club::clubCndMutex.unlock();
		}
		
		updateStatus();
	}
}


// --- UPDATE STATUS ---
void ClubUpdater::updateStatus() {
	// Adjust the club status using the values that got updated by the interrupt handler(s).
	Club::clubChanged = false;
	
	if (Club::lockChanged) {
		string state = (Club::clubLocked) ? "locked" : "unlocked";
		Club::log(LOG_INFO, string("ClubUpdater: lock status changed to ") + state);
		Club::lockChanged = false;
		
		if (Club::clubLocked == Club::previousLockValue) {
			Club::log(LOG_WARNING, string("ClubUpdater: lock interrupt triggered, but value hasn't changed. Aborting."));
			return;
		}
		
		Club::previousLockValue = Club::clubLocked;
	}
	else if (Club::statusChanged) {		
		string state = (Club::clubOff) ? "off" : "on";
		Club::log(LOG_INFO, string("ClubUpdater: status switch status changed to ") + state);
		Club::statusChanged = false;
		
		if (Club::clubOff == Club::previousStatusValue) {
			Club::log(LOG_WARNING, string("ClubUpdater: status interrupt triggered, but value hasn't changed. Aborting."));
			return;
		}
		
		Club::previousStatusValue = Club::clubOff;
	}
	else if (Club::firstRun) {
		Club::log(LOG_INFO, string("ClubUpdater: starting initial update run."));
		Club::firstRun = false;
	}
	else {
		Club::log(LOG_ERROR, string("ClubUpdater: update triggered, but no change detected. Aborting."));
		return;
	}
	
	// Check whether we are opening or closing the club.
	if (Club::clubIsClosed && !Club::clubOff) {
		Club::clubIsClosed = false;
		
		Club::log(LOG_INFO, string("ClubUpdater: Opening club."));
		
		// Power has to be on. Write to relay with a 10 second delay.
		Club::powerOn = true;
		
		// Wait on condition variable if another timer is active.
		/* if (powerTimerActive) {
			timerMutex.lock();
			if (!timerCnd.tryWait(timerMutex, 30 * 1000)) {
				// Time-out, cancel this timer.
				timerMutex.unlock();
				Club::log(LOG_ERROR, "ClubUpdater: 30s time-out on new power timer. Cancelling...");
				return;
			}
			
			timerMutex.unlock();
		} */
		
		//while (powerTimerActive) { Thread::sleep(500); }
		
		Club::log(LOG_INFO, string("ClubUpdater: Finished sleeping."));
		
		//timer = new Timer(10 * 1000, 0); // 10 second start interval.
		try {
			if (!powerTimerStarted) {
				timer->start(*cb);
				powerTimerStarted = true;
			}
			else { timer->restart(); }
		}
		catch (Poco::IllegalStateException &e) {
			Club::log(LOG_ERROR, "ClubUpdater: IllegalStateException on timer start: " + e.message());
			return;
		}
		catch (...) {
			Club::log(LOG_ERROR, "ClubUpdater: Unknown exception on timer start.");
			return;
		}
		
		powerTimerActive = true;
		
		Club::log(LOG_INFO, "ClubUpdater: Started power timer...");
		
		// Send MQTT notification. Payload is '1' (true) as ASCII.
		char msg = { '1' };
		Club::mqtt->sendMessage(Club::mqttTopic, &msg, 1);
		
		Club::log(LOG_DEBUG, "ClubUpdater: Sent MQTT message.");
	}
	else if (!Club::clubIsClosed && Club::clubOff) {
		Club::clubIsClosed = true;
		
		Club::log(LOG_INFO, string("ClubUpdater: Closing club."));
		
		// Power has to be off. Write to relay with a 10 second delay.
		Club::powerOn = false;
		
		// Wait on condition variable if another timer is active.
		/* if (powerTimerActive) {
			timerMutex.lock();
			if (!timerCnd.tryWait(timerMutex, 30 * 1000)) {
				// Time-out, cancel this timer.
				timerMutex.unlock();
				Club::log(LOG_ERROR, "ClubUpdater: 30s time-out on new power timer. Cancelling...");
				return;
			}
		
			timerMutex.unlock();
		} */
		
		//while (powerTimerActive) { Thread::sleep(500); }
		
		Club::log(LOG_INFO, string("ClubUpdater: Finished sleeping."));
		
		//timer = new Timer(10 * 1000, 0); // 10 second start interval.
		//Club::log(LOG_INFO, string("ClubUpdater: Created new timer."));
		try {
			if (!powerTimerStarted) {
				timer->start(*cb);
				powerTimerStarted = true;
			}
			else { timer->restart(); }
		}
		catch (Poco::IllegalStateException &e) {
			Club::log(LOG_ERROR, "ClubUpdater: IllegalStateException on timer start: " + e.message());
			return;
		}
		catch (...) {
			Club::log(LOG_ERROR, "ClubUpdater: Unknown exception on timer start.");
			return;
		}
		
		Club::log(LOG_INFO, string("ClubUpdater: Started timer."));
		powerTimerActive = true;
		
		Club::log(LOG_INFO, "ClubUpdater: Started power timer...");
		
		// Send MQTT notification. Payload is '0' (false) as ASCII.
		char msg = { '0' };
		Club::mqtt->sendMessage(Club::mqttTopic, &msg, 1);
		
		Club::log(LOG_DEBUG, "ClubUpdater: Sent MQTT message.");
	}
	
	// Set the new colours on the traffic light.
	if (Club::clubOff) {
		Club::log(LOG_INFO, string("ClubUpdater: New lights, clubstatus off."));
		
		mutex.lock();
		string state = (Club::powerOn) ? "on" : "off";
		if (powerTimerActive) {
			Club::log(LOG_DEBUG, string("ClubUpdater: Power timer active, inverting power state from: ") + state);
			regOut0 = !Club::powerOn; // Take the inverse of what the timer callback will set.
		}
		else {
			Club::log(LOG_DEBUG, string("ClubUpdater: Power timer not active, using current power state: ") + state);
			regOut0 = Club::powerOn; // Use the current power state value.
		}
		
		if (Club::clubLocked) {
			// Light: Red.
			Club::log(LOG_INFO, string("ClubUpdater: Red on."));
			regOut0 |= (1UL << RELAY_RED); 
		} 
		else {
			// Light: Yellow.
			Club::log(LOG_INFO, string("ClubUpdater: Yellow on."));
			regOut0 |= (1UL << RELAY_YELLOW);
		} 
		
		Club::log(LOG_DEBUG, "ClubUpdater: Changing output register to: 0x" + NumberFormatter::formatHex(regOut0));
		
		writeRelayOutputs();
		mutex.unlock();
	}
	else { // Club on.
		Club::log(LOG_INFO, string("ClubUpdater: New lights, clubstatus on."));
		
		mutex.lock();
		string state = (Club::powerOn) ? "on" : "off";
		if (powerTimerActive) {
			Club::log(LOG_DEBUG, string("ClubUpdater: Power timer active, inverting power state from: ") + state);
			regOut0 = !Club::powerOn; // Take the inverse of what the timer callback will set.
		}
		else {
			Club::log(LOG_DEBUG, string("ClubUpdater: Power timer not active, using current power state: ") + state);
			regOut0 = Club::powerOn; // Use the current power state value.
		}
		
		if (Club::clubLocked) {
			// Light: Yellow and Red.
			Club::log(LOG_INFO, string("ClubUpdater: Yellow & Red on."));
			regOut0 |= (1UL << RELAY_YELLOW);
			regOut0 |= (1UL << RELAY_RED);
		}
		else {
			// Light: Green.
			Club::log(LOG_INFO, string("ClubUpdater: Green on."));
			regOut0 |= (1UL << RELAY_GREEN);
		}
		
		Club::log(LOG_DEBUG, "ClubUpdater: Changing output register to: 0x" + NumberFormatter::formatHex(regOut0));
		
		writeRelayOutputs();
		mutex.unlock();
	}
}


// --- WRITE RELAY OUTPUTS ---
void ClubUpdater::writeRelayOutputs() {
	// Write the current state of the locally saved output 0 register contents to the device.
	wiringPiI2CWriteReg8(i2cHandle, REG_OUTPUT_PORT0, regOut0);
	
	Club::log(LOG_DEBUG, "ClubUpdater: Finished writing relay outputs with: 0x" 
			+ NumberFormatter::formatHex(regOut0));
}


// --- SET POWER STATE ---
void ClubUpdater::setPowerState(Timer &t) {
	Club::log(LOG_INFO, string("ClubUpdater: setPowerState called."));
	
	// Update register with current power state, then update remote device.
	mutex.lock();
	if (Club::powerOn) { regOut0 |= (1UL << RELAY_POWER); }
	else { regOut0 &= ~(1UL << RELAY_POWER); }
	
	Club::log(LOG_DEBUG, "ClubUpdater: Writing relay with: 0x" + NumberFormatter::formatHex(regOut0));
	
	writeRelayOutputs();
	
	//delete timer;
	powerTimerActive = false;
	mutex.unlock();
	
	//timerCnd.signal();
}


// === CLUB ===
// --- START ---
bool Club::start(bool relayactive, uint8_t relayaddress, string topic) {
	Club::log(LOG_INFO, "Club: starting up...");
	// Defaults.
	relayActive = relayactive;
	relayAddress = relayaddress;
	mqttTopic = topic;
	
	// Start the WiringPi framework.
	// We assume that this service is running inside the 'gpio' group.
	// Since we use this setup method we are expected to use WiringPi pin numbers.
	wiringPiSetup();
	
	Club::log(LOG_INFO,  "Club: Finished wiringPi setup.");
	
	// Configure and read current GPIO inputs.
	// Lock: 	BCM GPIO 17 (pin 11, GPIO 0)
	// Status: 	BCM GPIO 4 (pin 7, GPIO 7)
	pinMode(0, INPUT);
	pinMode(7, INPUT);
	pullUpDnControl(0, PUD_DOWN);
	pullUpDnControl(7, PUD_DOWN);
	clubLocked = digitalRead(0);
	clubOff = !digitalRead(7);
	
	previousLockValue = clubLocked;
	previousStatusValue = clubOff;
	
	Club::log(LOG_INFO, "Club: Finished configuring pins.");
	
	// Register GPIO interrupts for the lock and club status switches.
	wiringPiISR(0, INT_EDGE_BOTH, &lockISRCallback);
	wiringPiISR(7, INT_EDGE_BOTH, &statusISRCallback);
	
	Club::log(LOG_INFO, "Club: Configured interrupts.");
	
	// Start update thread.
	running = true;
	updateThread.start(updater);
	
	Club::log(LOG_INFO, "Club: Started update thread.");
	
	return true;
}


// --- STOP ---
void Club::stop() {
	running = false;
	
	// unregister the GPIO interrupts.
	// TODO: research whether clean-up is needed with a restart instead of an application shutdown.
	
	// Stop the i2c bus.
	// FIXME: no method found in WiringPi library. Unneeded?
}


// --- LOCK ISR CALLBACK ---
void Club::lockISRCallback() {
	// Update the current lock GPIO value.
	clubLocked = digitalRead(0);
	lockChanged = true;
	
	// Trigger condition variable.
	clubChanged = true;
	clubCnd.signal();
}


// --- STATUS ISR CALLBACK ---
void Club::statusISRCallback() {
	// Update the current status GPIO value.
	clubOff = !digitalRead(7);
	statusChanged = true;
	
	// Trigger condition variable.
	clubChanged = true;
	clubCnd.signal();
}


// --- LOG ---
void Club::log(Log_level level, string msg) {
	logMutex.lock();
	switch (level) {
		case LOG_FATAL: {
			cerr << "FATAL:\t" << msg << endl;
			string message = string("ClubStatus FATAL: ") + msg;
			mqtt->sendMessage("/log/fatal", message);
			break;
		}
		case LOG_ERROR: {
			cerr << "ERROR:\t" << msg << endl;
			string message = string("ClubStatus ERROR: ") + msg;
			mqtt->sendMessage("/log/error", message);
			break;
		}
		case LOG_WARNING: {
			cerr << "WARNING:\t" << msg << endl;
			string message = string("ClubStatus WARNING: ") + msg;
			mqtt->sendMessage("/log/warning", message);
			break;
		}
		case LOG_INFO: {
			cout << "INFO: \t" << msg << endl;
			string message = string("ClubStatus INFO: ") + msg;
			mqtt->sendMessage("/log/info", message);
			break;
		}
		case LOG_DEBUG: {
			cout << "DEBUG:\t" << msg << endl;
			string message = string("ClubStatus DEBUG: ") + msg;
			mqtt->sendMessage("/log/debug", message);
			break;
		}
		default:
			// Shouldn't get here.
			break;
	}
	
	logMutex.unlock();
}
