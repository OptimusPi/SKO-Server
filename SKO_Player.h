/* INCLUDES */
#include "library.h"
#include "GE_Socket.h"

#ifndef __SKO_PLAYER_H_
#define __SKO_PLAYER_H_

class SKO_Player
{

      private:
      public:
        void init();

        /* FUNCTIONS */
        SKO_Player();
        ~SKO_Player();

        void Init();
        void Clear();
        std::string getInventoryOrder();

        //stats
        bool addXP(int xp_in);

        /* VARIABLES */
        // Socket pointer
        GE_Socket *Sock = NULL;
        bool Status;
        bool Que;
        bool Ident;
        bool Save;
        unsigned long long int QueTime;
        unsigned int ping;
        unsigned long long int pingTicker;
        bool pingWaiting;

        // Name
        std::string Nick, Clan;

        // Status
        bool Mute = false;
        bool Moderator = false;

        //player id in database
        std::string ID;
        std::string tempClanId;
        std::string inventory_order;

        /* GAME STUFF */
        //physics
        bool attacking;
        bool ground;
        bool facing_right;
        unsigned long long int attack_ticker;
        unsigned long long int regen_ticker;
        int queue_action;

        void setX();
        void addToX();
        void setY();
        void addToY();
        void setXSpeed();
        void setYSpeed();

        //stats
        //health
        unsigned char hp;
        unsigned char max_hp;
        unsigned char regen;
        //experience
        unsigned char level;
        unsigned char stat_points;
        unsigned int xp;
        unsigned int max_xp;
        //strength and defence
        unsigned char strength;
        unsigned char defence;

        //items
        unsigned int inventory[256];
        unsigned int bank[256];
        char inventory_index;
        char bank_index;

        //equip
        unsigned int equip[3];

        //coords
        float x;
        float y;
        float y_speed;
        float x_speed;
        unsigned char mapId;

        //trading
        int tradeStatus;
        int tradePlayer;
        unsigned int localTrade[256];

        //clan
        int clanStatus;
        int clanPlayer;
        //party
        int party;
        int partyStatus;

        //fun server stuff
        unsigned long long int loginTime;
        unsigned long long int minutesPlayed;
        unsigned short int OS;
        unsigned long int mobKills;
        unsigned long int pvpKills;
};

#endif