#ifndef OPI_MYSQL_H_
#define OPI_MYSQL_H_

#include <stdint.h>
#include <mysql/mysql.h>
#include <string.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>
#include <ctime>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> 
#include <mutex>
#include "../SKO_Utilities/OPI_Clock.h"
#include "../SKO_Utilities/OPI_Sleep.h"

class OPI_MYSQL
{
	public:
		std::mutex queryMutex, cleanMutex;
		//constructor
		OPI_MYSQL();

		//destructor
		~OPI_MYSQL(); 

		//error reporting
		int getStatus();
		bool reconnect();

		//connection factory
		bool connect(std::string server, std::string database, int port, std::string user, std::string password);

		//query
		int query(std::string statement);
		int query(std::string statement, bool once);

		//clean
		std::string _clean(const char* dataIn, int len);  
		std::string clean(std::string dirty_str);

		//get a row/advance
		void nextRow();    
		char * getData(int i);
		int getNumOfFields();
		int count();

		//get data back from results
		std::string getString(unsigned int i);        
		int getInt(int i);
		float getFloat(int i);

	private:

		//clean up
		void cleanup();
		//logging
		void log(std::string output);
		int logCount;

		//Database connection
		MYSQL * conn = new MYSQL();

		//login values
		char * server = new char[100];
		char * database = new char[100];
		char * user = new char[100];
		char * password = new char [100]; 
		int port = 3306;

		//results
		MYSQL_RES *result;
		MYSQL_ROW row;
		unsigned long int *length;
		int numOfFields;
		int rowCount;
};

//logging
static std::ofstream logFile;
            
#endif
