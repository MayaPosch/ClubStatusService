/*
	club.h - Header file for the Club class.
	
	Revision 0
	
	Notes:
			- 
			
	2018/02/28, Maya Posch
*/

#ifndef CLUB_H
#define CLUB_H


#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Timestamp.h>
#include <Poco/Timer.h>
#include <Poco/Mutex.h>
#include <Poco/Condition.h>
#include <Poco/Thread.h>

using namespace Poco;
using namespace Poco::Net;

#include <string>
#include <vector>
#include <map>
#include <queue>

using namespace std;

// Raspberry Pi GPIO, i2c, etc. functionality.
#include <wiringPi.h>
#include <wiringPiI2C.h>


enum Log_level {
	LOG_FATAL = 1,
	LOG_ERROR = 2,
	LOG_WARNING = 3,
	LOG_INFO = 4,
	LOG_DEBUG = 5
};


class Listener;


// TODO: refactor the power timer into its own module.
/* class PowerTimer {
	Condition* cnd;
	
public:
	PowerTimer(Condition* cnd);
	void start();
}; */


class ClubUpdater : public Runnable {
	TimerCallback<ClubUpdater>* cb;
	uint8_t regDir0;
	uint8_t regOut0;
	int i2cHandle;
	Timer* timer;
	Mutex mutex;
	Mutex timerMutex;
	Condition timerCnd;
	bool powerTimerActive;
	bool powerTimerStarted;
	
public:
	void run();
	void updateStatus();
	void writeRelayOutputs();
	void setPowerState(Timer &t);
};


class Club {
	static Thread updateThread;
	static ClubUpdater updater;
	
	static void lockISRCallback();
	static void statusISRCallback();
	
public:
	static bool clubOff;
	static bool clubLocked;
	static bool powerOn;
	static Listener* mqtt;
	static bool relayActive;
	static uint8_t relayAddress;
	static string mqttTopic;	// Topic we publish status updates on.
	
	static Condition clubCnd;
	static Mutex clubCndMutex;
	static Mutex logMutex;
	static bool clubChanged ;
	static bool running;
	static bool clubIsClosed;
	static bool firstRun;
	static bool lockChanged;
	static bool statusChanged;
	static bool previousLockValue;
	static bool previousStatusValue;
	
	static bool start(bool relayactive, uint8_t relayaddress, string topic);
	static void stop();
	static void togglePower();
	static void setRelay();
	static void log(Log_level level, string msg);
};

#endif
