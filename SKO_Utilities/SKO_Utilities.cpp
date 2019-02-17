#include <string>
#include "SKO_Utilities.h"

std::string SKO_Utilities::nextParameter(std::string &parameters)
{
    //grab first parameter from list
    std::string next = parameters.substr(0, parameters.find_first_of(" "));

    //shrink list of parameters 
    parameters = parameters.substr(parameters.find_first_of(" ") + 1);
    
    //return a single parameter; the next in line
    return next;
}