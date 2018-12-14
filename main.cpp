/* INCLUDES */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>

#include "Global.h"
#include "SKO_Player.h"
#include "SKO_Item.h"
#include "SKO_ItemObject.h"
#include "SKO_Enemy.h"
#include "OPI_MYSQL.h"
#include "KE_Timestep.h"
#include "SKO_Stall.h"
#include "SKO_Shop.h"
#include "OPI_Clock.h"
#include "SKO_Map.h"
#include "SKO_item_defs.h"
#include "SKO_Portal.h"
#include "SKO_Target.h"
#include "base64.h"
#include "SKO_Repository.h"
#include "SKO_PacketTypes.h"
#include "SKO_Network.h"

SKO_Network *network;

/* DEFINES */
// Maximum number of clients allowed to connect
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

//holiday events
unsigned const char HOLIDAY_NPC_DROP = ITEM_EASTER_EGG, HOLIDAY_BOX_DROP = ITEM_BUNNY_EARS;
const bool HOLIDAY = false;

bool blocked(int current_map, float box1_x1, float box1_y1, float box1_x2, float box1_y2, bool npc);

//attack time
int attack_speed = 40 * 6;

//Database connection
OPI_MYSQL *db; // = new OPI_MYSQL();;

//this waits for auto-save
unsigned int persistTicker;
bool persistLock = false;


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
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		//well are they online
		if (lower(User[i].Nick).compare(lower(Username)) == 0)
		{
			return 0;
		}
	}

	return 1;
}

int mute(int Mod_i, std::string Username, int flag)
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
	}
	else
	{
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
	}
	else
	{
		printf("not ip banned. good.\n");
	}

	//create unique salt for user password
	sql = "SELECT REPLACE(UUID(), '-', '');";
	db->query(sql);
	db->nextRow();
	std::string player_salt = db->getString(0);

	sql = "INSERT INTO player (username, password, level, facing_right, x, y, hp, str, def, xp_max, hp_max, current_map, salt) VALUES('";
	sql += db->clean(Username);
	sql += "', '";
	sql += db->clean(Hash(Password, player_salt));
	sql += "', '1', b'1', '314', '300', '10', '2', '1', '10', '10', '2', '";
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

static void *Physics(void *Arg);
static void *TargetLoop(void *Arg);
static void *EnemyLoop(void *arg);
static void *UserLoop(void *arg);

int snap_distance = 64;

/* CODE */
int main()
{
	//stdout buffer so we can tail -f the logs
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("Starting Server...\n");

	//load maps and stuff
	for (int mp = 0; mp < NUM_MAPS; mp++)
	{
		//map filename
		std::stringstream fileName;
		fileName << "map";
		fileName << mp;

		map[mp] = SKO_Map(fileName.str());
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
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
		std::getline(ConfigFile, line);
		snap_distance = atoi(line.c_str());
		printf("snap_distance: %d\n\r", snap_distance);

		//server port
		std::getline(ConfigFile, line);
		serverPort = atoi(line.c_str());
		printf("server port: %d\n\r", serverPort);

		//SQL Server Hostname
		std::getline(ConfigFile, line);
		databaseHostname = line.c_str();
		printf("hostname: %s\n\r", databaseHostname.c_str());

		//SQL Server Schema
		std::getline(ConfigFile, line);
		databaseSchema = line.c_str();
		printf("schema: %s\n\r", databaseSchema.c_str());

		//SQL Server Username
		std::getline(ConfigFile, line);
		databaseUsername = line.c_str();
		printf("user: %s\n\r", databaseUsername.c_str());

		//SQL Server Password
		std::getline(ConfigFile, line);
		databasePassword = line.c_str();
		printf("password: %s\n", databasePassword.c_str());

		ConfigFile.close();
	}
	else
	{
		printf("COULD NOT OPEN CONFIG\n");
		return 1;
	}

	trim(databaseHostname);
	trim(databaseUsername);
	trim(databasePassword);
	trim(databaseSchema);

	printf("About to connect to database.\n");

	db = new OPI_MYSQL();
	if (!db->connect(databaseHostname, databaseSchema, databaseUsername, databasePassword))
	{
		printf("Could not connect to MYSQL database.\n");
		db->getError();
		return 1;
	}

	//items                            width,  height, type, def,  str,  hp,  reach, equipID
	//values                              w      h     t     d     s     h      r     e  s = sell
	Item[ITEM_GOLD] = SKO_Item(8, 8, 0, 0, 0, 0, 0, 0, 0);
	Item[ITEM_DIAMOND] = SKO_Item(15, 14, 0, 0, 0, 0, 0, 0, 100);
	Item[ITEM_MYSTERY_BOX] = SKO_Item(16, 15, 4, 0, 0, 0, 0, 0, 0);
	//food
	Item[ITEM_CARROT] = SKO_Item(15, 17, 1, 0, 0, 1, 0, 0, 1);
	Item[ITEM_CHEESE] = SKO_Item(13, 20, 1, 0, 0, 2, 0, 0, 1);
	Item[ITEM_BEER] = SKO_Item(9, 31, 1, 0, 0, 3, 0, 0, 2);
	Item[ITEM_WINE] = SKO_Item(9, 31, 1, 0, 0, 5, 0, 0, 4);
	Item[ITEM_POTION] = SKO_Item(10, 16, 1, 0, 0, 20, 0, 0, 5);
	//weapon
	Item[ITEM_SWORD_TRAINING] = SKO_Item(32, 16, 2, 0, 1, 0, 23, 1, 5);
	//hat
	Item[ITEM_BLUE_PHAT] = SKO_Item(10, 13, 3, 0, 0, 0, 0, 1, 0);
	Item[ITEM_SANTA_HAT] = SKO_Item(10, 13, 3, 0, 0, 0, 0, 2, 0);
	Item[ITEM_GOLD_PHAT] = SKO_Item(10, 13, 3, 0, 0, 0, 0, 3, 0);

	//pi day event
	Item[ITEM_NERD_GLASSES] = SKO_Item(21, 13, 3, 0, 0, 0, 0, 4, 0);
	Item[ITEM_CHERRY_PI] = SKO_Item(32, 28, 5, 0, 0, 100, 0, 1, 0);

	//easter event
	Item[ITEM_BUNNY_EARS] = SKO_Item(14, 10, 3, 0, 0, 0, 0, 5, 0);
	Item[ITEM_EASTER_EGG] = SKO_Item(8, 10, 5, 0, 0, 100, 0, 2, 0);

	//halloween event
	Item[ITEM_PUMPKIN] = SKO_Item(21, 23, 5, 0, 0, 0, 0, 3, 0);
	Item[ITEM_SKULL_MASK] = SKO_Item(13, 18, 3, 0, 0, 0, 0, 6, 0);

	//welcome to summer event
	Item[ITEM_ICECREAM] = SKO_Item(9, 16, 5, 0, 0, 0, 0, 4, 0);
	Item[ITEM_SUNGLASSES] = SKO_Item(21, 13, 3, 0, 0, 0, 0, 8, 0);

	Item[ITEM_SWORD_RUSTED] = SKO_Item(11, 37, 2, 1, 2, 0, 32, 3, 6);
	Item[ITEM_SWORD_STEEL] = SKO_Item(11, 37, 2, 2, 4, 0, 32, 4, 80);
	Item[ITEM_SWORD_GOLD] = SKO_Item(11, 42, 2, 3, 11, 0, 40, 5, 6030);
	Item[ITEM_SWORD_CRYSTAL] = SKO_Item(15, 43, 2, 5, 22, 0, 42, 6, 100300);

	Item[ITEM_AXE_RUSTED] = SKO_Item(12, 31, 2, 1, 3, 0, 28, 7, 8);
	Item[ITEM_AXE_STEEL] = SKO_Item(12, 31, 2, 1, 5, 0, 28, 8, 85);
	Item[ITEM_AXE_GOLD] = SKO_Item(21, 36, 2, 2, 13, 0, 37, 9, 6100);
	Item[ITEM_AXE_CRYSTAL] = SKO_Item(21, 38, 2, 3, 24, 0, 40, 10, 98525);

	Item[ITEM_HAMMER_RUSTED] = SKO_Item(17, 33, 2, 0, 4, 0, 22, 11, 10);
	Item[ITEM_HAMMER_STEEL] = SKO_Item(17, 33, 2, 0, 7, 0, 22, 12, 90);
	Item[ITEM_HAMMER_GOLD] = SKO_Item(17, 33, 2, 2, 16, 0, 22, 13, 6120);
	Item[ITEM_HAMMER_CRYSTAL] = SKO_Item(27, 33, 2, 3, 25, 0, 32, 14, 99085);

	Item[ITEM_SCYTHE] = SKO_Item(23, 25, 2, 3, 3, 0, 22, 15, 7120);
	Item[ITEM_SCYTHE_REAPER] = SKO_Item(27, 32, 2, 10, 10, 10, 32, 16, 19085);
	Item[ITEM_CANDY_CANE] = SKO_Item(10, 13, 3, 3, 3, 3, 0, 17, 0);

	///halloween event items
	Item[ITEM_HALLOWEEN_MASK] = SKO_Item(18, 21, 3, 0, 0, 0, 0, 10, 0);
	Item[ITEM_GUARD_HELM] = SKO_Item(18, 21, 3, 5, 1, 10, 0, 11, 2500);
	Item[ITEM_JACK_OLANTERN] = SKO_Item(21, 23, 5, 0, 0, 0, 0, 5, 0);

	Item[ITEM_WHITE_PHAT] = SKO_Item(10, 13, 3, 0, 0, 0, 0, 12, 0);
	Item[ITEM_SKELETON_HELM] = SKO_Item(18, 21, 3, 1, 2, 1, 0, 13, 9000);
	Item[ITEM_TRAINING_HELM] = SKO_Item(18, 21, 3, 1, 2, 1, 0, 14, 75);

	Item[ITEM_PURPLE_PHAT] = SKO_Item(10, 13, 3, 0, 0, 0, 0, 15, 0);
	Item[ITEM_SNOW_BALL] = SKO_Item(12, 12, 5, 0, 0, 0, 0, 6, 0);

	for (int mp = 0; mp < NUM_MAPS; mp++)
		for (int i = 0; i < 256; i++)
			map[mp].ItemObj[i] = SKO_ItemObject();

	network = new SKO_Network(db, serverPort, 30);
	printf("Initialized SKO_Network.\n");

	printf("Starting up SKO_Network...\n");
	std::string networkStatus = network->Startup();
	printf("SKO_Network status is: %s\n", networkStatus);

	if (networkStatus == "success")
	{
		//TODO: make nice logging function that abstracts the console colors
		printf(kGreen "SKO Network initialized successfully.\n" kNormal);
	}
	else
	{
		printf(kRed "Could not initialize SKO_Network. Here is the status: [%s]\n%s", networkStatus.c_str());
		network->Cleanup();
		return 1;
	}

	/* initialize random seed: */
	srand(Clock());

	//multi threading
	pthread_t physicsThread, enemyThread, targetThread;
	pthread_t mainThread[MAX_CLIENTS];

	for (unsigned long int i = 0; i < NUM_MAPS; i++)
	{
		if (pthread_create(&physicsThread, NULL, Physics, (void *)i))
		{
			printf("Could not create thread for physics...\n");
			return 1;
		}

		if (pthread_create(&enemyThread, NULL, EnemyLoop, (void *)i))
		{
			printf("Could not create thread for enemys...\n");
			return 1;
		}

		if (pthread_create(&targetThread, NULL, TargetLoop, (void *)i))
		{
			printf("Could not create thread for targets...\n");
			return 1;
		}
	}

	for (unsigned long int i = 0; i < MAX_CLIENTS; i++)
	{
		if (pthread_create(&mainThread[i], NULL, UserLoop, (void *)i))
		{
			printf("Could not create thread for UserLoop...\n");
			return 1;
		}
	}

	printf("Done loading!\nStopping main, threads will continue.\n");
	if (pthread_join(mainThread[0], NULL))
	{
		printf("Could not join thread for que...\n");
	}

	return 0;
}

void *UserLoop(void *arg)
{
	long int id = (intptr_t)arg;

	while (!SERVER_QUIT)
	{
		// Ignore socket if it is not connected
		if (!User[id].Status)
		{
			Sleep(1);
			continue;
		}
		
		network->HandleClient(id);

		// Sleep a bit
		Sleep(1);
	} //end while
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
			if (!map[current_map].Target[i].active && Clock() - map[current_map].Target[i].respawn_ticker > 5000)
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

			if (map[current_map].Enemy[i]->y > map[current_map].death_pit)
			{
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

					p = (char *)&map[current_map].Enemy[i]->x;
					Packet += p[0];
					Packet += p[1];
					Packet += p[2];
					Packet += p[3];
					p = (char *)&map[current_map].Enemy[i]->y;
					Packet += p[0];
					Packet += p[1];
					Packet += p[2];
					Packet += p[3];

					Packet[0] = Packet.length();

					//enemy health bars
					int hp = (unsigned char)((float)map[current_map].Enemy[i]->hp / map[current_map].Enemy[i]->hp_max * hpBar);
					//packet
					std::string hpPacket = "0";
					hpPacket += ENEMY_HP;
					hpPacket += i;
					hpPacket += current_map;
					hpPacket += hp;
					hpPacket[0] = hpPacket.length();

					//tell everyone they spawned
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
			} //end if dead
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
						if (x_dist >= -10 && x_dist <= 10 && y_dist >= -60 && y_dist <= 60)
						{
							confront = true;

							//timer override if first time
							if (map[current_map].Enemy[i]->x_speed != 0)
								map[current_map].Enemy[i]->AI_period = 0;
						}

					} //end for user Ident only

					if (confront)
					{
						next_action = ENEMY_MOVE_STOP;
						break;
					}

				} //end for user: u

				//not dead, do some actions.
				if ((Clock() - map[current_map].Enemy[i]->AI_ticker) > map[current_map].Enemy[i]->AI_period)
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
										if (map[current_map].Enemy[i]->hp > (map[current_map].Enemy[i]->hp_max / 2))
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
									else // if (User[u].x < map[current_map].Enemy[i]->x)
									{
										//chase
										if (map[current_map].Enemy[i]->hp > (map[current_map].Enemy[i]->hp_max / 2))
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

								} //end same plane actions

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

							} //end u ident
						}	 //end for User u
					}		  //end !confront
					else	  // if confront is true then:
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
						p = (char *)&map[current_map].Enemy[i]->x;
						Packet += p[0];
						Packet += p[1];
						Packet += p[2];
						Packet += p[3];
						p = (char *)&map[current_map].Enemy[i]->y;
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

					} //end no spam

				} //end enemy AI

			} //end not dead
		}	 // end enemy for loop

		//npc
		for (int i = 0; i < map[current_map].num_npcs; i++)
		{

			int next_action = 0;

			std::string Packet = "0";

			{

				//not dead, do some actions.
				if ((Clock() - map[current_map].NPC[i]->AI_ticker) > map[current_map].NPC[i]->AI_period)
				{
					// UP TO quarter second extra delay
					int random_delay = 2500 + rand() % 169;

					int random_action = rand() % 4; //2:1 ratio stopping to moving. To make him not seem like a meth tweaker

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
						p = (char *)&map[current_map].NPC[i]->x;
						Packet += p[0];
						Packet += p[1];
						Packet += p[2];
						Packet += p[3];
						p = (char *)&map[current_map].NPC[i]->y;
						Packet += p[0];
						Packet += p[1];
						Packet += p[2];
						Packet += p[3];

						Packet[0] = Packet.length();

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

					} //end no spam

				} //end enemy AI

			} //end not dead

			//amoothly operate by sleeping at least a tick before going to the next enemy
			Sleep(10);

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

	int a = 0;
	char *p;
	bool block_y;
	bool block_x;
	int ID = 0;
	float box1_x1, box1_y1, box1_x2, box1_y2;
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
				block_y = blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y2, true);

				map[current_map].Enemy[i]->ground = true;

				//vertical movement
				if (!block_y)
				{ //not blocked, fall

					//animation
					map[current_map].Enemy[i]->ground = false;

					map[current_map].Enemy[i]->y += map[current_map].Enemy[i]->y_speed;
				}
				else
				{ //blocked, stop
					if (map[current_map].Enemy[i]->y_speed > 0)
					{
						map[current_map].Enemy[i]->y_speed = 1;

						//while or if TODO
						for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25 + map[current_map].Enemy[i]->y2, true)); loopVar++)
							map[current_map].Enemy[i]->y += map[current_map].Enemy[i]->y_speed;

						map[current_map].Enemy[i]->y = (int)(map[current_map].Enemy[i]->y + 0.5);
					}
					if (map[current_map].Enemy[i]->y_speed < 0)
					{
						map[current_map].Enemy[i]->y_speed = -1;

						//while or if TODO
						for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y_speed + map[current_map].Enemy[i]->y1 + 0.25 + map[current_map].Enemy[i]->y2, true)); loopVar++)
							map[current_map].Enemy[i]->y += map[current_map].Enemy[i]->y_speed;
					}

					map[current_map].Enemy[i]->y_speed = 0;
				}

				//horizontal collision detection
				block_x = blocked(current_map, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x_speed + map[current_map].Enemy[i]->x1, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y1, map[current_map].Enemy[i]->x + map[current_map].Enemy[i]->x_speed + map[current_map].Enemy[i]->x2, map[current_map].Enemy[i]->y + map[current_map].Enemy[i]->y2, true);

				//horizontal movement
				if (!block_x)
				{ //not blocked, walk
					map[current_map].Enemy[i]->x += map[current_map].Enemy[i]->x_speed;
				}

				if ((map[current_map].Enemy[i]->x_speed == 0 && map[current_map].Enemy[i]->y_speed == 0) || !map[current_map].Enemy[i]->attacking)
					map[current_map].Enemy[i]->current_frame = 0;
			} //end enemies/npc

			//npcs
			for (int i = 0; i < map[current_map].num_npcs; i++)
			{
				//enemy physics
				if (map[current_map].NPC[i]->y_speed < 10)
					map[current_map].NPC[i]->y_speed += GRAVITY;

				//verical collision detection
				block_y = blocked(current_map, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x1, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x2, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y2, true);

				map[current_map].NPC[i]->ground = true;

				//vertical movement
				if (!block_y)
				{ //not blocked, fall

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
						for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x1, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x2, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25 + map[current_map].NPC[i]->y2, true)); loopVar++)
							map[current_map].NPC[i]->y += map[current_map].NPC[i]->y_speed;

						map[current_map].NPC[i]->y = (int)(map[current_map].NPC[i]->y + 0.5);
					}
					if (map[current_map].NPC[i]->y_speed < 0)
					{
						map[current_map].NPC[i]->y_speed = -1;

						//while or if TODO
						for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x1, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25, map[current_map].NPC[i]->x + map[current_map].NPC[i]->x2, map[current_map].NPC[i]->y + map[current_map].NPC[i]->y_speed + map[current_map].NPC[i]->y1 + 0.25 + map[current_map].NPC[i]->y2, true)); loopVar++)
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
			} //end npc

			//players
			for (int i = 0; i < MAX_CLIENTS; i++)
				if (User[i].current_map == current_map && User[i].Ident)
				{
					//check for portal collisions
					for (int portal = 0; portal < map[current_map].num_portals; portal++)
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
						float box2_y2 = map[current_map].Portal[portal].y + map[current_map].Portal[portal].h;

						if (box1_x2 > box2_x1 && box1_x1 < box2_x2 && box1_y2 > box2_y1 && box1_y1 < box2_y2)
						{
							if (User[i].level >= map[current_map].Portal[portal].level_required)
							{
								Warp(i, map[current_map].Portal[portal]);
								//TODO: how do players stay in a party on different maps?
								//Maybe disconnect after a minute.
								quitParty(i);
							}
						} //end portal collison
					}	 //end for portal

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
							switch (User[i].que_action)
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
							} //end switch

							//add coords
							char *p;
							char b1, b2, b3, b4;

							//ID
							Packet += i;

							//break up X into 4 bytes
							p = (char *)&numx;
							b1 = p[0];
							b2 = p[1];
							b3 = p[2];
							b4 = p[3];
							Packet += b1;
							Packet += b2;
							Packet += b3;
							Packet += b4;

							//break up Y into 4 bytes
							p = (char *)&numy;
							b1 = p[0];
							b2 = p[1];
							b3 = p[2];
							b4 = p[3];
							Packet += b1;
							Packet += b2;
							Packet += b3;
							Packet += b4;

							//tell everyone
							Packet[0] = Packet.length();
							for (int qu = 0; qu < MAX_CLIENTS; qu++)
								if (User[qu].Ident)
									User[qu].SendPacket(Packet);

							//reset the cue
							User[i].que_action = 0;

						} // end que actions
					}	 //end attack ticker

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

								for (int pl = 0; pl < MAX_CLIENTS; pl++)
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
					block_y = blocked(current_map, User[i].x + 25, User[i].y + User[i].y_speed + 13 + 0.25, User[i].x + 38, User[i].y + User[i].y_speed + 64, false);

					//vertical movement
					if (!block_y)
					{ //not blocked, fall

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
							for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, User[i].x + 25, User[i].y + User[i].y_speed + 13 + 0.25, User[i].x + 38, User[i].y + User[i].y_speed + 64 + 0.25, false)); loopVar++)
								User[i].y += User[i].y_speed;

							User[i].y = (int)(User[i].y + 0.5);
						}
						if (User[i].y_speed < 0)
						{
							User[i].y_speed = -1;

							//while or if TODO
							for (int loopVar = 0; loopVar < HIT_LOOP && (!blocked(current_map, User[i].x + 25, User[i].y + User[i].y_speed + 13 + 0.25, User[i].x + 38, User[i].y + User[i].y_speed + 64 + 0.25, false)); loopVar++)
								User[i].y += User[i].y_speed;
						}

						User[i].y_speed = 0;
					}

					//horizontal collision detection
					block_x = blocked(current_map, User[i].x + User[i].x_speed + 23, User[i].y + 13, User[i].x + User[i].x_speed + 40, User[i].y + 64, false);

					//horizontal movement
					if (!block_x)
					{ //not blocked, walk
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
					block_x = blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y + Item[map[current_map].ItemObj[i].itemID].w, false);

					if (map[current_map].ItemObj[i].y_speed < 10)
						map[current_map].ItemObj[i].y_speed += GRAVITY;

					//vertical collision detection
					block_y = blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y + map[current_map].ItemObj[i].y_speed + 0.2, map[current_map].ItemObj[i].x + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y + map[current_map].ItemObj[i].y_speed + Item[map[current_map].ItemObj[i].itemID].h, false);

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
					box1_x2 = map[current_map].ItemObj[i].x + Item[map[current_map].ItemObj[i].itemID].w;
					box1_y2 = map[current_map].ItemObj[i].y + Item[map[current_map].ItemObj[i].itemID].h;

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
					} //end despawn items that fell off the edge of the map

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
									p = (char *)&amt;
									b1 = p[0];
									b2 = p[1];
									b3 = p[2];
									b4 = p[3];
									Packet += b1;
									Packet += b2;
									Packet += b3;
									Packet += b4;

									Packet[0] = Packet.length();
									User[c].SendPacket(Packet);
								} //end else not inventory full (you can pick up)
							}
						}
					}
				}
		} //end timestep

		Sleep(1);
	} //end while true
}

void despawnTarget(unsigned int target, unsigned int current_map)
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

/* perform player actions */
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
		printf("%s tried to attack while jumping...\7\7\n", User[CurrSock].Nick.c_str());
		return;
	}

	printf("%s is attacking!\n", User[CurrSock].Nick.c_str());
	User[CurrSock].attacking = true;
	User[CurrSock].attack_ticker = Clock();
	User[CurrSock].x_speed = 0;

	bool good = false;

	if (((
			 (numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance) ||
			 (numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)) &&
		 ((numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance) ||
		  (numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance))))
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

	printf("%s attack good? %i\n", User[CurrSock].Nick.c_str(), good);

	std::string Packet = "0";

	char *p;
	char b1, b2, b3, b4;
	//build correction packet
	Packet = "0";
	Packet += ATTACK;
	Packet += CurrSock;
	//ID

	//break up X into 4 bytes
	p = (char *)&numx;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p = (char *)&numy;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
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
	printf("Looping all players.\n");
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!User[i].Ident)
			continue;

		bool inParty = false;
		if (User[i].partyStatus == PARTY && User[i].party == User[CurrSock].party)
			inParty = true;

		if (i != CurrSock && !inParty && User[i].current_map == User[CurrSock].current_map)
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
			if (weap != 0)
			{
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

				} //end died
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
						if (User[i1].Ident && User[i1].current_map == User[i].current_map)
							;
						{
							User[i1].SendPacket(Packet);

							if (i != i1 && User[i1].partyStatus == PARTY && User[i1].party == User[i].party)
								User[i1].SendPacket(hpPacket);
						}
					}
				} //end lose hp
			}	 //end hit
		}
	} //end loop everyone

	//loop all targets
	printf("Checking all targets\n");
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
	}

	//loop all enemies
	printf("Checking all enemies: %i\n", map[current_map].num_enemies);
	for (int i = 0; i < map[current_map].num_enemies; i++)
	{
		bool partyBlocked = false;

		//if enemy is blocked by a party dibs
		if (map[current_map].Enemy[i]->dibsPlayer >= 0)
		{
			//then assume it's blocked
			partyBlocked = true;
			printf("partyBlocked because dibsPlayer = %i\n", map[current_map].Enemy[i]->dibsPlayer);
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
			printf("Checking enemy: %i on map: %i\n", i, current_map);
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

				printf("%s hit an enemy! damage is: %i\n", User[CurrSock].Nick.c_str());

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
					else //not in a party, they get all the loots!
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
						bonus_clan_xp = map[current_map].Enemy[i]->xp * 0.10; //10% bonus
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
									   User[p].Nick.c_str(), gimmieXP * 100, map[current_map].Enemy[i]->dibsDamage[p], (int)totalDamage);
								gimmieXP *= splitXP;

								//add bonus XP
								gimmieXP += bonusXP;
								printf("Player %s earned %.2f XP points\n", User[p].Nick.c_str(), gimmieXP);

								int pXpGiven = (int)gimmieXP;

								//get the decimal alone
								gimmieXP = gimmieXP - pXpGiven;
								gimmieXP *= 100;

								//slight chance of bonus xp :)
								printf("Player %s will be given %i XP points for sure but has a %.2f chance for a bonus XP\n", User[p].Nick.c_str(), pXpGiven, gimmieXP);
								if (Roll(gimmieXP))
								{
									pXpGiven++;
								}

								GiveXP(p, pXpGiven + bonus_clan_xp);
								printf("Player %s was given %i XP points\n", User[p].Nick.c_str(), pXpGiven);
							}
					} //end split xp with party
					else
					{
						//just give myself xp
						GiveXP(CurrSock, map[current_map].Enemy[i]->xp + bonus_clan_xp);
						printf("[SOLO] Player %s was given %i XP points\n", User[CurrSock].Nick.c_str(), map[current_map].Enemy[i]->xp);
					}
				} //end died
				else
				{
					map[current_map].Enemy[i]->hp -= dam;

					//enemy health bars
					int hp = (unsigned char)((float)map[current_map].Enemy[i]->hp / map[current_map].Enemy[i]->hp_max * hpBar);

					//packet
					std::string hpPacket = "0";
					hpPacket += ENEMY_HP;
					hpPacket += i;
					hpPacket += current_map;
					hpPacket += hp;
					hpPacket[0] = hpPacket.length();

					for (int c = 0; c < MAX_CLIENTS; c++)
						if (User[c].Ident)
							User[c].SendPacket(hpPacket);
					printf("sending bandit hp shiz :)\n");
				} //end not died enemy, just damage
			}	 //end hit
			else
			{
				printf("%s did not hit enemy because x_dist:%i, y_dist:%i, max_dist:%i\n",
					   User[CurrSock].Nick.c_str(), (int)x_dist, (int)y_dist, (int)max_dist);
			}
		} //end loop if you hit enemys
		else
		{
			printf("Skippy enemy[%i] because dead:%i, partyBlocked:%i, partBlockedBy:%i\n",
				   i, (int)map[current_map].Enemy[i]->dead, partyBlocked, map[current_map].Enemy[i]->dibsPlayer);
		}
	} // end if enemy is dead
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

	if (((
			 (numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance) ||
			 (numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)) &&
		 ((numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance) ||
		  (numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance))))
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
	p = (char *)&numx;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p = (char *)&numy;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
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

	if (((
			 (numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance) ||
			 (numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)) &&
		 ((numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance) ||
		  (numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance))))
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
	p = (char *)&numx;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p = (char *)&numy;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//send to yourself
	Packet[0] = Packet.length();

	for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
	{
		if (i1 != CurrSock && User[i1].Ident && User[i1].current_map == User[CurrSock].current_map) //don't send to yourself or other maps
		{
			// Send data
			User[i1].SendPacket(Packet);
		}
	}
}

void Left(int CurrSock, float numx, float numy)
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

	if (((
			 (numx - User[CurrSock].x <= 0 && numx - User[CurrSock].x > -snap_distance) ||
			 (numx - User[CurrSock].x >= 0 && numx - User[CurrSock].x < snap_distance)) &&
		 ((numy - User[CurrSock].y <= 0 && numy - User[CurrSock].y > -snap_distance) ||
		  (numy - User[CurrSock].y >= 0 && numy - User[CurrSock].y < snap_distance))))
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
	p = (char *)&numx;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p = (char *)&numy;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//send to yourself
	Packet[0] = Packet.length();

	for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
	{
		if (User[i1].Ident && (i1 != CurrSock || !good) && User[i1].current_map == User[CurrSock].current_map)
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

	if (((
			 (numx - User[CurrSock].x <= 0 && numx - (int)User[CurrSock].x > -snap_distance) ||
			 (numx - User[CurrSock].x >= 0 && numx - (int)User[CurrSock].x < snap_distance)) &&
		 ((numy - User[CurrSock].y <= 0 && numy - (int)User[CurrSock].y > -snap_distance) ||
		  (numy - User[CurrSock].y >= 0 && numy - (int)User[CurrSock].y < snap_distance))))
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
	p = (char *)&numx;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//break up Y into 4 bytes
	p = (char *)&numy;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	//send to yourself
	Packet[0] = Packet.length();

	for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
	{
		if (User[i1].Ident && (i1 != CurrSock || !good) && User[i1].current_map == User[CurrSock].current_map)
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
		for (int p = randStart; p < MAX_CLIENTS; p++)
			if (User[p].Ident && User[p].partyStatus == PARTY && User[p].party == party)
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
					if (totalLootGiven >= map[current_map].Enemy[enemy]->lootAmount)
						return;
				}

				//printf("Loot given so far for %s is %i\n", User[p].Nick.c_str(), lootGiven);

				//check if the max items has been given
				if (totalLootGiven >= map[current_map].Enemy[enemy]->lootAmount)
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
		   current_map, enemy, (int)map[current_map].Enemy[enemy]->x, (int)map[current_map].Enemy[enemy]->y, (int)map[current_map].Enemy[enemy]->sx, (int)map[current_map].Enemy[enemy]->sy);
	//set enemy to dead
	map[current_map].Enemy[enemy]->dead = true;

	//set their respawn timer
	map[current_map].Enemy[enemy]->respawn_ticker = Clock();

	//disappear
	map[current_map].Enemy[enemy]->x = -100000;
	map[current_map].Enemy[enemy]->y = -100000;

	printf("Now he's at (%i, %i) and spawns at (%i, %i)\n", (int)map[current_map].Enemy[enemy]->x, (int)map[current_map].Enemy[enemy]->y, (int)map[current_map].Enemy[enemy]->sx, (int)map[current_map].Enemy[enemy]->sy);

	//tell players he disappeared
	std::string Packet = "0";
	Packet += ENEMY_MOVE_STOP;
	Packet += enemy;
	Packet += current_map;
	char *p = (char *)&map[current_map].Enemy[enemy]->x;
	Packet += p[0];
	Packet += p[1];
	Packet += p[2];
	Packet += p[3];
	p = (char *)&map[current_map].Enemy[enemy]->y;
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
	char b1, b2, b3, b4;
	std::string Packet = "0";
	Packet += SPAWN_ITEM;
	Packet += rand_i;
	Packet += current_map;
	Packet += map[current_map].ItemObj[rand_i].itemID;

	p = (char *)&map[current_map].ItemObj[rand_i].x;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	p = (char *)&map[current_map].ItemObj[rand_i].y;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	p = (char *)&map[current_map].ItemObj[rand_i].x_speed;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	p = (char *)&map[current_map].ItemObj[rand_i].y_speed;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
	Packet += b1;
	Packet += b2;
	Packet += b3;
	Packet += b4;

	Packet[0] = Packet.length();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident && User[i].current_map == current_map)
			User[i].SendPacket(Packet);
	}

	printf("Item spawn packet (loot): ");
	for (int pk = 0; pk < Packet.length(); pk++)
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

	lootItem.x = map[current_map].Enemy[enemy]->x + 16 + (32 - Item[lootItem.itemID].w) / 2.0;
	lootItem.y = map[current_map].Enemy[enemy]->y +
				 map[current_map].Enemy[enemy]->y1 + Item[lootItem.itemID].h;
	lootItem.x_speed = (float)(rand() % 50 - 25) / 100.0;
	lootItem.y_speed = (float)(rand() % 300 - 500) / 100.0;

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
				} //end died
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

					for (int pl = 0; pl < MAX_CLIENTS; pl++)
					{
						if (pl != u && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[u].party)
							User[pl].SendPacket(hpPacket);
					}
				} //end lose hp
			}	 //end hit
		}
	} //end loop everyone
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
	char *p = (char *)&User[i].x;
	char b1 = p[0], b2 = p[1], b3 = p[2], b4 = p[3];

	//spit out each of the bytes
	warpPacket += b1;
	warpPacket += b2;
	warpPacket += b3;
	warpPacket += b4;

	p = (char *)&User[i].y;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];

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
	char b1, b2, b3, b4;

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
		p = (char *)&User[CurrSock].xp;
		b1 = p[0];
		b2 = p[1];
		b3 = p[2];
		b4 = p[3];
		Packet += b1;
		Packet += b2;
		Packet += b3;
		Packet += b4;
		Packet[0] = Packet.length();
		User[CurrSock].SendPacket(Packet);

		//max xp
		Packet = "0";
		Packet += STATMAX_XP;
		p = (char *)&User[CurrSock].max_xp;
		b1 = p[0];
		b2 = p[1];
		b3 = p[2];
		b4 = p[3];
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
	} //end gain xp

	//xp
	Packet = "0";
	Packet += STAT_XP;
	p = (char *)&User[CurrSock].xp;
	b1 = p[0];
	b2 = p[1];
	b3 = p[2];
	b4 = p[3];
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
	char *p = (char *)&numx;
	Packet1 += p[0];
	Packet1 += p[1];
	Packet1 += p[2];
	Packet1 += p[3];

	float numy = User[i].y;
	p = (char *)&numy;
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
