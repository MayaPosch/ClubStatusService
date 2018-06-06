/*
	listener.cpp - Handle MQTT events.
	
	Revision 0
	
	Features:
				- 
				
	Notes:
				-
				
	2018/02/28, Maya Posch
*/

#include "listener.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#include <Poco/StringTokenizer.h>
#include <Poco/String.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Thread.h>
#include <Poco/NumberFormatter.h>

using namespace Poco;


// --- CONSTRUCTOR ---
Listener::Listener(string clientId, string host, int port, string user, string pass) : mosquittopp(clientId.c_str()) {
	int keepalive = 60;
	username_pw_set(user.c_str(), pass.c_str());
	connect(host.c_str(), port, keepalive);

	// Done for now.
}


// --- DECONSTRUCTOR ---
Listener::~Listener() {
	//
}


// --- ON CONNECT ---
void Listener::on_connect(int rc) {
	cout << "Connected. Subscribing to topics...\n";
	
	// Check code.
	if (rc == 0) {
		// Subscribe to desired topics.
		string topic = "/club/status";
		subscribe(0, topic.c_str(), 1);
	}
	else {
		// handle.
		cerr << "Connection failed. Aborting subscribing.\n";
	}
}


// --- ON MESSAGE ---
void Listener::on_message(const struct mosquitto_message* message) {
	string topic = message->topic;
	string payload = string((const char*) message->payload, message->payloadlen);
	
	if (topic == "/club/status") {
		// No payload. Just publish the current club status.
		string topic = "/club/status/response";
		char payload[] = { 0x10 }; // TODO
		publish(0, topic.c_str(), 1, payload, 1); // QoS 1.	
	}
}


// --- ON SUBSCRIBE ---
void Listener::on_subscribe(int mid, int qos_count, const int* granted_qos) {
	// Report success, with details.
}


// --- SEND MESSAGE ---
void Listener::sendMessage(string topic, string &message) {
	//cout << "Listener: Sending MQTT message with length " << message.length() 
	//		<< " on topic: " << topic << endl;
	//cout << "Listener: Topic " << topic << ", message: 0x" << NumberFormatter::formatHex(*(uint8_t*)  message.data()) 
	//		<< endl;
	publish(0, topic.c_str(), message.length(), message.c_str(), true);
}


// --- SEND MESSAGE ---
void Listener::sendMessage(string &topic, char* message, int msgLength) {
	//cout << "Listener: Sending MQTT message with length " << msgLength 
	//		<< " on topic: " << topic << endl;
	//cout << "Listener: Topic " << topic << ", message: 0x" 
	//		<< NumberFormatter::formatHex((uint8_t)  *message) << endl;
	publish(0, topic.c_str(), msgLength, message, true);
}
