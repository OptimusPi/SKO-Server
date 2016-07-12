#include "SKO_Player.h"


void SKO_Player::init()
{
	    Sock = new GE_Socket();
	    Sock->Connected = false;
		printf("Nick is %s\n", Nick.c_str());
		
        Nick = "";
        printf("=Nick is %s\n", Nick.c_str());
		
		Mute = true;
        Moderator = false;
        Ident = false;
        Status = false;
        Que = false;
        Save = false;
        QueTime = 0;
        ground = false;
        x = 1000;
        y = 0;
		tempClanId = "";
		
	//the noob map was added later and the id is 2
        current_map = 2;
        hp = 0;
        max_hp = 10;
        xp = 0;
        max_xp = 10;
        level = 1;
        regen = 0;
        regen_ticker = Clock();
        strength = 2;
        defence = 1;
        attacking = false;
        y_speed = 0;
        x_speed = 0;
        que_action = 0;
        stat_points = 0;
        pingTicker = Clock();
        ping = 0;
        pingWaiting = false;
	attack_ticker = Clock();
		
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
    current_map = 0;

    mobKills = 0;
    pvpKills = 0;
}

void SKO_Player::Clear()
{
	printf("Clear();\n");
	if (Sock)
	{
		printf("deleting Sock...");
		delete Sock;
	}
	init();
	printf("Init() called.\n");
	
}
SKO_Player::~SKO_Player()
{
	printf("~SKO_Player() called\n");
	Clear();
}
SKO_Player::SKO_Player()
{

	init();

}

std::string SKO_Player::getInventoryOrder(){
 	std::string iorder = "";
	for (int i = 0; i < 48; i++)
		iorder += (char)0;
	for (int i = 0; i < inventory_order.length() && i < 48; i++)
		iorder[i] = inventory_order[i];
	return iorder;
}
void SKO_Player::RecvPacket()
{
     bool error = false;
     
     //socket status
        if (Sock->Connected)
        {
           if (Sock->GetStatus() & GE_Socket_Error)
           {
               printf("I can't Recv. My socket status is GE_Socket_Error!\n"); 
               error = true;
           }
           else
           {
               
               if (Sock->Recv() == GE_Socket_Error)
               {
                   printf("Recv() failed and returned GE_Socket_Error!\n"); 
                   printf("ABCD\n");
		   error = false;                  
               }
               else
               {
                   ;//recv fine
               }
           }
        }
        else
        {
            printf("I can't Recv. My socket is not connected!\n"); 
            error = true;
        } 
     
     if (error)
     {
            printf("***\n***\n");
            
             printf("SKO_Player::RecvPacket calling SKO_Player::Clear();\n");

			Clear();
     }
}

void SKO_Player::SendPacket(std::string Packet)
{
     
   //  printf("SendPacket(");
//             for (int i = 0; i < Packet.length(); i++){
//                 printf("[%i]", Packet[i]);  
//             }
//             printf("\n");
             
     bool error = false;
     
     
        //socket status
        if (Sock->Connected)
        {
           if (Sock->GetStatus() & GE_Socket_Error)
           {
               printf("I can't send that packet. My socket status is GE_Socket_Error!\n"); 
               error = true;
           }
           else
           {
               
               if (Sock->Send(Packet) == GE_Socket_Error)
               {
                   printf("Send() failed and returned GE_Socket_Error!\n"); 
                   error = true;                  
	       }
               else
               {
                   ;//sent fine
               }
           }
        }
        else
        {
            printf("I can't send that packet. My socket is not connected!\n"); 
            error = true;
        } 
     
     if (error)
     {
            printf("***\n***\n");
            
             printf("Clear();\n");
            
			Clear();
            
     }
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
