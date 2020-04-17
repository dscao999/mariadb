#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <stdlib.h>
#include "virtmach.h"
#include "base64.h"
#include "tok_block.h"

static const char *dbname = "electoken";

static int insert_vendors(MYSQL *mcon);
static int insert_etoken_cat(MYSQL *mcon);
static int insert_etoken_type(MYSQL *mcon);
static int insert_sales(MYSQL *mcon);
static int insert_first_block(MYSQL *mcon);

struct table_desc {
	const char *tbname;
	const char *desc;
	int (*insert_table)(MYSQL *);
};
static const struct table_desc tables[] = {
	{
		"vendors",
		"(id smallint unsigned primary key, " \
			"name char(16) not null unique, " \
			"descp char(128) not null)",
		insert_vendors
	},
	{
		"etoken_cat",
		"(id smallint unsigned primary key, " \
			"name char(16) not null unique, " \
			"descp char(128) not null, " \
			"vendor_id smallint unsigned not null, " \
			"constraint foreign key (vendor_id) references " \
			"vendors(id))",
		insert_etoken_cat
	},
	{
		"etoken_type",
		"(id smallint unsigned primary key, " \
			"name char(16) not null unique, " \
			"descp char(128) not null, " \
			"cat_id smallint unsigned not null, " \
			"constraint foreign key (cat_id) references " \
			"etoken_cat(id))",
		insert_etoken_type
	},
	{
		"sales",
		"(keyhash char(29) not null, " \
			"etoken_id smallint unsigned not null, " \
			"lockscript blob not null, "
			"constraint foreign key (etoken_id) references " \
			"etoken_type(id))",
		insert_sales
	},
	{
		"blockchain", \
		"(blockid bigint unsigned primary key auto_increment, " \
			"hdr_hash binary(32) not null unique, " \
			"blockdata blob not null)",
		insert_first_block
	},
	{
		"utxo",
		"(keyhash char(29) not null, " \
			"etoken_id smallint unsigned not null, " \
			"value bigint unsigned not null, " \
			"vout_idx tinyint unsigned not null, " \
			"blockid bigint unsigned not null, " \
			"txid binary(32) not null, " \
			"in_process boolean default false " \
			"constraint foreign key (etoken_id) references " \
			"etoken_type(id), " \
			"constraint foreign key (blockid) references " \
			"blockchain(blockid))",
		NULL
	},
	{
		"txrec_pool",
		"(txhash binary(32) not null unique, txdata blob not null, " \
			"seq int unsigned auto_increment primary_key, " \
			"in_process boolean default false)",
		NULL
	},
	{
		NULL, NULL, NULL
	}
};

static char query_stmt[1024];

static int create_tables(MYSQL *mcon, const struct table_desc table_set[],
		int nume)
{
	int i, retv = 0;

	for (i = 0; i < nume; i++) {
		if (table_set[i].tbname == NULL)
			continue;
		strcpy(query_stmt, "create table ");
		strcat(query_stmt, table_set[i].tbname);
		strcat(query_stmt, table_set[i].desc);
		printf("Creating table: %s\n", query_stmt);
		if (mysql_query(mcon, query_stmt)) {
			fprintf(stderr, "%s\n", mysql_error(mcon));
			retv = 3;
			goto exit_10;
		}
	}

exit_10:
	return retv;
}


int main(int argc, char *argv[])
{
	MYSQL *mcon;
	MYSQL_RES *result;
	MYSQL_ROW row;
	int dbfound, skip;
	int retv = 0, i, numtbs;
	struct table_desc table_set[sizeof(tables)/sizeof(struct table_desc)];
	const struct table_desc *tbset;

	numtbs = sizeof(tables) / sizeof(struct table_desc);
	for (i = 0; i < numtbs; i++)
		table_set[i] = tables[i];
	printf("My Mariadb client version: %s\n", mysql_get_client_info());
	mcon = mysql_init(NULL);
	if (!mcon) {
		fprintf(stderr, "%s\n", mysql_error(mcon));
		return 1;
	}
	if (mysql_real_connect(mcon, "localhost", "dscao", NULL, NULL, 0, NULL, 0) == NULL) {
		retv = 2;
		goto err_exit_10;
	}
	if (mysql_query(mcon, "show databases")) {
		retv = 3;
		goto err_exit_10;
	}
	result = mysql_store_result(mcon);
	if (result == NULL) {
		retv = 4;
		goto err_exit_10;
	}
	printf("Databases: \n");
	dbfound = 0;
	while ((row = mysql_fetch_row(result))) {
		if (!dbfound)
			dbfound = strstr((char *)(*row), dbname) != NULL;
		printf("	%s\n", (char *)row[0]);
	}

	mysql_free_result(result);
	if (!dbfound) {
		strcpy(query_stmt, "create database ");
		strcat(query_stmt, dbname);
		if (mysql_query(mcon, query_stmt)) {
			retv = 5;
			goto err_exit_10;
		}
	}
	strcpy(query_stmt, "use ");
	strcat(query_stmt, dbname);
	if (mysql_query(mcon, query_stmt)) {
		retv = 6;
		goto err_exit_10;
	}

	printf("Tables in %s:\n", dbname);
	strcpy(query_stmt, "show tables");
	if (mysql_query(mcon, query_stmt)) {
		retv = 7;
		goto err_exit_10;
	}
	result = mysql_store_result(mcon);
	while ((row = mysql_fetch_row(result))) {
		for (i = 0; i < numtbs; i++) {
			if (tables[i].tbname &&
					strcmp(*row, tables[i].tbname) == 0)
				table_set[i].tbname = NULL;
		}
		printf("	%s\n", (*row));
	}
	mysql_free_result(result);
	printf("Tables need to be created:\n");
	for(i =  0; i < numtbs; i++) {
		if (table_set[i].tbname)
			printf("	%s\n", table_set[i].tbname);
	}
	create_tables(mcon, table_set, numtbs);

	tbset = tables;
	while (tbset->tbname) {
		strcpy(query_stmt, "select count(*) from ");
		strcat(query_stmt, tbset->tbname);
		if (mysql_query(mcon, query_stmt)) {
			retv = 7;
			fprintf(stderr, "msql_query: %s.\n", query_stmt);
			goto err_exit_10;
		}
		skip = 0;
		result = mysql_store_result(mcon);
		row = mysql_fetch_row(result);
		if (strcmp(*row, "0") != 0)
			skip = 1;
		mysql_free_result(result);
		if (skip) {
			tbset++;
			continue;
		}

		if (tbset->insert_table)
			tbset->insert_table(mcon);
		tbset++;
	}

	mysql_close(mcon);
	return retv;

err_exit_10:
	fprintf(stderr, "%s\n", mysql_error(mcon));
	mysql_close(mcon);
	return retv;
}

struct vendors {
	unsigned short id;
	char *name;
	char *desc;
};
struct etoken_cat {
	unsigned short id;
	char *name;
	char *desc;
	unsigned short vendor_id;
};
struct etoken_type {
	unsigned short id;
	char *name;
	char *desc;
	unsigned short cat_id;
};
struct sales {
	char *pkhash;
	unsigned short etoken_id;
	char *lockscript;
};

static const char *ven_insert = "INSERT INTO vendors(id, name, descp) " \
	"VALUES(?, ?, ?)";
static const struct vendors vens[] = {
	{10001, "Lenovo Corp", "Lenovo Corporation Limited"},
	{10002, "Wanhai Tech", "Wanhai Technology Limited"},
	{10003, "Dingjin High Tech", "Dingjin High Technology Limited"},
	{0, NULL, NULL}
};

static int insert_vendors(MYSQL *mcon)
{
	MYSQL_STMT *mstmt;
	MYSQL_BIND mbind[3];
	unsigned long name_len, desc_len;
	struct vendors *ven;
	int retv = 0, i;

	mstmt = mysql_stmt_init(mcon);
	if (!mstmt) {
		fprintf(stderr, "mysql_stmt_init out of memory!\n");
		exit(11);
	}
	if (mysql_stmt_prepare(mstmt, ven_insert, strlen(ven_insert))) {
		fprintf(stderr, "mysql_stmt_prepare failed.\n");
		fprintf(stderr, " %s\n", mysql_stmt_error(mstmt));
		exit(12);
	}
	ven = malloc(sizeof(struct vendors)+32+128);
	if (!ven) {
		fprintf(stderr, "Out of Memory!\n");
		exit(100);
	}
	ven->name = ((void *)ven) + sizeof(struct vendors);
	ven->desc = ven->name + 32;
	memset(mbind, 0, sizeof(mbind));

	mbind[0].buffer_type = MYSQL_TYPE_SHORT;
	mbind[0].buffer = &ven->id;
	mbind[0].is_unsigned = 1;

	mbind[1].buffer_type = MYSQL_TYPE_STRING;
	mbind[1].buffer = ven->name;
	mbind[1].buffer_length = 32;
	mbind[1].length = &name_len;

	mbind[2].buffer_type = MYSQL_TYPE_STRING;
	mbind[2].buffer = ven->desc;
	mbind[2].buffer_length = 128;
	mbind[2].length = &desc_len;

	if (mysql_stmt_bind_param(mstmt, mbind)) {
		fprintf(stderr, "mysql_stmt_bind_param failed: %s\n",
				mysql_stmt_error(mstmt));
		retv = 5;
		goto exit_10;
	}
	for (i = 0; vens[i].name; i++) {
		ven->id = vens[i].id;
		strcpy(ven->name, vens[i].name);
		name_len = strlen(ven->name);
		strcpy(ven->desc, vens[i].desc);
		desc_len = strlen(ven->desc);
		if (mysql_stmt_execute(mstmt)) {
			fprintf(stderr, "mysql_stmt_execute failed: %s\n",
					mysql_stmt_error(mstmt));
			retv = 6;
			goto exit_10;
		}
	}
	free(ven);

exit_10:
	mysql_stmt_close(mstmt);
	return retv;
}

static const char *cat_insert = "INSERT INTO etoken_cat(id, name, descp, vendor_id) " \
	"VALUES(?, ?, ?, ?)";
static const struct etoken_cat tokcat[] = {
	{30001, "ThinkServer", "Think Server Service Warranty", 10001},
	{30002, "ThinkSystem", "ThinkSystem Service Warranty", 10001},
	{30003, "SystemX", "SystemX Service Warranty", 10001},
	{30004, "ThinkPad", "ThinkPad Service Warranty", 10001},
	{31001, "BigPay", "BigPay pad service warranty", 10002},
	{31002, "AgileMotor", "AgileMotor television set service warranty", 10002},
	{31003, "BravolDrone", "BravolDrone Drone service warranty", 10002},
	{32001, "CoronaHat", "CoronaHat Motor service warranty", 10003},
	{32002, "HastHorse", "HastHorse Motor service warranty", 10003},
	{32003, "RapidHawk", "RapidHawk vehicle service warranty", 10003},
	{0, NULL, NULL, 0}
};

static int insert_etoken_cat(MYSQL *mcon)
{	MYSQL_STMT *mstmt;
	MYSQL_BIND mbind[4];
	struct etoken_cat *cat;
	unsigned long name_len, desc_len;
	int retv = 0, i;

	cat = malloc(sizeof(struct etoken_cat)+32+128);
	if (!cat) {
		fprintf(stderr, "Out of Memory!\n");
		exit(100);
	}
	cat->name = ((void *)cat) + sizeof(struct etoken_cat);
	cat->desc = cat->name + 32;
	mstmt = mysql_stmt_init(mcon);
	if (!mstmt) {
		fprintf(stderr, "mysql_stmt_init out of memory!\n");
		exit(100);
	}
	if (mysql_stmt_prepare(mstmt, cat_insert, strlen(cat_insert))) {
		fprintf(stderr, "mysql_stmt_prepare failed %s\n", cat_insert);
		fprintf(stderr, " %s\n", mysql_stmt_error(mstmt));
		exit(12);
	}

	memset(mbind, 0, sizeof(mbind));
	mbind[0].buffer_type = MYSQL_TYPE_SHORT;
	mbind[0].buffer = &cat->id;
	mbind[0].is_unsigned = 1;

	mbind[1].buffer_type = MYSQL_TYPE_STRING;
	mbind[1].buffer = cat->name;
	mbind[1].buffer_length = 32;
	mbind[1].length = &name_len;

	mbind[2].buffer_type = MYSQL_TYPE_STRING;
	mbind[2].buffer = cat->desc;
	mbind[2].buffer_length = 128;
	mbind[2].length = &desc_len;

	mbind[3].buffer_type = MYSQL_TYPE_SHORT;
	mbind[3].buffer = &cat->vendor_id;
	mbind[3].is_unsigned = 1;

	if (mysql_stmt_bind_param(mstmt, mbind)) {
		fprintf(stderr, "mysql_stmt_bind_param failed: %s, %s\n",
				cat_insert, mysql_stmt_error(mstmt));
		retv = 5;
		goto exit_10;
	}
	for (i = 0; tokcat[i].name; i++) {
		cat->id = tokcat[i].id;
		strcpy(cat->name, tokcat[i].name);
		name_len = strlen(cat->name);
		strcpy(cat->desc, tokcat[i].desc);
		desc_len = strlen(cat->desc);
		cat->vendor_id = tokcat[i].vendor_id;
		if (mysql_stmt_execute(mstmt)) {
			fprintf(stderr, "mysql_stmt_execute failed: %s, %s\n",
					cat_insert, mysql_stmt_error(mstmt));
			retv = 6;
			goto exit_10;
		}
	}

	free(cat);
exit_10:
	mysql_stmt_close(mstmt);
	return retv;
}

static const char *etype_insert = "INSERT INTO etoken_type(id, name, descp, cat_id) " \
	"VALUES(?, ?, ?, ?)";
static const struct etoken_type ek_types[] = {
	{40003, "ThinkServer3Y", "Think Server 3 years of Service Warranty", 30001},
	{40005, "ThinkServer5Y", "Think Server 5 years of Service Warranty", 30001},
	{40013, "ThinkSystem3Y", "ThinkSystem Service Warranty", 30002},
	{40015, "ThinkSystem5Y", "ThinkSystem Service Warranty", 30002},
	{40023, "SystemX3Y", "SystemX 3 years of Service Warranty", 30003},
	{40025, "SystemX5Y", "SystemX 5 years of Service Warranty", 30003},
	{40031, "ThinkPad1Y", "ThinkPad 1 year of Service Warranty", 30004},
	{40033, "ThinkPad3Y", "ThinkPad 3 years of Service Warranty", 30004},
	{41005, "BigPay5Y", "BigPay pad service warranty", 31001},
	{41008, "BigPay8Y", "BigPay pad service warranty", 31001},
	{41011, "AgileMotor1Y", "AgileMotor 1 year of television set service warranty", 31002},
	{41013, "AgileMotor3Y", "AgileMotor 3 years of television set service warranty", 31002},
	{41021, "BravolDrone1Y", "BravolDrone Drone 1 year of service warranty", 31003},
	{41022, "BravolDrone2Y", "BravolDrone Drone 2 years of service warranty", 31003},
	{42003, "CoronaHat3Y", "CoronaHat Motor 3 years of service warranty", 32001},
	{42005, "CoronaHat5Y", "CoronaHat Motor 5 years of service warranty", 32001},
	{42013, "HastHorse3Y", "HastHorse Motor 3 years of service warranty", 32002},
	{42015, "HastHorse5Y", "HastHorse Motor 5 years of service warranty", 32002},
	{42021, "RapidHawk1Y", "RapidHawk vehicle 1 year of service warranty", 32003},
	{42022, "RapidHawk2Y", "RapidHawk vehicle 2 years of service warranty", 32003},
	{0, NULL, NULL, 0}
};

static int insert_etoken_type(MYSQL *mcon)
{
	MYSQL_STMT *mstmt;
	MYSQL_BIND mbind[4];
	struct etoken_type *etype;
	unsigned long name_len, desc_len;
	int retv = 0, i;

	etype = malloc(sizeof(struct etoken_type)+32+128);
	if (!etype) {
		fprintf(stderr, "Out of Memory!\n");
		exit(100);
	}
	etype->name = ((void *)etype) + sizeof(struct etoken_type);
	etype->desc = etype->name + 32;
	mstmt = mysql_stmt_init(mcon);
	if (!mstmt) {
		fprintf(stderr, "mysql_stmt_init out of memory!\n");
		exit(100);
	}
	if (mysql_stmt_prepare(mstmt, etype_insert, strlen(etype_insert))) {
		fprintf(stderr, "mysql_stmt_prepare failed %s\n", etype_insert);
		fprintf(stderr, " %s\n", mysql_stmt_error(mstmt));
		exit(12);
	}

	memset(mbind, 0, sizeof(mbind));
	mbind[0].buffer_type = MYSQL_TYPE_SHORT;
	mbind[0].buffer = &etype->id;
	mbind[0].is_unsigned = 1;

	mbind[1].buffer_type = MYSQL_TYPE_STRING;
	mbind[1].buffer = etype->name;
	mbind[1].buffer_length = 32;
	mbind[1].length = &name_len;

	mbind[2].buffer_type = MYSQL_TYPE_STRING;
	mbind[2].buffer = etype->desc;
	mbind[2].buffer_length = 128;
	mbind[2].length = &desc_len;

	mbind[3].buffer_type = MYSQL_TYPE_SHORT;
	mbind[3].buffer = &etype->cat_id;
	mbind[3].is_unsigned = 1;

	if (mysql_stmt_bind_param(mstmt, mbind)) {
		fprintf(stderr, "mysql_stmt_bind_param failed: %s, %s\n",
				etype_insert, mysql_stmt_error(mstmt));
		retv = 5;
		goto exit_10;
	}
	for (i = 0; ek_types[i].name; i++) {
		etype->id = ek_types[i].id;
		strcpy(etype->name, ek_types[i].name);
		name_len = strlen(etype->name);
		strcpy(etype->desc, ek_types[i].desc);
		desc_len = strlen(etype->desc);
		etype->cat_id = ek_types[i].cat_id;
		if (mysql_stmt_execute(mstmt)) {
			fprintf(stderr, "mysql_stmt_execute failed: %s, %s\n",
					cat_insert, mysql_stmt_error(mstmt));
			retv = 6;
			goto exit_10;
		}
	}

	free(etype);
exit_10:
	mysql_stmt_close(mstmt);
	return retv;
}

static const char *sales_insert = "INSERT INTO sales(keyhash, etoken_id, lockscript) " \
	"VALUES(?, ?, ?)";
static const struct sales sales_cap[] = {
	{"utYeYEvZuR1UqgPTQFDMhbP5wi4=", 40003, NULL},
	{"utYeYEvZuR1UqgPTQFDMhbP5wi4=", 40005, NULL},
	{"utYeYEvZuR1UqgPTQFDMhbP5wi4=", 40013, NULL},
	{"utYeYEvZuR1UqgPTQFDMhbP5wi4=", 40015, NULL},
	{"utYeYEvZuR1UqgPTQFDMhbP5wi4=", 40023, NULL},
	{"utYeYEvZuR1UqgPTQFDMhbP5wi4=", 40025, NULL},
	{"1nHW46+6QyeDjdWoRTfsleFNmlg=", 40031, NULL},
	{"1nHW46+6QyeDjdWoRTfsleFNmlg=", 40033, NULL},
	{"NBqOwGhPQMEzVhpGxG0mkNEDkxc=", 41005, NULL},
	{"NBqOwGhPQMEzVhpGxG0mkNEDkxc=", 41008, NULL},
	{"NBqOwGhPQMEzVhpGxG0mkNEDkxc=", 41011, NULL},
	{"NBqOwGhPQMEzVhpGxG0mkNEDkxc=", 41013, NULL},
	{"NBqOwGhPQMEzVhpGxG0mkNEDkxc=", 41021, NULL},
	{"NBqOwGhPQMEzVhpGxG0mkNEDkxc=", 41022, NULL},
	{"VV4JpIK1SfGrlfIaFbW9FBkUq5g=", 42003, NULL},
	{"VV4JpIK1SfGrlfIaFbW9FBkUq5g=", 42005, NULL},
	{"VV4JpIK1SfGrlfIaFbW9FBkUq5g=", 42013, NULL},
	{"VV4JpIK1SfGrlfIaFbW9FBkUq5g=", 42015, NULL},
	{"VV4JpIK1SfGrlfIaFbW9FBkUq5g=", 42021, NULL},
	{"VV4JpIK1SfGrlfIaFbW9FBkUq5g=", 42022, NULL},
	{NULL, 0, NULL}
};

static int insert_sales(MYSQL *mcon)
{
	int retv = 0, i, numb;
	MYSQL_STMT *mstmt;
	MYSQL_BIND mbind[3];
	struct sales *sale;
	unsigned long pkhash_len, script_len;

	sale = malloc(sizeof(struct sales)+32+32);
	if (!sale) {
		fprintf(stderr, "Out of Memory!\n");
		exit(100);
	}
	sale->pkhash = ((void *)sale) + sizeof(struct sales);
	sale->lockscript = sale->pkhash + 32;
	mstmt = mysql_stmt_init(mcon);
	if (!mstmt) {
		fprintf(stderr, "mysql_stmt_init out of memory!\n");
		exit(100);
	}
	if (mysql_stmt_prepare(mstmt, sales_insert, strlen(sales_insert))) {
		fprintf(stderr, "mysql_stmt_prepare failed %s\n", sales_insert);
		fprintf(stderr, " %s\n", mysql_stmt_error(mstmt));
		exit(12);
	}

	memset(mbind, 0, sizeof(mbind));
	mbind[0].buffer_type = MYSQL_TYPE_STRING;
	mbind[0].buffer = sale->pkhash;
	mbind[0].buffer_length = 29;
	mbind[0].length = &pkhash_len;

	mbind[1].buffer_type = MYSQL_TYPE_SHORT;
	mbind[1].buffer = &sale->etoken_id;
	mbind[1].is_unsigned = 1;

	mbind[2].buffer_type = MYSQL_TYPE_BLOB;
	mbind[2].buffer = sale->lockscript;
	mbind[2].buffer_length = 32;
	mbind[2].length = &script_len;

	if (mysql_stmt_bind_param(mstmt, mbind)) {
		fprintf(stderr, "mysql_stmt_bind_param failed: %s, %s\n",
				sales_insert, mysql_stmt_error(mstmt));
		retv = 5;
		goto exit_10;
	}
	pkhash_len = 28;
	sale->lockscript[0] = OP_DUP;
	sale->lockscript[1] = OP_RIPEMD160;
	sale->lockscript[2] = 20;
	sale->lockscript[23] = OP_EQUALVERIFY;
	sale->lockscript[24] = OP_CHECKSIG;
	script_len = 25;
	for (i = 0; sales_cap[i].pkhash; i++) {
		sale->etoken_id = sales_cap[i].etoken_id;
		strcpy(sale->pkhash, sales_cap[i].pkhash);

		numb = str2bin_b64((unsigned char *)sale->lockscript+3, 20,
				sale->pkhash);
		assert(numb == 20);
		if (mysql_stmt_execute(mstmt)) {
			fprintf(stderr, "mysql_stmt_execute failed: %s, %s\n",
					sales_insert, mysql_stmt_error(mstmt));
			retv = 6;
			goto exit_10;
		}
	}

exit_10:
	free(sale);
	mysql_stmt_close(mstmt);
	return retv;
}

struct block_chain {
	unsigned long blockid;
	char *blkbuf;
};

static const char *block_insert_sql = "INSERT INTO blockchain(blockid, " \
		       "hdr_hash, blockdata) VALUES(?, ?, ?)";

static int insert_first_block(MYSQL *mcon)
{
	int retv = 0;
	MYSQL_STMT *mstmt;
	MYSQL_BIND mbind[3];
	struct block_chain blkchain;
	unsigned long hash_len, blk_len;
	const struct bl_header *bhdr;
	struct sha256 sha;

	blkchain.blkbuf = malloc(512);
	if (!blkchain.blkbuf) {
		fprintf(stderr, "Out of Memory!\n");
		exit(100);
	}
	tok_block_init(27);

	mstmt = mysql_stmt_init(mcon);
	if (!mstmt) {
		fprintf(stderr, "mysql_stmt_init out of memory!\n");
		exit(100);
	}
	if (mysql_stmt_prepare(mstmt, block_insert_sql,
				strlen(block_insert_sql))) {
		fprintf(stderr, "mysql_stmt_prepare failed %s\n", sales_insert);
		fprintf(stderr, " %s\n", mysql_stmt_error(mstmt));
		exit(12);
	}

	memset(mbind, 0, sizeof(mbind));
	mbind[0].buffer_type = MYSQL_TYPE_LONG;
	mbind[0].buffer = &blkchain.blockid;
	mbind[0].is_unsigned = 1;

	mbind[1].buffer_type = MYSQL_TYPE_BLOB;
	mbind[1].buffer = sha.H;
	mbind[1].buffer_length = 32;
	mbind[1].length = &hash_len;

	mbind[2].buffer_type = MYSQL_TYPE_BLOB;
	mbind[2].buffer = blkchain.blkbuf;
	mbind[2].buffer_length = 512;
	mbind[2].length = &blk_len;

	if (mysql_stmt_bind_param(mstmt, mbind)) {
		fprintf(stderr, "mysql_stmt_bind_param failed: %s, %s\n",
				sales_insert, mysql_stmt_error(mstmt));
		retv = 5;
		goto exit_10;
	}

	blkchain.blockid = 0;
	blk_len = gensis_block(blkchain.blkbuf, 512);
	bhdr = (const struct bl_header *)blkchain.blkbuf;
	sha256_reset(&sha);
	sha256(&sha, (const unsigned char *)bhdr, sizeof(struct bl_header));
	hash_len = 32;

	if (mysql_stmt_execute(mstmt)) {
		fprintf(stderr, "mysql_stmt_execute failed: %s, %s\n",
				sales_insert, mysql_stmt_error(mstmt));
		retv = 6;
		goto exit_10;
	}

exit_10:
	free(blkchain.blkbuf);
	mysql_stmt_close(mstmt);
	return retv;
}
