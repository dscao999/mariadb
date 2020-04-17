#include <stdlib.h>
#include <string.h>
#include "mariadb.h"

const char dbname[] = "electoken";

int mariadb_init(struct mariadb *mdb, const char *user,
		const char *passwd, const char *host)
{
	int retv = 0;

	mdb->con = mysql_init(NULL);
	if (mdb->con == NULL) {
		logmsg_mysql(mdb->con);
		return 1;
	}
	if (mysql_real_connect(mdb->con, host, user, passwd, dbname,
			0, NULL, 0) == NULL) {
		logmsg_mysql(mdb->con);
		retv = 1;
		goto err_10;
	}
	return retv;

err_10:
	mysql_close(mdb->con);
	return retv;
}
