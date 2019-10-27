#ifndef MARIADB_DSCAO__
#define MARIADB_DSCAO__
#include <stdlib.h>
#include <my_global.h>
#include <stdarg.h>
#include <mysql.h>
#include "loglog.h"

static inline void logmsg_mysql(MYSQL *mysql)
{
	logmsg(LOG_ERR, "Error(%d) [%s] \"%s\"", mysql_errno(mysql),
			mysql_sqlstate(mysql), mysql_error(mysql));
}

static inline void logmsg_stmt(MYSQL_STMT *stmt)
{
	logmsg(LOG_ERR, "Error(%d) [%s] \"%s\"", mysql_stmt_errno(stmt),
			mysql_stmt_sqlstate(stmt), mysql_stmt_error(stmt));
}

struct mariadb {
	MYSQL *con;
	MYSQL_RES *res;
	MYSQL_STMT *stmt;
	MYSQL_ROW row;
};

struct mariadb *mariadb_init(const char *user, const char *passwd,
		const char *dbname);
static inline void mariadb_exit(struct mariadb *db)
{
	if (db) {
		mysql_stmt_close(db->stmt);
		mysql_close(db->con);
		free(db);
	}
}

int mariadb_stmt_prepare(struct mariadb *mdb, const char *query, int len,
		MYSQL_BIND *bds, ...);
static inline void mariadb_stmt_close(struct mariadb *mdb)
{
	mysql_stmt_close(mdb->stmt);
}
static inline int mariadb_stmt_execute(struct mariadb *mdb)
{
	int retv;

	if ((retv = mysql_stmt_execute(mdb->stmt)))
		logmsg_stmt(mdb->stmt);
	return retv;
}

unsigned long mariadb_row_count(struct mariadb *mdb, const char *table);
#endif /* MARIADB_DSCAO__ */
