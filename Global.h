
// TODO: Make this better, move it to a variadic function
//Colors for fun console log statements
#define kNormal     "\x1B[0m"
#define kRed        "\x1B[31m"
#define kGreen      "\x1B[32m"
#define kYellow     "\x1B[33m"
#define kBlue       "\x1B[34m"
#define kMagenta    "\x1B[35m"
#define kCyan       "\x1B[36m"
#define kWhite      "\x1B[37m"

#define MAX_CLIENTS 16

#include "SKO_Player.h"

//Quit all threads global flag
extern bool SERVER_QUIT;

//Shared objects
extern SKO_Player User[];