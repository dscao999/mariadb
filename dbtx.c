#include <stdio.h>
#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "loglog.h"
#include "base64.h"

static const char *selsql = "SELECT keyhash, etoken_id, value FROM utxo " \
			     "WHERE in_process = false AND blockid > 1";

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
	unsigned int etoken_id;
	unsigned long value;
	unsigned char ripe[24], ripe_str[32];
	unsigned long hash_len;
	int retv = 0, mysql_retv, len;
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
	memset(rsbnd, 0, sizeof(rsbnd));
	rsbnd[0].buffer_type = MYSQL_TYPE_BLOB;
	rsbnd[0].buffer = ripe;
	rsbnd[0].buffer_length = sizeof(ripe);
	rsbnd[0].length = &hash_len;
	rsbnd[1].buffer_type = MYSQL_TYPE_SHORT;
	rsbnd[1].buffer = &etoken_id;
	rsbnd[1].is_unsigned = 1;
	rsbnd[2].buffer_type = MYSQL_TYPE_LONGLONG;
	rsbnd[2].buffer = &value;
	rsbnd[2].is_unsigned = 1;
	if (mysql_stmt_bind_result(smt, rsbnd)) {
		logmsg(LOG_ERR, "Cannot bind result for: %s, %s\n",
				selsql, mysql_stmt_error(smt));
		mysql_stmt_close(smt);
		mysql_close(mcon);
		return 4;
	}

	if (mysql_stmt_execute(smt)) {
		logmsg(LOG_ERR, "Cannot execute statement %s, %s\n",
				selsql, mysql_stmt_error(smt));
		retv = 5;
		goto exit_10;
	}
	mysql_stmt_store_result(smt);
	do {
		mysql_retv = mysql_stmt_fetch(smt);
		if (mysql_retv != 0)
			break;
		len = bin2str_b64((char *)ripe_str, sizeof(ripe_str), ripe, hash_len);
		ripe_str[len] = 0;
		printf("Key: %s, token ID: %hu, Value: %lu\n", ripe_str,
				etoken_id, value);

	} while (1);
	mysql_stmt_free_result(smt);

exit_10:
	printf("Exit Flag: %d\n", gexit);
	mysql_stmt_close(smt);
	mysql_close(mcon);
	return retv;
}
