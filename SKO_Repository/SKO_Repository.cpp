#include "SKO_Repository.h"
#include "../SKO_Utilities/OPI_Hasher.h"

//TODO remove this when possible
#include "../Global.h"

SKO_Repository::SKO_Repository() 
{
	this->database = new OPI_MYSQL();
}

std::string SKO_Repository::Connect(std::string hostname, std::string schema, int port, std::string username, std::string password)
{
	if (!this->database->connect(hostname, schema, port, username, password))
	{
		printf("Could not connect to MYSQL database.\n");
		return "error";
	}

	return "success";
}

std::string SKO_Repository::GetClanTag(std::string username, std::string defaultValue)
{
	std::string str_SQL = "SELECT clan.name FROM player INNER JOIN clan ON clan.id = player.clan_id WHERE player.username LIKE '";
	str_SQL += database->clean(username);
	str_SQL += "'";

	database->query(str_SQL);

	std::string clanTag = defaultValue;
	if (database->count())
	{
		database->nextRow();
		clanTag = "{";
		clanTag += database->getString(0);
		clanTag += "}";
	}

	return clanTag;
}

// TODO- SQL to stored procedures
int SKO_Repository::loginPlayer(std::string username, std::string password)
{
	bool mute = false;
	std::string player_id = "", player_salt = "";
	std::string sql = "SELECT * FROM player WHERE username like '";
	sql += database->clean(username);
	sql += "'";
	database->query(sql);

	if (database->count())
	{
		database->nextRow();
		//get the id for loading purposes
		player_id = database->getString(0);
		//get the salt for login purposes
		player_salt = database->getString(26);
	}
	else //could not open file!
	{
		//user does not exist
		return 4;
	}

	sql = "SELECT * FROM ban WHERE player_id like '";
	sql += player_id;
	sql += "'";
	database->query(sql);

	if (database->count())
	{
		//user is banned
		return 3;
	}

	sql = "SELECT * FROM mute WHERE player_id like '";
	sql += player_id;
	sql += "'";
	database->query(sql);

	if (database->count())
	{
		//user is mute
		mute = true;
	}

	sql = "SELECT * FROM player WHERE username like '";
	sql += database->clean(username);
	sql += "' AND password like '";
	sql += database->clean(OPI_Hasher::hash(password, player_salt));
	sql += "'";
	database->query(sql);

	if (!database->count())
	{
		printf("[%s] had no result.\n", sql.c_str());
		//wrong credentials
		return 1;
	}

	//mute or not
	if (mute)
		return 5;

	//server shutdown command
	if (username == "SHUTDOWN")
		return 2;

	//not mute, logged in
	return 0;
}

int SKO_Repository::loadPlayerData(unsigned char userId)
{
	int returnVals = 0;

	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(User[userId].Nick);
	sql += "'";

	for (int i = 0; i < 256; i++)
	{
		User[userId].inventory[i] = 0;
		User[userId].bank[i] = 0;
	}
	User[userId].inventory_index = 0;
	User[userId].bank_index = 0;

	database->query(sql);

	if (database->count())
	{
		database->nextRow();
		std::string player_id = database->getString(0);
		User[userId].ID = player_id;
		User[userId].level = database->getInt(3);
		User[userId].x = database->getFloat(4);
		User[userId].y = database->getFloat(5);
		User[userId].xp = database->getInt(6);
		User[userId].hp = database->getInt(7);
		User[userId].strength = database->getInt(8);
		User[userId].defense = database->getInt(9);
		User[userId].max_xp = database->getInt(10);
		User[userId].max_hp = database->getInt(11);
		User[userId].y_speed = database->getFloat(12);
		User[userId].x_speed = 0;
		User[userId].stat_points = database->getInt(14);
		User[userId].regen = database->getInt(15);
		User[userId].facing_right = (bool)(0 + database->getInt(16));
		User[userId].equip[0] = database->getInt(17);
		User[userId].equip[1] = database->getInt(18);
		User[userId].equip[2] = database->getInt(19);

		//20 = Operating System, we don't care

		//playtime stats
		User[userId].minutesPlayed = database->getInt(21);
		//what map is the current user on
		User[userId].mapId = database->getInt(22);

		User[userId].inventory_order = base64_decode(database->getString(23));

		//playtime stats
		User[userId].loginTime = OPI_Clock::milliseconds();

		sql = "SELECT * FROM inventory WHERE player_id LIKE '";
		sql += database->clean(player_id);
		sql += "'";

		returnVals += database->query(sql);

		if (database->count())
		{
			database->nextRow();

			for (int i = 0; i < NUM_ITEMS; i++)
			{
				//grab an item from the row
				User[userId].inventory[i] = database->getInt(i + 1);
			}
		}
		else
		{
			//fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
			return 1;
		}

		printf("After loading, inventory_index is: %i\n", User[userId].inventory_index);

		sql = "SELECT * FROM bank WHERE player_id LIKE '";
		sql += database->clean(player_id);
		sql += "'";

		returnVals += database->query(sql);

		if (database->count())
		{
			database->nextRow();
			printf("loading bank...\n");
			for (int i = 0; i < NUM_ITEMS; i++)
			{
				//grab an item from the row
				User[userId].bank[i] = database->getInt(i + 1);
			}
		}
		else
		{
			// Since the data could not load properly,
			// Do not let the player proceed in order to prevent data loss upon game save.
			return 1;
		}
	}
	else
	{
		//fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
		return 1;
	}

	sql = "SELECT * FROM moderator WHERE player_id LIKE '";
	sql += database->clean(User[userId].ID);
	sql += "'";

	returnVals += database->query(sql);

	if (database->count())
	{
		User[userId].Moderator = true;
	}
	else
	{
		User[userId].Moderator = false;
	}

	return returnVals;
}

void SKO_Repository::LogPlayerAddress(unsigned char userId)
{ 
		//log ip
	printf("i.p. logging...\n");
	std::string player_ip = User[userId].socket->Get_IP();
	std::string player_id = User[userId].ID;
	std::string sql = "INSERT INTO ip_log (player_id, ip) VALUES('";
	sql += database->clean(player_id);
	sql += "', '";
	sql += database->clean(player_ip);
	sql += "')";

	printf("%s", sql.c_str());;
	database->query(sql, true);
	printf("Logged i.p. [%s]\n", player_ip.c_str());
}

bool SKO_Repository::IsAddressBanned(std::string ipAddress)
{
	std::string sql = "SELECT * FROM ip_ban WHERE ip like '";
	sql += database->clean(ipAddress);
	sql += "'";
	database->query(sql);

	if (database->count())
	{
		return true;
	}

	return false;
}

int SKO_Repository::createPlayer(std::string Username, std::string Password, std::string IP)
{
	printf("create_profile()\n");
	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(Username);
	sql += "'";
	printf("%s", sql.c_str());;
	database->query(sql);

	if (database->count())
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
	sql += database->clean(IP);
	sql += "'";
	printf("%s", sql.c_str());;
	database->query(sql);

	if (database->count())
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
	database->query(sql);
	database->nextRow();
	std::string player_salt = database->getString(0);

	sql = "INSERT INTO player (username, password, level, facing_right, x, y, hp, str, def, xp_max, hp_max, current_map, salt) VALUES('";
	sql += database->clean(Username);
	sql += "', '";
	sql += database->clean(OPI_Hasher::hash(Password, player_salt));
	sql += "', '1', b'1', '314', '300', '10', '2', '1', '10', '10', '2', '";
	sql += database->clean(player_salt);
	sql += "')";
	printf("%s", sql.c_str());;
	database->query(sql);

	//make sure it worked
	sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(Username);
	sql += "'";
	printf("%s", sql.c_str());;
	database->query(sql);

	if (database->count())
	{
		printf("well, user exists. good.\n");
		//get results form the query. Save id.
		database->nextRow();
		std::string player_id = database->getString(0);

		//make inventory blank for them.
		sql = "INSERT INTO inventory (player_id) VALUES('";
		sql += database->clean(player_id);
		sql += "')";
		printf("%s", sql.c_str());;
		database->query(sql);

		//make inventory blank for them.
		printf("%s", sql.c_str());;
		sql = "INSERT INTO bank (player_id) VALUES('";
		sql += database->clean(player_id);
		sql += "')";
		database->query(sql);
	}
	else
	{
		return 3;
	}

	return 0;
}

int SKO_Repository::mutePlayer(unsigned char userId, std::string username, std::string reason, int flag)
{
	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(username);
	sql += "'";
	printf("%s", sql.c_str());;
	database->query(sql);

	//see if the noob exists
	if (database->count())
	{
		//get noob id
		database->nextRow();
		std::string player_id = database->getString(0);

		//see if the noob is a mod
		sql = "SELECT * FROM moderator WHERE player_id like '";
		sql += database->clean(player_id);
		sql += "'";
		printf("%s", sql.c_str());;
		database->query(sql);

		if (database->count())
		{
			//Cannot mute moderators
			return 2;
		}
		else //the person is not a mod, mute/unmute them
		{
			//mute
			if (flag == 1)
			{
				// TODO insert reason why the player was muted
				sql = "INSERT INTO mute (player_id, muted_by) VALUES('";
				sql += database->clean(player_id);
				sql += "', '";
				sql += database->clean(User[userId].Nick);
				sql += "')";

				printf("%s", sql.c_str());;
				database->query(sql);

				return 0;
			}
			//unmute
			if (flag == 0)
			{
				sql = "DELETE FROM mute WHERE player_id LIKE '";
				sql += database->clean(player_id);
				sql += "'";
				database->query(sql);

				return 0;
			}
		}
	}
	else //the account does not exist
	{
		return 1;
	}

	return 0;
}

int SKO_Repository::banPlayer(unsigned char userId, std::string Username, std::string Reason, int flag)
{
	//TODO, if unban, remove ipbans from this player as well.
	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(Username);
	sql += "'";
	printf("%s", sql.c_str());;
	database->query(sql);

	//see if the noob exists
	if (database->count())
	{
		//get noob id
		database->nextRow();
		std::string player_id = database->getString(0);

		//see if the noob is a mod
		sql = "SELECT * FROM moderator WHERE player_id like '";
		sql += database->clean(player_id);
		sql += "'";
		printf("%s", sql.c_str());;
		database->query(sql);

		if (database->count())
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
				sql += database->clean(player_id);
				sql += "', '";
				sql += database->clean(User[userId].Nick);
				sql += "', '";
				sql += database->clean(Reason);
				sql += "')";

				printf("%s", sql.c_str());;
				database->query(sql);

				return 0;
			}
			//unban
			if (flag == 0)
			{

				sql = "DELETE FROM ban WHERE player_id LIKE '";
				sql += database->clean(player_id);
				sql += "'";
				database->query(sql);

				return 0;
			}
		}
	}
	else //the account does not exist
	{
		return 1;
	}

	return 0;
}

void SKO_Repository::saveServerStatus(unsigned int playersLinux, unsigned int playersWindows, unsigned int playersMac, unsigned int averagePing)
{
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
	statusQuery << "');";

	database->query(statusQuery.str(), false);
}


int SKO_Repository::banIP(std::string moderatorName, std::string IP, std::string Reason)
{
	//get noob id
	database->nextRow();
	std::string player_id = database->getString(0);

	std::string sql = "INSERT INTO ip_ban (ip, banned_by, ban_reason) VALUES('";
	sql += database->clean(IP);
	sql += "', '";
	sql += database->clean(moderatorName);
	sql += "', '";
	sql += database->clean(Reason);
	sql += "')";

	printf("%s", sql.c_str());;
	database->query(sql);

	return 0;
}

std::string SKO_Repository::getIP(std::string username)
{
	//get noob id
	std::string sql = "SELECT * FROM player WHERE username like '";
	sql += database->clean(username);
	sql += "'";
	printf("%s", sql.c_str());;
	database->query(sql);

	if (database->count())
	{
		//get noob id
		database->nextRow();
		std::string noob_id = database->getString(0);

		//get IP
		std::string IP = "";
		sql = "SELECT * FROM ip_log WHERE player_id like '";
		sql += database->clean(noob_id);
		sql += "' ORDER BY idip_log DESC"; //latest ip logged
		printf("%s", sql.c_str());;
		database->query(sql);

		if (database->count())
		{
			//get noob IP
			database->nextRow();
			IP = database->getString(2);
			return IP;
		}

		return "Could not get IP of " + username + ": ip address does not exist.";
	}

	return "Could not get IP of " + username + ": player does not exist.";
}

std::string SKO_Repository::getOwnerClanId(std::string clanOwner)
{
	//make sure they are clan owner
	std::string clanId = "";
	std::string strl_SQL = "";
	strl_SQL += "SELECT COUNT(clan.id) FROM clan INNER JOIN player ON clan.owner_id = player.id";
	strl_SQL += " WHERE player.username LIKE '";
	strl_SQL += clanOwner;
	strl_SQL += "'";

	database->query(strl_SQL);

	if (database->count())
	{
		database->nextRow();
		//if you own the clan...
		if (database->getInt(0))
		{
			//get clan id
			std::string strl_SQL = "";
			strl_SQL += "SELECT clan_id FROM player";
			strl_SQL += " WHERE username LIKE '";
			strl_SQL += clanOwner;
			strl_SQL += "'";

			database->query(strl_SQL);
			if (database->count())
			{
				database->nextRow();
				clanId = database->getString(0);
			}
		}
	}
	return clanId;
}

void SKO_Repository::setClanId(std::string username, std::string clanId)
{
	//set the clan of the player in the DB
	std::string strl_SQL = "UPDATE player SET clan_id = ";
	strl_SQL += clanId;
	strl_SQL += " WHERE username LIKE '";
	strl_SQL += username;
	strl_SQL += "'";

	database->query(strl_SQL);
}

int SKO_Repository::createClan(std::string username, std::string clanTag)
{
	//find out if it exists already...
	std::string sql = "SELECT COUNT(*) FROM clan WHERE clan.name LIKE '";
	sql += database->clean(clanTag);
	sql += "'";

	database->query(sql);
	if (database->count())
	{
		database->nextRow();
		if (database->getInt(0) > 0)
		{
			printf("clan already exists...%s\n", clanTag.c_str());
			return 1;
		}
	}

	sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(username);
	sql += "'";

	database->query(sql);
	if (database->count())
	{
		database->nextRow();
		//get the id for loading purposes
		std::string creatorId = database->getString(0);

		//update the clan tables!
		std::string sql = "";
		sql += "INSERT INTO clan (name, owner_id) VALUES ('";
		sql += database->clean(clanTag);
		sql += "', ";
		sql += creatorId;
		sql += ")";
		database->query(sql, true);

		//set player clan id
		sql = "";
		sql += "UPDATE player SET player.clan_id = (SELECT clan.id FROM clan WHERE clan.name LIKE '";
		sql += database->clean(clanTag);
		sql += "') WHERE player.id LIKE '";
		sql += creatorId;
		sql += "'";
		database->query(sql);
	}

	return 0;
}

bool SKO_Repository::savePlayerData(unsigned char userId)
{
	std::string username = User[userId].Nick;
	printf("Saving username: %s\n", username.c_str());
	if (username.length() == 0)
		return false;
 
    std::string player_id = User[userId].ID;
		
	//save inventory
	std::ostringstream sql;
	sql << "UPDATE inventory SET";
	for (int itm = 0; itm < NUM_ITEMS; itm++)
	{
		sql << " ITEM_";
		sql << itm;
		sql << "=";
		sql << (int)User[userId].inventory[itm];
		
		if (itm+1 < NUM_ITEMS)
		sql << ", ";
	}
	sql << " WHERE player_id LIKE '";
	sql << database->clean(player_id);
	sql << "'";

	database->query(sql.str());
		
		
		
	//save inventory
	sql.str("");
	sql.clear();
	sql << "UPDATE bank SET";
	
	for (int itm = 0; itm < NUM_ITEMS; itm++)
	{
		sql << " ITEM_";
		sql << itm;
		sql << "=";
		sql << (int)User[userId].bank[itm];
		if (itm+1 < NUM_ITEMS)
			sql << ", ";
	}
	
	sql << " WHERE player_id LIKE '";
	sql << database->clean(player_id);
	sql << "'";

	database->query(sql.str());
	

	sql.str("");
	sql.clear();
	sql << "UPDATE player SET";
	sql << " level=";
	sql << (int)User[userId].level;
	sql << ", x=";
	sql << User[userId].x;
	sql << ", y=";
	sql << User[userId].y;
	sql << ", xp=";
	sql << User[userId].xp;
	sql << ", hp=";
	sql << (int)User[userId].hp;  
	sql << ", str=";
	sql << (int)User[userId].strength; 
	sql << ", def=";
	sql << (int)User[userId].defense;
	sql << ", xp_max=";
	sql << User[userId].max_xp;
	sql << ", hp_max="; 
	sql << (int)User[userId].max_hp;
	sql << ", y_speed=";
	sql << User[userId].y_speed;
	sql << ", x_speed=";
	sql << User[userId].x_speed;
	sql << ", stat_points=";
	sql << (int)User[userId].stat_points;
	sql << ", regen=";
	sql << (int)User[userId].regen;
	sql << ", facing_right=";
	sql << (int)User[userId].facing_right;
	sql << ", EQUIP_0=";
	sql << (int)User[userId].equip[0];
	sql << ", EQUIP_1=";
	sql << (int)User[userId].equip[1];
	sql << ", EQUIP_2=";
	sql << (int)User[userId].equip[2];


	//operating system
	sql << ", VERSION_OS=";
	sql << (int)User[userId].OS;

	//time played
	unsigned long int total_minutes_played = User[userId].minutesPlayed;
	double this_session_milli = (OPI_Clock::milliseconds() - User[userId].loginTime);
	//add the milliseconds to total time
	total_minutes_played += (unsigned long long int)(this_session_milli/1000.0/60.0);

	sql << ", minutes_played=";
	sql << total_minutes_played;

		
	//what map are they on?
	sql << ", current_map=";
	sql << (int)User[userId].mapId;

	sql << ", inventory_order='";
	sql << database->clean(base64_encode(User[userId].getInventoryOrder()));

	sql << "' WHERE id=";
	sql << database->clean(player_id);
	sql << ";";

	database->query(sql.str());       

	return true;
}