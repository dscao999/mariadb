#include <stdio.h>
#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "loglog.h"
#include "base64.h"

static const char *insert_sql = "INSERT INTO txt_unique values (?, ?)";

static volatile int gexit = 0;

static void msig_handler(int sig)
{
	if (sig == SIGINT)
		gexit = 1;
}

int main(int argc, char *argv[])
{
	MYSQL *mcon;
	MYSQL_STMT *smt;
	MYSQL_BIND rsbnd[3];
	unsigned int seq_id;
	char animal[16];
	unsigned long slen;
	int retv = 0;
	struct sigaction mact;

	memset(&mact, 0, sizeof(mact));
	mact.sa_handler = msig_handler;
	sigaction(SIGINT, &mact, NULL);

	mcon = mysql_init(NULL);
	if (mysql_real_connect(mcon, "localhost", "dscao", NULL, "electoken",
			0, NULL, 0) == NULL) {
		logmsg(LOG_ERR, "Cannot connect to DB: %s\n", mysql_error(mcon));
		return 1;
	}
	smt = mysql_stmt_init(mcon);
	if (mysql_stmt_prepare(smt, insert_sql, strlen(insert_sql))) {
		logmsg(LOG_ERR, "Statement Preparation failed: %s, %s\n",
				insert_sql, mysql_stmt_error(smt));
		mysql_close(mcon);
		return 2;
	}
	memset(rsbnd, 0, sizeof(rsbnd));
	rsbnd[0].buffer_type = MYSQL_TYPE_LONG;
	rsbnd[0].buffer = &seq_id;
	rsbnd[0].is_unsigned = 1;
	rsbnd[1].buffer_type = MYSQL_TYPE_STRING;
	rsbnd[1].buffer = &animal;
	rsbnd[1].buffer_length = sizeof(animal);
	rsbnd[1].length = &slen;
	if (mysql_stmt_bind_param(smt, rsbnd)) {
		logmsg(LOG_ERR, "Cannot bind result for: %s, %s\n",
				insert_sql, mysql_stmt_error(smt));
		mysql_stmt_close(smt);
		mysql_close(mcon);
		return 4;
	}

	strcpy(animal, "lion");
	seq_id = 2;
	slen = strlen(animal);
	if (mysql_stmt_execute(smt)) {
		logmsg(LOG_ERR, "Cannot execute statement %s, %s, no: %d\n",
				insert_sql, mysql_stmt_error(smt), mysql_stmt_errno(smt));
		retv = 5;
		goto exit_10;
	}

exit_10:
	printf("Exit Flag: %d\n", gexit);
	mysql_stmt_close(smt);
	mysql_close(mcon);
	return retv;
}
