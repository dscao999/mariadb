#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include "loglog.h"
#include "alsarec.h"
#include "mariadb.h"

static volatile int stop_job = 0;

static void me_handler(int sig)
{
	if (sig == SIGTERM || sig == SIGINT || sig == SIGHUP)
		stop_job = 1;
}

static int inst_handler(void)
{
	struct sigaction mact;
	int sysret;

	sysret = sigaction(SIGINT, NULL, &mact);
	if (sysret == -1)
		return errno;
	mact.sa_handler = me_handler;
	sysret = sigaction(SIGINT, &mact, NULL);
	if (sysret == -1)
		return errno;
	sysret = sigaction(SIGTERM, &mact, NULL);
	if (sysret == -1)
		return errno;
	sysret = sigaction(SIGHUP, &mact, NULL);
	if (sysret == -1)
		return errno;
	return sysret;
}

static int trigger_one(unsigned int *rnum, const unsigned char *buf, int buflen)
{
	unsigned char dgst[32], *byte;
	int idx, i, retv = 0;
	union {
		unsigned int num;
		unsigned char str[4];
	} rnd;

	*rnum = 0;
	alsa_random((unsigned int *)dgst, buf, buflen);
	byte = dgst;
	idx = byte[31] & 0x1f;
	for (i = 0; i < 4; i++) {
		rnd.str[i] = byte[idx];
		idx = (idx + 1) & 0x1f;
	}
	*rnum = rnd.num;

	return retv;
}

static const char *insert_stmt = "INSERT INTO rndnum (number) VALUES (?)";

int main(int argc, char *argv[])
{
	unsigned int rno, seq;
	unsigned long rnum;
	int cnt, retv = 0, buflen;
	struct mariadb *mdb;
	MYSQL_BIND bids;
	unsigned char *buf;

	retv = inst_handler();
	if (retv) {
		logmsg(LOG_ERR, "Cannot install signal handler: %s\n",
				strerror(retv));
		return 1;
	}

	mdb = mariadb_init("dscao", NULL, "electoken");
	if (!mdb) {
		logmsg(LOG_ERR, "Cannot Initialize DB\n");
		return 1;
	}
	seq = mariadb_row_count(mdb, "rndnum");
	printf("Total %d records in table: %s\n", seq, "rndnum");

	if (mariadb_stmt_prepare(mdb, insert_stmt, strlen(insert_stmt), &bids,
				&rnum, MYSQL_TYPE_LONGLONG, NULL)) {
		logmsg(LOG_ERR, "Prepared failed.\n");
		goto exit_10;
	}

	alsa_init(NULL);
	buflen = alsa_reclen(1);
	buf = malloc(buflen);
	if (!check_pointer(buf, nomem))
		exit(100);

	cnt = 0;
	while (stop_job == 0) {
		alsa_record(1, buf, buflen);
		if (!trigger_one(&rno, buf, buflen)) {
			rnum = rno;
			if (mariadb_stmt_execute(mdb)) {
				retv = 2;
				break;
			}
			cnt++;
		}
	}

	alsa_exit();
	
exit_10:
	mariadb_exit(mdb);
	return retv;
}
