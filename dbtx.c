#include <stdio.h>
#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "loglog.h"

static const char *selsql = "SELECT txhash, seq FROM txrec_pool WHERE in_process = ?";

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
	MYSQL_BIND pmbnd[1], rsbnd[2];
	unsigned int seq, count;
	char inproc = 0;
	unsigned long shalen;
	int retv = 0, mysql_retv;
	unsigned char sha_dgst[32];
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
	if (mysql_stmt_prepare(smt, selsql, strlen(selsql))) {
		logmsg(LOG_ERR, "Statement Preparation failed: %s, %s\n",
				selsql, mysql_stmt_error(smt));
		mysql_close(mcon);
		return 2;
	}
	memset(pmbnd, 0, sizeof(pmbnd));
	pmbnd[0].buffer_type = MYSQL_TYPE_TINY;
	pmbnd[0].buffer = &inproc;
	if (mysql_stmt_bind_param(smt, pmbnd)) {
		logmsg(LOG_ERR, "Cannto bind parameter to: %s, %s\n",
				selsql, mysql_stmt_error(smt));
		mysql_stmt_close(smt);
		mysql_close(mcon);
		return 3;
	}
	memset(rsbnd, 0, sizeof(rsbnd));
	rsbnd[0].buffer_type = MYSQL_TYPE_BLOB;
	rsbnd[0].buffer = sha_dgst;
	rsbnd[0].buffer_length = 32;
	rsbnd[0].length = &shalen;
	rsbnd[1].buffer_type = MYSQL_TYPE_LONG;
	rsbnd[1].buffer = &seq;
	rsbnd[1].is_unsigned = 1;
	if (mysql_stmt_bind_result(smt, rsbnd)) {
		logmsg(LOG_ERR, "Cannot bind result for: %s, %s\n",
				selsql, mysql_stmt_error(smt));
		mysql_stmt_close(smt);
		mysql_close(mcon);
		return 4;
	}

	count = 0;
	do {
		printf("Result for inproc: %d\n", inproc);
		if (mysql_stmt_execute(smt)) {
			logmsg(LOG_ERR, "Cannot execute statement %s, %s\n",
					selsql, mysql_stmt_error(smt));
			retv = 5;
			break;
		}
		mysql_stmt_store_result(smt);
		do {
			mysql_retv = mysql_stmt_fetch(smt);
			if (mysql_retv != 0)
				break;
			printf("	Seq: %u\n", seq);

		} while (1);
		mysql_stmt_free_result(smt);
		sleep(1);
		inproc = (inproc + 1) & 1;
	} while (gexit == 0 && count++ < 10);
	printf("Exit Flag: %d\n", gexit);

	mysql_stmt_close(smt);
	mysql_close(mcon);
	return retv;
}
