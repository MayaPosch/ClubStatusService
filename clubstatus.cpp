/*
	clubstatus.cpp - Clubstatus service for monitoring a club room.
	
	Revision 0
	
	Features:
				- Interrupt-based switch monitoring.
				- REST API 
				
	Notes:
				- First argument to main is the location of the configuration
					file in INI format.
				
	2018/02/28, Maya Posch
*/


#include "listener.h"

#include <iostream>
#include <string>

using namespace std;

#include <Poco/Util/IniFileConfiguration.h>
#include <Poco/AutoPtr.h>
#include <Poco/Net/HTTPServer.h>

using namespace Poco::Util;
using namespace Poco;
using namespace Poco::Net;

#include "httprequestfactory.h"
#include "club.h"


int main(int argc, char* argv[]) {
	cout << "Starting ClubStatus server...\n";
	
	int rc;
	mosqpp::lib_init();
	
	cout << "Initialised C++ Mosquitto library.\n";
	
	// Read configuration.
	string configFile;
	if (argc > 1) { configFile = argv[1]; }
	else { configFile = "config.ini"; }
	
	AutoPtr<IniFileConfiguration> config;
	try {
		config = new IniFileConfiguration(configFile);
	}
	catch (Poco::IOException &e) {
		cerr << "I/O exception when opening configuration file: " << configFile << ". Aborting..." << endl;
		return 1;
	}
	
	string mqtt_host = config->getString("MQTT.host", "localhost");
	int mqtt_port = config->getInt("MQTT.port", 1883);
	string mqtt_user = config->getString("MQTT.user", "");
	string mqtt_pass = config->getString("MQTT.pass", "");
	string mqtt_topic = config->getString("MQTT.clubStatusTopic", "/public/clubstatus");
	bool relayactive = config->getBool("Relay.active", true);
	uint8_t relayaddress = config->getInt("Relay.address", 0x20);
	
	// Start the MQTT listener.
	Listener listener("ClubStatus", mqtt_host, mqtt_port, mqtt_user, mqtt_pass);
	
	cout << "Created listener, entering loop...\n";
	
	// Initialise the HTTP server.
	// TODO: catch IOException if HTTP port is already taken.
	UInt16 port = config->getInt("HTTP.port", 80);
	HTTPServerParams* params = new HTTPServerParams;
	params->setMaxQueued(100);
	params->setMaxThreads(10);
	HTTPServer httpd(new RequestHandlerFactory, port, params);
	try {
		httpd.start();
	}
	catch (Poco::IOException &e) {
		cerr << "I/O Exception on HTTP server: port already in use?" << endl;
		return 1;
	}
	catch (...) {
		cerr << "Exception thrown for HTTP server start. Aborting." << endl;
		return 1;
	}
	
	cout << "Initialised the HTTP server." << endl;
	
	// Initialise the GPIO and i2c-related handlers.
	Club::mqtt = &listener;
	Club::start(relayactive, relayaddress, mqtt_topic);
	
	//cout << "Started the Club." << endl;
	
	while(1) {
		rc = listener.loop();
		if (rc){
			cout << "Disconnected. Trying to reconnect...\n";
			listener.reconnect();
		}
	}
	
	cout << "Cleanup...\n";

	mosqpp::lib_cleanup();
	httpd.stop();
	Club::stop();

	return 0;
}
