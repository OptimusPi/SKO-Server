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
	sem_init(&queryMutex, 0, 1);
	sem_init(&cleanMutex, 0, 1);    
 

	conn = new MYSQL();

	//just initialize the connection
        if (mysql_init(conn) == NULL){
		printf("\nFailed to initate MySQL connection\n");
		exit(1);
	} else {
	printf("\nSucceeded to initate MySQL connection\n");
	}
        
        Connected = false;
}

OPI_MYSQL::~OPI_MYSQL()
{
	printf("destructing OPI_MYSQL...\n");
	delete server;
	delete user;
	delete database;
	delete password;
	//cleanup();
}


//log file
void OPI_MYSQL::log(std::string output)
{
     
	//logFile << "[" << Clock() << "] " << output << std::endl;
	printf("[OPI_MYSQL] %s\n", output.c_str());
	logCount++;
}


int OPI_MYSQL::ping()
{
	return 0;
     int a = 1;
     printf("ping a\n");
     if (conn != NULL){
	printf("ping b\n");
         a = (int) mysql_ping(conn);
     }
     printf("ping c\n");
     return a;
}

bool OPI_MYSQL::connect(std::string server_in, std::string database_in, std::string user_in, std::string password_in)
{
	//save values for reconnect
	std::strcpy(server, server_in.c_str());
	std::strcpy(database, database_in.c_str());
	std::strcpy(user, user_in.c_str());
	std::strcpy(password, password_in.c_str());
       
	conn = mysql_init(NULL); 

        //make the connection
        //return reconnect();
        //MYSQL * returnVal = 
	if (mysql_real_connect(
		(MYSQL *)conn, 
		server, 
		user, 
		password, 
		database, 
		0, 
		NULL, 
		(unsigned long)0
	))
		Connected = true;
    
	mysql_close(conn);
       
    return Connected;
}


int OPI_MYSQL::reconnect()
{

	         

        Connected = false;
        //log("reconnect()\n");
         
	//log("conn = mysql_init(NULL);\n");
             conn = mysql_init(NULL);    
	printf("mysql_real_connect((MYSQL *)conn, %s, %s, %s, %s, 0, NULL, (unsigned long)0);", server, user, password, database);
        do {//make the connection
	 

			if (!mysql_real_connect((MYSQL *)conn, server, user, password, database, 0, NULL, (unsigned long)0)) {
       	      log("-1\n");
       	      //if it is NULL there is some error
       	      log("Error was:\n" + (std::string)(getError()) + "*");
       	  }else {
       	      Connected = true;          
       	  }
	} while (!Connected);
}

int OPI_MYSQL::getStatus()
{
    return (int)mysql_errno(conn);
}

std::string OPI_MYSQL::getError()
{       
      return error; 
}


int OPI_MYSQL::query(std::string statement)
{
	return query(statement, false);
}


int OPI_MYSQL::query(std::string statement, bool once)
{
    sem_wait(&queryMutex);
	
    log("OPI_MYSQL::query()\n\t");
    log(statement);
  
    printf("connecting to db...\n");
	
    reconnect();	
 
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
			while (reconnect()) 
			{
				log("Trying to reconnect in Mr. Failed Query if-statement...It didn't work..but I'll wait a second and try again.");
				Sleep(1000);
				if (++reconnectAttempts > 5)
					return 1;
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
 
        printf("mysql_real_query(%d, [%s], %i);\n", (long)conn, statement.c_str(), statement.size()); 
	    returnVal = mysql_real_query(conn, statement.c_str(), statement.size());
	    printf("return value is: %i \r\n", returnVal);
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
		error = std::string(mysql_error(conn));
       
    }while (fail && !once);
    
    mysql_close(conn);
    Connected = false; 

    sem_post(&queryMutex);

    //return error/success
    return returnVal;
}

int OPI_MYSQL::nextRow()
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
    sem_wait(&cleanMutex);
    
    conn = mysql_init(NULL);
    
    //variables
    char * clean_char = new char[(dirty_str.size() * 2) + 1];

    //clean string
    printf("mysql_real_escape_string(%d, [clean_char], [%s], %i);", 
		(long)conn, dirty_str.c_str(), dirty_str.size());
    mysql_real_escape_string(conn, clean_char, dirty_str.c_str(), dirty_str.size());
    
    sem_post(&cleanMutex);

    //return the value
    return (const char*)clean_char;
}
