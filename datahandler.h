/*
	datahandler.h - Header file for the DataHandler class.
	
	Revision 0
	
	Notes:
			- 
			
	2018/02/28, Maya Posch
*/


#ifndef DATAHANDLER_H
#define DATAHANDLER_H

#include <iostream>
#include <vector>

using namespace std;

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/URI.h>
#include <Poco/File.h>

using namespace Poco::Net;
using namespace Poco;


class DataHandler: public HTTPRequestHandler { 
public: 
	void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) {
		// Process the request. A request for data (HTML, JS, etc.) is returned
		// with either the file & a 200 response, or a 404.
		
		cout << "DataHandler: Request from " + request.clientAddress().toString() << endl;
		
		// Get the path and check for any endpoints to filter on.
		URI uri(request.getURI());
		string path = uri.getPath();
		
		// No endpoint found, continue serving the requested file.
		string fileroot = "htdocs";
		if (path.empty() || path == "/") { path = "/index.html"; }
		
		File file(fileroot + path);
		
		cout << "DataHandler: Request for " << file.path() << endl;
		
		if (!file.exists() || file.isDirectory()) {
			// Return a 404.
			response.setStatus(HTTPResponse::HTTP_NOT_FOUND);
			ostream& ostr = response.send();
			ostr << "File Not Found.";
			return;
		}
		
		// Determine file type.
		string::size_type idx = path.rfind('.');
		string ext = "";
		if (idx != std::string::npos) {
			ext = path.substr(idx + 1);
		}
		
		string mime = "text/plain";
		if (ext == "html") { mime = "text/html"; }
		if (ext == "css") { mime = "text/css"; }
		else if (ext == "js") { mime = "application/javascript"; }
		else if (ext == "zip") { mime = "application/zip"; }
		else if (ext == "json") { mime = "application/json"; }
		else if (ext == "png") { mime = "image/png"; }
		else if (ext == "jpeg" || ext == "jpg") { mime = "image/jpeg"; }
		else if (ext == "gif") { mime = "image/gif"; }
		else if (ext == "svg") { mime = "image/svg"; }
		
		try {
			response.sendFile(file.path(), mime);
		}
		catch (FileNotFoundException &e) {
			cout << "File not found exception triggered...\n";
			cerr << e.displayText() << endl;
			
			// Return a 404.
			response.setStatus(HTTPResponse::HTTP_NOT_FOUND);
			ostream& ostr = response.send();
			ostr << "File Not Found.";
			return;
		}
		catch (OpenFileException &e) {
			cout << "Open file exception triggered...\n";
			cerr << e.displayText() << endl;
			
			// Return a 500.
			response.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
			ostream& ostr = response.send();
			ostr << "Internal Server Error. Couldn't open file.";
			return;
		}
	}
};

#endif
