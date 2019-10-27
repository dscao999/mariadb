#include <stdio.h>
#include "loglog.h"
#include "ecc256/alsa_random.h"
#include "mariadb.h"

static int trigger_one(unsigned int *rnum, struct alsa_param *alsa)
{
	unsigned char dgst[32], *byte;
	int idx, i;
	union {
		unsigned int num;
		unsigned char str[4];
	} rnd;

	*rnum = 0;
	if (alsa_random(alsa, (unsigned int *)dgst) != 0) {
		logmsg(LOG_ERR, "Failed to get an random number!\n");
		return -1;
	}
	byte = dgst;
	idx = byte[31] & 0x1f;
	for (i = 0; i < 4; i++) {
		rnd.str[i] = byte[idx];
		idx = (idx + 1) & 0x1f;
	}
	*rnum = rnd.num;

	return 0;
}

static const char *insert_stmt = "INSERT INTO rndnum (number) VALUES (?)";

int main(int argc, char *argv[])
{
	unsigned int rno, seq;
	unsigned long rnum;
	int cnt, retv = 0;
	struct alsa_param *alsa;
	struct mariadb *mdb;
	MYSQL_BIND bids;

	mdb = mariadb_init("dscao", NULL, "electoken");
	check_pointer(mdb);
	seq = mariadb_row_count(mdb, "rndnum");
	printf("Total %d records in table: %s\n", seq, "rndnum");

	if (mariadb_stmt_prepare(mdb, insert_stmt, strlen(insert_stmt), &bids,
				&rnum, MYSQL_TYPE_LONGLONG, NULL)) {
		logmsg(LOG_ERR, "Prepared failed.\n");
		goto exit_10;
	}

	alsa = alsa_init("hw:0,0", 0);
	check_pointer(alsa);

	cnt = 0;
	while (cnt < 18000) {
		trigger_one(&rno, alsa);
		rnum = rno;
		if (mariadb_stmt_execute(mdb)) {
			retv = 2;
			break;
		}
		cnt++;
	}

	mariadb_stmt_close(mdb);
	alsa_exit(alsa);
	
exit_10:
	mariadb_exit(mdb);
	return retv;
}
