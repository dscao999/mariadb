#include <stdlib.h>
#include <string.h>
#include "mariadb.h"

struct mariadb *mariadb_init(const char *user, const char *passwd,
		const char *dbname)
{
	struct mariadb *mdb;

	mdb = malloc(sizeof(struct mariadb));
	if (!mdb)
		return mdb;

	mdb->con = mysql_init(NULL);
	if (mdb->con == NULL) {
		logmsg_mysql(mdb->con);
		goto err_10;
	}
	if (mysql_real_connect(mdb->con, NULL, user, passwd, dbname,
			0, NULL, 0) == NULL) {
		logmsg_mysql(mdb->con);
		goto err_10;
	}
	mdb->stmt = mysql_stmt_init(mdb->con);
	if (!mdb->stmt)
		goto err_20;
	return mdb;

err_20:
	mysql_close(mdb->con);
err_10:
	free(mdb);
	return NULL;
}

int mariadb_stmt_prepare(struct mariadb *mdb, const char *query, int len,
		MYSQL_BIND *bds, ...)
{
	va_list ap;
	int retv = 0, no, remlen;
	MYSQL_BIND *bd;
	void *ptr;
	enum enum_field_types typeid;

	retv = mysql_stmt_prepare(mdb->stmt, query, len);
	if (retv != 0) {
		logmsg_stmt(mdb->stmt);
		return retv;
	}
	no = 0;
	ptr = memchr(query, '?', len);
	while (ptr) {
		remlen = len - ((char *)ptr - query) - 1;
		no++;
		ptr = memchr(ptr+1, '?', remlen);
	}
	if (no == 0) {
		logmsg(LOG_WARNING, "Statement has no '?' "
				"should not be prepared\n");
		return 1;
	}
	memset(bds, 0, no*sizeof(MYSQL_BIND));

	bd = bds;
	va_start(ap, bds);
	ptr = va_arg(ap, void *);
	while (ptr) {
		typeid = va_arg(ap, enum enum_field_types);
		bd->buffer = ptr;
		bd->buffer_type = typeid;
		ptr = va_arg(ap, void *);
		bd++;
	}
	va_end(ap);
	mysql_stmt_bind_param(mdb->stmt, bds);

	return retv;
}

unsigned long mariadb_row_count(struct mariadb *mdb, const char *table)
{
	static const char *selcount = "SELECT COUNT(*) FROM ";
	char *query;
	unsigned long retv = 0;
	MYSQL_RES *sres;
	MYSQL_ROW row;

	query = malloc(22 + strlen(table));
	if (!query) {
		logmsg(LOG_CRIT, "Out of Memory!\n");
		return retv;
	}
	strcpy(query, selcount);
	strcat(query, table);
	if (mysql_query(mdb->con, query)) {
		logmsg_mysql(mdb->con);
		goto exit_10;
	}
	sres = mysql_store_result(mdb->con);
	if (!sres) {
		logmsg(LOG_ERR, "Number of rows undefined.\n");
		goto exit_10;
	}
	row = mysql_fetch_row(sres);
	retv = strtoul(*row, NULL, 10);
	mysql_free_result(sres);

exit_10:
	free(query);
	return retv;
}
