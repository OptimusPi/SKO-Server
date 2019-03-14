#include <string>

#ifndef __SKO_Utilities_H_ 
#define __SKO_Utilities_H_

class SKO_Utilities
{
    public:
    // Returns the next parameter from a slash command
    // Also shrinks the parameter list by one
    // Example:
    //    slash command: "/warp pifreak 2500 100 2"
    //    parameters: "pifreak 2500 100 2"
    //   SKO_Utilities::nextParameter(): "pifreak"
    //    parameters: "2500 100 2"
    //   SKO_Utilities::nextParameter(): "2500"
    //    parameters: "100 2"
    //   SKO_Utilities::nextParameter(): "100"
    //    parameters: "2"
    //   SKO_Utilities::nextParameter(): "2"
    //    parameters: ""
    //   SKO_Utilities::nextParameter: ""
    static std::string nextParameter(std::string &parameters);
    static std::string trimString(std::string str);
    static std::string lowerString(std::string myString);
};
 
#endif