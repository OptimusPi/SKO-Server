#include "SKO_Player.h"

void SKO_Player::init()
{
	Sock = new GE_Socket();
	Sock->Connected = false;
	Nick = "";
	Mute = false;
	Moderator = false;
	Ident = false;
	Status = false;
	Que = false;
	Save = false;
	QueTime = 0;
	ground = false;
	tempClanId = "";

//TODO - move these values to a config file.	
	//the tutorial map was added later and the id is 2
	x = 1000;
	y = 0;
	mapId = 2;
	hp = 0;
	max_hp = 10;
	xp = 0;
	max_xp = 10;
	level = 1;
	regen = 0;
	strength = 2;
	defence = 1;
	attacking = false;
	y_speed = 0;
	x_speed = 0;
	que_action = 0;
	stat_points = 0;
	ping = 0;


	attack_ticker = OPI_Clock::milliseconds();
	regen_ticker = OPI_Clock::milliseconds();
	pingTicker = OPI_Clock::milliseconds();
	pingWaiting = false;
	
	//holds all the worn items
	equip[0] = -1;
	equip[1] = -1;
	equip[2] = -1;
		
	for (int i = 0; i < 256; i++)
	{
		inventory[i] = 0;
		bank[i] = 0;
		localTrade[i] = 0;
	}
	
    inventory_index = 0;
    bank_index = 0;
    inventory_order = "";
    //MySQL id
    ID="";
    //trading state
    tradeStatus = 0;
    tradePlayer = 0;
    clanStatus = 0;
    clanPlayer = 0;
    //party
    party = -1;
    partyStatus = 0;
    //current map
    mapId = 0;

    mobKills = 0;
    pvpKills = 0;
}

void SKO_Player::Clear()
{
	if (Sock)
	{ 
		delete Sock;
	}
	init();
}

SKO_Player::~SKO_Player()
{
	Clear();
}

SKO_Player::SKO_Player()
{
	init();
}

//Cosmetic only, take what the clinet gives and store it for now. Wow, how nice!
std::string SKO_Player::getInventoryOrder()
{
 	std::string iorder = "";
	
	//initialize list
	for (int i = 0; i < 48; i++)
		iorder += (char)0;

	//set values of list
	for (int i = 0; i < inventory_order.length() && i < 48; i++)
		iorder[i] = inventory_order[i];

	return iorder;
}

bool SKO_Player::addXP(int xp_in)
{
	printf("SKO_Player::addXp(%i) note max_xp is %i level [%i] ---> ", xp_in, max_xp, level); 

	if (xp_in > 5000)
	{
		printf("*\n*\n\n* ?????\n?????\n????\n????\n");
		return false;
	}

	//add the xp
	xp += xp_in;

	//level up
	if (xp >= max_xp)
	{
		//put remainder to your use
		xp = xp-max_xp;

		//get new stats
		level++;
		max_xp *= 1.5; 
		max_hp += 3;
		hp = max_hp;
		stat_points++;

		printf("level[%i]\n", level);
		//tell the program you leveled
		return true;    
	}
	
	printf("level[%i]\n", level);

	return false;
}