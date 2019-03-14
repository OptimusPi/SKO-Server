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

std::string SKO_Utilities::trimString(std::string str) 
{
	std::size_t first = str.find_first_not_of(' ');

	// If there is no non-whitespace character, both first and last will be std::string::npos (-1)
	// There is no point in checking both, since if either doesn't work, the
	// other won't work, either.
	if (first == std::string::npos)
		return "";

	std::size_t last = str.find_last_not_of(' ');

	std::string returnVal = str.substr(first, last - first + 1);
	return returnVal;
}

std::string SKO_Utilities::lowerString(std::string myString)
{
	for (unsigned long int i = 0; i < myString.length(); ++i)
		myString[i] = std::tolower(myString[i]);

	return myString;
}