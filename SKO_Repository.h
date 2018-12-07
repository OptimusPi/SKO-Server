#ifndef __SKO_REPOSITORY_H_
#define __SKO_REPOSITORY_H_

#include "OPI_MYSQL.h"

class SKO_Repository
{

	public:
	SKO_Repository();


	private:
	OPI_MYSQL *db;
};

#endif
