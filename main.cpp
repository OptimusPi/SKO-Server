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
unsigned char hpBar = 25;

//gravity
const float GRAVITY = 0.169;

//holiday events TODO use a config not hard coded magic 
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


void GiveLoot(int enemy, int player);
void Attack(unsigned char userId, float x, float y);
void Jump(unsigned char userId, float x, float y);
void Left(unsigned char userId, float x, float y);
void Right(unsigned char userId, float x, float y);
void Stop(unsigned char userId, float x, float y);
void GiveXP(unsigned char userId, int xp);
void quitParty(unsigned char userId);
void PlayerDamaged(unsigned char userId, unsigned char damage);
void EnemyHit(unsigned char enemyId, unsigned char current_map, unsigned char userId);
void RespawnEnemy(int current_map, int enemy);
void KillEnemy(unsigned char enemyId, unsigned char current_map, unsigned char killerUserId);
void SpawnItem(unsigned char current_map, unsigned char itemObjId, unsigned char itemId, float x, float y, float x_speed, float y_speed);
void DespawnItem(unsigned char itemId, unsigned char current_map);
void PocketItem(unsigned char userId, unsigned char itemID, unsigned int amount);
void EnemyAttack(int i, int current_map);
void Warp(int userId, SKO_Portal portal);
void Respawn(int current_map, int userId);
void SpawnLoot(int current_map, SKO_ItemObject lootItem);

static void *Physics(void *Arg);
static void *TargetLoop(void *Arg);
static void *EnemyLoop(void *arg);
static void *UserLoop(void *arg);

void trim(std::string& s)
{
    size_t first = s.find_first_not_of(' ');
    if (std::string::npos == first)
        return;
    
    size_t last = s.find_last_not_of(' ');
    s = s.substr(first, (last - first + 1));
}
  
std::string lower(std::string myString) 
{ 
  for(int i=0; i < myString.length(); ++i)
    myString[i] = std::tolower(myString[i]);
  
  return myString;
}

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
				network->SendSpawnTarget(i, current_map);
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


			if (map[current_map].Enemy[i]->y > map[current_map].death_pit)
			{
				RespawnEnemy(current_map, i);
				printf("Fix enemy from falling into death pit: map %i Enemy %i spawns at (%i,%i)\n", current_map, i, map[current_map].Enemy[i]->sx, map[current_map].Enemy[i]->sy);
			}

			//check for spawn
			if (map[current_map].Enemy[i]->dead)
			{
				if (Clock() - map[current_map].Enemy[i]->respawn_ticker >= 7000)
				{
					map[current_map].Enemy[i]->Respawn();
					network->SendSpawnEnemy(map[current_map].Enemy[i], i, current_map);
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
						//if it wasn't redundant, reset the ticker
						network->SendEnemyAction(map[current_map].Enemy[i], next_action, i, current_map);
						map[current_map].Enemy[i]->AI_ticker = Clock();
					} //end no spam

				} //end enemy AI

			} //end not dead
		}	 // end enemy for loop

		//npc
		for (unsigned char i = 0; i < map[current_map].num_npcs; i++)
		{

			unsigned char next_action = 0;

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
					//if it wasn't redundant, reset the ticker
					map[current_map].NPC[i]->AI_ticker = Clock();
					network->SendNpcAction(map[current_map].NPC[i], next_action, i, current_map);
				} //end no spam

			} //end enemy AI
		} // end enemy for loop

		//Sleep for 2 ticks
		Sleep(33);
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
	unsigned char itemId = 0;
	float box1_x1, box1_y1, box1_x2, box1_y2;
	unsigned int amt = 0;

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
							float numx = User[i].x;
							float numy = User[i].y;
							switch (User[i].que_action)
							{
							case MOVE_LEFT:
								Left(i, numx, numy);
								printf("\e[0;32mCorrection! que action being sent NOW: MOVE_LEFT\e[m\n");
								break;

							case MOVE_RIGHT:
								Right(i, numx, numy);
								printf("\e[0;32mCorrection! que action being sent NOW: MOVE_RIGHT\e[m\n");
								break;

							case MOVE_JUMP:
								Jump(i, numx, numy); 
								printf("\e[0;32mCorrection! que action being sent NOW: JUMP\e[m\n");
								break;

							case ATTACK:
								printf("\e[0;32mCorrection! que action being sent NOW: ATTACK\e[m\n");

								//do attack actions
								Attack(i, numx, numy);
								break;

							case MOVE_STOP:
								printf("\e[0;32mCorrection! que action being sent NOW: MOVE_STOP\e[m\n");

								//do attack actions
								Stop(i, numx, numy);
								break;

							default:
								printf("\e[0;32mCorrection! que action glitch: %i\e[m\n", User[i].que_action);

								break;
							} //end switch

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

								//cap the hp to their max
								if (User[i].hp > User[i].max_hp)
									User[i].hp = User[i].max_hp;

								//Send changed HP to client player's party members
								unsigned char displayHp = (int)((User[i].hp / (float)User[i].max_hp) * 80);
								for (int pl = 0; pl < MAX_CLIENTS; pl++)
								{
									if (pl != i && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[i].party)
										network->SendBuddyStatHp(pl, i, displayHp);
								}
							}

							//Send changed HP stat to client player
							network->SendStatHp(i, User[i].hp);
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

					itemId = map[current_map].ItemObj[i].itemID;

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
						DespawnItem(i, current_map);
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
								if (User[c].inventory[itemId] == 0 && User[c].inventory_index > 23)
								{
									printf("User %s inventory is full and cant pick up item %i!", User[c].Nick.c_str(), itemId);
								}
								else
								{
									DespawnItem(i, current_map);
									unsigned int amount = map[current_map].ItemObj[i].amount + User[c].inventory[itemId];
									PocketItem(c, itemId, amount);
								} //end else not inventory full (you can pick up)
							}
						}
					}
				}
		} //end timestep

		Sleep(1);
	} //end while true
}


/* perform player actions */
void Attack(unsigned char userId, float numx, float numy)
{
	//set the current map
	int current_map = User[userId].current_map;

	//Que next action
	if (User[userId].attacking)
	{
		printf("%s tried to attack while attacking...\7\7\n", User[userId].Nick.c_str());
		User[userId].que_action = ATTACK;
		return;
	}

	//Cannot attack while mid-air
	if (!User[userId].ground)
	{
		printf("%s tried to attack while jumping...\7\7\n", User[userId].Nick.c_str());
		return;
	}

	printf("%s is attacking!\n", User[userId].Nick.c_str());
	User[userId].attacking = true;
	User[userId].attack_ticker = Clock();
	User[userId].x_speed = 0;

	bool good = false;

	if (((
			 (numx - User[userId].x <= 0 && numx - User[userId].x > -snap_distance) ||
			 (numx - User[userId].x >= 0 && numx - User[userId].x < snap_distance)) &&
		 ((numy - User[userId].y <= 0 && numy - User[userId].y > -snap_distance) ||
		  (numy - User[userId].y >= 0 && numy - User[userId].y < snap_distance))))
	{
		//update server variables
		User[userId].x = numx;
		User[userId].y = numy;
		good = true;
	}
	else
	{
		printf("ATTACK FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[userId].x, User[userId].y);

		//correction packet
		numx = (int)User[userId].x;
		numy = (int)User[userId].y;
	}

	network->SendPlayerAction(true, ATTACK, userId, numx, numy);

	//loop all players to check for collisions
	printf("Looping all players.\n");
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!User[i].Ident)
			continue;

		bool inParty = false;
		if (User[i].partyStatus == PARTY && User[i].party == User[userId].party)
			inParty = true;

		if (i != userId && !inParty && User[i].current_map == User[userId].current_map)
		{
			//check for collision
			//horizontal
			float x_dist, y_dist;
			if (User[userId].facing_right)
				x_dist = User[i].x - User[userId].x;
			else
				x_dist = User[userId].x - User[i].x;

			//vertical
			y_dist = User[i].y - User[userId].y;

			//how far away you can hit, plus weapon
			int max_dist = 30;
			int weap = User[userId].equip[0];
			if (weap != 0)
			{
				max_dist += Item[weap].reach;
			}

			//attack events
			if (x_dist > 0 && x_dist < max_dist && y_dist > -60 && y_dist < 30)
			{
				int dam = User[userId].strength - User[i].defence;
				int hat = User[userId].equip[1];

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

				
				if (User[i].hp <= dam)
				{
					printf("Respawn, x:%.2f y:%.2f\n", User[i].x, User[i].y);
					Respawn(User[i].current_map, i);
					printf("Respawned, x:%.2f y:%.2f\n", User[i].x, User[i].y);

				} //end died
				else
				{
					PlayerDamaged(i, dam);
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
		if (User[userId].facing_right)
			x_dist = map[current_map].Target[i].x - (User[userId].x + 38);
		else
			x_dist = (User[userId].x + 25) - (map[current_map].Target[i].x + map[current_map].Target[i].w);
		//vertical
		y_dist = map[current_map].Target[i].y - User[userId].y;

		//how far away you can hit, plus weapon
		int max_dist = 30;
		int weap = User[userId].equip[0];
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
			network->SendDespawnTarget(i, current_map);

			//spawn some loot
			if (map[current_map].Target[i].loot >= 0)
			{
				SKO_ItemObject lootItem = map[current_map].Target[i].getLootItem();
				lootItem.owner = userId;
				SpawnLoot(current_map, lootItem);
			}

			//give player some XP
			GiveXP(userId, 1);
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
			if (map[current_map].Enemy[i]->dibsPlayer == userId)
				partyBlocked = false;

			//but if you are part of the party it isn't blocked
			if (User[userId].partyStatus == PARTY)
			{
				if (User[map[current_map].Enemy[i]->dibsPlayer].partyStatus == PARTY &&
					User[map[current_map].Enemy[i]->dibsPlayer].party == User[userId].party)
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
			if (User[userId].facing_right)
				x_dist = map[current_map].Enemy[i]->x - User[userId].x;
			else
				x_dist = User[userId].x - map[current_map].Enemy[i]->x;

			//vertical
			y_dist = map[current_map].Enemy[i]->y - User[userId].y;
			//how far away you can hit, plus weapon
			int max_dist = 30;
			int weap = User[userId].equip[0];
			if (weap != 0)
			{
				max_dist += Item[weap].reach;
			}

			//attack events
			if (x_dist >= 0 && x_dist < max_dist && y_dist > -60 && y_dist < 60)
			{
				EnemyHit(i, current_map, userId);
			}
		} //end loop if you hit enemys
	} // end if enemy is dead
}

void Stop(unsigned char userId, float numx, float numy)
{
	//you cant move when attacking!
	if (User[userId].attacking)
	{
		User[userId].que_action = MOVE_STOP;

		return;
	}

	//physics
	User[userId].x_speed = 0;

	bool good = false;

	if (((
			 (numx - User[userId].x <= 0 && numx - User[userId].x > -snap_distance) ||
			 (numx - User[userId].x >= 0 && numx - User[userId].x < snap_distance)) &&
		 ((numy - User[userId].y <= 0 && numy - User[userId].y > -snap_distance) ||
		  (numy - User[userId].y >= 0 && numy - User[userId].y < snap_distance))))
	{
		//update server variables
		User[userId].x = numx;
		User[userId].y = numy;
	}
	else
	{
		printf("MOVE STOP FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[userId].x, User[userId].y);

		//correction packet
		numx = (int)User[userId].x;
		numy = (int)User[userId].y;
	}

	bool isCorrection = !good;
	network->SendPlayerAction(isCorrection, MOVE_STOP, userId, numx, numy);
}

void Right(unsigned char userId, float numx, float numy)
{
	//you cant move when attacking!
	if (User[userId].attacking)
	{
		User[userId].que_action = MOVE_RIGHT;
		return;
	}

	// Physics
	User[userId].facing_right = true;
	User[userId].x_speed = 2;

	bool isCorrection = false;

	if (((
		(numx - User[userId].x <= 0 && numx - User[userId].x > -snap_distance) ||
		(numx - User[userId].x >= 0 && numx - User[userId].x < snap_distance)) &&
		((numy - User[userId].y <= 0 && numy - User[userId].y > -snap_distance) ||
		(numy - User[userId].y >= 0 && numy - User[userId].y < snap_distance))))
	{
		//update server variables
		User[userId].x = numx;
		User[userId].y = numy;
	}
	else
	{
		printf("MOVE RIGHT FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[userId].x, User[userId].y);

		//correction packet
		numx = (int)User[userId].x;
		numy = (int)User[userId].y;
		isCorrection = true;
	}

	network->SendPlayerAction(isCorrection, MOVE_RIGHT, userId, numx, numy);
}

void Left(unsigned char userId, float numx, float numy)
{
	//you cant move when attacking!
	if (User[userId].attacking)
	{
		User[userId].que_action = MOVE_LEFT;
		return;
	}

	//physics
	User[userId].facing_right = false;
	User[userId].x_speed = -2;

	// Determine if the client is lagged behind the server physics, and correct them.
	bool isCorrection = false;
	if (((
			 (numx - User[userId].x <= 0 && numx - User[userId].x > -snap_distance) ||
			 (numx - User[userId].x >= 0 && numx - User[userId].x < snap_distance)) &&
		 ((numy - User[userId].y <= 0 && numy - User[userId].y > -snap_distance) ||
		  (numy - User[userId].y >= 0 && numy - User[userId].y < snap_distance))))
	{
		//update server variables
		User[userId].x = numx;
		User[userId].y = numy;
	}
	else
	{
		printf("MOVE RIGHT FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[userId].x, User[userId].y);

		//correction packet
		numx = (int)User[userId].x;
		numy = (int)User[userId].y;
		isCorrection = true;
	}

	network->SendPlayerAction(isCorrection, MOVE_LEFT, userId, numx, numy);
}

void Jump(unsigned char userId, float numx, float numy)
{
	//you cant move when attacking!
	if (User[userId].attacking)
	{
		User[userId].que_action = MOVE_JUMP;
		return;
	}
	
	// Physics
	User[userId].y_speed = -6;

	// Determine if the client is lagged behind the server physics, and correct them.
	bool isCorrection = false;
	if (((
			 (numx - User[userId].x <= 0 && numx - (int)User[userId].x > -snap_distance) ||
			 (numx - User[userId].x >= 0 && numx - (int)User[userId].x < snap_distance)) &&
		 ((numy - User[userId].y <= 0 && numy - (int)User[userId].y > -snap_distance) ||
		  (numy - User[userId].y >= 0 && numy - (int)User[userId].y < snap_distance))))
	{
		//update server variables
		User[userId].x = numx;
		User[userId].y = numy;
	}
	else
	{
		printf("MOVE JUMP FAIL: (%.2f,%.2f)\n", numx, numy);
		printf("ACTUAL VAL: (%.2f,%.2f)\n\n", User[userId].x, User[userId].y);

		//correction packet
		numx = (int)User[userId].x;
		numy = (int)User[userId].y;
		isCorrection = true;
	}

	network->SendPlayerAction(isCorrection, MOVE_JUMP, userId, numx, numy);
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

void RespawnEnemy(int current_map, int enemy)
{
	printf("Map %i Enemy %i killed at (%i, %i) spawns at (%i, %i)\n",
		   current_map, enemy, (int)map[current_map].Enemy[enemy]->x, (int)map[current_map].Enemy[enemy]->y, (int)map[current_map].Enemy[enemy]->sx, (int)map[current_map].Enemy[enemy]->sy);
	//set enemy to dead
	map[current_map].Enemy[enemy]->dead = true;

	//set their respawn timer
	map[current_map].Enemy[enemy]->respawn_ticker = Clock();

	//disappear TODO - remove this hack add enemy killed pakcet for client & server
	map[current_map].Enemy[enemy]->x = -100000;
	map[current_map].Enemy[enemy]->y = -100000;

	network->SendEnemyAction(map[current_map].Enemy[enemy], ENEMY_MOVE_STOP, enemy, current_map);
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

	
	unsigned char itemObjId = rand_i;
	unsigned char itemId = map[current_map].ItemObj[rand_i].itemID;
	float x = map[current_map].ItemObj[rand_i].x;
	float y = map[current_map].ItemObj[rand_i].y;
	float x_speed = map[current_map].ItemObj[rand_i].x_speed;
	float y_speed = map[current_map].ItemObj[rand_i].y_speed;
	SpawnItem(current_map, itemObjId, itemId, x, y, x_speed, y_speed);
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
	for (unsigned char u = 0; u < MAX_CLIENTS; u++)
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

				if (User[u].hp <= dam)
				{
					printf("Respawning, x:%i y:%i\n", (int)User[u].x, (int)User[u].y);

					Respawn(User[u].current_map, u);

					printf("Respawned!, x:%i y:%i\n", (int)User[u].x, (int)User[u].y);
				} //end died
				else
				{
					PlayerDamaged(u, dam);
				} //end lose hp
			}	 //end hit
		}
	} //end loop everyone
}

void Warp(int userId, SKO_Portal portal)
{
	//move the player to this spot
	unsigned char oldMap = User[userId].current_map;
	User[userId].current_map = portal.map;
	User[userId].x = portal.spawn_x;
	User[userId].y = portal.spawn_y;

	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && (User[c].current_map == User[userId].current_map || User[c].current_map == oldMap))
			network->SendWarpPlayer(c, userId, User[userId].current_map, User[userId].x, User[userId].y);
	}

	//TODO show all items and targets on warp?
}

void GiveXP(unsigned char userId, int xp)
{
	if (xp < 0 || xp > 10000)
	{
		printf(kRed "[FATAL] Tried to give weird xp: %i\n" kNormal, xp);
		return;
	}

	//add xp
	if (User[userId].addXP(xp))
	{
		//you leveled up, send all stats
		//level
		network->SendStatLevel(userId, User[userId].level);

		//hp max
		network->SendStatXpMax(userId, User[userId].max_xp);

		//hp
		network->SendStatHp(userId, User[userId].hp);

		//stat_points
		network->SendStatPoints(userId, User[userId].stat_points);

		//xp
		network->SendStatXp(userId, User[userId].xp);

		//max xp
		network->SendStatXpMax(userId, User[userId].max_xp);

		//tell party members my new stats - TODO - make these cosmetic stat bar calls into a function
		unsigned char displayXp = (int)((User[userId].xp / (float)User[userId].max_xp) * 80);
		unsigned char displayHp = (int)((User[userId].hp / (float)User[userId].max_hp) * 80);

		for (int pl = 0; pl < MAX_CLIENTS; pl++)
		{
			if (pl != userId && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[userId].party)
			{
				network->SendBuddyStatHp(pl, userId, User[userId].hp);
				network->SendBuddyStatXp(pl, userId, User[userId].xp);
				network->SendBuddyStatLevel(pl, userId, User[userId].level);
			}
		}
		return;
	} //end gain xp

	//xp
	network->SendStatXp(userId, User[userId].xp);

	//tell party members my xp
	unsigned char displayXp = (int)((User[userId].xp / (float)User[userId].max_xp) * 80);
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (c != userId && User[c].Ident && User[c].partyStatus == PARTY && User[c].party == User[userId].party)
			network->SendBuddyStatXp(c, userId, displayXp);
	}
}

void Respawn(int current_map, int userId)
{
	//place their coords
	User[userId].x = map[current_map].spawn_x;
	User[userId].y = map[current_map].spawn_y;
	User[userId].y_speed = 0;
	User[userId].hp = User[userId].max_hp;

	//tell everyone they respawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].current_map == current_map)
			network->SendPlayerRespawn(c, userId, User[userId].x, User[userId].y);
	}
}

void quitParty(unsigned char userId)
{
	printf("quitParty(%s)\n", User[userId].Nick.c_str());
	if (User[userId].partyStatus == 0 || User[userId].party < 0)
	{
		printf("User %s is in no party.\n", User[userId].Nick.c_str());
	}
	else
	{
		printf("User %s has party status %i and in party %i\n",
			   User[userId].Nick.c_str(),
			   User[userId].partyStatus,
			   User[userId].party);
	}
	int partyToLeave = User[userId].party;

	User[userId].party = -1;
	User[userId].partyStatus = 0;

	printf("telling everyone that %s left his party.\n", User[userId].Nick.c_str());
	//tell everyone
	for (int c = 0; c < MAX_CLIENTS; c++)
		if (User[c].Ident)
			network->SendQuitParty(c, userId);

	//quit the inviter if there is only one
	unsigned char partyHostUserId;
	int count = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (User[i].Ident && User[i].party == partyToLeave)
		{
			count++;
			partyHostUserId = i;
		}

	// If there is now only a party of 1 left, make them quit their party as well
	if (count == 1)
	{
		User[partyHostUserId].party = -1;
		User[partyHostUserId].partyStatus = 0;
		network->SendQuitParty(partyHostUserId, partyHostUserId);
	}
}

void DespawnItem(unsigned char itemObjId, unsigned char current_map)
{
	//remove from map
	for (int userId = 0; userId < MAX_CLIENTS; userId++)
	{
		if (User[userId].Ident && User[userId].current_map == current_map)
			network->SendDespawnItem(userId, itemObjId, current_map);
	} 
}

void SpawnItem(unsigned char current_map, unsigned char itemObjId, unsigned char itemId, float x, float y, float x_speed, float y_speed)
{
	//remove from map
	for (int userId = 0; userId < MAX_CLIENTS; userId++)
	{
		if (User[userId].Ident && User[userId].current_map == current_map)
			network->SendSpawnItem(userId, itemObjId, current_map, itemId, x, y, x_speed, y_speed);
	}
} 

void PocketItem(unsigned char userId, unsigned char itemId, unsigned int amount)
{
	// If it's a new item, increase the count of held items for the player
	if (User[userId].inventory[itemId] == 0 && amount > 0)
			User[userId].inventory_index++;

	// If the item was removed fromt he player's inventory, decrease inventory index
	if (User[userId].inventory[itemId] > 0 && amount == 0) 
			User[userId].inventory_index--;

	//Set the user inventory amount
	User[userId].inventory[itemId] = amount;

	//put in client player's inventory
	network->SendPocketItem(userId, itemId, amount);
}

void PlayerDamaged(unsigned char userId, unsigned char damage)
{
	//Notify client player about their new HP
	User[userId].hp -= damage;
	network->SendStatHp(userId, User[userId].hp);

	//party hp notification
	unsigned char displayHp = (int)((User[userId].hp / (float)User[userId].max_hp) * 80);
	
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].current_map == User[c].current_map)
		{
			network->SendPlayerHit(c, userId);
			if (userId != c && User[c].partyStatus == PARTY && User[c].party == User[userId].party)
				network->SendBuddyStatHp(c, userId, displayHp);
		}
	}
}

unsigned int GetTotalStrength(unsigned char userId)
{
	unsigned int totalStrength =  User[userId].strength;
	unsigned char weapon = User[userId].equip[0];
	unsigned char hat = User[userId].equip[1];
	unsigned char trophy = User[userId].equip[2];
	

	//Add strength of all equipment
	if (weapon)
		totalStrength += Item[weapon].str;
	if (hat)
		totalStrength += Item[hat].str;
	if (trophy)
		totalStrength += Item[trophy].str;
	
	return totalStrength;
}

unsigned int GetTotalDefence(unsigned char userId)
{
	unsigned int totalDefence =  User[userId].defence;
	unsigned char weapon = User[userId].equip[0];
	unsigned char hat = User[userId].equip[1];
	unsigned char trophy = User[userId].equip[2];
	

	//Add strength of all equipment
	if (weapon)
		totalDefence += Item[weapon].def;
	if (hat)
		totalDefence += Item[hat].def;
	if (trophy)
		totalDefence += Item[trophy].def;
	
	return totalDefence;
}

void KillEnemy(unsigned char enemyId, unsigned char current_map, unsigned char killerUserId)
{
	//Divide Loot between a party
	if (User[killerUserId].partyStatus == PARTY)
	{
		DivideLoot(enemyId, User[killerUserId].party);
	}
	else //not in a party, they get all the loots!
	{
		for (int it = 0; it < map[current_map].Enemy[enemyId]->lootAmount; it++)
			GiveLoot(enemyId, killerUserId);
	}

	//kill the enemy after loot is dropped !! :P
	RespawnEnemy(current_map, enemyId);

	float bonus_clan_xp = 0;
	float splitXP = map[current_map].Enemy[enemyId]->xp;

	//if in a clan add bonus xp - TODO: remove "(" versus "[" logic for clans
	if (User[killerUserId].Clan[0] == '[')
	{
		bonus_clan_xp = map[current_map].Enemy[enemyId]->xp * 0.10; //10% bonus
	}
	printf("User is in a clan so bonus xp for them! %i\n", (int)bonus_clan_xp);
	if (User[killerUserId].partyStatus == PARTY)
	{
		//give bonus XP for party
		float bonusXP = splitXP * 0.10;
		float totalDamage = 0;

		//find total damage from all party members
		for (int p = 0; p < MAX_CLIENTS; p++)
			if (User[p].Ident && User[p].partyStatus == PARTY && User[p].party == User[killerUserId].party)
				totalDamage += map[current_map].Enemy[enemyId]->dibsDamage[p];

		//find total damage from all party members
		for (int p = 0; p < MAX_CLIENTS; p++)
			if (User[p].Ident && User[p].partyStatus == PARTY && User[p].party == User[killerUserId].party)
			{
				//base XP they deserve
				float gimmieXP = 0;
				float pl_dmg = (float)map[current_map].Enemy[enemyId]->dibsDamage[p];

				if (pl_dmg > 0)
					gimmieXP = pl_dmg / totalDamage;

				printf("Player %s dealt %.2f percent of the damage or %i HP out of %i HP done by the party.\n",
						User[p].Nick.c_str(), gimmieXP * 100, map[current_map].Enemy[enemyId]->dibsDamage[p], (int)totalDamage);
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
				printf("[PARTY] Player %s was given %i XP points\n", User[p].Nick.c_str(), pXpGiven);
			}
	} //end split xp with party
	else
	{
		//just give myself xp
		GiveXP(killerUserId, map[current_map].Enemy[enemyId]->xp + bonus_clan_xp);
		printf("[SOLO] Player %s was given %i XP points\n", User[killerUserId].Nick.c_str(), map[current_map].Enemy[enemyId]->xp);
	}
}

void EnemyHit(unsigned char enemyId, unsigned char current_map, unsigned char userId)
{
	//Sum of all strength a player has, including equipment
	int strength = GetTotalStrength(userId);
	int damage = strength - map[current_map].Enemy[enemyId]->defence;

	if (damage <= 0)
		return;

	//keep track of damage
	map[current_map].Enemy[enemyId]->dibsDamage[userId] += damage;
	map[current_map].Enemy[enemyId]->dibsPlayer = userId;
	map[current_map].Enemy[enemyId]->dibsTicker = Clock();

	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].current_map == current_map)
			network->SendEnemyHit(c, enemyId);
	}

	if (map[current_map].Enemy[enemyId]->hp <= damage)
	{
		//keep track of damage but remove overflow
		int extraHpHurt = (damage - map[current_map].Enemy[enemyId]->hp);
		map[current_map].Enemy[enemyId]->dibsDamage[userId] -= extraHpHurt;

		KillEnemy(enemyId, current_map, userId);
	} //end died
	else
	{
		map[current_map].Enemy[enemyId]->hp -= damage;

		//enemy health bars
		unsigned char displayHp = (unsigned char)((float)map[current_map].Enemy[enemyId]->hp / map[current_map].Enemy[enemyId]->hp_max * hpBar);

		for (int c = 0; c < MAX_CLIENTS; c++)
		{
			if (User[c].Ident)
				network->SendEnemyHp(c, enemyId, current_map, displayHp);
		}
	}
}