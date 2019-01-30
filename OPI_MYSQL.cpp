#include "OPI_MYSQL.h"
            
void OPI_MYSQL::cleanup()
{ 
	log("cleanup MYSQL...\n");
	//just initialize the connection
	if (result){
		   log("result is true.\nmysql_free_result(result);\n");
		   mysql_free_result(result);
	} else { 
			log("result is NULL.\n");
	}

	if (conn){
	   log("connection is true. checking result.\n");
	   log("mysql_close(conn);\n");
	   mysql_close(conn);
	} 
	else {
	   log("conn is NULL.\n");
	}

	log("result = NULL;\n");
	result = NULL;
}
 

OPI_MYSQL::OPI_MYSQL()
{
	conn = new MYSQL();

	//just initialize the connection
	if (mysql_init(conn) == NULL){
		printf("\nFailed to initate MySQL connection\n");
		exit(1);
	} else {
		printf("\nSucceeded to initate MySQL connection\n");
	}
}

OPI_MYSQL::~OPI_MYSQL()
{
	printf("destructing OPI_MYSQL...\n");
	delete server;
	delete user;
	delete database;
	delete password;
}


//log file
void OPI_MYSQL::log(std::string output)
{
	//logFile << "[" << Clock() << "] " << output << std::endl;
	printf("[OPI_MYSQL] %s\n", output.c_str());
	logCount++;
}

bool OPI_MYSQL::connect(std::string server_in, std::string database_in, std::string user_in, std::string password_in)
{
	//return value
	bool success = false;

	//save values for reconnect
	std::strcpy(server, server_in.c_str());
	std::strcpy(database, database_in.c_str());
	std::strcpy(user, user_in.c_str());
	std::strcpy(password, password_in.c_str());
       
	printf("Set OPI_MYSQL server details [%s][%s][%s][%s]\n", server, database, user, password);

	conn = mysql_init(NULL); 

	printf("mysql_init() called\n");


	//make the connection
	//return reconnect();
	//MYSQL * returnVal = 
	if (mysql_real_connect(
		(MYSQL *)conn, 
		server, 
		user, 
		password, 
		database, 
		3306, 
		NULL, 
		(unsigned long)0
	))
	{
		success = true;
	}
	else
	{
		log("[FATAL ERROR IN MYSQL]\n> " + std::string(mysql_error(conn)) + "\n");
	}
    
	mysql_close(conn);
       
    return success;
}


bool OPI_MYSQL::reconnect()
{
	conn = mysql_init(NULL);    
	//printf("mysql_real_connect((MYSQL *)conn, %s, %s, %s, %s, 0, NULL, (unsigned long)0);", server, user, password, database);
	if (!mysql_real_connect((MYSQL *)conn, server, user, password, database, 3306, NULL, (unsigned long)0)) 
	{
		//if it is NULL there is some error
		log("[FATAL ERROR IN MYSQL] Error was:\n> " + std::string(mysql_error(conn)) + "*");
		return false;
	}
	
	return true;
}

int OPI_MYSQL::getStatus()
{
    return (int)mysql_errno(conn);
}

int OPI_MYSQL::query(std::string statement)
{
	return query(statement, false);
}


int OPI_MYSQL::query(std::string statement, bool once)
{
    std::lock_guard<std::mutex> lock(queryMutex);
	
    log("OPI_MYSQL::query()\n\t");
    log(statement);
  
    printf("connecting to db...\n");
	
    bool Connected = reconnect();	
 
    bool fail = false;
    int returnVal = 0; 
    //make the query
    do
    {
		log("OPI_MySQL::query(): do\n");
    	
		if (fail || !Connected)
		{
			printf("OPI_MYSQL`````````````fail : %i, Connected: %i", (int)fail, (int)Connected);
			int reconnectAttempts = 0;
			
			log("\nYou want a query but I'm not connected.\nI'll try to reconnect.\n");
			while (!Connected) 
			{
				log("Trying to reconnect in Mr. Failed Query if-statement...It didn't work..but I'll wait a second and try again.");
				Sleep(1000);
				if (++reconnectAttempts > 5)
					return 1;
				
				Connected = reconnect();
			}
			log("\nReconnected finally, now I'll try your damn query.\n");
		} 
	
	    //count is 0;
	    rowCount = 0;
	        	
	    fail = false;
	    
	    //fixed memleak by clearing necessary result
	    //if (result)
	    //{
	    //   printf("mysql_free_result(%d)\n", (long)result);
	    //   mysql_free_result(result);
		//}
 
        printf("mysql_real_query(%lu, [%s], %i);\n", (unsigned long)conn, statement.c_str(), (int)statement.size()); 
	    returnVal = mysql_real_query(conn, statement.c_str(), statement.size());
	    printf("return value is: %i \n", returnVal);
	    if (returnVal == 0)
	    {
			printf("query success!\n");
			//store the result
	        result = mysql_store_result(conn);

			if (result)
			{
			   rowCount = mysql_num_rows(result);
			   numOfFields = mysql_num_fields(result);
			}
	    } 
	    else 
	    {
	        fail = true;

	      	log("query failed!");
	      	log("query was [" + statement + "]");

			
	      	printf("This is dangerous! I must re-try your query!\n");
			Sleep(1000);
	    }
		
       
    }while (fail && !once);
    
    mysql_close(conn);

    //return error/success
    return returnVal;
}

void OPI_MYSQL::nextRow()
{
    row = mysql_fetch_row(result);
    length = mysql_fetch_lengths(result);
}

char * OPI_MYSQL::getData(int i)
{
     return row[i];
}

std::string OPI_MYSQL::getString(int index)
{
     char * a = getData(index);
     std::string returnStr = "";

     for (int i = 0; i < length[index]; i++)
     {	
	returnStr += a[i];
     }	
     return returnStr;
}
int OPI_MYSQL::getInt(int i)
{
    std::stringstream ss;
    ss << "getInt(";
    ss << i;
    ss << ") returned [";
    ss << getData(i);
    ss << "]";
    
    log(ss.str());   
    
     
    return (int)atoi(getData(i));
}
float OPI_MYSQL::getFloat(int i)
{
    return (float)atof(getData(i));
}

int OPI_MYSQL::getNumOfFields()
{
    return numOfFields;
}
int OPI_MYSQL::count()
{
    return rowCount;
}

std::string OPI_MYSQL::clean(std::string dirty_str)
{
   std::lock_guard<std::mutex> lock(cleanMutex);
    
    conn = mysql_init(NULL);
    
    //variables
    char * clean_char = new char[(dirty_str.size() * 2) + 1];

    //clean string
    printf("mysql_real_escape_string(%lu, [clean_char], [%s], %i);", 
		(unsigned long)conn, dirty_str.c_str(), (int)dirty_str.size());
    mysql_real_escape_string(conn, clean_char, dirty_str.c_str(), dirty_str.size());

    //return the value
    return (const char*)clean_char;
}
