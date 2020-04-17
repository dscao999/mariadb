#ifndef MARIADB_DSCAO__
#define MARIADB_DSCAO__
#include <stdlib.h>
#include <my_global.h>
#include <stdarg.h>
#include <mysql.h>
#include "loglog.h"

static inline void logmsg_mysql(MYSQL *mysql)
{
	logmsg(LOG_ERR, "Error(%d) [%s] \"%s\"\n", mysql_errno(mysql),
			mysql_sqlstate(mysql), mysql_error(mysql));
}

static inline void logmsg_stmt(MYSQL_STMT *stmt)
{
	logmsg(LOG_ERR, "Error(%d) [%s] \"%s\"\n", mysql_stmt_errno(stmt),
			mysql_stmt_sqlstate(stmt), mysql_stmt_error(stmt));
}

struct mariadb {
	MYSQL *con;
	MYSQL_RES *res;
	MYSQL_STMT *stmt;
	MYSQL_ROW row;
};

int mariadb_init(struct mariadb *mdb, const char *user,
		const char *passwd, const char *host);
static inline void mariadb_exit(struct mariadb *db)
{
	if (db)
		mysql_close(db->con);
}
#endif /* MARIADB_DSCAO__ */
