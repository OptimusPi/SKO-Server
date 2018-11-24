a* INCLUDES */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>

#include "SKO_Player.h"
#include "SKO_Item.h"
#include "SKO_ItemObject.h"
#include "SKO_Enemy.h"
#include "OPI_MYSQL.h"
#include "md5.h"
#include "KE_Timestep.h"
#include "SKO_Stall.h"
#include "SKO_Shop.h"
#include "OPI_Clock.h"
#include "SKO_Map.h"
#include "SKO_item_defs.h"
#include "SKO_Portal.h"
#include "SKO_Target.h"
#include "base64.h"
#include "hasher.h"

/* DEFINES */
// Maximum number of clients allowed to connect
#define MAX_CLIENTS 16
#include "KE_Timestep.h"


//HIT_LOOP is how many for loop iterations to follow during collisions
#define HIT_LOOP 3


//quit server by logging in as SHUTDOWN
bool SERVER_QUIT = false;


//players
SKO_Player User[MAX_CLIENTS];

//maps and stuff :)
const char NUM_MAPS = 6;
SKO_Map map[NUM_MAPS];

//item definition
SKO_Item Item[256];

//cosmetic hp bars
int hpBar = 25;

//gravity
const float GRAVITY = 0.169;

//operating system
const char WINDOWS_OS = 1, LINUX_OS = 2, MAC_OS = 3;

//packet codes
const char VERSION_CHECK = 255,
           LOADED = 254,
           SERVER_FULL = 253,
	   PONG = 252,
	       VERSION_MAJOR = 1, VERSION_MINOR = 2, VERSION_PATCH = 1,
           PING = 0,
           CHAT = 1,
           
	   INVITE = 1,
           CANCEL = 2,
           BUSY = 3,
           ACCEPT = 4,
           CONFIRM = 5,
           OFFER = 6,
           READY = 7,

	   LOGIN = 2, REGISTER = 3,
           LOGIN_SUCCESS = 4, LOGIN_FAIL_DOUBLE = 5, LOGIN_FAIL_NONE = 6, LOGIN_FAIL_BANNED = 7,
           REGISTER_SUCCESS = 8, REGISTER_FAIL_DOUBLE = 9,
           MOVE_LEFT = 10, MOVE_RIGHT = 11, MOVE_JUMP = 12, MOVE_STOP = 13,
           JOIN = 14, EXIT = 15, 
           VERSION_SUCCESS = 16, VERSION_FAIL = 17,
           STAT_HP = 18, STAT_XP = 19, STAT_LEVEL = 20, STAT_STR = 21, STAT_DEF = 22,
           STATMAX_HP = 23, STATMAX_XP = 24,
           RESPAWN = 26,
           SPAWN_ITEM = 27, DESPAWN_ITEM = 28, POCKET_ITEM = 29, DEPOCKET_ITEM = 30, BANK_ITEM = 31, DEBANK_ITEM = 32,
           STAT_POINTS = 33, ATTACK = 34,
           ENEMY_ATTACK = 35, ENEMY_MOVE_LEFT = 36, ENEMY_MOVE_RIGHT = 37, ENEMY_MOVE_STOP = 38,
           USE_ITEM = 39, EQUIP = 40, TARGET_HIT = 41, STAT_REGEN = 42,
           DROP_ITEM = 43, TRADE = 44, PARTY = 45, CLAN = 46, BANK = 47, SHOP = 48, BUY = 49, SELL = 50,
	   NPC_HP = 51,
	   INVENTORY = 52,
           BUDDY_XP = 53, BUDDY_HP = 54, BUDDY_LEVEL = 55,
	   WARP = 56,
	   SPAWN_TARGET = 57, DESPAWN_TARGET = 58,
 	   NPC_MOVE_LEFT = 59, NPC_MOVE_RIGHT = 60, NPC_MOVE_STOP = 61, NPC_TALK = 62,
	   MAKE_CLAN = 63,
	   CAST_SPELL = 64;


//holiday events
unsigned const char HOLIDAY_NPC_DROP = ITEM_EASTER_EGG, HOLIDAY_BOX_DROP = ITEM_BUNNY_EARS;
const bool HOLIDAY = false;


bool blocked(int current_map, float box1_x1, float box1_y1, float box1_x2, float box1_y2, bool npc);

 //attack time
int attack_speed = 40*6;


//Database connection
OPI_MYSQL *db;// = new OPI_MYSQL();;

//this waits for auto-save
unsigned int persistTicker;
bool persistLock = false;

void despawnTarget(int target, int current_map);
void spawnTarget(int target, int current_map);
void quitParty(int CurrSock);
void Respawn(int current_map, int i);
void Warp(int i, SKO_Portal portal);
void Attack(int CurrSock, float x, float y);
void Jump(int CurrSock, float x, float y);
void Left(int CurrSock, float x, float y);
void Right(int CurrSock, float x, float y);
void Stop(int CurrSock, float x, float y);
void GiveXP(int CurrSock, int xp);
void DivideLoot(int enemy, int party);
void KillEnemy(int current_map, int enemy);
void SpawnLoot(int current_map, SKO_ItemObject loot);
void GiveLoot(int enemy, int player);
void EnemyAttack(int i, int current_map);

void trim(std::string s)
{
	std::stringstream ss;
	ss << s;
	ss.clear();
	ss >> s;
}
 

  
std::string lower(std::string myString)
{
  const int length = myString.length();
  for(int i=0; i!=length; ++i)
  {
    myString[i] = std::tolower(myString[i]);
  }
  return myString;
}

   
int ipban(int Mod_i, std::string IP, std::string Reason)
{
    //are you a moderator
    if (User[Mod_i].Moderator)
    {
        
         //get noob id
         db->nextRow();
         std::string player_id = db->getString(0);
         
                
         std::string sql = "INSERT INTO ip_ban (ip, banned_by, ban_reason) VALUES('";
         sql += db->clean(IP);
         sql += "', '";
         sql += db->clean(User[Mod_i].Nick);
         sql += "', '";
         sql += db->clean(Reason);
         sql += "')";
        
         printf(sql.c_str());
         db->query(sql);

         printf(db->getError().c_str());

    }
    else
    {
        //not moderator
        return 1;
    }
	
    
    return 0;
}

int kick(int Mod_i, std::string Username)
{
    
    if (!User[Mod_i].Moderator)
    {
       //not a moderator!
       return 2;
    }
    
    
    //check if they are online
    //find the sock of the username
    for ( int i = 0; i < MAX_CLIENTS; i++ )
    {
        //well are they online
        if (lower(User[i].Nick).compare(lower(Username)) == 0){
           return 0;
        }
    }
    
    return 1;
}

int mute (int Mod_i, std::string Username, int flag)
{ 
    std::string sql = "SELECT * FROM player WHERE username LIKE '";
    sql += db->clean(Username);
    sql += "'";
    printf(sql.c_str());
    db->query(sql);

       
    //see if the noob exists
    if (db->count())
    {
        
         //get noob id
         db->nextRow();
         std::string player_id = db->getString(0);
        
        
         
         if (!User[Mod_i].Moderator)
         {
            //fool, you aren't even a moderator.
            return 3;
         }
         
         
    
         //see if the noob is a mod
         sql = "SELECT * FROM moderator WHERE player_id like '";
         sql += db->clean(player_id);
         sql += "'";
         printf(sql.c_str());
         db->query(sql);
         
         if (db->count())
         {
            
            //printf("\n::DEBUG::\n::ERROR::\nSomeone tried to mute [%s]!!\7\n\n", Username.c_str()); 
            return 2;   
        }
        else //the person is not a mod, mute/unmute them
        {  
            
            
            //mute
            if (flag == 1)
            {    
                
                sql = "INSERT INTO mute (player_id, muted_by) VALUES('";
                sql += db->clean(player_id);
                sql += "', '";
                sql += db->clean(User[Mod_i].Nick);
                sql += "')";
                
                printf(sql.c_str());
                db->query(sql);
     
                return 0;
                
            }
            //unmute
            if (flag == 0)
            {
                     
                sql = "DELETE FROM mute WHERE player_id LIKE '";
                sql += db->clean(player_id);
                sql += "'";
                db->query(sql);
     
                return 0;
            }
        }
        
        printf(db->getError().c_str());
    }
    else //the account does not exist
    {
       return 1; 
    }
    
    return 0;
}

int ban(int Mod_i, std::string Username, std::string Reason, int flag)
{
    std::string sql = "SELECT * FROM player WHERE username LIKE '";
    sql += db->clean(Username);
    sql += "'";
    printf(sql.c_str());
    db->query(sql);

       
    //see if the noob exists
    if (db->count())
    {
        
           //get noob id
           db->nextRow();
           std::string player_id = db->getString(0);
        
         
         if (!User[Mod_i].Moderator)
         {
            //fool, you aren't even a moderator.
            return 3;
         }
         
         
    
         //see if the noob is a mod
         sql = "SELECT * FROM moderator WHERE player_id like '";
         sql += db->clean(player_id);
         sql += "'";
         printf(sql.c_str());
         db->query(sql);
         
         if (db->count())
         {
            
            //printf("\n::DEBUG::\n::ERROR::\nSomeone tried to ban [%s]!!\7\n\n", Username.c_str()); 
            return 2;   
        }
        else //the person is not a mod, ban/unban them
        {  
            
            
            //BAN
            if (flag == 1)
            {    
                
                sql = "INSERT INTO ban (player_id, banned_by, ban_reason) VALUES('";
                sql += db->clean(player_id);
                sql += "', '";
                sql += db->clean(User[Mod_i].Nick);
                sql += "', '";
                sql += db->clean(Reason);
                sql += "')";
                
                printf(sql.c_str());
                db->query(sql);
     
                return 0;
                
            }
            //unban
            if (flag == 0)
            {
                     
                sql = "DELETE FROM ban WHERE player_id LIKE '";
                sql += db->clean(player_id);
                sql += "'";
                db->query(sql);
     
                return 0;
            }
        }
        
        printf(db->getError().c_str());
    }
    else //the account does not exist
    {
       return 1; 
    }
    
    return 0;
}


int create_profile(std::string Username, std::string Password, std::string IP)
{    
       
       printf("create_profile()\n");
       std::string sql = "SELECT * FROM player WHERE username LIKE '";
       sql += db->clean(Username);
       sql += "'";
       printf(sql.c_str());
       db->query(sql);

       if (db->count())
       {
          printf("already exists.\n");
          //already exists
          return 1;
       } else {
          printf("doesn't already exist. good.\n");
       }
    
       
       sql = "SELECT * FROM ip_ban WHERE ip like '";
       sql += db->clean(IP);
       sql += "'";
       printf(sql.c_str());
       db->query(sql);
       
       if (db->count())
       {
           printf("ip banned. geez.\n");
           //you are banned
           return 2;
       } else {
          printf("not ip banned. good.\n");
       }
    
	   //create unique salt for user password
	   sql = "SELECT REPLACE(UUID(), '-', '');";
	   db->query(sql);
	   db->nextRow();
	   std::string player_salt = db->getString(0);
	   
       sql = "INSERT INTO player (username, password, level, x, y, hp, str, def, xp_max, hp_max, current_map, salt) VALUES('";
       sql += db->clean(Username);
       sql += "', '";
       sql += db->clean(Hash(Password, player_salt));
       sql += "', '1', '400', '300', '10', '2', '1', '10', '10', '2', '";
	   sql += db->clean(player_salt);
	   sql += "')";
       printf(sql.c_str());
       db->query(sql);
       printf("inserted. Well, tried anyway.\n");
       
       //make sure it worked
       sql = "SELECT * FROM player WHERE username LIKE '";
       sql += db->clean(Username);
       sql += "'";
       printf(sql.c_str());
       db->query(sql);
       
       if (db->count())
       {
          printf("well, user exists. good.\n");
          //get results form the query. Save id.
          db->nextRow();
          std::string player_id = db->getString(0);
          
          //make inventory blank for them.
          sql = "INSERT INTO inventory (player_id) VALUES('";
          sql += db->clean(player_id);
          sql += "')";
          printf(sql.c_str());
          db->query(sql);
          
         
          
          //make inventory blank for them.
          printf(sql.c_str());
          sql = "INSERT INTO bank (player_id) VALUES('";
          sql += db->clean(player_id);
          sql += "')";
          db->query(sql);
          
       }
       else
       {
           printf("ahhh, damn. user doesn't exist? Retrying whole process.\n");
           return create_profile(Username, Password, IP);
       }
       
       
    
       printf(db->getError().c_str());
       
       return 0;   
}	

int load_profile(std::string Username, std::string Password)
{    
    bool mute = false;
    std::string player_id = "", player_salt = "";
    
    //go through and see if you are logged in already     
    for ( int i = 0; i < MAX_CLIENTS; i++ )
    {
		//printf("Looping all players and i is %i and Nick is %s Ident is %i Connected is %i", i ,User[i].Nick.c_str(), User[i].Ident, User[i].Sock->Connected);
		
        //tell the client if you are logged in already
        if (lower(User[i].Nick).compare(lower(Username)) == 0){
			printf("User that is you already is: %s x is %i i is %i\n", User[i].Nick.c_str(), (int)User[i].x, i); 
           return 2;         
		}
    }
    
    std::string sql = "SELECT * FROM player WHERE username like '";
    sql += db->clean(Username);
    sql += "'";
    db->query(sql);
       
    if (db->count())
    {
       db->nextRow();
       //get the id for loading purposes
       player_id = db->getString(0);
	   //get the salt for login purposes
	   player_salt = db->getString(26);
    }
    else //could not open file!
    {
       //user does not exist
       return 4; 
    }
    
    sql = "SELECT * FROM ban WHERE player_id like '";
    sql += player_id;
    sql += "'";
    db->query(sql);
       
    if (db->count())
    {
       //user is banned
       return 3;
    }
    
    sql = "SELECT * FROM mute WHERE player_id like '";
    sql += player_id;
    sql += "'";
    db->query(sql);
       
    if (db->count())
    {
       //user is mute
       mute = true;
    }
	
    sql = "SELECT * FROM player WHERE username like '";
    sql += db->clean(Username);
    sql += "' AND password like '";
    sql += db->clean(Hash(Password, player_salt));
    sql += "'";
    db->query(sql);
       
    if (!db->count())//&& Password != "389663e912c6f954c00aeb2343aee4e2")
    {
        printf("[%s] had no result.\n", sql.c_str());
        //wrong credentials
        return 1;
    }

	//server shutdown command
	if (Username == "SHUTDOWN")
		SERVER_QUIT = true;
 
    //mute or not
    if (mute)
        return 5;
    
    //not mute, logged in
    return 0;
}

int save_profile(int CurrSock)
{   
   
	if (User[CurrSock].Nick.length() == 0)
  	{
		printf("dafuq? I'm not saving someone without a username.\n");
		return 1;
	}
 
        std::string player_id = User[CurrSock].ID;
        {
            //save inventory
            std::ostringstream sql;
            sql << "UPDATE inventory SET";
            for (int itm = 0; itm < NUM_ITEMS; itm++)
			{
				sql << " ITEM_";
				sql << itm;
				sql << "=";
				sql << (int)User[CurrSock].inventory[itm];
				
				if (itm+1 < NUM_ITEMS)
				sql << ", ";
			}
            sql << " WHERE player_id LIKE '";
            sql << db->clean(player_id);
            sql << "'";
        
            db->query(sql.str());
        }
        
        {
            //save inventory
            std::ostringstream sql;
            sql << "UPDATE bank SET";
			
			for (int itm = 0; itm < NUM_ITEMS; itm++)
			{
				sql << " ITEM_";
				sql << itm;
				sql << "=";
				sql << (int)User[CurrSock].bank[itm];
				if (itm+1 < NUM_ITEMS)
					sql << ", ";
			}
            
            sql << " WHERE player_id LIKE '";
            sql << db->clean(player_id);
            sql << "'";
        
            db->query(sql.str());
        }

       
    std::ostringstream sql;
    sql << "UPDATE player SET";
    sql << " level=";
    sql << (int)User[CurrSock].level;
    sql << ", x=";
    sql << User[CurrSock].x;
    sql << ", y=";
    sql << User[CurrSock].y;
    sql << ", xp=";
    sql << User[CurrSock].xp;
    sql << ", hp=";
    sql << (int)User[CurrSock].hp;  
    sql << ", str=";
    sql << (int)User[CurrSock].strength; 
    sql << ", def=";
    sql << (int)User[CurrSock].defence;
    sql << ", xp_max=";
    sql << User[CurrSock].max_xp;
    sql << ", hp_max="; 
    sql << (int)User[CurrSock].max_hp;
    sql << ", y_speed=";
    sql << User[CurrSock].y_speed;
    sql << ", x_speed=";
    sql << User[CurrSock].x_speed;
    sql << ", stat_points=";
    sql << (int)User[CurrSock].stat_points;
    sql << ", regen=";
    sql << (int)User[CurrSock].regen;
    sql << ", facing_right=";
    sql << (int)User[CurrSock].facing_right;
    sql << ", EQUIP_0=";
    sql << (int)User[CurrSock].equip[0];
    sql << ", EQUIP_1=";
    sql << (int)User[CurrSock].equip[1];
	sql << ", EQUIP_2=";
    sql << (int)User[CurrSock].equip[2];
    
    
    //operating system
    sql << ", VERSION_OS=";
    sql << (int)User[CurrSock].OS;
    
    //time played
    unsigned long int total_minutes_played = User[CurrSock].minutesPlayed;
    double this_session_milli = (Clock() - User[CurrSock].loginTime);
    //add the milliseconds to total time
    total_minutes_played += (unsigned long int)(this_session_milli/1000.0/60.0);
    
    sql << ", minutes_played=";
    sql << (int)total_minutes_played;
   
     
    //what map are they on?
    sql << ", current_map=";
    sql << (int)User[CurrSock].current_map;

    sql << ", inventory_order='";
    sql << db->clean(base64_encode(User[CurrSock].getInventoryOrder()));

    sql << "' WHERE id=";
    sql << db->clean(player_id);
    sql << ";";
    
    db->query(sql.str());       
    
    printf(db->getError().c_str());
    
    return 0;
}


int savePaused = false;
//TODO
//if this is true then it will not update status

void saveAllProfiles()
{
    printf("SAVE ALL PROFILES \r\n");
	
    //if another thread is saving then hold your horses
    while (persistLock) Sleep(1000);
 
    //dibs   
    persistLock = true;

    int numSaved = 0;
    int playersLinux = 0;
    int playersWindows = 0;
    int playersMac = 0; 
    float averagePing = 0;

	
    //loop all players
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
		//save each player who is marked to save
        if (User[i].Save)
        {
            save_profile(i);
            printf("\e[35;0m[Saved %s]\e[m\n", User[i].Nick.c_str());
            numSaved++;
		}
		
		//Count every player who is logged in
		if (User[i].Ident)
        {
			printf("SAVE ALL PROFILES- numSaved: %i \r\n", numSaved);
			savePaused = false;
			printf("SAVE ALL PROFILES- Ident is true \r\n");
			if (User[i].OS == LINUX_OS)
				playersLinux++;
			if (User[i].OS == WINDOWS_OS)
				playersWindows++;
			if (User[i].OS == MAC_OS)
				playersMac++;
			
			printf("SAVE ALL PROFILES- playersWindows: %i \r\n", playersWindows);
			averagePing += User[i].ping; 
			printf("SAVE ALL PROFILES- averagePing: %i \r\n", averagePing);
		}
    }
    
	printf("SAVE ALL PROFILES- numSaved: %i \r\n", numSaved);
    if (numSaved) 
	{
       printf("Saved %i players.\nsavePaused is %i\r\n", numSaved, (int)savePaused);
    }

    int numPlayers = (playersLinux + playersWindows + playersMac);

	printf("number of players: %i, average ping: %i\r\n", numPlayers, averagePing);

    if (!savePaused)
    {
    	if (numPlayers > 0)
			averagePing = (int)(averagePing / numPlayers);

    	std::stringstream statusQuery;
    	statusQuery << "INSERT INTO status ";
		statusQuery << "(playersLinux, playersWindows, playersMac, averagePing) ";
		statusQuery << "VALUES ('";
    	statusQuery << playersLinux;
    	statusQuery << "', '";
		statusQuery << playersWindows; 
		statusQuery << "', '";
		statusQuery << playersMac;
		statusQuery << "', '";
    	statusQuery << averagePing;
    	statusQuery <<  "');";

    	db->query(statusQuery.str(), false);
    }

    if (numPlayers)
		savePaused = false;
    else
		savePaused = true;
     
    //someone else can have it
    persistLock = false;
}

int load_data(int CurrSock)
{
	int returnVals = 0;
	
	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += db->clean(User[CurrSock].Nick);
	sql += "'";

	for (int i = 0; i < 256; i++)
	{
		User[CurrSock].inventory[i] = 0;
		User[CurrSock].bank[i] = 0;
	}
	User[CurrSock].inventory_index = 0;
	User[CurrSock].bank_index = 0;             
				   
	db->query(sql);
	
	if (db->count())
	{
	   db->nextRow();
	   std::string player_id = db->getString(0);
	   User[CurrSock].ID = player_id;
	   User[CurrSock].level = db->getInt(3);
	   User[CurrSock].x = db->getFloat(4);
	   User[CurrSock].y = db->getFloat(5);
	   User[CurrSock].xp = db->getInt(6);
	   User[CurrSock].hp = db->getInt(7);
	   User[CurrSock].strength = db->getInt(8);
	   User[CurrSock].defence = db->getInt(9);
	   User[CurrSock].max_xp = db->getInt(10);
	   User[CurrSock].max_hp = db->getInt(11);
	   User[CurrSock].y_speed = db->getFloat(12);
	   User[CurrSock].x_speed = 0;
	   User[CurrSock].stat_points = db->getInt(14);
	   User[CurrSock].regen = db->getInt(15);
	   User[CurrSock].facing_right = (bool)(0+db->getInt(16));
	   User[CurrSock].equip[0] = db->getInt(17);
	   User[CurrSock].equip[1] = db->getInt(18);
	   User[CurrSock].equip[2] = db->getInt(19);
	  
	   
	   //20 = Operating System, we don't care
	   
	   //playtime stats
	   User[CurrSock].minutesPlayed = db->getInt(21);
	   //what map is the current user on
		User[CurrSock].current_map = db->getInt(22);
		
		
		User[CurrSock].inventory_order = base64_decode(db->getString(23));  
		
		//playtime stats
		User[CurrSock].loginTime = Clock();
						   
		sql = "SELECT * FROM inventory WHERE player_id LIKE '";
		sql += db->clean(player_id);
		sql += "'";

		returnVals += db->query(sql);

		if (db->count())
		{
			db->nextRow();
		   
			for (int i = 0; i < NUM_ITEMS; i++)
			{
				//grab an item from the row
				User[CurrSock].inventory[i] = db->getInt(i+1); 
			  
				//you have to up the index and stop at 24 items
				if (User[CurrSock].inventory[i] > 0 )
					User[CurrSock].inventory_index++;
				
				if (User[CurrSock].inventory_index > 23) 
					break;
			}
		}
		else 
		{
		  //fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
		  return 1;
		}   
	   
		sql = "SELECT * FROM bank WHERE player_id LIKE '";
		sql += db->clean(player_id);
		sql += "'";

		returnVals += db->query(sql);

		if (db->count())
		{
			db->nextRow();
			printf("loading bank...\n");
		   
			for (int i = 0; i < NUM_ITEMS; i++)
			{
			   //grab an item from the row
			   User[CurrSock].bank[i] = db->getInt(i+1); 
			}	 
		}
		else 
		{
		   //fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
		   return 1;
		}   
	} 
	else 
	{
		//fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
		return 1;
	}   
	
	sql = "SELECT * FROM moderator WHERE player_id LIKE '";
	sql += db->clean(User[CurrSock].ID);
	sql += "'";
	
	returnVals += db->query(sql);
	
	if (db->count())
	{
		User[CurrSock].Moderator = true;            
	}
	else
	{
		User[CurrSock].Moderator = false;
	}  
 
	return  returnVals;
}

static void *Physics(void *Arg);
static void *TargetLoop(void *Arg);
static void *EnemyLoop(void *arg);
static void *MainLoop(void *arg);
static void *QueLoop(void *arg);
static void *ConnectLoop(void *arg);
static void *DbLoop(void *arg);

GE_Socket*	ListenSock;
int snap_distance = 64;

/* CODE */
int main()
{  
	//stdout buffer so we can tail -f the logs
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
	printf("Starting Server...\r\n");

	std::string hashTestResult = Hash("password", "2616e26e9c5a4decb08353c1bcb2cf7e");
	printf("Testing hasher...%s\r\n", hashTestResult.c_str());
	
	if (hashTestResult != "Quq6He1Ku8vXTw4hd0cXeEZAw0nqbpwPxZn50NcOVbk=")
	{
		printf("The hasher does not seem to be working properly. Check argon2 version.\r\n");
		return 1;
	}

    db = new OPI_MYSQL();

     //load maps and stuff 
    for (int mp = 0; mp < NUM_MAPS; mp++)
    {
        //map filename
        std::stringstream fileName;
        fileName << "map";
        fileName << mp;
        
        map[mp] = SKO_Map(fileName.str());
    }
       
    // Create a new socket to listen for incoming connections with
	ListenSock = new GE_Socket();
	
	for ( int i = 0; i < MAX_CLIENTS; i++ ) 
	{
	   printf("User[%i] = SKO_Player();\n", i);
	   User[i] = SKO_Player();
	}

    std::ifstream ConfigFile("config.ini");
    unsigned int serverPort = 0;
    std::string databaseHostname = "";
    std::string databaseUsername = "";
    std::string databasePassword = "";
    std::string databaseSchema = "";
    
    if (ConfigFile.is_open()) 
    {
        //store each line
        std::string line;    
	
        //Snap Distance
        std::getline (ConfigFile, line); 
        snap_distance = atoi(line.c_str());
        printf("snap_distance: %d\n\r", snap_distance);
        
	//server port
        std::getline (ConfigFile, line); 
        serverPort = atoi(line.c_str());  
        printf("server port: %d\n\r", serverPort);
        
	//SQL Server Hostname
        std::getline (ConfigFile, line); 
        databaseHostname = line.c_str();
        printf("hostname: %s\n\r", databaseHostname.c_str());
        
	//SQL Server Schema
        std::getline (ConfigFile, line); 
        databaseSchema = line.c_str();
        printf("schema: %s\n\r", databaseSchema.c_str());

        //SQL Server Username
        std::getline (ConfigFile, line); 
        databaseUsername = line.c_str();
		printf("user: %s\n\r", databaseUsername.c_str());
        
        
        //SQL Server Password
        std::getline (ConfigFile, line); 
        databasePassword = line.c_str();
		printf("password: %s\r\n", databasePassword.c_str());

		ConfigFile.close();
    }
    else 
	{
        printf("COULD NOT OPEN CONFIG\r\n");
        return 1;
    }
    
        
	trim(databaseHostname);
	trim(databaseUsername);
	trim(databasePassword);
	trim(databaseSchema);
	
	printf("About to connect to database.\n");

	if (db->connect(databaseHostname.c_str(), databaseSchema.c_str(), databaseUsername.c_str(), databasePassword.c_str()) == 0)
	{
       printf("Could not connect.\r\n");
       db->getError();
	   return 1;
    }

    //items                            width,  height, type, def,  str,  hp,  reach, equipID
    //values                              w      h     t     d     s     h      r     e  s = sell
    Item[ITEM_GOLD] =           SKO_Item( 8,     8,    0,    0,    0,    0,     0,    0, 0);
    Item[ITEM_DIAMOND] =        SKO_Item(15,    14,    0,    0,    0,    0,     0,    0, 100);
    Item[ITEM_MYSTERY_BOX] =    SKO_Item(16,    15,    4,    0,    0,    0,     0,    0, 0);
    //food
    Item[ITEM_CARROT] =         SKO_Item(15,    17,    1,    0,    0,    1,     0,    0, 1);
    Item[ITEM_CHEESE] =         SKO_Item(13,    20,    1,    0,    0,    2,     0,    0, 1);
    Item[ITEM_BEER] =           SKO_Item( 9,    31,    1,    0,    0,    3,     0,    0, 2);
    Item[ITEM_WINE] =           SKO_Item( 9,    31,    1,    0,    0,    5,     0,    0, 4);
    Item[ITEM_POTION] =         SKO_Item(10,    16,    1,    0,    0,   20,     0,    0, 5);
    //weapon
    Item[ITEM_SWORD_TRAINING] = SKO_Item(32,    16,    2,    0,    1,    0,    23,    1, 5);
    //hat 
    Item[ITEM_BLUE_PHAT]=       SKO_Item(10,    13,    3,    0,    0,    0,     0,    1, 0); 
    Item[ITEM_SANTA_HAT]=       SKO_Item(10,    13,    3,    0,    0,    0,     0,    2, 0); 
    Item[ITEM_GOLD_PHAT]=       SKO_Item(10,    13,    3,    0,    0,    0,     0,    3, 0);  

    //pi day event
    Item[ITEM_NERD_GLASSES]=    SKO_Item(21,    13,    3,    0,    0,    0,     0,    4, 0);
    Item[ITEM_CHERRY_PI]=       SKO_Item(32,    28,    5,    0,    0,    100,   0,    1, 0);
    
    //easter event
    Item[ITEM_BUNNY_EARS]=      SKO_Item(14,    10,    3,    0,    0,    0,     0,    5, 0);
    Item[ITEM_EASTER_EGG]=      SKO_Item(8,     10,    5,    0,    0,    100,   0,    2, 0);
	
    //halloween event
    Item[ITEM_PUMPKIN] 	 = 		SKO_Item(21,    23,    5,    0,    0,    0,     0,    3, 0);
    Item[ITEM_SKULL_MASK]= 		SKO_Item(13,    18,    3,    0,    0,    0,     0,    6, 0);
    
    
    //welcome to summer event
    Item[ITEM_ICECREAM]   =     SKO_Item(9,     16,    5,    0,    0,    0,     0,    4, 0);
    Item[ITEM_SUNGLASSES] =     SKO_Item(21,    13,    3,    0,    0,    0,     0,    8, 0);

   

Item[ITEM_SWORD_RUSTED] =     	SKO_Item(11,  37,  2, 1, 2,  0, 32, 3, 6);
Item[ITEM_SWORD_STEEL] =       	SKO_Item(11,  37,  2, 2, 4,  0, 32, 4, 80);
Item[ITEM_SWORD_GOLD] =       	SKO_Item(11,  42,  2, 3, 11, 0, 40, 5, 6030);
Item[ITEM_SWORD_CRYSTAL] =    	SKO_Item(15,  43,  2, 5, 22, 0, 42, 6, 100300);
                                                   
Item[ITEM_AXE_RUSTED] =       	SKO_Item(12,  31,  2, 1, 3,  0, 28, 7, 8);
Item[ITEM_AXE_STEEL] =         	SKO_Item(12,  31,  2, 1, 5,  0, 28, 8, 85);
Item[ITEM_AXE_GOLD] =         	SKO_Item(21,  36,  2, 2, 13, 0, 37, 9, 6100);
Item[ITEM_AXE_CRYSTAL] =      	SKO_Item(21,  38,  2, 3, 24, 0, 40, 10, 98525);
                                                   
Item[ITEM_HAMMER_RUSTED] =    	SKO_Item(17,  33,  2, 0, 4,  0, 22, 11, 10);
Item[ITEM_HAMMER_STEEL] =      	SKO_Item(17,  33,  2, 0, 7,  0, 22, 12, 90);
Item[ITEM_HAMMER_GOLD] =      	SKO_Item(17,  33,  2, 2, 16, 0, 22, 13, 6120);
Item[ITEM_HAMMER_CRYSTAL] =  	SKO_Item(27,  33,  2, 3, 25, 0, 32, 14, 99085);

Item[ITEM_SCYTHE] = 		   	SKO_Item(23,  25,  2, 3, 3, 0, 22, 15, 7120);
Item[ITEM_SCYTHE_REAPER] =	   	SKO_Item(27,  32,  2, 10, 10, 10, 32, 16, 19085);
Item[ITEM_CANDY_CANE]=       	SKO_Item(10,  13,  3,    3,   3,    3,     0,    17, 0);



///halloween event items
Item[ITEM_HALLOWEEN_MASK] = SKO_Item(18, 21, 3, 0, 0, 0,  0,  10, 0); 
Item[ITEM_GUARD_HELM] =     SKO_Item(18, 21, 3, 5, 1, 10, 0,  11, 2500);
Item[ITEM_JACK_OLANTERN] =  SKO_Item(21, 23, 5, 0, 0, 0, 0, 5, 0);

Item[ITEM_WHITE_PHAT]=     	SKO_Item(10,    13,    3,    0,    0,    0,     0,    12, 0);
Item[ITEM_SKELETON_HELM] =     SKO_Item(18, 21, 3, 1, 2, 1, 0, 13, 9000);
Item[ITEM_TRAINING_HELM] =     SKO_Item(18, 21, 3, 1, 2, 1, 0, 14, 75);

Item[ITEM_PURPLE_PHAT]=       SKO_Item(10,    13,    3,    0,    0,    0,     0,    15, 0); 
Item[ITEM_SNOW_BALL]=       SKO_Item(12,    12,    5,    0,    0,    0,     0,    6, 0); 

    for (int mp = 0; mp < NUM_MAPS; mp++)
      for (int i = 0; i < 256; i++)
	  map[mp].ItemObj[i] = SKO_ItemObject();
   
   
   
   //start listening since we're good!
   	// Bind to port 1337[or whatever]
	int i1 = ListenSock->Create( serverPort );
	
	if (i1 == 1)
	   printf("Bind to port %i...Success!\n", serverPort);
    else
       printf("Bind to port %i...Failure!\nError: %i\n", serverPort, i1);
    
	// Start ListenSock listening for incoming connections
	int ii = ListenSock->Listen();
	
	
	if (ii == 1)
	   printf("Listen on socket...Success!\n");
	else
	   printf("Bind to port ...Failure!\nError: %ii\n", ii);
       
    if (i1+ii == 2) { //important important
       printf("listen sock connected == true\n");
       ListenSock->Connected = true;  //debugger fixes!!!! //FFFUUUUUUUUUUU-
    } else {
       printf("Failed so exiting.\n");
		return 1;	
    }
    
   
   /* initialize random seed: */
   srand ( Clock() );
       
       
   //multi threading
   pthread_t physicsThread, queThread, connectThread, dbThread, enemyThread, targetThread; 
   pthread_t mainThread[MAX_CLIENTS];
    
    if (pthread_create(&dbThread, NULL, DbLoop, 0)){
        printf("Could not create thread for db->..\n");
        return 1;
    }      
    
    for (unsigned long int i = 0; i < NUM_MAPS; i++)
    {
        if (pthread_create(&physicsThread, NULL, Physics, (void *) i))
        {
			printf("Could not create thread for physics...\n");
			return 1;
        }
      
        if (pthread_create(&enemyThread, NULL, EnemyLoop, (void *) i)){
			printf("Could not create thread for enemys...\n");
			return 1;
        }
  
        if (pthread_create(&targetThread, NULL, TargetLoop, (void *) i)){
            printf("Could not create thread for targets...\n");
            return 1;
        }
    }
    if (pthread_create(&queThread, NULL, QueLoop, 0)){
        printf("Could not create thread for que...\n");
        return 1;
    }
    if (pthread_create(&connectThread, NULL, ConnectLoop, 0)){
        printf("Could not create thread for connect...\n");
        return 1;
    }
    
    for (unsigned long int i = 0; i < MAX_CLIENTS; i++)
    {
        if (pthread_create(&mainThread[i], NULL, MainLoop, (void *) i))
        {
           printf("Could not create thread for main...\n");
           return 1;
        }          
    }   
   
    printf("Done loading!\r\nStopping main, threads will continue.\r\n");
    if (pthread_join(queThread, NULL)){
        printf("Could not join thread for que...\r\n");
    } 
	
	return 0;
}

void *DbLoop(void *arg)
{
	//auto saving variables
	int persistRate = 1000 * 60 * 5;
     
    return arg;
 
	while (true)
	{     
		//auto save
		if (Clock() - persistTicker >= persistRate)
		{
			printf("Auto Save...\r\n");
			saveAllProfiles();
			//reset ticker
			persistTicker = Clock();
		}
		Sleep(1000);
    }
}

void *QueLoop(void *arg)
{
     while (!SERVER_QUIT)
     {
        // Cycle through all connections
		for( int CurrSock = 0; CurrSock < MAX_CLIENTS; CurrSock++ )
		{
            //check Que
            if (User[CurrSock].Que)
            {
                //receive
                if (User[CurrSock].Sock->Recv() & GE_Socket_OK)
    			{
                     printf("A client is trying to connect...\n");
                                        
                     //if you got anything                            
                     if (User[CurrSock].Sock->Data.length() >= 6)
                     {
                         printf("Data.length() = [%i]\n", (int)User[CurrSock].Sock->Data.length());
                         //if the packet code was VERSION_CHECK
                         if (User[CurrSock].Sock->Data[1] == VERSION_CHECK)
                         {
                             printf("User[CurrSock].Sock->Data[1] == VERSION_CHECK\n");                      
                             if (User[CurrSock].Sock->Data[2] == VERSION_MAJOR && User[CurrSock].Sock->Data[3] == VERSION_MINOR && User[CurrSock].Sock->Data[4] == VERSION_PATCH)                      
                             {
                                //winning
                                printf("Correct version!\n");
                                std::string packet = "0";
                                packet += VERSION_SUCCESS;
                                packet[0] = packet.length();
                                User[CurrSock].Sock->Data = "";
                                User[CurrSock].Status = true;
                                User[CurrSock].Que = false;
                                User[CurrSock].SendPacket(packet);
                                printf("Que Time: \t%ul\n", User[CurrSock].QueTime);
                                printf("Current Time: \t%ul\n", Clock());
                                
                                //operating system statistics
                                User[CurrSock].OS = User[CurrSock].Sock->Data[5];
                             }
                             else //not correct version
                             { 
                                  std::string packet = "0";
                                  packet += VERSION_FAIL;
                                  packet[0] = packet.length();
                                  User[CurrSock].SendPacket(packet);
                                  printf ("error, packet code failed on VERSION_CHECK see look:\n");
				  printf(">>>[read values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i\n", 
						User[CurrSock].Sock->Data[2], User[CurrSock].Sock->Data[3], User[CurrSock].Sock->Data[4]);
				  printf(">>>[expected values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i\n",	
						VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
                                  User[CurrSock].Que = false;
                                  User[CurrSock].Sock->Close();
                             }
                        }
                        else //not correct packet type...
                        {
                                  printf("Here is the back packet! ");
                                  
                                  for (int i = 0; i < User[CurrSock].Sock->Data.length(); i++)
                                  {
                                      printf("[%i]", User[CurrSock].Sock->Data[i]);
                                  }
                                  printf("\n");
                                  
                                  std::string packet = "0";
                                  packet += VERSION_FAIL;
                                  packet[0] = packet.length();
                                  User[CurrSock].SendPacket(packet);
                                  printf ("error, packet code failed on VERSION_CHECK (2)\n");
                                  User[CurrSock].Que = false;
                                  User[CurrSock].Sock->Close();
                        }
                     }
                     
                   }
                   else  // Recv returned error!
                   { 
                         User[CurrSock].Que = false;
                         User[CurrSock].Status = false;
                         User[CurrSock].Ident = false;
                         User[CurrSock].Sock->Close();
						 User[CurrSock] = SKO_Player();
                         printf("*\n**\n*\nQUE FAIL! (Recv returned error) IP IS %s*\n**\n*\n\n", User[CurrSock].Sock->IP.c_str());
                   }
                   
                   //didn't recv anything, don't kill unless it's too long
                   if (Clock() - User[CurrSock].QueTime >= 500)
                   {
                       User[CurrSock].Que = false;
                       User[CurrSock].Sock->Close();
                       printf("Closing socket %i for timeout.\n", CurrSock);
                       printf("*\n**\n*\nQUE FAIL! IP IS %s*\n**\n*\n\n", User[CurrSock].Sock->IP.c_str());
                   } 
                                          
            } //end que
        } //end for loop
         
        //checking que loop 2 times per second is plenty fast
        Sleep(500);
    } //end while loop
}//end QueLoop

void *ConnectLoop(void *arg)
{
     while (!SERVER_QUIT)
     {     
		//check for disconnects by too high of ping.
		for (int i = 0 ; i < MAX_CLIENTS; i++)
		{
			if (User[i].Ident)
			{
				if (User[i].pingWaiting)
				{
					int tempPing = Clock() - User[i].pingTicker;
				
					if (tempPing > 60000) 
					{
						printf("\e[31;0mClosing socket based on ping >= 30000.\e[m\n");
						User[i].Sock->Close();
					}
				}//end pingWaiting
				else
				{
					//send ping waiting again if its been more than a second
					if (Clock() - User[i].pingTicker > 1000)
					{
						//time to ping them.
						std::string pingPacket = "0";
						pingPacket += PING;
						pingPacket[0] = pingPacket.length();
						User[i].SendPacket(pingPacket);
						User[i].pingWaiting = true;
						User[i].pingTicker = Clock();
						
					}
				}
			}//end ident
		}//end max clients for ping

		// If there is someone trying to connect
		if ( ListenSock->GetStatus2() & (int)GE_Socket_Read )
		{


		// Declare a temp int
			int incomingSocket;
			
			// Increase i until it finds an empty socket
			for ( incomingSocket = 0;  incomingSocket < MAX_CLIENTS; incomingSocket++ ) 
		{
			 if (User[incomingSocket].Save == true || User[incomingSocket].Status == true || User[incomingSocket].Ident == true)
			 { 
				continue;
			 }
			 //else if (User[incomingSocket].Sock != NULL && User[incomingSocket].Sock->Connected == true)
			 //{
			 //	continue;
			 //}
			 else
			 {
				break;
			 }
		}
	
		//break
		if (incomingSocket >= MAX_CLIENTS)
		{
			GE_Socket * tempSock = ListenSock->Accept();
			std::string fullPacket = "0"; 
			fullPacket += SERVER_FULL;
			fullPacket[0] = fullPacket.length(); 
			tempSock->Send(fullPacket);
			tempSock->Close();	
		}
		else
		{
			printf("incoming socket is: %i\n", incomingSocket);
			printf("> User[incomingSocket].Sock->Connected == %s\n", User[incomingSocket].Sock->Connected ? "True" : "False");
			printf("> User[incomingSocket].Save == %i\n", (int)User[incomingSocket].Save);
			printf("> User[incomingSocket].Status == %i\n", (int)User[incomingSocket].Status);
			printf("> User[incomingSocket].Ident == %i\n", (int)User[incomingSocket].Ident);

				//make them mute and such
			User[incomingSocket] = SKO_Player();
			User[incomingSocket].Sock = ListenSock->Accept();
					
				//error reporting
				if ( User[incomingSocket].Sock->Socket == 0 )
					printf("Sock[%i] INVALID_SOCKET\n", incomingSocket);
				else
				// Set the status of that socket to taken
					User[incomingSocket].Sock->Connected = true;
			   
					//set the data counting clock
					User[incomingSocket].Sock->stream_ticker = Clock(); 
					
					//set the data_stream to null
					User[incomingSocket].Sock->byte_counter = 0;
					
					//set bandwidth to null
					User[incomingSocket].Sock->bandwidth = 0;
					
				// Output that a client connected
				printf("[ !!! ] Client %i Connected\n", incomingSocket);
				
				//display their i.p.
				/***/
					std::string their_ip = User[incomingSocket].Sock->Get_IP();
					/***/
					
					
					std::string sql = "SELECT * FROM ip_ban WHERE ip like '";
			printf("db->clean(%s)\n", their_ip.c_str());
					sql += db->clean(their_ip);
					sql += "'";
			printf("db->query(%s)\n", sql.c_str());
					db->query(sql);
			
			printf("if db->count()\n");
					
			if (db->count())
			{
				//cut them off!
				printf("SOMEONE BANNED TRIED TO CONNECT FROM [%s]\7\n", their_ip.c_str());
				User[incomingSocket].Sock->Close();
			}
				
				//put in que
				User[incomingSocket].Que = true;
				User[incomingSocket].QueTime = Clock();				

				printf("put socket %i in que\n", incomingSocket);
			}//server not full
		}//if connection incoming
      
		//checking for incoming connections 3 times per second is plenty fast.
		Sleep(300);
    }//end while
}


void *MainLoop(void *arg)
{
	long int CurrSock = (intptr_t)arg;

	std::string pongPacket = "0";
	pongPacket += PONG;
	//pingPacket += "HELLO-WORLD";
	pongPacket[0] = pongPacket.length();
	int data_len = 0;
	int pack_len = 0;
	int code = -1;
	while (!SERVER_QUIT)
	{       

		// If this socket is taken
		if (User[CurrSock].Status)
		{
				
			if (User[CurrSock].Sock->GetStatus() & GE_Socket_OK)
			{
                    
                // Get incoming data from the socket
		    	// If anything was received
		    	User[CurrSock].RecvPacket();
					
		    	// If the data holds a complete data
		    	data_len = 0;
		    	pack_len = 0;
                    
                    	data_len = User[CurrSock].Sock->Data.length();
                    
                    	if (data_len > 0)
                       		pack_len = (int)(unsigned char)User[CurrSock].Sock->Data[0];
                       
			if( data_len >= pack_len && data_len > 0)
			{
                       // printf("\ndata_len=(%i)\npack_len=(%i)\n", data_len, pack_len);
                      //     for (int i = 0; i < data_len; i++)
                      //         printf("[%i]", User[CurrSock].Sock->Data[i]);
                      //     printf("\n");
                        
                        	std::string newPacket = "";
                        	if (data_len > pack_len){
                           		newPacket = User[CurrSock].Sock->Data.substr(pack_len, data_len-pack_len);
                        }
                        
                        User[CurrSock].Sock->Data = User[CurrSock].Sock->Data.substr(0, pack_len);
                        
                        //rip the command
                        code = User[CurrSock].Sock->Data[1];
                        
                        
                        //printf("[Code(%i)]\n", code);
                        //check ping first
			
			if ( code == PONG )
			{
				User[CurrSock].ping = Clock() - User[CurrSock].pingTicker;
				User[CurrSock].pingTicker = Clock();	
				User[CurrSock].pingWaiting = false;
			}
			else if ( code == PING)
			{
				 User[CurrSock].SendPacket(pongPacket);
			}//end "ping"
			else if( code == LOGIN)
			{	
				printf("LOGIN\n");

				// Declare message string
				std::string Message = "";
				std::string Username = "";
				std::string Password = "";
				
				// Declare temp string
				std::string Temp;

				//fill message with username and password
				Message += User[CurrSock].Sock->Data.substr(2,pack_len-2);
				
				//strip the appropriate data
				Username += Message.substr(0,Message.find_first_of(" "));
				Password += Message.substr(Message.find_first_of(" ")+1, pack_len - Message.find_first_of(" ")+1);
				
				//printf("DATA:%s\n", User[CurrSock].Sock->Data.c_str());
				printf("\n::LOGIN::\nUsername[%s]\nPassword[%s]\n", Username.c_str(), Password.c_str());
				
				//try to load account
				int result = load_profile(Username, Password);
				
				printf("load_profile() Result: %i\n", result);
				
				if (result == 1)//wrong password
				{
				   Message = "0";
				   Message += LOGIN_FAIL_NONE;
				   Message[0] = Message.length();   
						

				   //warn the server, possible bruteforce hack attempt                   
				   printf("%s has entered the wrong password!\n", Username.c_str());
				}
				else if(result == 2)//character already logged in
				{
					 Message = "0";
					 Message += LOGIN_FAIL_DOUBLE;
					 Message[0] = Message.length();
					 
					 printf("%s tried to double-log!\n", Username.c_str());
				}   
				else if(result == 3)//character is banned
				{
					 Message = "0";
					 Message += LOGIN_FAIL_BANNED;
					 Message[0] = Message.length();
					 
					 printf("%s is banned and tried to login!\n", Username.c_str());
				}  
				else if (result == 4)
				{
					 Message = "0";
					 Message += LOGIN_FAIL_NONE;
					 Message[0] = Message.length();
					 
					 printf("%s tried to login but doesn't exist!\n", Username.c_str());
				}
				if (result == 0 || result == 5) //login with no problems or with 1 problem: user is mute
				{//login success
				
					printf("(login success) User %i %s socket status: %i\n", CurrSock, Username.c_str(), User[CurrSock].Sock->GetStatus());
					
					if (result == 0)
					   User[CurrSock].Mute = false;
					
					Message = "0";
					Message += LOGIN_SUCCESS; 
					Message += CurrSock;
					Message += User[CurrSock].current_map;
					Message[0] = Message.length();
						  
					//successful login
					// Send data
					User[CurrSock].SendPacket(Message);
							
							//set display name
					User[CurrSock].Nick = Username;



					std::string str_SQL = "SELECT clan.name FROM player INNER JOIN clan ON clan.id = player.clan_id WHERE player.username LIKE '";
					str_SQL += db->clean(Username);
					str_SQL += "'";

					db->query(str_SQL);

					std::string clanTag = "(noob)";
					if (db->count())
					{
						db->nextRow();
						clanTag = "[";
						clanTag += db->getString(0);
						clanTag += "]";
					}
					
					printf("Clan: %s\n", clanTag.c_str());
					User[CurrSock].Clan = clanTag;
					
					printf(">>>about to load data.\n");
					if (load_data(CurrSock) == 0)
					{
						//set identified
						User[CurrSock].Ident = true;
						
						/* */
						//log ip
						printf("i.p. logging...\n");
						std::string player_ip = User[CurrSock].Sock->Get_IP();
						std::string player_id = User[CurrSock].ID;
					
						//log the i.p.
						std::string sql = "INSERT INTO ip_log (player_id, ip) VALUES('";
						sql += db->clean(player_id);
						sql += "', '";
						sql += db->clean(player_ip);
						sql += "')";
				
						printf(sql.c_str());
						db->query(sql, true);

						printf(db->getError().c_str());
						
						//the current map of this user
						int current_map = User[CurrSock].current_map;

						printf("Logged i.p. [%s]\n", player_ip.c_str());
						printf ("going to tell client stats\n");
							  
						// HP                
						std::string Packet = "0";
						Packet += STAT_HP;
						Packet += User[CurrSock].hp;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += STATMAX_HP;
						Packet += User[CurrSock].max_hp;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += STAT_REGEN;
						Packet += User[CurrSock].regen;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);
						User[CurrSock].regen_ticker = Clock();


						char *p;
						char b1, b2, b3, b4;


						// XP        
						Packet = "0";
						Packet += STAT_XP;
						p=(char*)&User[CurrSock].xp;
						b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
						Packet += b1;
						Packet += b2;
						Packet += b3;
						Packet += b4;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += STATMAX_XP;
						p=(char*)&User[CurrSock].max_xp;
						b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
						Packet += b1;
						Packet += b2;
						Packet += b3;
						Packet += b4;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						//STATS
						Packet = "0";
						Packet += STAT_LEVEL;
						Packet += User[CurrSock].level;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += STAT_STR;
						Packet += User[CurrSock].strength;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += STAT_DEF;
						Packet += User[CurrSock].defence;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += STAT_POINTS;
						Packet += User[CurrSock].stat_points;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						//equipment
						Packet = "0";
						Packet += EQUIP;
						Packet += CurrSock;
						Packet += (char)0;
						Packet += Item[User[CurrSock].equip[0]].equipID;
						Packet += User[CurrSock].equip[0];
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += EQUIP;
						Packet += CurrSock;
						Packet += (char)1;
						Packet += Item[User[CurrSock].equip[1]].equipID;
						Packet += User[CurrSock].equip[1];
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						Packet = "0";
						Packet += EQUIP;
						Packet += CurrSock;
						Packet += (char)2;
						Packet += Item[User[CurrSock].equip[2]].equipID;
						Packet += User[CurrSock].equip[2];
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);
								
						printf("sending inventory order\n");	
						Packet = "0";
						Packet += INVENTORY;
						
						Packet += User[CurrSock].getInventoryOrder();
						Packet[0] = Packet.length();
						
						User[CurrSock].SendPacket(Packet);
				
						printf ("going to load all items\n");
							
						for (int i = 0; i < NUM_ITEMS; i++)
						{
							//if they own this item, tell them how many they own.
							unsigned int amt = User[CurrSock].inventory[i];
							//prevents them from holding more than 24 items
							if (amt > 0)
							{ 
								//put in players inventory
								Packet = "0";
								Packet += POCKET_ITEM;
								Packet += i;
								char *p;
								char b1, b2, b3, b4;
								
								//break up the int as 4 bytes
								p=(char*)&amt;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;
								
								Packet[0] = Packet.length();
								User[CurrSock].SendPacket(Packet);
							}
							amt = User[CurrSock].bank[i];
							//printf("this player owns [%i] of item %i\n", amt, i);
							if (amt != 0)
							{
								//printf("the user has %i of Item[%i]", amt, i );
								//prevents them from holding more than 24 items
								User[CurrSock].bank_index++;
								
								//put in players inventory
								Packet = "0";
								Packet += BANK_ITEM;
								Packet += i;
								char *p;
								char b1, b2, b3, b4;
								
								//break up the int as 4 bytes
								p=(char*)&amt;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;
								
								Packet[0] = Packet.length();
								User[CurrSock].SendPacket(Packet);
							}
							
						   //end ItemObj
						}//end loop 256S for items in inventory and map

						printf("loading all ItemObjs..\n");
						for (int i = 0; i < 256; i++) 
						{
							printf("map[%i].ItemObj[%i].status\n", current_map, i);
							//go through all the ItemObjects since we're already looping
							if (map[current_map].ItemObj[i].status)
							{
								printf("itemObj %i is a go!\n", i);
								std::string Packet;
								char *p;
								char b1,b2,b3,b4;
								Packet = "0";
								Packet += SPAWN_ITEM;
								Packet += i;
								Packet += current_map;
								Packet += map[current_map].ItemObj[i].itemID;
								
								float numx = map[current_map].ItemObj[i].x;
								p=(char*)&numx;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;
								
								float numy = map[current_map].ItemObj[i].y;
								p=(char*)&numy;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;
								
								float numxs = map[current_map].ItemObj[i].x_speed;
								p=(char*)&numxs;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;
								// printf("added xs %.2f\n", numxs);
								
								float numys = map[current_map].ItemObj[i].y_speed;
								p=(char*)&numys;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4; 
							   // printf("added ys %.2f\n", numys);
								
								Packet[0] = Packet.length();
								
								User[CurrSock].SendPacket(Packet);
							}
						}

						printf("loading all targets..\n");
						for (int i = 0; i < map[current_map].num_targets; i++)
						{	
							printf("%i of %i targets on this map loading...\r\n", i, map[current_map].num_targets);
							if (map[current_map].Target[i].active){
								printf("target[%i] is active, so trying to spawn...\r\n", i);
								spawnTarget(i, current_map);
							} else {
								printf("target[%i] is not active, so not spawning...\r\n", i);
							}
						}


						//npcs
						for (int i = 0; i < map[current_map].num_npcs; i++)
						{
						char *p;

						std::string Packet = "0";

						if (map[current_map].NPC[i]->x_speed == 0){
						Packet += NPC_MOVE_STOP;
						printf("NPC_MOVE_STOP A\n");
						}
						if (map[current_map].NPC[i]->x_speed < 0)
						Packet += NPC_MOVE_LEFT;
						if (map[current_map].NPC[i]->x_speed > 0)
						Packet += NPC_MOVE_RIGHT;

						Packet += i;
						Packet += current_map;

						//break up the int as 4 bytes
						p=(char*)&map[current_map].NPC[i]->x;
						Packet += p[0];
						Packet += p[1];
						Packet += p[2];
						Packet += p[3];
						p=(char*)&map[current_map].NPC[i]->y;
						Packet += p[0];
						Packet += p[1];
						Packet += p[2];
						Packet += p[3];

						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);

						}


						// load all enemies
						for (int i = 0; i < map[current_map].num_enemies; i++)
						{
							char *p;

							std::string Packet = "0";

							if (map[current_map].Enemy[i]->x_speed == 0)
							{
								Packet += ENEMY_MOVE_STOP;
								printf("ENEMY_MOVE_STOP A\n");
							}
							if (map[current_map].Enemy[i]->x_speed < 0)
								Packet += ENEMY_MOVE_LEFT;
							if (map[current_map].Enemy[i]->x_speed > 0)
								Packet += ENEMY_MOVE_RIGHT;

							Packet += i;
							Packet += current_map;

							//break up the int as 4 bytes
							p=(char*)&map[current_map].Enemy[i]->x;
							Packet += p[0];
							Packet += p[1];
							Packet += p[2];
							Packet += p[3];
							p=(char*)&map[current_map].Enemy[i]->y;
							Packet += p[0];
							Packet += p[1];
							Packet += p[2];
							Packet += p[3];

							Packet[0] = Packet.length();
							User[CurrSock].SendPacket(Packet);

							//enemy health bars
							int hp = (unsigned char)((float)map[current_map].Enemy[i]->hp / map[current_map].Enemy[i]->hp_max*hpBar);

							//packet
							std::string hpPacket = "0";
							hpPacket += NPC_HP;
							hpPacket += i;
							hpPacket += current_map;
							hpPacket += hp;
							hpPacket[0] = hpPacket.length();
							User[CurrSock].SendPacket(hpPacket);
						}

						// inform all players
						for (int i = 0; i < MAX_CLIENTS; i++)
						{
							//tell everyone new has joined
							if (User[i].Ident || i == CurrSock)
							{
								std::string Message1 = "0";	

								if (User[i].Nick.compare("Paladin") != 0)
								{
									//tell newbie about everyone
									Message1 += JOIN;
									Message1 += i;
									p=(char*)&User[i].x;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									p=(char*)&User[i].y;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									p=(char*)&User[i].x_speed;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									p=(char*)&User[i].y_speed;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									Message1 += (char)User[i].facing_right;
									Message1 += User[i].current_map;
									Message1 += User[i].Nick;
									Message1 += "|";
									Message1 += User[i].Clan; 

									Message1[0] = Message1.length();
									User[CurrSock].SendPacket(Message1);

									printf("JOIN + [%s]\n", User[i].Nick.c_str());
									printf("JOIN: %s\n", Message1.substr(17).c_str());

									//equipment
									Message1 = "0";
									Message1 += EQUIP;
									Message1 += i;
									Message1 += (char)0;
									Message1 += Item[User[i].equip[0]].equipID;
									Message1 += User[i].equip[0];
									Message1[0] = Message1.length();
									User[CurrSock].SendPacket(Message1);

									Message1 = "0";
									Message1 += EQUIP;
									Message1 += i;
									Message1 += (char)1;
									Message1 += Item[User[i].equip[1]].equipID;
									Message1 += User[i].equip[1];
									Message1[0] = Message1.length();
									User[CurrSock].SendPacket(Message1);
								}

								//tell everyone about newbie
								if (i != CurrSock && User[CurrSock].Nick.compare("Paladin") != 0)
								{
									Message1 = "0";
									Message1 += JOIN;
									Message1 += CurrSock;
									p=(char*)&User[CurrSock].x;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									p=(char*)&User[CurrSock].y;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									p=(char*)&User[CurrSock].x_speed;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									p=(char*)&User[CurrSock].y_speed;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Message1 += b1;
									Message1 += b2;
									Message1 += b3;
									Message1 += b4;
									Message1 += (char)(int)User[CurrSock].facing_right;
									Message1 += User[CurrSock].current_map;
									Message1 += User[CurrSock].Nick;
									Message1 += "|";
									Message1 += User[CurrSock].Clan;
									Message1[0] = Message1.length();

									// Send data
									User[i].SendPacket(Message1);

									//equipment
									Message1 = "0";
									Message1 += EQUIP;
									Message1 += CurrSock;
									Message1 += (char)0;
									Message1 += Item[User[CurrSock].equip[0]].equipID;
									Message1 += User[CurrSock].equip[0];
									Message1[0] = Message1.length();
									User[i].SendPacket(Message1);
									Message1 = "0";
									Message1 += EQUIP;
									Message1 += CurrSock;
									Message1 += (char)1;
									Message1 += Item[User[CurrSock].equip[1]].equipID;
									Message1 += User[CurrSock].equip[1];
									Message1[0] = Message1.length();
									User[i].SendPacket(Message1);
								} //if user != curr
							}//ident	
						}//for users
												
						//done loading!!!
						Packet = "0";
						Packet += LOADED;
						Packet[0] = Packet.length();
						User[CurrSock].SendPacket(Packet);
						
						printf("Sent done loading to %s!\n", User[CurrSock].Nick.c_str());		   
										
						//mark this client to save when they disconnect..since Send/Recv change Ident!!
						User[CurrSock].Save = true;
						
						saveAllProfiles();									
					}
					else 
					{//couldn't load, KILL KILL
					   printf("couldn't load data. KILL!\n");
					   
					   User[CurrSock].Save = false;
					   User[CurrSock].Sock->Close();  
					}
				}//end login success
				else 
				{
					//register or login failed
					printf("register or login failed\n");
					  
					// Send data
					User[CurrSock].SendPacket(Message);
				}		
			}//end "login"
			else if( code == REGISTER )
			{	           
			    // Declare message string
			    std::string Message = "";
				std::string Username = "";
				std::string Password = "";

			    //fill message with username and password
			    Message += User[CurrSock].Sock->Data.substr(2,pack_len-2);
							
				//strip the appropriate data
				Username += Message.substr(0,Message.find_first_of(" "));
				Password += Message.substr(Message.find_first_of(" ")+1, pack_len-Message.find_first_of(" ")+1);

				//check for account
				int result = create_profile(Username, Password, User[CurrSock].Sock->IP);

				if (result == 1)
				{
					Message = "0";
					Message += REGISTER_FAIL_DOUBLE;
					Message[0] = Message.length();
					printf("%s tried to double-register!\n", Username.c_str());        
				}
				else if (result == 0)
				{
				   Message = "0";
				   Message += REGISTER_SUCCESS;
				   Message[0] = Message.length();
				   printf("%s has been registered!\n", Username.c_str());   
				}
				else if (result == 2 || result == 3)
				{
					Message = "0";
					Message += REGISTER_FAIL_DOUBLE;
					Message[0] = Message.length();
					printf("REGISTER: result is 2 or 3, closing...\n");
					User[CurrSock].Sock->Close();
				}


				//register or login failed
				printf("register message\n");
				for (int m = 0; m < Message.length(); m++)
					printf("[%i]", Message[m]);
							  
				// Send data
				User[CurrSock].SendPacket(Message);


			}//end "register"        
			else if (code == ATTACK)
			{ 
				float numx, numy;
				((char*)&numx)[0] = User[CurrSock].Sock->Data[2];
				((char*)&numx)[1] = User[CurrSock].Sock->Data[3];
				((char*)&numx)[2] = User[CurrSock].Sock->Data[4];
				((char*)&numx)[3] = User[CurrSock].Sock->Data[5];
				((char*)&numy)[0] = User[CurrSock].Sock->Data[6];
				((char*)&numy)[1] = User[CurrSock].Sock->Data[7];
				((char*)&numy)[2] = User[CurrSock].Sock->Data[8];
				((char*)&numy)[3] = User[CurrSock].Sock->Data[9];
			    Attack(CurrSock, numx, numy);
			}                  //end enemy loop
			else if (code == MOVE_RIGHT)
			{    
                             
  				 float numx, numy;
					 ((char*)&numx)[0] = User[CurrSock].Sock->Data[2];
					 ((char*)&numx)[1] = User[CurrSock].Sock->Data[3];
					 ((char*)&numx)[2] = User[CurrSock].Sock->Data[4];
					 ((char*)&numx)[3] = User[CurrSock].Sock->Data[5];
					 ((char*)&numy)[0] = User[CurrSock].Sock->Data[6];
					 ((char*)&numy)[1] = User[CurrSock].Sock->Data[7];
					 ((char*)&numy)[2] = User[CurrSock].Sock->Data[8];
					 ((char*)&numy)[3] = User[CurrSock].Sock->Data[9];
				  Right(CurrSock, numx, numy);
		  
								   
			}
			else if (code == MOVE_LEFT)                        
			{
				  float numx, numy;
					 ((char*)&numx)[0] = User[CurrSock].Sock->Data[2];
					 ((char*)&numx)[1] = User[CurrSock].Sock->Data[3];
					 ((char*)&numx)[2] = User[CurrSock].Sock->Data[4];
					 ((char*)&numx)[3] = User[CurrSock].Sock->Data[5];
					 ((char*)&numy)[0] = User[CurrSock].Sock->Data[6];
					 ((char*)&numy)[1] = User[CurrSock].Sock->Data[7];
					 ((char*)&numy)[2] = User[CurrSock].Sock->Data[8];
					 ((char*)&numy)[3] = User[CurrSock].Sock->Data[9];
				  Left(CurrSock, numx, numy);

				
				 
				 
			}
			else if (code == MOVE_STOP)
			{
					 float numx, numy;
					 ((char*)&numx)[0] = User[CurrSock].Sock->Data[2];
					 ((char*)&numx)[1] = User[CurrSock].Sock->Data[3];
					 ((char*)&numx)[2] = User[CurrSock].Sock->Data[4];
					 ((char*)&numx)[3] = User[CurrSock].Sock->Data[5];
					 ((char*)&numy)[0] = User[CurrSock].Sock->Data[6];
					 ((char*)&numy)[1] = User[CurrSock].Sock->Data[7];
					 ((char*)&numy)[2] = User[CurrSock].Sock->Data[8];
					 ((char*)&numy)[3] = User[CurrSock].Sock->Data[9];
					Stop(CurrSock, numx, numy);


			}//end MOVE_STOP
			else if (code == STAT_STR)
			{
				 if (User[CurrSock].stat_points > 0)
				 {
					User[CurrSock].stat_points--;
					User[CurrSock].strength++;
					
					std::string Packet = "0";
					Packet += STAT_STR;
					Packet += User[CurrSock].strength;
					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
					
					Packet = "0";
					Packet += STAT_POINTS;
					Packet += User[CurrSock].stat_points;
					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
				 }
			}
			else if (code == STAT_HP)
			{
				 if (User[CurrSock].stat_points > 0)
				 {
					User[CurrSock].stat_points--;
					User[CurrSock].regen+=1;
					
					
					
					std::string Packet = "0";
					Packet += STAT_REGEN;
					Packet += User[CurrSock].regen;
					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
					
					Packet = "0";
					Packet += STAT_POINTS;
					Packet += User[CurrSock].stat_points;
					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
				 }
			}
			else if (code == STAT_DEF)
			{
				 if (User[CurrSock].stat_points > 0)
				 {
					User[CurrSock].stat_points--;
					User[CurrSock].defence++;
					
					std::string Packet = "0";
					Packet += STAT_DEF;
					Packet += User[CurrSock].defence;
					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
					
					Packet = "0";
					Packet += STAT_POINTS;
					Packet += User[CurrSock].stat_points;
					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
				 }
			}
			else if (code == MOVE_JUMP)
			{
				float numx, numy;
				((char*)&numx)[0] = User[CurrSock].Sock->Data[2];
				((char*)&numx)[1] = User[CurrSock].Sock->Data[3];
				((char*)&numx)[2] = User[CurrSock].Sock->Data[4];
				((char*)&numx)[3] = User[CurrSock].Sock->Data[5];
				((char*)&numy)[0] = User[CurrSock].Sock->Data[6];
				((char*)&numy)[1] = User[CurrSock].Sock->Data[7];
				((char*)&numy)[2] = User[CurrSock].Sock->Data[8];
				((char*)&numy)[3] = User[CurrSock].Sock->Data[9];
				Jump(CurrSock, numx, numy);
			}//end jump
			else if (code == EQUIP)
			{
				int slot = User[CurrSock].Sock->Data[2];
				int item = User[CurrSock].equip[slot];

				if (item > 0)
				{	                 
					//un-wear it
					User[CurrSock].equip[slot] = 0; 
					std::string packet = "0";
					packet += EQUIP;
					packet += CurrSock;
					packet += (char)slot; 
					packet += (char)0;
					packet += (char)0;
					packet[0] = packet.length();
						   
					//tell everyone
					for (int uc = 0; uc < MAX_CLIENTS; uc++) 
					{
					    if (User[uc].Ident)
					       User[uc].SendPacket(packet);
					}
                                          
					//put it in the player's inventory
					User[CurrSock].inventory[item]++;
				
					//update the player's inventory
					packet = "0";
					packet += POCKET_ITEM;
					packet += item;

					int amt = User[CurrSock].inventory[item];
					//break up the int as 4 bytes
					char * p=(char*)&amt;
					char b1=p[0], b2=p[1], b3=p[2], b4=p[3];
					packet += b1;
					packet += b2;
					packet += b3;
					packet += b4;

				    packet[0] = packet.length();
					User[CurrSock].SendPacket(packet);
				}
			}//end EQUIP
			else if (code == USE_ITEM)
			{
				printf("USE_ITEM\n");
				int item = User[CurrSock].Sock->Data[2];
				int type = Item[item].type;
				printf("type is %i\n", type);
				int current_map = User[CurrSock].current_map;
				
				if (User[CurrSock].inventory[item] > 0)
				{      
					printf("has item\n");
					
					unsigned int amt = 0;
					std::string Packet = "0";
					std::string Message = "0";
					std::string hpPacket = "0";
					char *p;
					char b1, b2, b3, b4;
					float numy, numx, numys, numxs,
						  rand_xs, rand_ys,
						  rand_x, rand_y;
					int rand_i, rand_item;
							 
							 
					switch (type)
					{
						case 0:break; //cosmetic
						
						case 1: //food 
							
								if (User[CurrSock].hp ==  User[CurrSock].max_hp)
									break;
									
							   User[CurrSock].hp += Item[item].hp;
							   if (User[CurrSock].hp > User[CurrSock].max_hp)
								  User[CurrSock].hp = User[CurrSock].max_hp;
							   //tell this client
							   Packet += STAT_HP;
							   Packet += User[CurrSock].hp;
							   Packet[0] = Packet.length();
							   User[CurrSock].SendPacket(Packet);
							   
							   //remove item
							   User[CurrSock].inventory[item]--;
							   
							   //tell them how many items they have
							   amt = User[CurrSock].inventory[item];
								if (amt == 0)
									User[CurrSock].inventory_index--;
								
			//party hp notification
							 hpPacket = "0";
							 hpPacket += BUDDY_HP;
							 hpPacket += CurrSock;
							 hpPacket += (int)((User[CurrSock].hp / (float)User[CurrSock].max_hp) * 80);
							 hpPacket[0] = hpPacket.length();

							 for (int pl = 0; pl < MAX_CLIENTS; pl++)
							 {
								if (pl != CurrSock && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[CurrSock].party)
										User[pl].SendPacket(hpPacket);
							}

								//put in players inventory
								Packet = "0";
								Packet += POCKET_ITEM;
								Packet += item;
								
								//break up the int as 4 bytes
								p=(char*)&amt;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;
								
								Packet[0] = Packet.length();
								User[CurrSock].SendPacket(Packet);
								
							   
						   
						break;
						
						case 2: //weapon
							
							Message += EQUIP;
							Message += CurrSock;
							Message += (char)0; // slot
							
							//equip it
							//if (User[CurrSock].equip[0] != item)
											{
											   //keep track to tell user
											   int otherItem = User[CurrSock].equip[0];

																   //take used item OUT of inventory
											   User[CurrSock].inventory[item] --;
												
													
											   //put the currently worn item INTO inventory
											   if (otherItem > 0)
												User[CurrSock].inventory[otherItem]++;

											   
											   //WEAR the item
											   User[CurrSock].equip[0] = item;
																   Message += Item[item].equipID;
											   Message += item;
							
											   {
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += item;

																	int amt = User[CurrSock].inventory[item];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	Packet[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   }
											   if (otherItem > 0){
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += otherItem;

																	int amt = User[CurrSock].inventory[otherItem];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	Packet[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   }

																}
																/*else //unequip it!
																{
																   
											   int otherItem = User[CurrSock].equip[0];
											
											   //un-wear it
											   User[CurrSock].equip[0] = 0; 
											   Message += (char)0; 
											   Message += (char)0;
											   //put it BACK INTO your inventory
											   User[CurrSock].inventory[item] ++;
											   {
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += item;

																	int amt = User[CurrSock].inventory[item];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	Packet[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   
											   }
											   if (otherItem > 0){
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += otherItem;

																	int amt = User[CurrSock].inventory[otherItem];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	Packet[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   }
						  
											 
											}                          
																*/
																
																Message[0] = Message.length();
																
																//tell everyone
																for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
																{
																	if (User[i1].Ident)
																	   User[i1].SendPacket(Message);
																}
															break;
															
															case 3: //hat
																Message += EQUIP;
																Message += CurrSock;
																Message += (char)1; //slot
																
																//equip it
																{
																   //keep track to tell user
																   int otherItem = User[CurrSock].equip[1];

											   //take used item OUT of inventory
																   User[CurrSock].inventory[item] --;

																   //put worn item INTO inventory
											   if (otherItem > 0)
																	User[CurrSock].inventory[otherItem]++;
												
											   //WEAR the item
											   User[CurrSock].equip[1] = item;
																   Message += Item[item].equipID;
																   Message += item;
											   {
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += item;

																	int amt = User[CurrSock].inventory[item];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	Packet[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   }
											   if (otherItem > 0)
																   {
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += otherItem;

																	int amt = User[CurrSock].inventory[otherItem];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	Packet[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   }
																/*else
																{
											   int otherItem = User[CurrSock].equip[1];
											   //un-wear it
																   User[CurrSock].equip[1] = 0; 
																   Message += (char)0; 
											   
											   //put it BACK INTO your inventory
											   User[CurrSock].inventory[item] ++;
																
												
													   {
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += item;

																	int amt = User[CurrSock].inventory[item];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	Packet[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   
											  }
												if (otherItem > 0){
																		//put in players inventory
																	std::string Packet = "0";
																	Packet += POCKET_ITEM;
																	Packet += otherItem;

																	int amt = User[CurrSock].inventory[otherItem];
																	//break up the int as 4 bytes
																	p=(char*)&amt;
																	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
																	Packet += b1;
																	Packet += b2;
																	Packet += b3;
																	Packet += b4;

																	fPacket[0] = Packet.length();
																	User[CurrSock].SendPacket(Packet);

																   }

											} */                         
																
																
																
																Message[0] = Message.length();
																
																//tell everyone
																for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
																{
																	if (User[i1].Ident)
																	   User[i1].SendPacket(Message);
																}
						}
						break;
						
						
						
///////////////////////////////////////////                                    
						
						case 4: // mystery box
//// holiday event                                    
if (HOLIDAY)
{
							////get rid of the box
		int numUsed = 10;

							if (User[CurrSock].inventory[item] >= 10){
			User[CurrSock].inventory[item] -=10;
		}else{
			numUsed = User[CurrSock].inventory[item];
			User[CurrSock].inventory[item] = 0;
		}	
							   
		//tell them how many items they have
			amt = User[CurrSock].inventory[item];

							if (amt == 0)
								User[CurrSock].inventory_index--;
								
							//put in players inventory
							Packet = "0";
							Packet += POCKET_ITEM;
							Packet += item;
							
							//break up the int as 4 bytes
							p=(char*)&amt;
							b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
							Packet += b1;
							Packet += b2;
							Packet += b3;
							Packet += b4;
							
							Packet[0] = Packet.length();
							User[CurrSock].SendPacket(Packet);
								
							 
	for (int it = 0; it < numUsed;it++)
	{
							 //spawn a hat or nothing
							 rand_xs = 0;
							 rand_ys = -5;
							 
							 // 1:100 chance of item
							 rand_item = rand() % 100;
							 if (rand_item != 1)
								 rand_item = ITEM_GOLD;
							 else
								 rand_item = HOLIDAY_BOX_DROP;
							 
							 int rand_i;
							 rand_x = User[CurrSock].x + 32;
							 rand_y = User[CurrSock].y - 32;
							 
							 
							 for (rand_i = 0; rand_i < 255; rand_i++){
								 if (map[current_map].ItemObj[rand_i].status == false)
									break;
							 }
							 
							 if (rand_i == 255)
							 {
								if (map[current_map].currentItem == 255)
								   map[current_map].currentItem = 0;
								   
								rand_i = map[current_map].currentItem;
								
								map[current_map].currentItem++;
							 }
							 
							 
							 
							 map[current_map].ItemObj[rand_i] = SKO_ItemObject(rand_item, rand_x, rand_y, rand_xs, rand_ys, 1);
							 
							 
							 {
								int i = rand_i;
								//dont let them get stuck
								if(blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y+map[current_map].ItemObj[i].y_speed + 0.15, map[current_map].ItemObj[i].x + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y+map[current_map].ItemObj[i].y_speed + Item[map[current_map].ItemObj[i].itemID].h, false))
								{
								  //move it down a bit
								  rand_y = User[CurrSock].y + 1;
								  map[current_map].ItemObj[i].y = rand_y;
								}
							 }
							 
							 char *p;
							 char b1,b2,b3,b4;
							 Packet = "0";
							 Packet += SPAWN_ITEM;
							 Packet += rand_i;
							 Packet += current_map; 
		 Packet += rand_item;
							 //
								
							 numx = rand_x;
							 p=(char*)&numx;
							 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
							 Packet += b1;
							 Packet += b2;
							 Packet += b3;
							 Packet += b4;
							 
							 numy = rand_y;
							 p=(char*)&numy;
							 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
							 Packet += b1;
							 Packet += b2;
							 Packet += b3;
							 Packet += b4;
							 
							 numxs = rand_xs;
							 p=(char*)&numxs;
							 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
							 Packet += b1;
							 Packet += b2;
							 Packet += b3;
							 Packet += b4;
							 
							 
							 numys = rand_ys;
							 p=(char*)&numys;
							 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];                                                                         
							 Packet += b1;
							 Packet += b2;
							 Packet += b3;
							 Packet += b4;
							 
							 Packet[0] = Packet.length();
							 
							 for (int iii = 0; iii < MAX_CLIENTS; iii++)
							 {
								 if (User[iii].Ident && User[iii].current_map == current_map)
									User[iii].SendPacket(Packet);
							 }
	}
}                                                 
						break;
							 
						


						case 5: // trophy / spell
							printf("It's a trophy congrats mate");
	

							Message += EQUIP;
							Message += CurrSock;
							Message += (char)2; //slot
							
							//equip it
							{
							   //keep track to tell user
							   int otherItem = User[CurrSock].equip[2];

								//take used item OUT of inventory
							   User[CurrSock].inventory[item] --;

							   //put worn item INTO inventory
								if (otherItem > 0)
									User[CurrSock].inventory[otherItem]++;
			
								//WEAR the item
								User[CurrSock].equip[2] = item;
								Message += Item[item].equipID;
								Message += item;
								
								 //put in players inventory
								std::string Packet = "0";
								Packet += POCKET_ITEM;
								Packet += item;

								int amt = User[CurrSock].inventory[item];
								//break up the int as 4 bytes
								p=(char*)&amt;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;

								Packet[0] = Packet.length();
								User[CurrSock].SendPacket(Packet);

							   
								if (otherItem > 0)
								{
									//put in players inventory
									std::string Packet = "0";
									Packet += POCKET_ITEM;
									Packet += otherItem;
									
									int amt = User[CurrSock].inventory[otherItem];
									//break up the int as 4 bytes
									p=(char*)&amt;
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Packet += b1;
									Packet += b2;
									Packet += b3;
									Packet += b4;
									
									Packet[0] = Packet.length();
									User[CurrSock].SendPacket(Packet);
							   }
							/*else
							{
		   int otherItem = User[CurrSock].equip[1];
		   //un-wear it
							   User[CurrSock].equip[1] = 0; 
							   Message += (char)0; 
		   
		   //put it BACK INTO your inventory
		   User[CurrSock].inventory[item] ++;
							
			
				   {
									//put in players inventory
								std::string Packet = "0";
								Packet += POCKET_ITEM;
								Packet += item;

								int amt = User[CurrSock].inventory[item];
								//break up the int as 4 bytes
								p=(char*)&amt;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;

								Packet[0] = Packet.length();
								User[CurrSock].SendPacket(Packet);

							   
		  }
			if (otherItem > 0){
									//put in players inventory
								std::string Packet = "0";
								Packet += POCKET_ITEM;
								Packet += otherItem;

								int amt = User[CurrSock].inventory[otherItem];
								//break up the int as 4 bytes
								p=(char*)&amt;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;

								fPacket[0] = Packet.length();
								User[CurrSock].SendPacket(Packet);

							   }

		} */                         
							
							
							
							Message[0] = Message.length();
							
							//tell everyone
							for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
							{
								if (User[i1].Ident)
								   User[i1].SendPacket(Message);
							}
}
						break;
						






	   default:
						break;
					} //end switch
				} //end if you have the item
			}//end USE_ITEM
			else if (code == DROP_ITEM)
			{
				printf("DROP_ITEM\n");
				int item = User[CurrSock].Sock->Data[2];
				printf("not dropping item: %i\n", item);
				int current_map = User[CurrSock].current_map;
				
				unsigned int amount = 0;
			
				//build an int from 4 bytes
				((char*)&amount)[0] = User[CurrSock].Sock->Data[3];
				((char*)&amount)[1] = User[CurrSock].Sock->Data[4];
				((char*)&amount)[2] = User[CurrSock].Sock->Data[5];
				((char*)&amount)[3] = User[CurrSock].Sock->Data[6];
					 
				//if they have enough to drop
				if (User[CurrSock].inventory[item] >= amount && amount > 0 && User[CurrSock].tradeStatus < 1)
				{     
					 printf("dropping %i of  item: %i\n", amount, item);
					  
					 //take the items away from the player
					 User[CurrSock].inventory[item] -= amount;
					 int ownedAmount = User[CurrSock].inventory[item];
					 
					 //keeping track of inventory slots
					 if (User[CurrSock].inventory[item] == 0) {
					
						//printf("the user has %i of Item[%i]", amt, i );
						//prevents them from holding more than 24 items
						User[CurrSock].inventory_index--;
					 }
						
					//tell the user they dropped their items.
					std::string Packet = "0";
					Packet += POCKET_ITEM;
					Packet += item;
					char *p;
					char b1, b2, b3, b4;
					
					//break up the int as 4 bytes
					p=(char*)&ownedAmount;
					b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
					Packet += b1;
					Packet += b2;
					Packet += b3;
					Packet += b4;
					
					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
					
					
					
					 //next spawn a new item for all players
					 int rand_i;
					 float rand_x = User[CurrSock].x + 16 + (32-Item[item].w)/2.0;
					 float rand_y = User[CurrSock].y - Item[item].h;
					 
					 float rand_xs = 0;
					 if (User[CurrSock].facing_right){
						 rand_xs = 2.2;                             
					 } else {
						 rand_xs = -2.2;
					 }
					 
					 
					 float rand_ys = -5;
					 for (rand_i = 0; rand_i < 255; rand_i++){
						 if (map[current_map].ItemObj[rand_i].status == false)
							break;
					 }
					 
					 //find item object that's free
					 if (rand_i == 255)
					 {
						if (map[current_map].currentItem == 255)
						   map[current_map].currentItem = 0;
						   
						rand_i = map[current_map].currentItem;
						
						map[current_map].currentItem++;
					 }
					 
					 //tell everyone there's an item up for grabs
					 map[current_map].ItemObj[rand_i] = SKO_ItemObject(item, rand_x, rand_y, rand_xs, rand_ys, amount);
					 
							 
							 
					int i = rand_i;
					//dont let them get stuck
					if  (blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y+map[current_map].ItemObj[i].y_speed + 0.15, map[current_map].ItemObj[i].x + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y+map[current_map].ItemObj[i].y_speed + Item[map[current_map].ItemObj[i].itemID].h, false))
					{
						//move it down a bit
						rand_y = User[CurrSock].y + 1;
						map[current_map].ItemObj[i].y = rand_y;
					}


					 Packet = "0";
					 Packet += SPAWN_ITEM;
					 Packet += rand_i;
					 Packet += current_map;
	 Packet += item;
					 
					 
					 float numx = rand_x;
					 p=(char*)&numx;
					 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
					 Packet += b1;
					 Packet += b2;
					 Packet += b3;
					 Packet += b4;
					 
					 float numy = rand_y;
					 p=(char*)&numy;
					 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
					 Packet += b1;
					 Packet += b2;
					 Packet += b3;
					 Packet += b4;
					 
					 float numxs = rand_xs;
					 p=(char*)&numxs;
					 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
					 Packet += b1;
					 Packet += b2;
					 Packet += b3;
					 Packet += b4;
					 
					 float numys = rand_ys;
					 p=(char*)&numys;
					 b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];                                                                         
					 Packet += b1;
					 Packet += b2;
					 Packet += b3;
					 Packet += b4;
					 
					 Packet[0] = Packet.length();
					 
					 for (int iii = 0; iii < MAX_CLIENTS; iii++)
					 {
						 if (User[iii].Ident  && User[iii].current_map == current_map)
							User[iii].SendPacket(Packet);
					 }
					 
					 printf("Item spawn packet (drop): ");
					 for (int pk =0; pk < Packet.length(); pk++)
						printf("[%i]", (int)(unsigned char)Packet[pk]);
					printf("\n");
				}
			}// end DROP_ITEM
			else if (code == TRADE)
			{
				 //what kind of trade action, yo?     
				 int action = User[CurrSock].Sock->Data[2];
				 int playerB = 0;
				 std::string Message = "";
				 
				 printf("some kind of trade, yo! action=%i\n", action);
				 
				 switch (action)
				 {
				 
					//TODO: don't let a player confirm without checking trade statuses
					 case INVITE:
						  
						 playerB = User[CurrSock].Sock->Data[3];
						 printf("CurrSock=%i playerB=%i\n", CurrSock, playerB);
						 //FFFF, don't troll me. Is playerB even real? >___>
						 if (playerB < MAX_CLIENTS && playerB != CurrSock && User[playerB].Ident) 
						 {
							 
							 printf("someone wants to trade...legit?\n");
							 // make sure both players aren't busy!!
							 if (User[CurrSock].tradeStatus == 0 && User[playerB].tradeStatus == 0)
							 {
								 printf("%s trade with %s.\n", User[CurrSock].Nick.c_str(), User[playerB].Nick.c_str());
								 
								 //Send 'trade with A' to player B
								 Message = "0";
								 Message += TRADE;
								 Message += INVITE;
								 Message += CurrSock;
								 Message[0] = Message.length(); 
							 User[playerB].SendPacket(Message);
								
								 
								 //hold status of what the players are trying to do
								 // (tentative...)
								 User[CurrSock].tradeStatus = INVITE;
								 User[CurrSock].tradePlayer = playerB;
								 User[playerB].tradeStatus = INVITE;
								 User[playerB].tradePlayer = CurrSock;
							 } // end not busy, lets request, bro!
							 else {
								  printf("[ERR] A trade status=%i B trade status=%i\n", User[CurrSock].tradeStatus, User[playerB].tradeStatus);
							 }
						 }//end invite trade
						 
					 break;//end INVITE
					 
					 case ACCEPT:
						  //hold status of what the players are trying to do
						  // (trading)
						  playerB = User[CurrSock].tradePlayer;
						  
						  if (playerB >=0 && User[CurrSock].tradeStatus == INVITE &&  User[playerB].tradeStatus == INVITE)
						  {
							 User[CurrSock].tradeStatus = ACCEPT;
				 User[playerB].tradeStatus = ACCEPT;
						  }
						  
						  //tell both parties that they are now trading
						  
						  //Send 'trade with A' to player B
						  Message = "0";
						  Message += TRADE;
						  Message += ACCEPT;
						  Message += CurrSock;
						  Message[0] = Message.length(); 
						  User[playerB].SendPacket(Message);
						 
						  //Send 'trade with B' to player A
						  Message = "0";
						  Message += TRADE;
						  Message += ACCEPT;
						  Message += playerB;
						  Message[0] = Message.length(); 
						  User[CurrSock].SendPacket(Message);
						  
					 break;
					 
					 case CANCEL:
						 printf("cancel trade!\n");
						 
						  //okay, kill the trade for this player and the sellected player
						   playerB = User[CurrSock].tradePlayer;
						   printf("playerB is: %i\n", playerB);
		 
						   if (playerB >= 0)
						  {

							   //set them to blank state
							   User[CurrSock].tradeStatus = 0;
							   User[playerB].tradeStatus = 0;
									   
							   //set them to be trading with nobody
							   User[CurrSock].tradePlayer = -1;
							   User[playerB].tradePlayer = -1;
									   
							   //tell both players cancel trade
							   Message = "0";
							   Message += TRADE;
							   Message += CANCEL;
							   Message[0] = Message.length(); 
							   User[playerB].SendPacket(Message);
							   User[CurrSock].SendPacket(Message);
								   
							   //clear trade windows...
							   for (int i = 0 ; i < 256; i++)
							   {
										User[CurrSock].localTrade[i] = 0;
										User[playerB].localTrade[i] = 0;
								}
						   
					   printf("[just checkin' bro] A trade status=%i B trade status=%i\n", User[CurrSock].tradeStatus, User[playerB].tradeStatus);
							} 
					 break;
					 
					 case OFFER:
						  //only do something if both parties are in accept trade mode
						  if (User[CurrSock].tradeStatus == ACCEPT && User[User[CurrSock].tradePlayer].tradeStatus == ACCEPT)
						  {
							  int item = User[CurrSock].Sock->Data[3];
							  
							  int amount = 0;
							  ((char*)&amount)[0] = User[CurrSock].Sock->Data[4];
							  ((char*)&amount)[1] = User[CurrSock].Sock->Data[5];
							  ((char*)&amount)[2] = User[CurrSock].Sock->Data[6];
							  ((char*)&amount)[3] = User[CurrSock].Sock->Data[7];
							  
							  
							  printf("offer!\n ITEM: [%i]\nAMOUNT [%i/%i]\n...", item, amount, User[CurrSock].inventory[item]);
							  //check if you have that item
							  if (User[CurrSock].inventory[item] >= amount)
							  {
								 User[CurrSock].localTrade[item] = amount;
								 
								 
								 //send to both players!
								 std::string oPacket = "0";
								 oPacket += TRADE;
								 oPacket += OFFER;
								 oPacket += 1; //local
								 oPacket += item;
								 oPacket += ((char*)&amount)[0];
								 oPacket += ((char*)&amount)[1];
								 oPacket += ((char*)&amount)[2];
								 oPacket += ((char*)&amount)[3];
								 oPacket[0] = oPacket.length();
								 User[CurrSock].SendPacket(oPacket);
								 
								 //send to playerB
								 oPacket = "0";
								 oPacket += TRADE;
								 oPacket += OFFER;
								 oPacket += 2; //remote
								 oPacket += item;
								 oPacket += ((char*)&amount)[0];
								 oPacket += ((char*)&amount)[1];
								 oPacket += ((char*)&amount)[2];
								 oPacket += ((char*)&amount)[3];
								 oPacket[0] = oPacket.length();
								 User[User[CurrSock].tradePlayer].SendPacket(oPacket);
								 printf("YUP!\n");
							  }
							  else printf("NOPE!\n");
													  
						  } else {
								 printf("both parties not in trade mode!");
						  }
					 break;
					 
					 
					 case CONFIRM:
						  
						  printf("CONFIRM!");
						  
						  //easy to understand if we use A & B
						  int playerA = CurrSock;
						  int playerB = User[CurrSock].tradePlayer;
						  
						  if (playerB < 0)
						  {
							 printf("to prevent a crash, playerB is -1 while confirm trading so I'm escaping!\n");
							 //tell both players cancel trade
							 Message = "0";
							 Message += TRADE;
							 Message += CANCEL;
							 Message[0] = Message.length(); 
							 User[CurrSock].SendPacket(Message);
							 User[CurrSock].tradeStatus = -1;
							 User[CurrSock].tradePlayer = 0;
							 break;
						  }
						  
						  //make sure playerA is in accept mode before confirming
						  if (User[playerA].tradeStatus == ACCEPT)
							 User[playerA].tradeStatus = CONFIRM;
						  
						  //tell both players :)
						  std::string readyPacket = "0";
						  readyPacket += TRADE;
						  readyPacket += READY;
						  readyPacket += 1;
						  readyPacket[0] = readyPacket.length();
						  User[playerA].SendPacket(readyPacket);
						  
						  readyPacket = "0";
						  readyPacket += TRADE;
						  readyPacket += READY;
						  readyPacket += 2;
						  readyPacket[0] = readyPacket.length();
						  User[playerB].SendPacket(readyPacket);
								
						  
						  //if both players are now confirm, transact and reset!
						  if (User[playerA].tradeStatus == CONFIRM && User[playerB].tradeStatus == CONFIRM)
						  {
							  //lets make sure players have that many items
							  bool error = false;
							  for (int i = 0; i < 256; i++)
							  {
								  //compare the offer to the owned items
								 if (User[playerA].inventory[i] < User[playerA].localTrade[i]) {
									error = true;
									break;
								 }
								 //compare the offer to the owned items
								 if (User[playerB].inventory[i] < User[playerB].localTrade[i]) {
									error = true;
									break;
								 }
								  
							  }
							  
							  //tell them the trade is over!!! 
								   User[playerA].tradeStatus = 0;
								   User[playerB].tradeStatus = 0;
								   
								   //set them to be trading with nobody
								   User[playerA].tradePlayer = -1;
								   User[playerB].tradePlayer = -1;
								   
								   //tell both players cancel trade
								   std::string Message = "0";
								   Message += TRADE;
								   Message += CANCEL;
								   Message[0] = Message.length(); 
								   User[playerB].SendPacket(Message);
								   User[playerA].SendPacket(Message);
								 
								 
							  //make the transaction!
							  if (!error)
							  {
								 printf("trade transaction!\n");
								  
								 //go through each item and add them        
								 for (int i = 0; i < 256; i++)
								 {
									 //easy to follow with these variables broski :P
									 int aOffer = User[playerA].localTrade[i]; 
									 int bOffer = User[playerB].localTrade[i];
									 
									 //give A's stuff  to B
									 if (aOffer > 0) 
									 {
										printf("%s gives %i of item %i to %s\n", User[playerA].Nick.c_str(), aOffer, i, User[playerB].Nick.c_str());
										//trade the item and tell the player!
										User[playerB].inventory[i] += aOffer;
										User[playerA].inventory[i] -= aOffer;
										
										//put in players inventory
										std::string Packet = "0";
										Packet += POCKET_ITEM;
										Packet += i;
										char *p;
										char b1, b2, b3, b4;
										
										//break up the int as 4 bytes
										p=(char*)&User[playerB].inventory[i];
										b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
										Packet += b1;
										Packet += b2;
										Packet += b3;
										Packet += b4;
										
										Packet[0] = Packet.length();
										User[playerB].SendPacket(Packet);
										
										//take it out of A's inventory
										Packet = "0";
										Packet += POCKET_ITEM;
										Packet += i;
										
										//break up the int as 4 bytes
										p=(char*)&User[playerA].inventory[i];
										b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
										Packet += b1;
										Packet += b2;
										Packet += b3;
										Packet += b4;
										
										Packet[0] = Packet.length();
										User[playerA].SendPacket(Packet);
									 }
									 
									 //give B's stuff  to A
									 if (bOffer > 0) 
									 {
										printf("%s gives %i of item %i to %s\n", User[playerB].Nick.c_str(), aOffer, i, User[playerA].Nick.c_str());
										//trade the item and tell the player!
										User[playerA].inventory[i] += bOffer;
										User[playerB].inventory[i] -= bOffer;
										
										//put in players inventory
										std::string Packet = "0";
										Packet += POCKET_ITEM;
										Packet += i;
										char *p;
										char b1, b2, b3, b4;
										
										//break up the int as 4 bytes
										p=(char*)&User[playerA].inventory[i];
										b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
										Packet += b1;
										Packet += b2;
										Packet += b3;
										Packet += b4;
										
										Packet[0] = Packet.length();
										User[playerA].SendPacket(Packet);
										
										
										//take it away from B
										Packet = "0";
										Packet += POCKET_ITEM;
										Packet += i;
										
										//break up the int as 4 bytes
										p=(char*)&User[playerB].inventory[i];
										b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
										Packet += b1;
										Packet += b2;
										Packet += b3;
										Packet += b4;
										
										Packet[0] = Packet.length();
										User[playerB].SendPacket(Packet);
										
									 }
									 
									 //clear the items
									 User[playerA].localTrade[i] = 0;
									 User[playerB].localTrade[i] = 0;
								 }
								 
								 //don't cheat! save everyone!!
								 persistTicker = Clock();
								saveAllProfiles();
			
							  }                                           
						  }
					 break;
				 }//end trade action switch  
				 
			}//invite to trade
			else if (code == PARTY)
			{


	 int mode = User[CurrSock].Sock->Data[2];
	 int playerB = 0;

				 
	 switch (mode)
	 {

	case INVITE:
		
		playerB = User[CurrSock].Sock->Data[3];

		//invite the other user 
		//if the other user is not in your party
		if( User[playerB].partyStatus == 0)
		{
			int partyID = User[CurrSock].party;

			//set their party
			if (User[CurrSock].party == -1)
			{
				for (partyID = 0; partyID <= MAX_CLIENTS; partyID++)
				{
					//look for an open partyID
					bool taken = false;
					for (int i = 0; i < MAX_CLIENTS; i++)
					{	
					if (User[i].Ident && User[i].party == partyID)
					{
						taken = true; break;
						}
					}
					if (!taken) break;
				}//find oepn party id
			}//end if party is null
			
			//make the parties equal
			User[CurrSock].party = partyID; 
			User[playerB].party = partyID;

			//set the party status of the invited person
			User[playerB].partyStatus = INVITE;
			User[CurrSock].partyStatus = ACCEPT;
			

			//ask the user to party
			std::string invitePacket = "0";
			invitePacket += PARTY;
			invitePacket += INVITE;
			invitePacket += CurrSock;
			invitePacket[0] = invitePacket.length();
			User[playerB].SendPacket(invitePacket);
		}
	break;

	case ACCEPT:
		//only if I was invited :)
		if (User[CurrSock].partyStatus == INVITE)
		{
			//set my status and all that garbage to be 
			//in the party I was invited to
			User[CurrSock].partyStatus = PARTY;
			std::string acceptPacket = "0", acceptPacket2;
			acceptPacket += PARTY;
			acceptPacket += ACCEPT;
			acceptPacket += CurrSock;
			acceptPacket += User[CurrSock].party;
			acceptPacket[0] = acceptPacket.length();		


			int found = false;int inv;
			for (inv = 0; inv < MAX_CLIENTS; inv++)
				if (User[inv].partyStatus == ACCEPT && User[inv].party == User[CurrSock].party)
				{
					User[inv].partyStatus = PARTY;
					found = true;
					break;	
				}
			if (found)
			{
				acceptPacket2 = acceptPacket;
				acceptPacket2[3] = inv; 
			}
			//tell everyone
			for (int pl = 0; pl < MAX_CLIENTS; pl++)
				if (User[pl].Ident)
				{
					User[pl].SendPacket(acceptPacket);		
					
					//tell noob my hp
													std::string hpPacket = "0";
													hpPacket += BUDDY_HP;
													hpPacket += CurrSock;
													hpPacket += (int)((User[CurrSock].hp / (float)User[CurrSock].max_hp) * 80);
													hpPacket[0] = hpPacket.length();
													User[pl].SendPacket(hpPacket);


													//tell noob my xp
													std::string xpPacket = "0";
													xpPacket += BUDDY_XP;
													xpPacket += CurrSock;
													xpPacket += (int)((User[CurrSock].xp / (float)User[CurrSock].max_xp) * 80);
													xpPacket[0] = xpPacket.length();
													User[pl].SendPacket(xpPacket);

													//tell noob my level
													std::string lvlPacket = "0";
													lvlPacket += BUDDY_LEVEL;
													lvlPacket += CurrSock;
					lvlPacket += User[CurrSock].level;
													lvlPacket[0] = lvlPacket.length();
													User[pl].SendPacket(lvlPacket);


					if (found)
						User[pl].SendPacket(acceptPacket2);

					//tell the user about all the players in the party
					if (pl != CurrSock && User[pl].partyStatus == PARTY && User[pl].party == User[CurrSock].party)
					{
						//tell noob I am in his party
						std::string buddyPacket = "0";
						buddyPacket += PARTY;
						buddyPacket += ACCEPT;
						buddyPacket += pl;
						buddyPacket += User[pl].party;
						buddyPacket[0] = buddyPacket.length();
						User[CurrSock].SendPacket(buddyPacket);

						//tell noob my hp
						std::string hpPacket = "0";
						hpPacket += BUDDY_HP;
						hpPacket += pl;
						hpPacket += (int)((User[pl].hp / (float)User[pl].max_hp) * 80);
						hpPacket[0] = hpPacket.length();
						User[CurrSock].SendPacket(hpPacket);
						
					
						//tell noob my xp
															std::string xpPacket = "0";
															xpPacket += BUDDY_XP;
															xpPacket += pl;
															xpPacket += (int)((User[pl].xp / (float)User[pl].max_xp) * 80);
															xpPacket[0] = xpPacket.length();
															User[CurrSock].SendPacket(xpPacket);
						
						//tell noob my level
						std::string lvlPacket = "0";
						lvlPacket += BUDDY_LEVEL;
						lvlPacket += pl;
						lvlPacket += User[pl].level;
						lvlPacket[0] = lvlPacket.length();
						User[CurrSock].SendPacket(lvlPacket);

						}
					}

				}	
		
				break;

				
				case CANCEL:
						//quit them out of this party
												quitParty(CurrSock); 
				break;
			
				default:break;
				 }//end switch mode
			}//invite to party
			else if (code == CLAN)
			{
					 int mode = User[CurrSock].Sock->Data[2];
					 int playerB = 0;

					 printf("Clan mode is %i\n", mode);
					 switch (mode)
					 {

						case INVITE:  
							
							playerB = User[CurrSock].Sock->Data[3];

							//invite the other user 
							//if the other user is not in your clan
							if (User[playerB].Clan[0] == '(')
							{

								//check if the inviter player is even in a clan
								if (User[CurrSock].Clan[0] == '(')
								{
									std::string Message = "0";
									Message += CHAT;
									Message += "You Are Not In A Clan!";
									Message[0] = Message.length();
									
									User[CurrSock].SendPacket(Message);
								}
								else
								{
									//make sure they are clan owner
									std::string strl_SQL = "";
									strl_SQL += "SELECT COUNT(clan.id) FROM clan INNER JOIN player ON clan.owner_id = player.id";
									strl_SQL += " WHERE player.username LIKE '";
									strl_SQL += User[CurrSock].Nick;
									strl_SQL += "'";
									
									db->query(strl_SQL);
									
									if (db->count())
									{
										db->nextRow();
										//if you own the clan...
										if (db->getInt(0))
										{
											
											//get clan id
											std::string strl_SQL = "";
											strl_SQL += "SELECT clan_id FROM player";
											strl_SQL += " WHERE username LIKE '";
											strl_SQL += User[CurrSock].Nick;
											strl_SQL += "'";
											
											
											db->query(strl_SQL);
											std::string uidl_CLAN = "";
											if (db->count())
											{
												db->nextRow();
												uidl_CLAN = db->getString(0);
											}
											else
											{ 
												printf("IMPOSSSIBRUU!!!!!!!!! 3456456\n");
												break;
											} 
											
											
											//set the clan status of the invited person
											User[playerB].clanStatus = INVITE;
											User[playerB].tempClanId = uidl_CLAN;
										
								

											//ask the user to clan
											std::string invitePacket = "0";
											invitePacket += CLAN; 
											invitePacket += INVITE;
											invitePacket += CurrSock;
											invitePacket[0] = invitePacket.length();
											User[playerB].SendPacket(invitePacket);
											
										}
										else
										{
											std::string Message = "0";
											Message += CHAT;
											Message += "You Are Not The Clan Owner!";
											Message[0] = Message.length();
											
											User[CurrSock].SendPacket(Message);
										}
									}
								}
							}
							else
							{
								std::string Message = "0";
								Message += CHAT;
								Message += "Sorry, ";
								Message += User[playerB].Nick;
								Message += " Is Already In A Clan.";
								Message[0] = Message.length();
								
								User[CurrSock].SendPacket(Message);
							}
						break;
				
						case ACCEPT:
							printf("Clan Accept.----------------------------\n");
							//only if I was invited :)
							if (User[CurrSock].clanStatus == INVITE)
							{printf("Clan Accept.---&&&&&&&&&&&&&&&&&&&&&&&&&&&&------\n");
								//set the clan of the player in the DB
								std::string strl_SQL = "UPDATE player SET clan_id = ";
								strl_SQL += User[CurrSock].tempClanId;
								strl_SQL += " WHERE username LIKE '";
								strl_SQL += User[CurrSock].Nick;
								strl_SQL += "'";
								
								db->query(strl_SQL);
								
								 User[CurrSock].Sock->Close();
								 User[CurrSock].Sock->Connected = false;

									 
							}
							
						break;

						
						case CANCEL:
								//quit them out of this clan
								User[CurrSock].clanStatus = -1;
						break;
					
						default:break;
					 }//end switch mode
			}//invite to clan
			else if (code == SHOP)
			{
				 printf("stall...");
				 int action = User[CurrSock].Sock->Data[2];
				 int stallId = 0;
				 printf("Action %i\n", action);     
	 int current_map = User[CurrSock].current_map;
	  
	 switch (action)
				 {
						case INVITE:
							 printf("INVITE BABY\n");
		 stallId = User[CurrSock].Sock->Data[3];
							 if (stallId < map[current_map].num_stalls)
							 {
										if (map[current_map].Stall[stallId].shopid == 0)
										{
										   printf(" bank");
										   
										   if (User[CurrSock].tradeStatus < 1)
										   {
											   std::string packet = "0";
											   packet += BANK;
											   packet += INVITE;
											   packet[0] = packet.length();
											   User[CurrSock].SendPacket(packet);
											   
											   //set trade status to banking
											   User[CurrSock].tradeStatus = BANK;
										   }
										}
										if (map[current_map].Stall[stallId].shopid > 0)
										{
										   //tell the user "hey, shop #? opened up"
										   std::string packet = "0";
										   packet += SHOP;
										   packet += map[current_map].Stall[stallId].shopid;
										   packet[0] = packet.length();
										   User[CurrSock].SendPacket(packet);
										   
										   //set trade status to shopping
										   User[CurrSock].tradeStatus = SHOP;
										   User[CurrSock].tradePlayer = map[current_map].Stall[stallId].shopid;
										}
							 }
							 else
							 {
								   printf("That is not a shop.");
							 }
						break;
						
						
						case BUY:
							 {
			 printf("BUY BABY!\n");
								 int sitem, amount;
								 sitem = User[CurrSock].Sock->Data[3];
								 //hold the result...
								 //build an int from 4 bytes
								 ((char*)&amount)[0] = User[CurrSock].Sock->Data[4];
								 ((char*)&amount)[1] = User[CurrSock].Sock->Data[5];
								 ((char*)&amount)[2] = User[CurrSock].Sock->Data[6];
								 ((char*)&amount)[3] = User[CurrSock].Sock->Data[7];
								 
								 
								 int i = 0, item = 0, price = 0;
								 for (int y = 0; y < 4; y++)
								 for (int x = 0; x < 6; x++)
								 {
									 if (i == sitem)
									 {
										 item  = map[current_map].Shop[User[CurrSock].tradePlayer].item[x][y][0];
										 price = map[current_map].Shop[User[CurrSock].tradePlayer].item[x][y][1];
									 }
									 i++;
								 }
								 
								 //printf("%s wants to buy %d of item #%d\n", User[CurrSock].Nick.c_str(), amount, item);
								 
								 
								 //if the player can afford it
								 if (item > 0 && User[CurrSock].inventory[ITEM_GOLD] >= price*amount)
								 {
									 //take away gold
									 User[CurrSock].inventory[ITEM_GOLD] -= price*amount;
									 
									 //give them the item
									 User[CurrSock].inventory[item] += amount; 
									 
									 //tell the player cosmetically
									 //put in players inventory
									std::string Packet = "0";
									Packet += POCKET_ITEM;
									Packet += item;
									char *p;
									char b1, b2, b3, b4;
									
									//break up the int as 4 bytes
									p=(char*)&User[CurrSock].inventory[item];
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Packet += b1;
									Packet += b2;
									Packet += b3;
									Packet += b4;
									
									Packet[0] = Packet.length();
									User[CurrSock].SendPacket(Packet);
									
									Packet = "0";
									Packet += POCKET_ITEM;
									Packet += (char)ITEM_GOLD;
									
									//break up the int as 4 bytes
									p=(char*)&User[CurrSock].inventory[ITEM_GOLD];
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Packet += b1;
									Packet += b2;
									Packet += b3;
									Packet += b4;
									
									Packet[0] = Packet.length();
									User[CurrSock].SendPacket(Packet);
									 
								 }
							 }
					 
						break;
						
						
						case SELL:
							 {
								 printf("SELL baby!\n");
			 int item = 0, amount = 0, price = 0;
								 item = User[CurrSock].Sock->Data[3];
								 price = Item[item].price;

								 //hold the result...
								 //build an int from 4 bytes
								 ((char*)&amount)[0] = User[CurrSock].Sock->Data[4];
								 ((char*)&amount)[1] = User[CurrSock].Sock->Data[5];
								 ((char*)&amount)[2] = User[CurrSock].Sock->Data[6];
								 ((char*)&amount)[3] = User[CurrSock].Sock->Data[7];
								 
								 
								 //if they even have the item to sell
								 if (price > 0 && User[CurrSock].inventory[item] >= amount)
								 {
									 //take away gold
									 User[CurrSock].inventory[item] -= amount;
									 
									 //give them the item
									 User[CurrSock].inventory[ITEM_GOLD] += price*amount; 
									 
									 //tell the player cosmetically
									 //put in players inventory
									std::string Packet = "0";
									Packet += POCKET_ITEM;
									Packet += item;
									char *p;
									char b1, b2, b3, b4;
									
									//break up the int as 4 bytes
									p=(char*)&User[CurrSock].inventory[item];
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Packet += b1;
									Packet += b2;
									Packet += b3;
									Packet += b4;
									
									Packet[0] = Packet.length();
									User[CurrSock].SendPacket(Packet);
									
									Packet = "0";
									Packet += POCKET_ITEM;
									Packet += (char)ITEM_GOLD;
									
									//break up the int as 4 bytes
									p=(char*)&User[CurrSock].inventory[ITEM_GOLD];
									b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
									Packet += b1;
									Packet += b2;
									Packet += b3;
									Packet += b4;
									
									Packet[0] = Packet.length();
									User[CurrSock].SendPacket(Packet);
									 
								 }
							 }
					 
						break;
						
						
						case CANCEL:
		 printf("CANCEL, BABY!\n");
							 User[CurrSock].tradeStatus = 0;
							 User[CurrSock].tradePlayer = 0;
						break;
						
						default:
		printf("DEFAULT BABY!\n");
						break;
				 }
				 printf("\n");
			}//end shop
			else if (code == BANK)
			{
                             printf("bank use...\n");
                             int action = User[CurrSock].Sock->Data[2];
                             std::string packet;
                             switch (action)
                             {
                                  case CANCEL:
                                       if (User[CurrSock].tradeStatus == BANK)
                                       {   
                                           User[CurrSock].tradeStatus = 0;
                                           
                                       }
                                  break;
                                  
                                  case BANK_ITEM:
                                       if (User[CurrSock].tradeStatus == BANK)
                                       {   
                                           //get item type and amount
                                           int item = User[CurrSock].Sock->Data[3];
                                           int amount = 0;
                                           ((char*)&amount)[0] = User[CurrSock].Sock->Data[4];
                                           ((char*)&amount)[1] = User[CurrSock].Sock->Data[5];
                                           ((char*)&amount)[2] = User[CurrSock].Sock->Data[6];
                                           ((char*)&amount)[3] = User[CurrSock].Sock->Data[7];
                                           
                                           if (item < NUM_ITEMS && User[CurrSock].inventory[item] >= amount)
                                           {
                                              User[CurrSock].inventory[item] -= amount;
                                              User[CurrSock].bank[item] += amount;
                                              
                                              //send packets to user
                                              std::string packet = "0";
                                              packet += BANK_ITEM;
                                              packet += item;
                                              //break up the int as 4 bytes
                                              char* p=(char*)&User[CurrSock].bank[item];
                                              char b1, b2, b3, b4;
                                              b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
                                              packet += b1;
                                              packet += b2;
                                              packet += b3;
                                              packet += b4;
                                              packet[0] = packet.length();
                                              User[CurrSock].SendPacket(packet);
                                              
                                              packet = "0";
                                              packet += POCKET_ITEM;
                                              packet += item;
                                              //break up the int as 4 bytes
                                              p=(char*)&User[CurrSock].inventory[item];
                                              b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
                                              packet += b1;
                                              packet += b2;
                                              packet += b3;
                                              packet += b4;
                                              packet[0] = packet.length();
                                              User[CurrSock].SendPacket(packet);
                                           }
                                       }
                                  break;
                                  
                                  case DEBANK_ITEM:
                                       if (User[CurrSock].tradeStatus == BANK)
                                       {   
                                           //get item type and amount
                                           int item = User[CurrSock].Sock->Data[3];
                                           int amount = 0;
                                           ((char*)&amount)[0] = User[CurrSock].Sock->Data[4];
                                           ((char*)&amount)[1] = User[CurrSock].Sock->Data[5];
                                           ((char*)&amount)[2] = User[CurrSock].Sock->Data[6];
                                           ((char*)&amount)[3] = User[CurrSock].Sock->Data[7];
                                           
                                           if (item < NUM_ITEMS && User[CurrSock].bank[item] >= amount)
                                           {
                                              User[CurrSock].bank[item] -= amount;
                                              User[CurrSock].inventory[item] += amount;
                                              
					      //send packets to user
                                              std::string packet = "0";
                                              packet += BANK_ITEM;
                                              packet += item;
                                              //break up the int as 4 bytes
                                              char* p=(char*)&User[CurrSock].bank[item];
                                              char b1, b2, b3, b4;
                                              b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
                                              packet += b1;
                                              packet += b2;
                                              packet += b3;
                                              packet += b4;
                                              packet[0] = packet.length();
                                              User[CurrSock].SendPacket(packet);

                                              packet = "0";
                                              packet += POCKET_ITEM;
                                              packet += item;
                                              //break up the int as 4 bytes
                                              p=(char*)&User[CurrSock].inventory[item];
                                              b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
                                              packet += b1;
                                              packet += b2;
                                              packet += b3;
                                              packet += b4;
                                              packet[0] = packet.length();
                                              User[CurrSock].SendPacket(packet);
                                           }
                                       }
                                  break;
                                  
                                  //shouldn't happen
                                  default:
                                  break;  
                             }
                        }//end bank
			else if (code == INVENTORY)
			{
				printf("inventory order!\n");
				std::string inventory_order = User[CurrSock].Sock->Data.substr(2);
				for (int i = 0; i < inventory_order.length(); i++)
					printf("[%i]", inventory_order[i]);
				printf("\n");
				User[CurrSock].inventory_order = inventory_order;

			}
			else if (code == MAKE_CLAN)
			{
			    //check if the player has enough money.
			    if (User[CurrSock].inventory[ITEM_GOLD] >= 100000) // 100000 TODO make a const for this
			    { 
					User[CurrSock].inventory[ITEM_GOLD] -= 100000;
					//put in players inventory
					std::string Packet = "0";
					Packet += POCKET_ITEM;
					Packet += ITEM_GOLD;

					//break up the int as 4 bytes
					int amt = User[CurrSock].inventory[ITEM_GOLD];
					//  printf("\n\nPutting this item in player's inventory!\nAmount: %i\nWhich item object? [0-255]: %i\nWhich item? [0-1]: %i\n", amt, i, ID);
					//  printf("Player has [%i] ITEM_GOLD and [%i] ITEM_MYSTERY_BOX\n\n", User[c].inventory[0], User[c].inventory[1]);
					char * p=(char*)&amt;
					char b1,b2,b3,b4;
					b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
					Packet += b1;
					Packet += b2;
					Packet += b3;
					Packet += b4;

					Packet[0] = Packet.length();
					User[CurrSock].SendPacket(Packet);
					std::string clanTag = User[CurrSock].Sock->Data.substr(2);
					trim (clanTag);



					//find out if it exists already...
					std::string sql = "SELECT COUNT(*) FROM clan WHERE clan.name LIKE '";
					sql += db->clean(clanTag); 
					sql += "'";

					db->query(sql);
					if (db->count())
					{
						db->nextRow();
						if (db->getInt(0) > 0)
						{
							printf("clan already exists...%s\n", clanTag.c_str());

							std::string Message = "0";
							Message += CHAT;
							Message += "Clan [";
							Message += clanTag;
							Message += "] Already Exists!";
							Message[0] = Message.length();

							User[CurrSock].SendPacket(Message);
						}
						else
						{
							std::string sql = "SELECT * FROM player WHERE username LIKE '";
							sql += db->clean(User[CurrSock].Nick);
							sql += "'";

							db->query(sql);
							if (db->count())
							{
								db->nextRow();
								//get the id for loading purposes
								std::string player_id = db->getString(0);



								//update the clan tables!!!!
								sql = "";
								sql += "INSERT INTO clan (name, owner_id) VALUES ('";
								sql += db->clean(clanTag);
								sql += "',";

								std::string creator_id = "NULL";
								std::string sql2 = "SELECT * FROM player WHERE username LIKE '";
								sql2 += db->clean(User[CurrSock].Nick);
								sql2 += "'";
								db->query(sql2, true);
								if (db->count())
								{
									db->nextRow();
									creator_id = db->getString(0);
								}


								sql += creator_id;
								sql += ")";

								db->query(sql, true);


								std::string Message = "0";
								Message += CHAT;
								Message += "Clan [";
								Message += clanTag;
								Message += "] Has Been Established!";
								Message[0] = Message.length();

								//send to all players
								for (int cu1 = 0; cu1 < MAX_CLIENTS; cu1++)
								{
									if (User[cu1].Ident)
										User[cu1].SendPacket(Message);
								} //end loop all clients


								//set player clan id

								sql = "";
								sql += "UPDATE player SET player.clan_id = (SELECT clan.id FROM clan WHERE clan.name LIKE '";
								sql += db->clean(clanTag);
								sql += "') WHERE player.id LIKE '";
								sql += player_id;
								sql += "'";

								db->query(sql);

								User[CurrSock].Sock->Close();
								User[CurrSock].Sock->Connected = false;

							} //end username exists
						} //end else clan is not a dupe
					} //query success
				} //end you have enough gold
				else  
				{
					std::string Message = "0";
					Message += CHAT;
					Message += "You Do Not Have Enough Gold To Establish A Clan!";
					Message[0] = Message.length();

					// Send data
					User[CurrSock].SendPacket(Message);
				}//end you did not have enough $$	
			}//end make clan
			else if (code == CAST_SPELL)
			{
			   //What do you have equipped?
			   int spell = User[CurrSock].equip[2];

				if (spell > 0)
				{
					if (User[CurrSock].inventory[spell] > 0){
						User[CurrSock].inventory[spell]--;
				    }
					else {
						User[CurrSock].equip[2] = 0;//no item :) (gold is not gunna be equipped ever)
					}
					
					//tell all the users that an item has been thrown...
					//just use an item object with no value.
					SKO_ItemObject lootItem = SKO_ItemObject();
					lootItem.y = User[CurrSock].y + 24;
					
					if (User[CurrSock].facing_right) 
					{
						lootItem.x_speed = 10;
					    lootItem.x = User[CurrSock].x + 50;		
					}
					else	
					{
						lootItem.x_speed = -10;
						lootItem.x = User[CurrSock].x - 32;
					}
			
					lootItem.y_speed = -3.2;
					lootItem.itemID = spell;
					lootItem.owner = CurrSock;
					lootItem.amount = 1;

					SpawnLoot(User[CurrSock].current_map, lootItem);
					
					std::string eqPacket = "0";
					//equipment
					if (User[CurrSock].equip[2] > 0)
					{
						eqPacket += EQUIP;
						eqPacket += CurrSock;
						eqPacket += (char)2;
						eqPacket += Item[spell].equipID; 
						eqPacket += User[CurrSock].equip[2];
						eqPacket[0] = eqPacket.length(); 
					}
					else
					{
						//send packet that says you arent holding anything!
						eqPacket += EQUIP;
						eqPacket += CurrSock; 
						eqPacket += (char)2;
						eqPacket += (char)0; 
						eqPacket += (char)0; 
						eqPacket[0] = eqPacket.length(); 
					}
					
					for (int bossIsSexy = 0; bossIsSexy < MAX_CLIENTS; bossIsSexy++)
						if (User[bossIsSexy].Ident)
							User[bossIsSexy].SendPacket(eqPacket);
				

					if (User[CurrSock].inventory[spell] == 0)
					{
						User[CurrSock].inventory_index--; 
					}

					std::string outOfSpellMsg = "0";
					outOfSpellMsg += POCKET_ITEM;
					outOfSpellMsg += spell;
					
					int spellAmt = User[CurrSock].inventory[spell];
					char * p1=(char*)&spellAmt;
					char b11, b21, b31, b41;
					b11=p1[0]; b21=p1[1]; b31=p1[12]; b41=p1[3];
					outOfSpellMsg += b11;
					outOfSpellMsg += b21;
					outOfSpellMsg += b31;
					outOfSpellMsg += b41;
					
					outOfSpellMsg[0] = outOfSpellMsg.length();
					
					//send 
					User[CurrSock].SendPacket(outOfSpellMsg);
				}
			}
            else if (code == CHAT)
			{
                            if (!User[CurrSock].Mute && User[CurrSock].Ident)
                            {
                                std::string command = User[CurrSock].Sock->Data.substr(2,User[CurrSock].Sock->Data.find_first_of(" ")-2);
                                //printf("COMMAND: [%s]", command.c_str());
                                if ( command.compare("/ban") == 0 )
    							{
                                    // Declare message string
									std::string Message  = "";
                                    std::string Username = "";
                                    std::string Reason = "";
                                    
    								//fill message with username and reason
    								Message += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								
    								//strip the appropriate data
                                    Username += Message.substr(0,Message.find_first_of(" "));
                                    Reason += Message.substr(Message.find_first_of(" ")+1, Message.length()-Message.find_first_of(" ")+1);
                                    
                                    int result = 0;
                                    
                                    result = ban(CurrSock, Username, Reason, 1);
                                    
                                    if (result == 0)
                                    {
                                       Message = "0";
                                       Message += CHAT;
                                       Message += Username;
                                       Message += " has been banned (";
                                       Message += Reason;
                                       Message += ")";
                                       Message[0] = Message.length();
                                       
                                       for ( int i = 0; i < MAX_CLIENTS; i++ )
                                       { 
                                             // If this socket is taken  
                                            if ( User[i].Status )
                                            {
                                                // Send data
											    User[i].SendPacket(Message);
                                		
                                                //find which socket, yo
                                                if (lower(User[i].Nick).compare(lower(Username)) == 0)
                                                {
                                                   User[i].Sock->Close();
                                                }
                                            }
                                            
                                       }
                                    }
                                    else if (result == 1)
                                    {
                                         Message = "0";
                                         Message += Username;
                                         Message += " does not exist.";
                                         Message[0] = Message.length(); 
                						 // Send data
                					     User[CurrSock].SendPacket(Message);
                                    }
                                    else if (result == 2)
                                    {
                                         Message = "0";
                                         Message += CHAT;
                                         Message += Username;
                                         Message += " cannot be banned.";
                                         Message[0] = Message.length(); 
                						 // Send data
                					     User[CurrSock].SendPacket(Message);
                                    }
                                    else if (result == 3)
                                    {
                                         printf("The user [%s] tried to ban [%s] but they are not moderator!\n", User[CurrSock].Nick.c_str(), Username.c_str());
                                    }
    
                                     
                                }//end "/ban"
                                else if( command.compare("/unban") == 0 )
    							{
                                     // Declare message string
                                    std::string Username = "";
                                    std::string Message  = "";
                                    
    								//fill message with username and reason
    								Message += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								
                                    
    								//strip the appropriate data
                                    Username += Message.substr(Message.find_first_of(" ")+1,pack_len-Message.find_first_of(" ")+1);
                                    
                                    int result = 0;
                                    
                                    result = ban(CurrSock, Username, "null", 0);
                                    
                                    if (result == 0)
                                    {
                                       Message = "0";
                                       Message += CHAT;
                                       Message += Username;
                                       Message += " has been unbanned.";
                                       Message[0] = Message.length(); 
                						 // Send data
                					     User[CurrSock].SendPacket(Message);
                                    }
                                    else if (result == 1)
                                    {
                                         Message = "0";
                                         Message += CHAT;
                                         Message += Username;
                                         Message += " does not exist.";
                                         Message[0] = Message.length();
                						 // Send data
                					     User[CurrSock].SendPacket(Message);
                                    }
                                    else if (result == 2)
                                    {
                                         Message = "0";
                                         Message += CHAT;
                                         Message += Username;
                                         Message += " cannot be unbanned.";
                                         Message[0] = Message.length();
                						 // Send data
                					     User[CurrSock].SendPacket(Message);
                                    }
                                    else if (result == 3)
                                    {
                                         printf("The user [%s] tried to unban [%s] but they are not moderator!\n", User[CurrSock].Nick.c_str(), Username.c_str());
                                    }	
                                     
                                }//end "/unban"
                                else if( command.compare("/mute") == 0 ) 
    							{
                                    // Declare message string
    								std::string Message  = "";
                                    std::string Username = "";
                                    std::string Reason = "";
                                    
    								//fill message with username and reason
    								Message += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								
    								//strip the appropriate data
                                    Username += Message.substr(0,Message.find_first_of(" "));
                                    Reason += Message.substr(Message.find_first_of(" ")+1, Message.length()-Message.find_first_of(" ")+1);
                                    
                                    int result = 0;
                                    
                                    result = mute(CurrSock, Username, 1);
                                    
                                    printf("MUTE result is: %i\n", result);
                                    if (result == 0)
                                    {
                                               
                                       Message = "0";
                                       Message += CHAT;
                                       Message += Username;
                                       Message += " has been muted.";   
                                       Message[0] = Message.length();                                            
                                       
                                       //find the sock of the username
                                       for ( int i = 0; i < MAX_CLIENTS; i++ )
                                       {
                                           //well, unmute the person
                                           std::string lower_username = lower(Username);
                                           std::string lower_nick = lower(User[i].Nick);
                                           
                                           
                                           if (lower_username.compare(lower_nick) == 0){
                                               User[i].Mute = true;
                                               printf("User[i].Mute == true", i);
                                           }
                                               
                                           //well, tell everyone
                                           // If this socket is taken
       								       if ( User[i].Ident )
       								       {
                                              // Send data
                                		      User[i].SendPacket(Message);
            								}
                                       }
                                       
                                           
     							       // Clear socket data
     							       User[CurrSock].Sock->Data = ""; 
                            
     							       
                                    }
                                    else if (result == 1)
                                    {
                                         Message = "0";
                                         Message += CHAT;
                                         Message += "The user ";
                                         Message += Username;
                                         Message += " does not exist!";
                                         Message[0] = Message.length();
                                         
                                         // Send data
                                         User[CurrSock].SendPacket(Message);      
                                    }
                                    else if (result == 2)
                                    {
                                         Message = "0";
                                         Message += CHAT;
                                         Message = "The user ";
                                         Message += Username;
                                         Message += " cannot be muted!";
                                         Message[0] = Message.length();
                                         
                                         // Send data
                                         User[CurrSock].SendPacket(Message);
                                         
                                    }
                                    else if (result == 3)
                                    {
                                         printf("The user [%s] tried to mute [%s] but they are not moderator!\n", User[CurrSock].Nick.c_str(), Username.c_str());
                                    }
                                     
                                }//end "/mute"
                                else if( command.compare("/unmute") == 0 )
    							{
                                     // Declare message string
                                    std::string Username = "";
                                    std::string Message  = "";
    								//fill message with username and reason
    								Message += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								
    								
    								//strip the appropriate data
                                    Username += Message.substr(Message.find_first_of(" ")+1,pack_len-Message.find_first_of(" ")+1);
                                    
                                    int result = 0;
                                    
                                    result = mute(CurrSock, Username, 0);
                                    
                                    if (result == 0)
                                    {
                                               
                                       Message = "0";
                                       Message += CHAT;
                                       Message += Username;
                                       Message += " has been unmuted.";   
                                       Message[0] = Message.length();                                            
                                       
                                       //find the sock of the username
                                       for ( int i = 0; i < MAX_CLIENTS; i++ )
                                       {
                                           //well, unmute the person
                                           if (lower(User[i].Nick).compare(lower(Username)) == 0)
                                               User[i].Mute = false;
                                               
                                           //well, tell everyone
                                           // If this socket is taken
       					   if ( User[i].Ident)
					   {
                                               // Send data
                                	       User[i].SendPacket(Message);
    				           }
                                       }
                                       
     							       
                                    }
                                    else if (result == 1)
                                    {
                                         Message = "The user ";
                                         Message += Username;
                                         Message += " does not exist!\n\r";
                                         
                                         // Send data
                                         User[CurrSock].SendPacket(Message);
          
                                    }
                                    else if (result == 2)
                                    {
                                         Message = "The user ";
                                         Message += Username;
                                         Message += " cannot be unmuted!\n\r";
                                         
                                         // Send data
                                         User[CurrSock].SendPacket(Message);
                                    }
                                    else if (result == 3)
                                    {
                                         printf("The user [%s] tried to unmute [%s] but they are not moderator!\n", User[CurrSock].Nick.c_str(), Username.c_str());
                                    }
                                     
                                }//end "/unmute"       
                                else if( command.compare("/kick") == 0 )
    							{ 
                                    // Declare message string
    				    std::string Message  = "";
                                    std::string Username = "";
                                    std::string Reason = "";
                                    
    								//fill message with username and reason
    								Message += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								
    								//strip the appropriate data
                                    Username += Message.substr(0, Message.find_first_of(" "));
                                    Reason += Message.substr(Message.find_first_of(" ")+1, Message.length()-Message.find_first_of(" ")+1);
                                    
                                    int result = 0;
                                    
                                    result = kick(CurrSock, Username);
                                    
                                    if (result == 0)
                                    {
                                       Message = "0";
                                       Message += CHAT;
                                       Message += Username;
                                       Message += " has been kicked (";
                                       Message += Reason;
                                       Message += ")";
                                       Message[0] = Message.length();
                                       
                                                                        
                                       //okay, now send
                                       for (int i = 0; i < MAX_CLIENTS; i++)
                                       {   
                                           //if socket is taken
                                           if ( User[i].Ident )
                                           {
                                              // Send data
                                		      User[i].SendPacket(Message);
                                           }  
                                           
                                           //kick the user
                                           if (lower(Username).compare(lower(User[i].Nick)) == 0){
                                                 User[i].Sock->Close();
						 User[i].Sock->Connected = false;
                                           }                    
                                       }
            
                                    }
                                    else if (result == 1)
                                    {
                                         Message = "0";
                                         Message += CHAT;
                                         Message += Username;
                                         Message += " is not online.";
                                         Message[0] = Message.length();
                                         
                                         // Send data
                                         User[CurrSock].SendPacket(Message); 
                                    }
                                    else if (result == 2)
                                    {
                                         printf("The user [%s] tried to ban [%s] but they are not moderator!\n", User[CurrSock].Nick.c_str(), Username.c_str());
                                    }
             
                                }//end "/kick"  
                                else if (command.compare("/who") == 0)
                                {
                                     std::string Message = "0";
                                     Message += CHAT;
                                     std::string strNicks = "";
                                     
                                     //find how many players are online
                                     //see how many people are online
                                     int players = 0;
                                     bool flag = false;
                                     
                                     for (int i = 0; i < MAX_CLIENTS; i++ )
                                     {   
                                         //if socket is taken
                                         if (User[i].Ident && User[i].Nick.compare("Paladin") != 0)
					 {
                                            players++;
                                            
                                            //formatting
                                            if (flag)
                                               strNicks += ", ";
                                            else  if (User[i].Ident)
                                                  flag = true;
                                                
                                            //add the nickname to the list
                                            if (User[i].Ident)
                                               strNicks += User[i].Nick;
                                         }
                                     }                                                         
                                     char strPlayers[30];
                                     
                                     //grammar nazi
                                     if (players == 1)
                                        sprintf(strPlayers,  "There is %i player online: ", players);  
                                     else
                                        sprintf(strPlayers,  "There are %i players online: ", players); 
                                          
                                     Message += strPlayers;
                                       
                                    // Replace data with data to send
                                    Message[0] = Message.length();
                                    // Send data
   				    User[CurrSock].SendPacket(Message);
    							   
                                    if (players > 0)
                                    {
    					Message = "0";
    					Message += CHAT;
                                        Message += strNicks;
                                        Message[0] = Message.length();
                                        // Send data
        				User[CurrSock].SendPacket(Message);
        							   // printf("sending WHO like this: [%s]\n", Message.c_str());
                                    }
                                                               
                        
                                }//end /who
                                else if (command.compare("/ipban") == 0)
                                {
                                     // Declare message string
    								std::string Message  = "";
                                    std::string IP = "";
                                    std::string Reason = "";
                                    
    								//fill message with username and reason
    								Message += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								
    								//strip the appropriate data
                                    IP += Message.substr(0,Message.find_first_of(" "));
                                    Reason += Message.substr(Message.find_first_of(" ")+1, Message.length()-Message.find_first_of(" ")+1);
                                    
                                    int result = 0;
                                    
                                    result = ipban(CurrSock, IP, Reason);
                                    
                                    if (result == 0)
                                    {
                                       Message = "0";
                                       Message += "[";
                                       Message += IP;
                                       Message += "] has been banned (";
                                       Message += Reason;
                                       Message += ")";
                                       Message[0] = Message.length();
                                              
                                       // Send data
                             	       User[CurrSock].SendPacket(Message);
                                    }
                                    else if (result == 1)
                                    {
                                          Message = "0";
                                          Message += "[";
                                          Message += IP;
                                          Message += "] was not banned (ERROR!)";
                                          Message[0] = Message.length();
                                               
                                          // Send data
                            	          User[CurrSock].SendPacket(Message);
                                    }
                                                                    
                                }//end /ipban
                                else if (command.compare("/getip") == 0)
                                {
                                     
                                    // Declare message string
                                    std::string Username = "";
                                    std::string Message  = "";
                                    
    								//fill message with username and reason
    								Message += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								
    								//strip the appropriate data
                                    Username += Message.substr(Message.find_first_of(" ")+1,pack_len-Message.find_first_of(" ")+1);
                                    
                              
                                       
                                     //if they are moderator
                                     if (User[CurrSock].Moderator)
                                     {
                                          
                                             //get noob id
                                             std::string sql = "SELECT * FROM player WHERE username like '";
                                             sql += db->clean(Username);
                                             sql += "'";
                                             printf(sql.c_str());
                                             db->query(sql);
                                             
                                             if (db->count())
                                             {  
                                                                                                  
                                                 //get noob id
                                                 db->nextRow();  
                                                 std::string noob_id = db->getString(0);
                                                 
                                                 
                                                 
                                                 //get IP
                                                 std::string IP = "";
                                                 sql = "SELECT * FROM ip_log WHERE player_id like '";
                                                 sql += db->clean(noob_id);
                                                 sql += "' ORDER BY idip_log DESC"; //latest ip logged
                                                 printf(sql.c_str());
                                                 db->query(sql);
                                                
                                                
                                                 if (db->count())
                                                 {  
                                                    //get noob IP
                                                     db->nextRow();
                                                     IP = db->getString(2);
                                                 }
                                                 
                                                 Message = "0";
                                                 Message += CHAT;
                                                 Message += Username;
                                                 Message += " [";
                                                 for (int ch = 0; ch < IP.length(); ch++)
                                                     Message += IP[ch];
                                                 Message += "]";
                                                 Message[0] = Message.length();
                                                 // Send data
                                                 User[CurrSock].SendPacket(Message);
                                                }
                                     }
                                     //else not moderator :(
                                     
                                     
                                }//end if /getip
				else if (command.compare("/warp") == 0)
				{
					if (User[CurrSock].Moderator)
					{
						printf("warp...\n");
						std::string rip = User[CurrSock].Sock->Data;
						
						//get parameters first
						rip = rip.substr(rip.find_first_of(" ")+1);
						printf("rip is %s\n", rip.c_str());

						//username
						std::string warp_user = rip.substr(0, rip.find_first_of(" "));
						rip = rip.substr(rip.find_first_of(" ")+1);
						printf("rip is %s\n", rip.c_str());

						//X
						int warp_x = atoi(rip.substr(0, rip.find_first_of(" ")).c_str());
                                                rip = rip.substr(rip.find_first_of(" ")+1);
 						printf("rip is %s\n", rip.c_str());

						//Y
                                                int warp_y = atoi(rip.substr(0, rip.find_first_of(" ")).c_str());
						rip = rip.substr(rip.find_first_of(" ")+1);
						printf("rip is %s\n", rip.c_str());

						//MAP
						int warp_m = atoi(rip.substr(0, rip.find_first_of(" ")).c_str());
	
						if (warp_m >= NUM_MAPS)
							break;
 
						printf("Warp %s to (%i,%i) on map [%i]\n", warp_user.c_str(), warp_x, warp_y, warp_m);

						//find user
						for (int wu = 0; wu < MAX_CLIENTS; wu++)
						{

							if (User[wu].Ident &&  (lower(User[wu].Nick).compare(lower(warp_user)) == 0))
							{
								printf("SHOOOOOM!\n");

								SKO_Portal warp_p = SKO_Portal();
								warp_p.spawn_x = warp_x;
								warp_p.spawn_y = warp_y;
								warp_p.map = warp_m;
								
								Warp(wu, warp_p);
								break;
							}
						}						


					}
				}
				else if (command.compare("/ping") == 0)
				{
				// Declare message string
                                    std::string Username = "";
                                       
                                     //if they are moderator
                                     if (User[CurrSock].Moderator)
                                     {
					
					 //find username
                                        Username += User[CurrSock].Sock->Data.substr(User[CurrSock].Sock->Data.find_first_of(" ")+1,pack_len - User[CurrSock].Sock->Data.find_first_of(" ")+1);
    								

                              	     	int datUser;
				     	bool result = false;
			
				     	for (datUser = 0; datUser < MAX_CLIENTS; datUser++)
				     	{
						if (User[datUser].Ident && User[datUser].Nick.compare(Username) == 0)
						{
							printf("Moderator inquiry of %s\n", Username.c_str());
							result = true;
							break;
						}
				     	}		
						
					//find user
					std::string pingDumpPacket = "0";
					pingDumpPacket += CHAT;
		
					if (result)
					{
						std::stringstream ss;
						ss << "User[" << datUser << "] " << User[datUser].Nick << " has a ping of " << User[datUser].ping;
						pingDumpPacket += ss.str();
					}
					else
					{
						pingDumpPacket += "'";
						pingDumpPacket += Username;
						pingDumpPacket += "' not found.";
					}
				
					//tell the mod
					pingDumpPacket[0] = pingDumpPacket.length();
					User[CurrSock].SendPacket(pingDumpPacket);
				     }

				}//end if /ping
				else if (command[0] == '/')
				{
					printf("not a command...\n");
				}
				else//just chat 
				{ 
					//wrap long text messages
        				int max = 60 - User[CurrSock].Nick.length();
					
					// Declare message string
        				std::string Message;
        
 					// Add nick to the send
      					Message = "0";
       					Message += CHAT;
      					
       					if (User[CurrSock].Nick.compare("Paladin") != 0)
       					{
            							
							Message += User[CurrSock].Nick;
							Message += ": ";
						}
        				else
						max = 52;			
        							
        				std::string chatFull = User[CurrSock].Sock->Data.substr(2,pack_len);        					
					std::string chatChunk = "";
	

					bool doneWrapping = false;
        			if (chatFull.length() >= max)
					{
					   Message += chatFull.substr(0, max);
					   chatChunk = chatFull.substr(max, chatFull.length() - max);	
					}
					else
					{
					   Message += chatFull;
					   doneWrapping = true;
					}

        			Message[0] = Message.length();
	
					int nickLength = 0; 

					if (User[CurrSock].Nick.compare("Paladin") == 0)
					{
						if (!doneWrapping && chatFull.find(": ") != std::string::npos)
						{	
							nickLength += chatFull.find(": ");
							nickLength += 2;//colon and space
							max -= nickLength;
						}
					}//end paladin
					else
					{
						nickLength = User[CurrSock].Nick.length();
					}	

					while (!doneWrapping)
					{ 
						printf("wrapping...");
						std::string cPacket = "0"; 
					        cPacket += CHAT;

						//add spaces
						for (int c = 0; c < nickLength; c++)
							cPacket += " ";
	
						if (User[CurrSock].Nick.compare("Paladin") != 0)
							cPacket += "  ";

 
						if (chatChunk.length() > max)
						{
							//add message
							cPacket += chatChunk.substr(0, max);
							chatChunk = chatChunk.substr(max);
						}
						else 
						{
							cPacket += chatChunk;
							doneWrapping = true;
						}
						cPacket[0] = cPacket.length();
						
						Message += cPacket;
					}	



					//okay, now send
					for (int i = 0; i < MAX_CLIENTS; i++ )
					{    
						//if socket is taken
						if (User[i].Ident)
						User[i].SendPacket(Message);
					}


					printf("\e[0;33mChat: [%s] %s\e[m\n", User[CurrSock].Nick.c_str(), chatFull.c_str());
				}//end just chat
			}//end if not muted
		}//end chat
		else
		{
			//the user is hacking, or sent a bad packet
			User[CurrSock].Sock->Close();
			printf("ERR_BAD_PACKET_WAS: ");
			for (int l = 0; l < User[CurrSock].Sock->Data.length(); l++)
				printf("[%i]", (int)User[CurrSock].Sock->Data[l]);
			User[CurrSock].Sock->Data = "";
		}//end error packet
                            
		//   printf("X: %i\n", User[CurrSock].Sock->GetStatus());
		//put the extra data (if any) into data
		User[CurrSock].Sock->Data = newPacket;

   }//end execute packet


		}//end sock is good
		else
		{
				/** disconnect **/
				
			//immediately disconnect player?
			// TODO - yes lets try it 3/10/2015
				User[CurrSock].Ident = false;
				User[CurrSock].Sock->Close();
				User[CurrSock].Sock->Connected = false;
				
			// Output error
			perror("[ ERR ] Disconnect.");

			// Output socket status
			printf("[ ERR ] User status: %i\n", User[CurrSock].Status);

			// Output that socket was deleted
			printf("[ !!! ] Client %i Disconnected (%s)\n", CurrSock, User[CurrSock].Nick.c_str());
			printf("if User[CurrSock].Save ===> ");	
						if (User[CurrSock].Save)
						{	
							//save data
							//save_profile(CurrSock);
							printf("User was marked to be saved.\n");
							printf("clearing persisit ticker and saving all profiles...\n");
							persistTicker = Clock();
							saveAllProfiles();
							
							if (User[CurrSock].Nick.compare("Paladin") != 0)
							{
								printf("telling everyone...\n");
								//tell everyone
								std::string Message = "0";
								Message += EXIT;
								Message += CurrSock;
								Message[0] = Message.length();
								
								// Cycle all sockets
								for( int i = 0; i < MAX_CLIENTS; i++ )
								{
									// If this socket is taken
									if ( User[i].Ident && i != CurrSock)
									{
										// Send data
										User[i].SendPacket(Message);
									}
								}
							}
						}

			printf("resetting trade statuses...\n");
						
						//were they trading? notify the other player.
						if (User[CurrSock].tradeStatus > 0 && User[CurrSock].tradePlayer >= 0)
						{
						   int playerB = User[CurrSock].tradePlayer;
						   std::string cancelTradePacket = "0";
						   cancelTradePacket += TRADE;
						   cancelTradePacket += CANCEL;
						   cancelTradePacket[0] = cancelTradePacket.length();
						   User[playerB].SendPacket(cancelTradePacket);
						   
						   //set playerB to not trading at all
						   User[playerB].tradeStatus = -1;
						   User[playerB].tradePlayer = 0;
						}
					   

			quitParty(CurrSock);

			//clear the player
		//	if (User[CurrSock])
		//		delete User[CurrSock];
		//	User[CurrSock] = new SKO_Player();	
				printf("Clearing User #%i named %s\n", CurrSock, User[CurrSock].Nick.c_str());
			User[CurrSock].Clear();
			User[CurrSock] = SKO_Player();
			
			//but save them
			//User[CurrSock].init();

			printf("[DISCONNECT] User[%i].Clear();\n", CurrSock);

			}//end error else
			
		}//sock status
		
		// Sleep a bit
		Sleep(1);
    }//end while
}


bool blocked(int current_map, float box1_x1, float box1_y1, float box1_x2, float box1_y2, bool npc)
{
	for (int r = 0; r < map[current_map].number_of_rects; r++)
	{
		float box2_x1 = map[current_map].collision_rect[r].x;
		float box2_y1 = map[current_map].collision_rect[r].y;
		float box2_x2 = map[current_map].collision_rect[r].x + map[current_map].collision_rect[r].w;
		float box2_y2 = map[current_map].collision_rect[r].y + map[current_map].collision_rect[r].h;


		if (box1_x2 > box2_x1 && box1_x1 < box2_x2 && box1_y2 > box2_y1 && box1_y1 < box2_y2)
		return true;
	}

	for (int r = 0; r < map[current_map].num_targets; r++)
	{
		if (!map[current_map].Target[r].active)
		{
			if (!npc)
				continue;
		}
		
		float box2_x1 = map[current_map].Target[r].x;
		float box2_y1 = map[current_map].Target[r].y;
		float box2_x2 = map[current_map].Target[r].x + map[current_map].Target[r].w;
		float box2_y2 = map[current_map].Target[r].y + map[current_map].Target[r].h;

		if (box1_x2 > box2_x1 && box1_x1 < box2_x2 && box1_y2 > box2_y1 && box1_y1 < box2_y2)
			return true;
	}

	return false;
}


void *TargetLoop(void *arg)
{
	char *p;
	long int timea;
	long int current_map = (intptr_t)arg;

	while (!SERVER_QUIT)
	{
		timea = Clock();
		//npc
		for (int i = 0; i < map[current_map].num_targets; i++)
		{
			//respawn this box if it's been dead a while
			if (!map[current_map].Target[i].active 
				&& Clock() - map[current_map].Target[i].respawn_ticker > 5000) 
			{
				map[current_map].Target[i].active = true;
				spawnTarget(i, current_map);	
			}
		}
		Sleep(1000);
	}
}
	

void *EnemyLoop(void *arg)
{
     char *p;
     long int timea;  
   long int current_map = (intptr_t)arg;
     
   
     while (!SERVER_QUIT)
     {
         timea = Clock();
         
        //enemy
         for (int i = 0; i < map[current_map].num_enemies; i++)
         {
		 
		int next_action = 0;		


	     	//check for dibs on the kill, reset after 5 seconds
		if (Clock() - map[current_map].Enemy[i]->dibsTicker >= 5000)
        { 
			map[current_map].Enemy[i]->dibsPlayer = -1;
			map[current_map].Enemy[i]->dibsTicker = 0;
		}
		 
                std::string Packet = "0";
           

	        if (map[current_map].Enemy[i]->y > map[current_map].death_pit){
			KillEnemy(current_map, i);
			printf("Fix enemy from falling into death pit: map %i Enemy %i spawns at (%i,%i)\n", current_map, i, map[current_map].Enemy[i]->sx, map[current_map].Enemy[i]->sy);
		}

		 
             //check for spawn
             if (map[current_map].Enemy[i]->dead)
             {
                if (Clock() - map[current_map].Enemy[i]->respawn_ticker >= 7000)
                {

		   map[current_map].Enemy[i]->Respawn();
                   std::string Packet = "0";
                   Packet += ENEMY_MOVE_STOP;

		   Packet += i;
		   Packet += current_map;
                   //break up the int as 4 bytes
                 
                   p=(char*)&map[current_map].Enemy[i]->x;
                   Packet += p[0];
                   Packet += p[1];
                   Packet += p[2];
                   Packet += p[3];
                   p=(char*)&map[current_map].Enemy[i]->y;
                   Packet += p[0];
                   Packet += p[1];
                   Packet += p[2];
                   Packet += p[3];
                  
                   Packet[0] = Packet.length();
   

		  //enemy health bars
                  int hp = (unsigned char)((float)map[current_map].Enemy[i]->hp / map[current_map].Enemy[i]->hp_max*hpBar);
                  //packet
                  std::string hpPacket = "0";
                  hpPacket += NPC_HP;
                  hpPacket += i;
		  hpPacket += current_map;
		  hpPacket += hp;
                  hpPacket[0] = hpPacket.length();

 
                   //tell everyone they died
                   for (int ii = 0; ii < MAX_CLIENTS; ii++)
                   {
                       if (User[ii].Ident)
                       {
                           User[ii].SendPacket(Packet);
                           User[ii].SendPacket(hpPacket);
		       }
                   }
                   map[current_map].Enemy[i]->dead = false;
               }
             }//end if dead
             else 
             {
			 
                  //find out if bandit confronts someone
		  bool confront = false;
			//decide what to do
                       for (int u = 0; u < MAX_CLIENTS; u++)
                       {
                           //find a target baby!
                           // rules: first player to be an acceptable target
                            if (User[u].Ident && User[u].current_map == current_map)
                            {
                               float x_dist, y_dist;

			       //horizontal
			       if (map[current_map].Enemy[i]->facing_right)
                               	  x_dist = (User[u].x + 23) - (map[current_map].Enemy[i]->x + 41);
                               else
				  x_dist = (map[current_map].Enemy[i]->x + 23) - (User[u].x + 41);

			       //vertical
                   y_dist = User[u].y - map[current_map].Enemy[i]->y;
		
                                
                   // confront
				   if (x_dist >= -10  && x_dist <= 10  && y_dist >= -60 && y_dist <= 60) {
				       confront = true;

				   //timer override if first time
				   if (map[current_map].Enemy[i]->x_speed != 0)
					map[current_map].Enemy[i]->AI_period = 0; 
				}
				
                            }//end for user Ident only
			    
			    if (confront) {
				next_action = ENEMY_MOVE_STOP;
				break;
			    }
					
			} //end for user: u

		  //not dead, do some actions.
		  if((Clock() - map[current_map].Enemy[i]->AI_ticker) > map[current_map].Enemy[i]->AI_period )
                  {
                       //only move if you shouldnt defend yourself, dumb enemy.
		       if (!confront)
		       {
	                       //decide what to do
		               for (int u = 0; u < MAX_CLIENTS; u++)
			       {
	                           //find a target baby!
				   // rules: first player to be an acceptable target
				    if (User[u].Ident && User[u].current_map == current_map)
	                            {
				       float x_dist, y_dist;
	                               if (map[current_map].Enemy[i]->facing_right)
	                                  x_dist = User[u].x - map[current_map].Enemy[i]->x;
	                               else
	                                  x_dist = map[current_map].Enemy[i]->x - User[u].x;
	                               //vertical
	                               y_dist = User[u].y - map[current_map].Enemy[i]->y;
					
	                               bool persue = false;
				       /* chase */
				       if (y_dist <= 60 && y_dist >= -60 && x_dist > -200 && x_dist < 200)
				       {
						persue = true;
						//push walk into right direction _OR_ continue
				                if (User[u].x > map[current_map].Enemy[i]->x)
						{
							//chase
							if (map[current_map].Enemy[i]->hp > (map[current_map].Enemy[i]->hp_max/2))
							{
								next_action = ENEMY_MOVE_RIGHT;
							}
							//flee
							else 
							{
								if (rand() % 3)
                                                                	next_action = ENEMY_MOVE_LEFT;
								else if (rand() % 2)
									next_action = ENEMY_MOVE_STOP;
								else
									next_action = ENEMY_MOVE_RIGHT;
							}
						}
						else// if (User[u].x < map[current_map].Enemy[i]->x)				
						{
							//chase
							if (map[current_map].Enemy[i]->hp > (map[current_map].Enemy[i]->hp_max/2))
							{        
								next_action = ENEMY_MOVE_LEFT;
							}			
							else
							{
								if (rand() % 3)
									next_action = ENEMY_MOVE_RIGHT;
								else if (rand() % 2)
									next_action = ENEMY_MOVE_STOP;
								else
									next_action = ENEMY_MOVE_LEFT;
							}
						}
						
						u = MAX_CLIENTS;
						break;
					
        	        }//end same plane actions 
				       
					//quit running or chasing me!
				       if (!persue)
				       {
						// 1:15 chance of an action
					  int random_action = rand() % 15;
						// UP TO quarter second extra delay
					  int random_delay = 500 + rand() % 250;
	
					  switch (random_action)
					  {
						case 0:
							next_action = ENEMY_MOVE_LEFT;
							map[current_map].Enemy[i]->AI_period = random_delay;
						break;
						
						case 1:
							next_action = ENEMY_MOVE_RIGHT;
							map[current_map].Enemy[i]->AI_period = random_delay;
						break;

					  	default:
 				          		next_action = ENEMY_MOVE_STOP;
							map[current_map].Enemy[i]->AI_period = random_delay;
						break;
				       	  }
					}

			         }//end u ident
		            }//end for User u	
                     }//end !confront
                     else // if confront is true then:
                     {
			//if you've stopped then attack
			if (map[current_map].Enemy[i]->x_speed == 0)
			{
				next_action = ENEMY_ATTACK;
				//half a second plus UP TO another half second
				int attack_pause = 200 + rand() % 800; 
				map[current_map].Enemy[i]->AI_period = attack_pause;
            }
			//else if you're still moving, stop!
			else
			{
				next_action = ENEMY_MOVE_STOP;
			}
            }
		 
                    bool redundant = false; 
                    
		    switch (next_action)
		    {
			case ENEMY_MOVE_STOP:
				if (map[current_map].Enemy[i]->x_speed == 0)
					redundant = true;
				map[current_map].Enemy[i]->x_speed = 0;
			break;

			case ENEMY_MOVE_LEFT:
                                if (map[current_map].Enemy[i]->x_speed < 0)
                                        redundant = true;
                                map[current_map].Enemy[i]->x_speed = -2;
				map[current_map].Enemy[i]->facing_right = false;
                        break;


			case ENEMY_MOVE_RIGHT:
                                if (map[current_map].Enemy[i]->x_speed > 0)
                                        redundant = true;
                                map[current_map].Enemy[i]->x_speed = 2;
				map[current_map].Enemy[i]->facing_right = true;
                        break;
	
		        case ENEMY_ATTACK:
				EnemyAttack(i, current_map);	
			break;

	
			default:
				redundant = true;
			break;


                    }
          
		
		    //dont spam packets bitch
		    if (!redundant)
		    {
		       Packet += next_action;
                       Packet += i;
                       Packet += current_map;
 
                       //break up the int as 4 bytes
                       p=(char*)&map[current_map].Enemy[i]->x;
                       Packet += p[0];
                       Packet += p[1];
                       Packet += p[2];
                       Packet += p[3];
                       p=(char*)&map[current_map].Enemy[i]->y;
                       Packet += p[0];
                       Packet += p[1];
                       Packet += p[2];
                       Packet += p[3];
  
                        
                       Packet[0] = Packet.length();
                       
                      // printf("enemy packet length is: %i\n", Packet.length());                       
                      // prints 11 
                       
                       
                       
                       /* dummy packet */
                       //Packet = "0";
                       //Packet += CHAT;
                       //Packet += "BANDIT PACKET TEST WOOOOOOOOOOOOOOOOOOOOOOOO";
                       //Packet[0] = Packet.length();
                       
                       for (int u = 0; u < MAX_CLIENTS; u++)
                       {
                           if (User[u].Ident)
                             {    
                              //tell everyone the enemy moved!
                              //ntf("\tSEND BANDIT PACKET! Time: [%i]\n", Clock());
                              User[u].SendPacket(Packet);
                           }
                       }
			//if it wasn't redundant, reset the ticker
			map[current_map].Enemy[i]->AI_ticker = Clock();

   
		   }//end no spam
			
		
                }//end enemy AI
                 
             }//end not dead 
         
	  //amoothly operate by sleeping at least a tick before going to the next enemy
	  Sleep(2);

	  } // end enemy for loop
           
         //npc
         for (int i = 0; i < map[current_map].num_npcs; i++)
         {
		 
		int next_action = 0;		


           		 
                std::string Packet = "0"; 
           
             {
			 
                  //not dead, do some actions.
		  if((Clock() - map[current_map].NPC[i]->AI_ticker) > map[current_map].NPC[i]->AI_period )
                  {
						// UP TO quarter second extra delay
					  int random_delay = 2500 + rand() % 169;
					
					
					  int random_action = rand() % 6; //2:1 ratio stopping to moving. To make him not seem like a meth tweaker 					

					  switch (random_action)
					  {
						case 0:
							next_action = NPC_MOVE_LEFT;
							map[current_map].NPC[i]->AI_period = random_delay;
						break;
						
						case 1:
							next_action = NPC_MOVE_RIGHT;
							map[current_map].NPC[i]->AI_period = random_delay;
						break;

					  	default:
 				          		next_action = NPC_MOVE_STOP;
							map[current_map].NPC[i]->AI_period = random_delay;
						break;
				       	  }
		 
                    bool redundant = false; 
                    
		    switch (next_action)
		    {
			case NPC_MOVE_STOP:
				if (map[current_map].NPC[i]->x_speed == 0)
					redundant = true;
				map[current_map].NPC[i]->x_speed = 0;
			break;

			case NPC_MOVE_LEFT:
                                if (map[current_map].NPC[i]->x_speed < 0)
                                        redundant = true;
                                map[current_map].NPC[i]->x_speed = -2;
				map[current_map].NPC[i]->facing_right = false;
                        break;


			case NPC_MOVE_RIGHT:
                                if (map[current_map].NPC[i]->x_speed > 0)
                                        redundant = true;
                                map[current_map].NPC[i]->x_speed = 2;
				map[current_map].NPC[i]->facing_right = true;
                        break;
	
	
			default:
				redundant = true;
			break;


                    }
          
		
		    //dont spam packets bitch
		    if (!redundant)
		    {
		       Packet += next_action;
                       Packet += i;
                       Packet += current_map;
 
                       //break up the int as 4 bytes
                       p=(char*)&map[current_map].NPC[i]->x;
                       Packet += p[0];
                       Packet += p[1];
                       Packet += p[2];
                       Packet += p[3];
                       p=(char*)&map[current_map].NPC[i]->y;
                       Packet += p[0];
                       Packet += p[1];
                       Packet += p[2];
                       Packet += p[3];
  
                        
                       Packet[0] = Packet.length();
                       
                      // printf("enemy packet length is: %i\n", Packet.length());                       
                      // prints 11 
                       
                       
                       
                       /* dummy packet */
                       //Packet = "0";
                       //Packet += CHAT;
                       //Packet += "BANDIT PACKET TEST WOOOOOOOOOOOOOOOOOOOOOOOO";
                       //Packet[0] = Packet.length();
                       
                       for (int u = 0; u < MAX_CLIENTS; u++)
                       {
                           if (User[u].Ident)
                             {    
                              //tell everyone the enemy moved!
                              //ntf("\tSEND BANDIT PACKET! Time: [%i]\n", Clock());
                              User[u].SendPacket(Packet);
                           }
                       }
			//if it wasn't redundant, reset the ticker
			map[current_map].NPC[i]->AI_ticker = Clock();

   
		   }//end no spam
			
		
                }//end enemy AI
                 
             }//end not dead 
         
	  //amoothly operate by sleeping at least a tick before going to the next enemy
	  Sleep(2);

	  } // end enemy for loop
           
           //if (Clock() - timea > 20){
           //   printf("NPC loop (%i) took [%i] milliseconds.\n\n", current_map, (Clock() - timea)); 
           //}
                    
           //you don't need to check more than x times per second...
           Sleep(10);
     }
}






void *Physics(void *arg)
{

    //initialize the timestep
	KE_Timestep *timestep = new KE_Timestep(60);


    long int current_map = (intptr_t)arg;
     
    int a  = 0; 
    char *p;
    bool block_y;
    bool block_x;
    int ID = 0;
    float box1_x1,box1_y1,box1_x2,box1_y2;
    unsigned int amt = 0;
    char b1, b2, b3, b4;
    
	unsigned int updates = 0;
	unsigned int lastTime = Clock();

    while (!SERVER_QUIT)
    {
        //update the timestep
        timestep->Update();
             
        while (timestep->Check())
        {      
			//enemies
			for (int i = 0; i < map[current_map].num_enemies; i++)
			{

				//enemy physics
				if (map[current_map].Enemy[i]->y_speed < 10)
					map[current_map].Enemy[i]->y_speed += GRAVITY;
    
				//verical collision detection
                block_y = blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y+map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y+map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y2, true);
    
				map[current_map].Enemy[i]->ground = true;
    
    
                //vertical movement
                if (!block_y)
                {//not blocked, fall
    
                   //animation
                   map[current_map].Enemy[i]->ground = false;
    
                   map[current_map].Enemy[i]->y += map[current_map].Enemy[i]->y_speed;
    
                }
                else
                {  //blocked, stop
                    if (map[current_map].Enemy[i]->y_speed > 0)
                    {
                        map[current_map].Enemy[i]->y_speed = 1;
    
						//while or if TODO
						for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y+map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y+map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25 + map[current_map].Enemy[i]->y2, true)) ; loopVar++)
                              map[current_map].Enemy[i]->y += map[current_map].Enemy[i]->y_speed;
    
                        map[current_map].Enemy[i]->y = (int) (map[current_map].Enemy[i]->y + 0.5);

                    }
                    if (map[current_map].Enemy[i]->y_speed < 0)
                    {
                        map[current_map].Enemy[i]->y_speed = -1;
    
						//while or if TODO
                        for (int loopVar = 0; loopVar < HIT_LOOP &&  (!blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y+map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y+map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25 + map[current_map].Enemy[i]->y2, true)) ; loopVar++)
                           map[current_map].Enemy[i]->y += map[current_map].Enemy[i]->y_speed;
                    }
    
                    map[current_map].Enemy[i]->y_speed = 0; 
                 }
    
                 //horizontal collision detection
                 block_x = blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x_speed + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y1, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x_speed + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y2, true);
    
                 //horizontal movement
                 if (!block_x)
                 {//not blocked, walk
                    map[current_map].Enemy[i]->x += map[current_map].Enemy[i]->x_speed;
                 }
    
                 if ((map[current_map].Enemy[i]->x_speed == 0 && map[current_map].Enemy[i]->y_speed == 0) || !map[current_map].Enemy[i]->attacking)
                    map[current_map].Enemy[i]->current_frame = 0;
			}//end enemies/npc


			//npcs
			for (int i = 0; i < map[current_map].num_npcs; i++)
			{
				//enemy physics
				if (map[current_map].NPC[i]->y_speed < 10)
					map[current_map].NPC[i]->y_speed += GRAVITY;
    
				//verical collision detection
                block_y = blocked(current_map, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x1, map[current_map].NPC[i]->y+map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x2, map[current_map].NPC[i]->y+map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y2, true);
    
                map[current_map].NPC[i]->ground = true;
    
    
                //vertical movement
                if (!block_y)
                {//not blocked, fall
    
                   //animation
                   map[current_map].NPC[i]->ground = false;
    
                   map[current_map].NPC[i]->y += map[current_map].NPC[i]->y_speed;
    
                }
                else
                {   
					//blocked, stop
                    if (map[current_map].NPC[i]->y_speed > 0)
                    {
                       map[current_map].NPC[i]->y_speed = 1;
    
                        //while or if TODO
						for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x1, map[current_map].NPC[i]->y+map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x2, map[current_map].NPC[i]->y+map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25 + map[current_map].NPC[i]->y2, true)) ; loopVar++)
                            map[current_map].NPC[i]->y += map[current_map].NPC[i]->y_speed;
    
                        map[current_map].NPC[i]->y = (int) (map[current_map].NPC[i]->y + 0.5);
                    }
                    if (map[current_map].NPC[i]->y_speed < 0)
                    {
                        map[current_map].NPC[i]->y_speed = -1;
    
						//while or if TODO
                        for (int loopVar = 0; loopVar < HIT_LOOP &&  (!blocked(current_map, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x1, map[current_map].NPC[i]->y+map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x2, map[current_map].NPC[i]->y+map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25 + map[current_map].NPC[i]->y2, true)) ; loopVar++)
                           map[current_map].NPC[i]->y += map[current_map].NPC[i]->y_speed;
                    }
    
                    map[current_map].NPC[i]->y_speed = 0;
                 }
    
                 //horizontal collision detection
                 block_x = blocked(current_map, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x_speed + map[current_map].NPC[i]->x1, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y1, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x_speed + map[current_map].NPC[i]->x2, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y2, true);
    
                 //horizontal movement
                 if (!block_x)
                 {
					//not blocked, walk
                    map[current_map].NPC[i]->x += map[current_map].NPC[i]->x_speed;
                 }
			}//end npc

			//players
			for (int i = 0; i < MAX_CLIENTS; i++)
			if (User[i].current_map == current_map && User[i].Ident)
			{
					//check for portal collisions
					for (int portal = 0; portal < map[current_map].num_portals;  portal++)
					{
          			
						//player box
						float box1_x1 = User[i].x + 23;
						float box1_y1 = User[i].y + 13;
						float box1_x2 = User[i].x + 40;
						float box1_y2 = User[i].y + 64;
		
										
						//portal box
						float box2_x1 = map[current_map].Portal[portal].x;
						float box2_y1 = map[current_map].Portal[portal].y;
						float box2_x2 = map[current_map].Portal[portal].x + map[current_map].Portal[portal].w;
						float box2_y2 = map[current_map].Portal[portal].y +map[current_map].Portal[portal].h;


						if (box1_x2 > box2_x1 && box1_x1 < box2_x2 && box1_y2 > box2_y1 && box1_y1 < box2_y2)
             			{
							if (User[i].level >= map[current_map].Portal[portal].level_required)
							{
								Warp(i, map[current_map].Portal[portal]);
								//TODO: how do players stay in a party on different maps? 
								//Maybe disconnect after a minute.
								quitParty(i);		
							}
						}//end portal collison
					}//end for portal


					//stop attacking
					if (Clock() - User[i].attack_ticker > attack_speed)
					{
						User[i].attacking = false;

						//if you made an action during an attack, it is saved utnil now. Execute
						if (User[i].que_action != 0)
						{ 
							printf("Glitched user is %s :O\n", User[i].Nick.c_str());	
							std::string Packet = "0";
							float numx = User[i].x;
							float numy = User[i].y;
							switch(User[i].que_action) 
							{
								case MOVE_LEFT:
									Packet += MOVE_LEFT;
										Left(i, numx, numy);
									printf("\e[0;32mCorrection! que action being sent NOW: MOVE_LEFT\e[m\n");	 
								break;
								
								case MOVE_RIGHT:
									Packet += MOVE_RIGHT;
									Right(i, numx, numy);
														printf("\e[0;32mCorrection! que action being sent NOW: MOVE_RIGHT\e[m\n");
								break;
								
								case MOVE_JUMP:
									Packet += MOVE_JUMP;
														Jump(i, numx, numy);
									printf("\e[0;32mCorrection! que action being sent NOW: JUMP\e[m\n");
								break;
								
								case ATTACK:
									Packet += ATTACK;
									printf("\e[0;32mCorrection! que action being sent NOW: ATTACK\e[m\n");
								
									//do attack actions
									Attack(i, numx, numy);
								break;
						
								case MOVE_STOP:
										Packet += MOVE_STOP;
										printf("\e[0;32mCorrection! que action being sent NOW: MOVE_STOP\e[m\n");

										//do attack actions
										Stop(i, numx, numy);
								break;
					
								default:
									printf("\e[0;32mCorrection! que action glitch: %i\e[m\n", User[i].que_action);
									
								break;
							}//end switch

							//add coords
							char *p;
							char b1, b2, b3, b4;
                        
							//ID
							Packet += i;
                        
							//break up X into 4 bytes
							p=(char*)&numx;
							b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
							Packet += b1;
							Packet += b2;
							Packet += b3;
							Packet += b4;

							//break up Y into 4 bytes
							p=(char*)&numy;
							b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
							Packet += b1;
							Packet += b2;
							Packet += b3;
							Packet += b4;

							//tell everyone
							Packet[0] = Packet.length();
							for (int qu = 0; qu < MAX_CLIENTS; qu++) if (User[qu].Ident)
								User[qu].SendPacket(Packet);


							//reset the cue
							User[i].que_action = 0;

						} // end que actions
					}//end attack ticker
                 
					//HP regen
					int regen = User[i].regen;
					int weap = User[i].equip[0];
					int hat = User[i].equip[1];
		 
					if (weap > 0)
						regen += Item[weap].hp;

					if (hat > 0)
						regen += Item[hat].hp;

					if (regen > 0)
					{   
						if (Clock() - User[i].regen_ticker >= 1200)
						{ 
							//regen hp
							if (User[i].hp < User[i].max_hp)
							{         
								User[i].hp += regen;

								if (User[i].hp > User[i].max_hp)
								{
								//cap the hp to their max
								User[i].hp = User[i].max_hp;
								}

								//party hp notification
								std::string hpPacket = "0";
								hpPacket += BUDDY_HP;
								hpPacket += i;
								hpPacket += (int)((User[i].hp / (float)User[i].max_hp) * 80);
								hpPacket[0] = hpPacket.length();

								for (int pl = 0; pl < MAX_CLIENTS; pl ++)
								{
									if (pl != i && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[i].party)
										User[pl].SendPacket(hpPacket);
								}
							}
						  
							std::string Packet = "0";
							Packet += STAT_HP;
							Packet += User[i].hp;
							Packet[0] = Packet.length();
							User[i].SendPacket(Packet);
							User[i].regen_ticker = Clock();
						}
					}
                 
                 
					if (User[i].y_speed < 10)
					{
						User[i].y_speed += GRAVITY;
					}
                        
					//verical collision detection
					block_y = blocked(current_map, User[i].x + 25, User[i].y+User[i].y_speed + 13 + 0.25, User[i].x + 38, User[i].y+User[i].y_speed + 64, false);
    
                 
					//vertical movement
					if (!block_y)
					{//not blocked, fall
                 
						//animation
						User[i].ground = false;
						  
						User[i].y += User[i].y_speed;
						
						//bottomless pit
						if (User[i].y > map[User[i].current_map].death_pit)
						{
							printf("user died in death pit on map %i because %i > %i\n", (int)current_map, (int)User[i].y, (int)map[current_map].death_pit);
							Respawn(User[i].current_map, i);
						}
					}
					else
					{   
						User[i].ground = true;
                    
						//blocked, stop
						if (User[i].y_speed > 0)
						{
							User[i].y_speed = 1;
    
							//while or if TODO
							for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, User[i].x + 25, User[i].y+User[i].y_speed + 13 + 0.25, User[i].x + 38, User[i].y+User[i].y_speed + 64 + 0.25, false)) ; loopVar++)
								User[i].y += User[i].y_speed;  
                           
							User[i].y = (int) (User[i].y + 0.5);
						}
						if (User[i].y_speed < 0)
						{
							User[i].y_speed = -1;
    
							//while or if TODO
							for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, User[i].x + 25, User[i].y+User[i].y_speed + 13 + 0.25, User[i].x + 38, User[i].y+User[i].y_speed + 64 + 0.25, false)) ; loopVar++)
							User[i].y += User[i].y_speed;
						}
   
						User[i].y_speed = 0;
					}
    
					//horizontal collision detection
					block_x = blocked(current_map, User[i].x + User[i].x_speed + 23, User[i].y + 13, User[i].x + User[i].x_speed + 40, User[i].y + 64, false);
		
					//horizontal movement
					if (!block_x)
					{//not blocked, walk
						User[i].x += User[i].x_speed;     
					} 
				}
             
			//item objects
			for (int i = 0; i < 256; i++)
			if (map[current_map].ItemObj[i].status)
			{
					//ninja loot
					if (Clock() - map[current_map].ItemObj[i].ownerTicker > 10000) 
					{
						//reset the item's owner
						map[current_map].ItemObj[i].owner = -1;
						map[current_map].ItemObj[i].ownerTicker = 0;
					}
		

					ID = map[current_map].ItemObj[i].itemID;
                 
    
					// printf("(%.2f,%.2f)\n", map[current_map].ItemObj[i].x, map[current_map].ItemObj[i].y);
					//horizontal collision detection
					block_x = blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y , map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y + Item[map[current_map].ItemObj[i].itemID].w, false);
    
					if (map[current_map].ItemObj[i].y_speed < 10)
						map[current_map].ItemObj[i].y_speed += GRAVITY;
                        
					//vertical collision detection
					block_y = blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y+map[current_map].ItemObj[i].y_speed + 0.2, map[current_map].ItemObj[i].x + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y+map[current_map].ItemObj[i].y_speed + Item[map[current_map].ItemObj[i].itemID].h,false);
    
                 
					//vertical movement
					if (!block_y)  
					{						
						map[current_map].ItemObj[i].y += map[current_map].ItemObj[i].y_speed;
					}
					else
					{
						map[current_map].ItemObj[i].y_speed = 0;
						map[current_map].ItemObj[i].x_speed *= 0.5;
					}
                 
    
					//horizontal movement
					if (!block_x)
					{
						//not blocked, move
						map[current_map].ItemObj[i].x += map[current_map].ItemObj[i].x_speed;
					} 
					else 
					{
						map[current_map].ItemObj[i].x_speed = 0;
					}
                 
					//item             
					box1_x1 = map[current_map].ItemObj[i].x;
					box1_y1 = map[current_map].ItemObj[i].y;
					box1_x2 = map[current_map].ItemObj[i].x+Item[map[current_map].ItemObj[i].itemID].w;
					box1_y2 = map[current_map].ItemObj[i].y+Item[map[current_map].ItemObj[i].itemID].h;

					
					//Despawn items that fell off the edge 
					if (map[current_map].ItemObj[i].y > map[current_map].death_pit)
					{
					std::string Packet = "0";
					Packet += DESPAWN_ITEM;
					Packet += i;
					Packet += current_map;
					Packet[0] = Packet.length();

					//remove from map
					for (int a = 0; a < MAX_CLIENTS; a++)
					{
						if (User[a].Ident)
						{
						   // printf("i is: %i and itm is: %i and\nPacket is:\n", i, (int)itm);
						   // for (int aa = 0; aa < Packet.length(); aa++)
						   //     printf("[%i]", Packet[aa]);

							User[a].SendPacket(Packet);
						}
					}
					// printf("Despawn item\n");

					map[current_map].ItemObj[i].remove();
				}//end despawn items that fell off the edge of the map
	 
					//check for players intersecting
					for (int c = 0; c < MAX_CLIENTS; c++)
					{
						//ninja loot
						bool isMine = false;

						if (map[current_map].ItemObj[i].owner == -1 || map[current_map].ItemObj[i].owner == c)
							isMine = true;
					
					//player
					float box2_x1 = User[c].x + 25;
					float box2_y1 = User[c].y;
					float box2_x2 = User[c].x + 38;
					float box2_y2 = User[c].y + 64;

					if (box1_x2 > box2_x1 && box1_x1 < box2_x2 && box1_y2 > box2_y1 && box1_y1 < box2_y2)
					{
						printf("You're touching an item. IsMine is %i Ident is %i and inventory index is %i\n", isMine, User[c].Ident, User[c].inventory_index);
						if (User[c].Ident && isMine)
						{   
							//if user doesnt have it, can only pick up if they dont have a full inventory
							if (User[c].inventory[ID] == 0 && User[c].inventory_index > 23)
							{
								printf("User %s inventory is full and cant pick up item %i!", User[c].Nick.c_str(), ID);
							}
							else
							{
								unsigned char itm = i;
								std::string Packet = "0";
								Packet += DESPAWN_ITEM;
								Packet += itm;
								Packet += current_map;
								Packet[0] = Packet.length();

								//remove from map
								for (int a = 0; a < MAX_CLIENTS; a++)
								{
									if (User[a].Ident)
									{
										User[a].SendPacket(Packet);
									}
								}
								
								//put in players inventory
								if (User[c].inventory[ID] == 0)
									User[c].inventory_index++;

								User[c].inventory[ID] += map[current_map].ItemObj[i].amount;

								map[current_map].ItemObj[i].remove();

								//put in players inventory    
								Packet = "0";
								Packet += POCKET_ITEM;
								Packet += ID;

								//break up the int as 4 bytes
								amt = User[c].inventory[ID];
								printf("\n\nPutting this item in player's inventory!\nAmount: %i\nWhich item object? [0-255]: %i\nWhich item? [0-1]: %i\n", amt, i, ID);
								printf("Player has [%i] ITEM_GOLD and [%i] ITEM_MYSTERY_BOX\n\n", User[c].inventory[0], User[c].inventory[1]);
								p=(char*)&amt;
								b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
								Packet += b1;
								Packet += b2;
								Packet += b3;
								Packet += b4;

								Packet[0] = Packet.length();
								User[c].SendPacket(Packet);
                            }//end else not inventory full (you can pick up)
                        }
                    }
                }
            }
        } //end timestep
        Sleep(1);
    }//end while true
}


void despawnTarget(int target, int current_map)
{
	std::string packet = "0";
	packet += DESPAWN_TARGET;
	packet += target;
	packet += current_map;
	packet[0] = packet.length();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
			User[i].SendPacket(packet);
	}
}

void spawnTarget(int target, int current_map)
{
	std::string packet = "0";
	packet += SPAWN_TARGET;
	packet += target;
	packet += current_map;
	packet[0] = packet.length();
	
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
			User[i].SendPacket(packet);
	}	
}


/* perform attack actions */
void Attack(int CurrSock, float numx, float numy)
{
	//set the current map
	int current_map = User[CurrSock].current_map;
    
	//Que next action 
	if (User[CurrSock].attacking)
	{
		printf("%s tried to attack while attacking...\7\7\n", User[CurrSock].Nick.c_str());

		//if not in que, put it there.
	    if (User[CurrSock].que_action == 0)
	    {
			User[CurrSock].que_action = ATTACK;
		}
	   
	   return;
	}

	//Cannot attack while mid-air
	if (!User[CurrSock].ground)
	{
	   printf("%s tried to attack while jumping...\7\7\r\n", User[CurrSock].Nick.c_str());
	   return;
	}
	
	printf("%s is attacking!\r\n", User[CurrSock].Nick.c_str());
	User[CurrSock].attacking = true;
	User[CurrSock].attack_ticker = Clock();
	User[CurrSock].x_speed = 0;

	bool good = false;

	if 
	(((
		 (numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance) 
		 ||
		 (numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)
	 ) 
	 &&
	 (
		 (numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance) 
		 ||
		 (numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance)
	)))
	{
		//update server variables
		User[CurrSock].x = numx;
		User[CurrSock].y = numy;
		good = true;
	}
	else
	{
		printf("ATTACK FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[CurrSock].x, User[CurrSock].y);     

		//correction packet
		numx = (int)User[CurrSock].x;
		numy = (int)User[CurrSock].y;
	}
	
	
	printf("%s attack good? %i\r\n", User[CurrSock].Nick.c_str(), good);

	std::string Packet = "0";


	char *p;
	char b1, b2, b3, b4;
	//build correction packet
	Packet = "0";
	Packet += ATTACK;
	Packet += CurrSock;
	//ID

	//break up X into 4 bytes
	p=(char*)&numx;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p=(char*)&numy;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;  
	Packet[0] = Packet.length();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident && (i != CurrSock || !good) && User[i].current_map == User[CurrSock].current_map)
			User[i].SendPacket(Packet);
	}

	Packet = "0";

	//loop all players		
	printf("Looping all players.\r\n");
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!User[i].Ident)
			continue;

		bool inParty = false;
		if (User[i].partyStatus == PARTY && User[i].party == User[CurrSock].party)
			inParty = true;

		if (i != CurrSock && !inParty  && User[i].current_map == User[CurrSock].current_map)
		{
			//check for collision
			//horizontal
			float x_dist, y_dist;
			if (User[CurrSock].facing_right)
				x_dist = User[i].x - User[CurrSock].x;
			else
				x_dist = User[CurrSock].x - User[i].x;
			  
			//vertical   
			y_dist = User[i].y - User[CurrSock].y;
		   
			//how far away you can hit, plus weapon
			int max_dist = 30;
			int weap = User[CurrSock].equip[0];
			if (weap != 0){
			   max_dist += Item[weap].reach;
			}
		   
			//attack events
			if (x_dist > 0 && x_dist < max_dist && y_dist > -60 && y_dist < 30)
			{
				int dam = User[CurrSock].strength - User[i].defence; 
				int hat = User[CurrSock].equip[1];

				//weapon
				if (weap != 0)
					dam += Item[weap].str;
				 
				if (hat > 0)
					dam += Item[hat].str;					 

				//other user
				weap = User[i].equip[0];
				hat = User[i].equip[1];

				if (weap != 0)
					dam -= Item[weap].def;

				if (hat > 0)
					dam -= Item[hat].def;
				
				//don't give free hp
				if (dam < 0)
				{ 
					dam = 0;
				}
				 
				std::string Packet;
				if (User[i].hp <= dam)
				{
					printf("Respawn, x:%.2f y:%.2f\n", User[i].x, User[i].y);
					Respawn(User[i].current_map, i);
					printf("Respawned, x:%.2f y:%.2f\n", User[i].x, User[i].y);
				
				}//end died
				else
				{
					User[i].hp -= dam;
					Packet = "0";
					Packet += STAT_HP;
					Packet += User[i].hp;
					Packet[0] = Packet.length();
					User[i].SendPacket(Packet);
					
					Packet = "0";
					Packet += TARGET_HIT;
					Packet += 1; // player
					Packet += i;
					Packet[0] = Packet.length();
					
					//party hp notification					     
					std::string hpPacket = "0";
					hpPacket += BUDDY_HP;
					hpPacket += i;
					hpPacket += (int)((User[i].hp / (float)User[i].max_hp) * 80);
					hpPacket[0] = hpPacket.length();

					for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
					{
						if (User[i1].Ident && User[i1].current_map == User[i].current_map);
						{
							User[i1].SendPacket(Packet);
												 
							if (i != i1 && User[i1].partyStatus == PARTY && User[i1].party == User[i].party)
								User[i1].SendPacket(hpPacket);
						}
					}
				}//end lose hp
			}//end hit
		}
	}//end loop everyone

	//loop all targets 
	printf("Checking all targets\r\n");
	for (int i = 0; i < map[current_map].num_targets; i++)
	{
		if (!map[current_map].Target[i].active)
			continue;

		//horizontal
		float x_dist, y_dist;
		if (User[CurrSock].facing_right)
		   x_dist = map[current_map].Target[i].x - (User[CurrSock].x + 38);
		else
		   x_dist = (User[CurrSock].x + 25) - (map[current_map].Target[i].x + map[current_map].Target[i].w);
		//vertical   
		y_dist = map[current_map].Target[i].y - User[CurrSock].y;
	
		//how far away you can hit, plus weapon
		int max_dist = 30;
		int weap = User[CurrSock].equip[0];
		if (weap != 0)
		{
			max_dist += Item[weap].reach;
		}

		//attack events
		if (x_dist > -map[current_map].Target[i].w && x_dist < max_dist && y_dist > 0 && y_dist < 60)
		{
			//make this target disappear
			map[current_map].Target[i].respawn_ticker = Clock();
			map[current_map].Target[i].active = false;
			despawnTarget(i, current_map);

			//spawn some loot
			if (map[current_map].Target[i].loot >= 0) 
			{
			  SKO_ItemObject lootItem = map[current_map].Target[i].getLootItem();
			  lootItem.owner = CurrSock;
			  SpawnLoot(current_map, lootItem);
			}

			//give player some XP
			GiveXP(CurrSock, 1);
		} 
		else 
		{
			printf("missed target %i. x_dist: %.2f y_dist: %.2fi -map[current_map].Target[i].w: %i\n", 
				i, x_dist, y_dist, -map[current_map].Target[i].w);	
				
			for (int f = 0; f < map[current_map].num_targets; f++)
				printf("Target[%i]: x:%i y:%i w:%i h:%i\n", f, map[current_map].Target[f].x, map[current_map].Target[f].y, map[current_map].Target[f].w, map[current_map].Target[f].h);
		}
	}

	//loop all enemies
	printf("Checking all enemies: %i\r\n", map[current_map].num_enemies);
	for (int i = 0; i < map[current_map].num_enemies; i++)
	{
		bool partyBlocked = false;
		   
		//if enemy is blocked by a party dibs
		if (map[current_map].Enemy[i]->dibsPlayer >= 0)
		{
			//then assume it's blocked
			partyBlocked = true;
			printf("partyBlocked because dibsPlayer = %i\r\n", map[current_map].Enemy[i]->dibsPlayer);
			//if the player is you, it's not blocked
			if (map[current_map].Enemy[i]->dibsPlayer == CurrSock)
				partyBlocked = false;
			
			//but if you are part of the party it isn't blocked
			if (User[CurrSock].partyStatus == PARTY)
			{
				if (User[map[current_map].Enemy[i]->dibsPlayer].partyStatus == PARTY && 
				User[map[current_map].Enemy[i]->dibsPlayer].party == User[CurrSock].party)
					partyBlocked = false;
				
			}
		}
						   
		//check is they are active and not dib'sed by a party
		if (!map[current_map].Enemy[i]->dead && !partyBlocked)
		{
			printf("Checking enemy: %i on map: %i\r\n", i, current_map);
			//check for collision
			//horizontal
			float x_dist, y_dist;
			if (User[CurrSock].facing_right)
				x_dist = map[current_map].Enemy[i]->x - User[CurrSock].x;
			else
				x_dist = User[CurrSock].x - map[current_map].Enemy[i]->x;
			
			//vertical   
			y_dist = map[current_map].Enemy[i]->y - User[CurrSock].y;
			//how far away you can hit, plus weapon
			int max_dist = 30;
			int weap = User[CurrSock].equip[0];
			if (weap != 0)
			{
			   max_dist += Item[weap].reach;
			}
						   
			//attack events
			if (x_dist >= 0 && x_dist < max_dist && y_dist > -60 && y_dist < 60)
			{
				int dam = User[CurrSock].strength - map[current_map].Enemy[i]->defence; 
	
				//keep track of damage
				map[current_map].Enemy[i]->dibsDamage[CurrSock] += dam;
								
				//weapon
				if (weap != 0)
					dam += Item[weap].str;
							
				printf("%s hit an enemy! damage is: %i\r\n", User[CurrSock].Nick.c_str());			
					
					
				//don't give free hp
				if (dam < 0)
				{ 
					dam = 0;
				}
				else
				{
					//damage! dibs this mob for the current player who hit it
					map[current_map].Enemy[i]->dibsPlayer = CurrSock;
					map[current_map].Enemy[i]->dibsTicker = Clock();
								
					Packet = "0";
					Packet += TARGET_HIT;
					Packet += (char)0; // npc
					Packet += i;
					Packet[0] = Packet.length();
								
					for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
					{
						if (User[i1].Ident && User[i1].current_map == current_map)
						{
							User[i1].SendPacket(Packet);
						}
					}
				}
								
				std::string Packet;
				if (map[current_map].Enemy[i]->hp <= dam)
				{
					//keep track of damage but remove overflow
					int extraHpHurt = (dam - map[current_map].Enemy[i]->hp); 
					map[current_map].Enemy[i]->dibsDamage[CurrSock] -= extraHpHurt;
							
					//Divide Loot between a party
					if (User[CurrSock].partyStatus == PARTY)
					{
						DivideLoot(i, User[CurrSock].party);
					} 
					else//not in a party, they get all the loots!
					{
						for (int it = 0; it < map[current_map].Enemy[i]->lootAmount; it++)
							GiveLoot(i, CurrSock);
					}
									
					//kill the enemy after loot is dropped !! :P
					KillEnemy(current_map, i);
					 
					float bonus_clan_xp = 0;
					float splitXP = map[current_map].Enemy[i]->xp;
					
					//if in a clan add bonus xp
					if (User[CurrSock].Clan[0] == '[')
					{
						bonus_clan_xp = map[current_map].Enemy[i]->xp * 0.10;//10% bonus
					}
					printf("User is in a clan so bonus xp for them! %i\n", (int)bonus_clan_xp);
					if (User[CurrSock].partyStatus == PARTY)
					{
						//give bonus XP for party
						float bonusXP = splitXP * 0.10;
						float totalDamage = 0;
						
						//find total damage from all party members
						for (int p = 0; p < MAX_CLIENTS; p++)
							if (User[p].Ident && User[p].partyStatus == PARTY && User[p].party == User[CurrSock].party)
								totalDamage += map[current_map].Enemy[i]->dibsDamage[p];
							
						
						
						//find total damage from all party members
						for (int p = 0; p < MAX_CLIENTS; p++)
							if (User[p].Ident && User[p].partyStatus == PARTY && User[p].party == User[CurrSock].party)
							{
								//base XP they deserve
								float gimmieXP = 0;
								float pl_dmg = (float)map[current_map].Enemy[i]->dibsDamage[p];
								
								if (pl_dmg > 0)
									gimmieXP = pl_dmg / totalDamage;
								
								printf("Player %s dealt %.2f percent of the damage or %i HP out of %i HP done by the party.\n", 
									User[p].Nick.c_str(), gimmieXP*100, map[current_map].Enemy[i]->dibsDamage[p], (int)totalDamage);
								gimmieXP *= splitXP;
								
								//add bonus XP
								gimmieXP += bonusXP;
								printf("Player %s earned %.2f XP points\n", User[p].Nick.c_str(), gimmieXP);
								
								int pXpGiven = (int)gimmieXP;
									
								//get the decimal alone
								gimmieXP = gimmieXP - pXpGiven;
								gimmieXP *= 100;
									
								//slight chance of bonus xp :)
								printf("Player %s will be given %i XP points for sure but has a %.2f chance for a bonus XP\n", User[p].Nick.c_str(),  pXpGiven, gimmieXP);
								if (Roll(gimmieXP))
								{
									pXpGiven++;
								}
								
								GiveXP(p, pXpGiven + bonus_clan_xp);
								printf("Player %s was given %i XP points\n", User[p].Nick.c_str(), pXpGiven);	
							}
						}//end split xp with party
						else
						{
							//just give myself xp
							GiveXP(CurrSock, map[current_map].Enemy[i]->xp + bonus_clan_xp);
							printf("[SOLO] Player %s was given %i XP points\n", User[CurrSock].Nick.c_str(), map[current_map].Enemy[i]->xp);
						}
				}//end died
				else
				{
					 map[current_map].Enemy[i]->hp -= dam;
	 
					 //enemy health bars
					 int hp = (unsigned char)((float)map[current_map].Enemy[i]->hp / map[current_map].Enemy[i]->hp_max*hpBar);

					 //packet
					 std::string hpPacket = "0";
					 hpPacket += NPC_HP;
					 hpPacket += i;
					 hpPacket += current_map;
					 hpPacket += hp;
					 hpPacket[0] = hpPacket.length();
	
					for (int c = 0; c < MAX_CLIENTS; c ++)
						if (User[c].Ident)
							User[c].SendPacket(hpPacket);
					printf("sending bandit hp shiz :)\n");	
				}//end not died enemy, just damage
			}//end hit
			else 
			{
				printf("%s did not hit enemy because x_dist:%i, y_dist:%i, max_dist:%i\r\n", 
					User[CurrSock].Nick.c_str(), (int)x_dist, (int)y_dist, (int)max_dist);
			}
		}//end loop if you hit enemys
		else
		{
			printf("Skippy enemy[%i] because dead:%i, partyBlocked:%i, partBlockedBy:%i\r\n", 
				i, (int)map[current_map].Enemy[i]->dead, partyBlocked, map[current_map].Enemy[i]->dibsPlayer);
		}
	}  // end if enemy is dead
}

void Stop(int CurrSock, float numx, float numy)
{
 //you cant move when attacking!
	if (User[CurrSock].attacking)
	{
		printf("tried to move stop when attacking.\7\n");
		//if not in que, put it there.
		if (User[CurrSock].que_action == 0)
		{
		   User[CurrSock].que_action = MOVE_STOP;
		}
		
		return;
	}
	
	//physics
	User[CurrSock].x_speed = 0;
	 
	bool good = false;

	if 
	(((
		(numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance) 
		||
		(numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)
		) 
		&&
		(
		(numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance) 
		||
		(numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance)
	)))
	{
	  //update server variables
	  User[CurrSock].x = numx;
	  User[CurrSock].y = numy;
	}
	else
	{
	 printf("MOVE STOP FAIL: (%.2f,%.2f)\n", numx, numy);
	 printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[CurrSock].x, User[CurrSock].y);     

	 //correction packet
	 numx = (int)User[CurrSock].x;
	 numy = (int)User[CurrSock].y;
	}
	  
	//corrections
	char *p;
	char b1, b2, b3, b4;
	//build correction packet
	std::string Packet = "0";
	Packet += MOVE_STOP;
	Packet += CurrSock;

	//break up X into 4 bytes
	p=(char*)&numx;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p=(char*)&numy;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;  
	Packet[0] = Packet.length();

	for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
	{
		if (User[i1].Ident && (i1 != CurrSock || !good) && User[i1].current_map == User[CurrSock].current_map) //don't send to yourself or other maps
		{
			// Send data
			User[i1].SendPacket(Packet);
		}  
	}    
}

void Right(int CurrSock, float numx, float numy)
{
	//you cant move when attacking!
	if (User[CurrSock].attacking)
	{
		printf("tried to move right when attacking.\7\n");
		//if not in que, put it there.
		if (User[CurrSock].que_action == 0)
		{
			User[CurrSock].que_action = MOVE_RIGHT;
		}
		
		return;
	}

	User[CurrSock].facing_right = true;
	//physics
	User[CurrSock].x_speed = 2;

	bool good = false;

	if
	(((
		(numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance)
		||
		(numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)
		)
		&&
		(
		(numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance)
		||
		(numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance)
	)))
	{
		//update server variables
		User[CurrSock].x = numx;
		User[CurrSock].y = numy;
		good = true;
	}
	else
	{
		printf("MOVE RIGHT FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[CurrSock].x, User[CurrSock].y);

		//correction packet
		numx = (int)User[CurrSock].x;
		numy = (int)User[CurrSock].y;
	}
	//corrections

	char *p;
	char b1, b2, b3, b4;
	//build correction packet
	std::string Packet = "0";
	Packet += MOVE_RIGHT;
	Packet += CurrSock;

	//break up X into 4 bytes
	p=(char*)&numx;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p=(char*)&numy;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//send to yourself
	Packet[0] = Packet.length();


	for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
	{
		if (i1 != CurrSock && User[i1].Ident  && User[i1].current_map == User[CurrSock].current_map) //don't send to yourself or other maps
		{
			// Send data
			User[i1].SendPacket(Packet);
		}
	}
}

void Left (int CurrSock, float numx, float numy)
{
	//you cant move when attacking!
	if (User[CurrSock].attacking)
	{
		printf("tried to move left when attacking.\7\n");
		//if not in que, put it there.
		if (User[CurrSock].que_action == 0)
		{
			User[CurrSock].que_action = MOVE_LEFT;
		}
		return;
	}

	User[CurrSock].facing_right = false;
	//physics
	User[CurrSock].x_speed = -2;

	bool good = false;

	if 
	(((
		(numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance) 
		||
		(numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)
		) 
		&&
		(
		(numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance) 
		||
		(numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance)
	)))
	{
		//update server variables
		User[CurrSock].x = numx;
		User[CurrSock].y = numy;
		good = true;
	}
	else
	{
		printf("MOVE RIGHT FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[CurrSock].x, User[CurrSock].y);     

		//correction packet
		numx = (int)User[CurrSock].x;
		numy = (int)User[CurrSock].y;
	}

	//corrections
	char *p;
	char b1, b2, b3, b4;
	//build correction packet
	std::string Packet = "0";
	Packet += MOVE_LEFT;
	Packet += CurrSock;

	//break up X into 4 bytes
	p=(char*)&numx;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p=(char*)&numy;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;  

	//send to yourself
	Packet[0] = Packet.length();


	for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
	{
		if (User[i1].Ident && (i1!=CurrSock || !good)  && User[i1].current_map == User[CurrSock].current_map)
		{
			// Send data
			User[i1].SendPacket(Packet);
		} 
	}  
}

void Jump(int CurrSock, float numx, float numy)
{
	//you cant move when attacking!
	if (User[CurrSock].attacking)
	{
		printf("tried to jump while attacking...\7\n");
		//if not in que, put it there.
		if (User[CurrSock].que_action == 0)
		{
			User[CurrSock].que_action = MOVE_JUMP;
		}
		return;
	}          
	bool good = false;

	if 
	(((
		(numx - User[CurrSock].x <= 0 && numx - (int)User[CurrSock].x > -snap_distance) 
		||
		(numx - User[CurrSock].x >= 0 && numx - (int)User[CurrSock].x < snap_distance)
		) 
		&&
		(
		(numy - User[CurrSock].y <= 0 && numy - (int)User[CurrSock].y > -snap_distance) 
		||
		(numy - User[CurrSock].y >= 0 && numy - (int)User[CurrSock].y < snap_distance)
	)))
	{
		//update server variables
		User[CurrSock].x = numx;
		User[CurrSock].y = numy;
		good = true;
	}
	else
	{
		printf("MOVE JUMP FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[CurrSock].x, User[CurrSock].y);

		//correction packet
		numx = (int)User[CurrSock].x;
		numy = (int)User[CurrSock].y;
	}


	//corrections
	char *p;
	char b1, b2, b3, b4;
	//build correction packet
	std::string Packet = "0";
	Packet += MOVE_JUMP;
	//ID
	Packet += CurrSock;

	//break up X into 4 bytes
	p=(char*)&numx;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p=(char*)&numy;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4; 

	//send to yourself
	Packet[0] = Packet.length();

	for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
	{
		if (User[i1].Ident && (i1!=CurrSock || !good)  && User[i1].current_map == User[CurrSock].current_map) 
		{
			User[i1].SendPacket(Packet);   
		} 
	}

	//physics
	User[CurrSock].y_speed = -6;
}

void DivideLoot(int enemy, int party)
{
	int current_map = 0;
	
	//some function variables
	float totalDamage = 0;
	int totalLootGiven = 0;
	int numInParty = 0;
	
	//find total damage from all party members
	for (int p = 0; p < MAX_CLIENTS; p++)
		if (User[p].Ident && User[p].partyStatus == PARTY && User[p].party == party)
		{
			//set the current map
		        current_map = User[p].current_map;
			totalDamage += map[current_map].Enemy[enemy]->dibsDamage[p];
			numInParty++;
		}
	
	//find a random starting point
	int randStart = rand() % numInParty;
	
    //give away items until they are all gone	
	while (totalLootGiven < map[current_map].Enemy[enemy]->lootAmount)
	{
		//find total damage from all party members
		for (int p = randStart; p < MAX_CLIENTS; p++) if (User[p].Ident && User[p].partyStatus == PARTY && User[p].party == party)
		{
			//calculate chance of getting the loot
			float lootEarned = (float)map[current_map].Enemy[enemy]->dibsDamage[p] / totalDamage * map[current_map].Enemy[enemy]->lootAmount;
			int lootGiven = 0;
			printf("%s has earned %.2f loot items.\n", User[p].Nick.c_str(), lootEarned);
			
			//give the player their whole items
			for (int i = 0; i < lootEarned; i++)
			{
				GiveLoot(enemy, p);
				lootGiven++;
				totalLootGiven++;
				
				//check if the max items has been given
				if  (totalLootGiven >= map[current_map].Enemy[enemy]->lootAmount)
					return;
			}
			
			//printf("Loot given so far for %s is %i\n", User[p].Nick.c_str(), lootGiven);
			
			//check if the max items has been given
			if  (totalLootGiven >= map[current_map].Enemy[enemy]->lootAmount)
				return;
			
			//printf("Total loot given so far is %i/%i so going to attempt a bonus item for %s.\n", totalLootGiven, map[current_map].Enemy[enemy]->lootAmount, User[p].Nick.c_str());
			
			//give a possible bonus item
			if (Roll(lootEarned - lootGiven))
			{
				GiveLoot(enemy, p);
				lootGiven++;
				totalLootGiven++;
			}
			
			//printf("All the loot given to %s is %i\n", User[p].Nick.c_str(), lootGiven);
			
			//check if the max items has been given
			if (totalLootGiven >= map[current_map].Enemy[enemy]->lootAmount)
				return;
			
		}
		
		//be fair, just loop in order now
		randStart = 0;
	}
}

void KillEnemy(int current_map, int enemy)
{
  
	printf("Map %i Enemy %i killed at (%i, %i) spawns at (%i, %i)\n", 
		current_map, enemy, (int)map[current_map].Enemy[enemy]->x,  (int)map[current_map].Enemy[enemy]->y, (int)map[current_map].Enemy[enemy]->sx, (int)map[current_map].Enemy[enemy]->sy);
	//set enemy to dead
	map[current_map].Enemy[enemy]->dead = true;

	
	//set their respawn timer
	map[current_map].Enemy[enemy]->respawn_ticker = Clock();
	
		 
	//disappear
	map[current_map].Enemy[enemy]->x = -100000;
	map[current_map].Enemy[enemy]->y = -100000;

	printf("Now he's at (%i, %i) and spawns at (%i, %i)\n", (int)map[current_map].Enemy[enemy]->x,  (int)map[current_map].Enemy[enemy]->y, (int)map[current_map].Enemy[enemy]->sx, (int)map[current_map].Enemy[enemy]->sy);
        
	 
	//tell players he disappeared
	std::string Packet = "0";
	Packet += ENEMY_MOVE_STOP;
	Packet += enemy;
	Packet += current_map;
	char*p=(char*)&map[current_map].Enemy[enemy]->x;
	Packet += p[0];
	Packet += p[1];
	Packet += p[2];
	Packet += p[3];
	p=(char*)&map[current_map].Enemy[enemy]->y;
	Packet += p[0];
	Packet += p[1];
	Packet += p[2];
	Packet += p[3];
	  
	//send packet to all players
	Packet[0] = Packet.length();
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
			User[i].SendPacket(Packet);
	}

	printf("Enemy killed, (%i,%i)\n", (int)map[current_map].Enemy[enemy]->x, (int)map[current_map].Enemy[enemy]->y);
}

void SpawnLoot(int current_map, SKO_ItemObject lootItem)
{
	int rand_i;
	for (rand_i = 0; rand_i < 255; rand_i++)
	{
		if (map[current_map].ItemObj[rand_i].status == false)
			break;
	}

	if (rand_i == 255)
	{
		if (map[current_map].currentItem == 255)
			map[current_map].currentItem = 0;

		rand_i = map[current_map].currentItem;
		map[current_map].currentItem++;
	}
	

	map[current_map].ItemObj[rand_i] = SKO_ItemObject(lootItem.itemID, lootItem.x, lootItem.y, lootItem.x_speed, lootItem.y_speed, lootItem.amount);
	map[current_map].ItemObj[rand_i].owner = lootItem.owner;
	map[current_map].ItemObj[rand_i].ownerTicker = Clock();

	char *p;
	char b1,b2,b3,b4;
	std::string Packet = "0";
	Packet += SPAWN_ITEM;
	Packet += rand_i;
	Packet += current_map;
	Packet += map[current_map].ItemObj[rand_i].itemID;


	p=(char*)&map[current_map].ItemObj[rand_i].x;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	p=(char*)&map[current_map].ItemObj[rand_i].y;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	p=(char*)&map[current_map].ItemObj[rand_i].x_speed;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	p=(char*)&map[current_map].ItemObj[rand_i].y_speed;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];                                                                         
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	Packet[0] = Packet.length();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident  && User[i].current_map == current_map)
			User[i].SendPacket(Packet);
	}
	
	printf("Item spawn packet (loot): ");
	for (int pk =0; pk < Packet.length(); pk++)
		printf("[%i]", (int)(unsigned char)Packet[pk]);
	printf("\n");
}

void GiveLoot(int enemy, int player)
{
	int current_map = User[player].current_map;
	 
	// remember about disabled items
	SKO_ItemObject lootItem = map[current_map].Enemy[enemy]->getLootItem();

	//holiday event    
	if (HOLIDAY)
	{        
		  //50% chance of a mystery box
		  if (Roll(50))
			 lootItem.itemID = ITEM_MYSTERY_BOX;
		
		  //12.5% chance of a holiday drop
		  if (Roll(25))
			  lootItem.itemID = HOLIDAY_NPC_DROP;
		
		  //37.5 chance of a normal drop
	}          
	
	lootItem.x = map[current_map].Enemy[enemy]->x + 16 + (32-Item[lootItem.itemID].w)/2.0;
	lootItem.y = map[current_map].Enemy[enemy]->y +
	map[current_map].Enemy[enemy]->y1 + Item[lootItem.itemID].h;
	lootItem.x_speed = (float)(rand() % 50 - 25)/100.0;
	lootItem.y_speed = (float)(rand() % 300 - 500)/100.0;
	  
	//printf("Enemy[enemy]->x is %i\n", (int)Enemy[enemy]->x);
	//printf("Item[lootItem.itemID].w is %i\n", (int)Item[lootItem.itemID].w);
	//printf("lootItem.itemID is %i\n", (int)lootItem.itemID);
	 
	lootItem.owner = player;

	SpawnLoot(current_map, lootItem);
}

void EnemyAttack(int i, int current_map)
{
	map[current_map].Enemy[i]->x_speed = 0;
	//check if anyone got hit
	for (int u = 0; u < MAX_CLIENTS; u++)
	{
		if (User[u].current_map == current_map && User[u].Ident)
		{
			//check for collision
			//horizontal
			float x_dist, y_dist;
			if (map[current_map].Enemy[i]->facing_right)
				x_dist = User[u].x - map[current_map].Enemy[i]->x;
			else
				x_dist = map[current_map].Enemy[i]->x - User[u].x;
			//vertical   
			y_dist = User[u].y - map[current_map].Enemy[i]->y;

			if (x_dist >= 0 && x_dist < 60 && y_dist >= -64 && y_dist <= 64)
			{
				int dam = 0;
				int weap = User[u].equip[0];
				int hat = User[u].equip[1];

				dam = map[current_map].Enemy[i]->strength - User[u].defence;
							  
				if (weap > 0)
				dam -= Item[weap].def;
				if (hat > 0)
				dam -= Item[hat].def;

				//dont give free hp
			    if (dam <= 0)
			    {
					dam = 0;
			    }
				else
				{
					std::string hpPacket = "0";
					hpPacket += TARGET_HIT;
					hpPacket += (char)1; // player
					hpPacket += u;
					hpPacket[0] = hpPacket.length();


					for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
					{
						if (User[i1].Ident)
						{
							User[i1].SendPacket(hpPacket);
						}
					}
				}
				std::string Packet;
				if (User[u].hp <= dam)
				{	
					printf("Respawning, x:%i y:%i\n", (int)User[u].x, (int)User[u].y);

					Respawn(User[u].current_map, u);

					printf("Respawned!, x:%i y:%i\n", (int)User[u].x, (int)User[u].y);
				}//end died
				else
				{
					User[u].hp -= dam;
					std::string Packet2 = "0";
					Packet2 += STAT_HP;
					Packet2 += User[u].hp;
					Packet2[0] = Packet2.length();
					User[u].SendPacket(Packet2);

					//party hp notification
					std::string hpPacket = "0";
					hpPacket += BUDDY_HP;
					hpPacket += u;
					hpPacket += (int)((User[u].hp / (float)User[u].max_hp) * 80);
					hpPacket[0] = hpPacket.length();

					for (int pl = 0; pl < MAX_CLIENTS; pl ++)
					{
						if (pl != u && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[u].party)
							User[pl].SendPacket(hpPacket);
					}
				}//end lose hp
			}//end hit
		}
	}//end loop everyone
}

void Warp(int i, SKO_Portal portal)
{
	//move the player to this spot
	User[i].current_map = portal.map;
	User[i].x = portal.spawn_x;
	User[i].y = portal.spawn_y;

	//tell everyone
	std::string warpPacket = "0";
	warpPacket += WARP;
	warpPacket += i;
	warpPacket += User[i].current_map;

	printf("Warping player %s to map %i at (%i, %i)\n", User[i].Nick.c_str(), User[i].current_map, (int)User[i].x, (int)User[i].y);	

	//break up the int as 4 bytes
	char * p = (char*)&User[i].x;
	char b1=p[0], b2=p[1], b3=p[2], b4=p[3];

	//spit out each of the bytes
	warpPacket += b1;
	warpPacket += b2;
	warpPacket += b3;
	warpPacket += b4;

	p=(char*)&User[i].y;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];

	//spit out each of the bytes
	warpPacket += b1;
	warpPacket += b2;
	warpPacket += b3;
	warpPacket += b4;


	warpPacket[0] = warpPacket.length();


	for (int pla = 0; pla < MAX_CLIENTS; pla++)
	{
		if (User[pla].Ident)
		{
		   //send packet to evertyone :)
		   User[pla].SendPacket(warpPacket);
		}
	}

	//TODO show all items and targets on warp?

}

void GiveXP(int CurrSock, int xp)
{
	printf("GiveXP(%i, %i);\n", CurrSock, xp);

	if (xp < 0 || xp > 1000) 
	{
		printf("[WARN] Tried to give weird xp: %i\n", xp);
		return;
	} 
	else 
	{
		printf("Clear to give %i XP \n", xp);
	}

	std::string Packet = "";
	char *p;
	char b1,b2,b3,b4;
	
	//add xp
	if (User[CurrSock].addXP(xp))
	{
		//you leveled up, send all stats

		//level
		Packet = "0";
		Packet += STAT_LEVEL;
		Packet += User[CurrSock].level;
		Packet[0] = Packet.length();
		User[CurrSock].SendPacket(Packet);

		//hp max
		Packet = "0";
		Packet += STATMAX_HP;
		Packet += User[CurrSock].max_hp;
		Packet[0] = Packet.length();
		User[CurrSock].SendPacket(Packet);

		//hp
		Packet = "0";
		Packet += STAT_HP;
		Packet += User[CurrSock].hp;
		Packet[0] = Packet.length();
		User[CurrSock].SendPacket(Packet);

		//stat_points
		Packet = "0";
		Packet += STAT_POINTS;
		Packet += User[CurrSock].stat_points;
		Packet[0] = Packet.length();
		User[CurrSock].SendPacket(Packet);

		//xp
		Packet = "0";
		Packet += STAT_XP;
		p=(char*)&User[CurrSock].xp;
		b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
		Packet += b1;
		Packet += b2;
		Packet += b3;
		Packet += b4;
		Packet[0] = Packet.length();
		User[CurrSock].SendPacket(Packet);

		//max xp
		Packet = "0";
		Packet += STATMAX_XP;
		p=(char*)&User[CurrSock].max_xp;
		b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
		Packet += b1;
		Packet += b2;
		Packet += b3;
		Packet += b4;
		Packet[0] = Packet.length();
		User[CurrSock].SendPacket(Packet);

		//tell party members my level
		std::string lvlPacket = "0";
		lvlPacket += BUDDY_LEVEL;
		lvlPacket += CurrSock;
		lvlPacket += User[CurrSock].level;
		lvlPacket[0] = lvlPacket.length();

		for (int pl = 0; pl < MAX_CLIENTS; pl++)
		{
			if (pl != CurrSock && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[CurrSock].party)
				User[pl].SendPacket(lvlPacket);
		}
	}//end gain xp
	   
	//xp
	Packet = "0";
	Packet += STAT_XP;
	p=(char*)&User[CurrSock].xp;
	b1=p[0]; b2=p[1]; b3=p[2]; b4=p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;
	Packet[0] = Packet.length();
	User[CurrSock].SendPacket(Packet);

	//tell party members my xp
	std::string xpPacket = "0";
	xpPacket += BUDDY_XP;
	xpPacket += CurrSock;
	xpPacket += (int)((User[CurrSock].xp / (float)User[CurrSock].max_xp) * 80);
	xpPacket[0] = xpPacket.length();

	for (int pl = 0; pl < MAX_CLIENTS; pl++)
	{
		if (pl != CurrSock && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[CurrSock].party)
			User[pl].SendPacket(xpPacket);
	}
}


void Respawn(int current_map, int i)
{
	//place their coords
	User[i].x = map[current_map].spawn_x;
	User[i].y = map[current_map].spawn_y;
	User[i].y_speed = 0;
	User[i].hp = User[i].max_hp;

	std::string Packet1 = "0";
	Packet1 += RESPAWN;
	Packet1 += i;
                              
	float numx = User[i].x;       
	char * p = (char*)&numx;
	Packet1 += p[0];
	Packet1 += p[1];
    Packet1 += p[2];
    Packet1 += p[3];	

	float numy = User[i].y;
	p = (char*)&numy;
    Packet1 += p[0];
	Packet1 += p[1];
	Packet1 += p[2];
	Packet1 += p[3];

	Packet1[0] = Packet1.length();

	//tell everyone they died
	for (int ii = 0; ii < MAX_CLIENTS; ii++)
	{
		if (User[ii].Ident)
		{
		   User[ii].SendPacket(Packet1);
		}
	} 
}



void quitParty(int CurrSock)
{
	
	printf("quitParty(%s)\n", User[CurrSock].Nick.c_str());
	if (User[CurrSock].partyStatus == 0 || User[CurrSock].party < 0)
	{
		printf("User %s is in no party.\n", User[CurrSock].Nick.c_str());
	}		
	else 
	{
		printf("User %s has party status %i and in party %i\n", 
			User[CurrSock].Nick.c_str(), 
			User[CurrSock].partyStatus,
			User[CurrSock].party);
	}
	int partyToLeave = User[CurrSock].party;

	User[CurrSock].party = -1;
        User[CurrSock].partyStatus = 0;

	std::string acceptPacket = "0";
	acceptPacket += PARTY;
	acceptPacket += ACCEPT;
	acceptPacket += CurrSock;
	acceptPacket += (char)-1;
	acceptPacket[0] = acceptPacket.length();

	printf("telling everyone that %s left his party.\n", User[CurrSock].Nick.c_str());
	//tell everyone
	for (int pl = 0; pl < MAX_CLIENTS; pl++)
		if (User[pl].Ident)
			User[pl].SendPacket(acceptPacket);

	//quit the inviter if there is only one
	int inv;
	int count = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (User[i].Ident && User[i].party == partyToLeave)
		{
			count++;
			inv = i;
		}

	//only if they are the last one remaining
	if (count == 1)
	{
		User[inv].party = -1;
		User[inv].partyStatus = 0;
							
		acceptPacket = "0";
                acceptPacket += PARTY;
                acceptPacket += ACCEPT;
                acceptPacket += inv;
                acceptPacket += (char)-1;
                acceptPacket[0] = acceptPacket.length();

                //tell everyone
                for (int pl = 0; pl < MAX_CLIENTS; pl++)
                    if (User[pl].Ident) 
                      User[pl].SendPacket(acceptPacket);

	}
}
