#include <string>
#include "OPI_Utilities.h"

std::string OPI_Utilities::nextParameter(std::string &parameters)
{
    //grab first parameter from list
    std::string next = parameters.substr(0, parameters.find_first_of(" "));

    //shrink list of parameters 
    parameters = parameters.substr(parameters.find_first_of(" ") + 1);
    
    //return a single parameter; the next in line
    return next;
}

std::string OPI_Utilities::trimString(std::string str) 
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

std::string OPI_Utilities::lowerString(std::string myString)
{
	for (unsigned long int i = 0; i < myString.length(); ++i)
		myString[i] = std::tolower(myString[i]);

	return myString;
}


std::vector<std::string> OPI_Utilities::split(const std::string& s, char seperator)
{
    if (s.empty()) {
        return std::vector<std::string>();
    }

    std::vector<std::string> output;
    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );

        output.push_back(substring);

        prev_pos = ++pos;
    }

    output.push_back(s.substr(prev_pos, pos-prev_pos));

    return output;
}