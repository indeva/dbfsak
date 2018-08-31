#ifndef DBF_H
#define DBF_H

#include <stdio.h>
// 2017-01-18 paulf: include file to define precise integer/long types
#include <stdint-gcc.h>
#include "bool.h"

#ifndef uchar
#define uchar unsigned char
#endif

#ifndef ushort
#define ushort unsigned short
#endif

#ifndef ulong
#define ulong unsigned long
#endif

// 2017-01-12 paulf: changed from 127 to 4096
#define MAXPATH 4096

#define F_BROWSE 0x01
#define F_FDESC  0x02
#define F_HDR    0x04
#define F_SQLERR 0x07
#define F_SQL    0x08
#define F_TRIM   0x10
#define F_RDB    0x20
#define F_NOSQL  0x40
#define F_APPEND 0x80
#define F_MYSQL  0x100
#define F_SQL_INSERT    0x0200
#define F_OMITDEL 0x0400

#define F_HDR_VERSION_OFS 0
#define F_HDR_VERSION_LEN 1
#define F_HDR_NUMRECS_OFS 4
#define F_HDR_NUMRECS_LEN 4
#define F_HDR_HDRLEN_OFS 8
#define F_HDR_HDRLEN_LEN 2
#define F_HDR_RECLEN_OFS 10
#define F_HDR_RECLEN_LEN 2
#define F_HDR_LEN 32

/**
 * Raw field specs:
 * significance posn length
 * ------------ ---- ------
 * name         0    11
 * type         11   1
 * junk*        12   4
 * length       16   1
 * decimals     17   1
 * junk*        18   14
 * TOTAL             32
 *
 * * "junk" just means we don't need the info for our purposes
 */

#define F_NAME_OFS 0
#define F_NAME_LEN 11
#define F_TYPE_OFS 11
#define F_TYPE_LEN 1
#define F_LENGTH_OFS 16
#define F_LENGTH_LEN 1
#define F_DECIMALS_OFS 17
#define F_DECIMALS_LEN 1
#define F_FSPEC_LEN 32

typedef struct
{
   char fbuf[32];
   char name[11];
   uchar type;
   // 2017-01-12 paulf: change from uchar to unsigned int
   // 2017-01-18 paulf: changes needed to overcome changes of length on 64 bit platforms
   uint8_t length;
   uint8_t decimals;
   unsigned long offset;

} dbf_fspec;

void dbf_dump_fspecs_header();
dbf_fspec *dbf_fspec_from_dbf(char[32], ulong);
void dbf_fspec_dump(dbf_fspec *);

typedef struct
{
	char memofilename[MAXPATH];
	FILE *memofp;
	unsigned short blocksize;
	unsigned int nextfree;

	int blocksig;
	unsigned int memolen;
	char *mbuf;

} dbf_memo;

void dbf_memo_init(dbf_memo *, char *);
void dbf_memo_free(dbf_memo *);
char *dbf_memo_get_memo(dbf_memo *, long);

typedef struct
{
   // main dbf header from file

   uchar version_id;
   char last_update[3];
   // 2017-01-17 paulf change from ulong to uint32_t
   uint32_t rcount;
   ulong falloced;
   // 2017-01-17 paulf change from ushort to uint16_t
   uint16_t hlength;
   uint16_t rlength;

   unsigned int fcount;
   char filename[MAXPATH];
   FILE *datafile;
   char *rbuf;
   char *hbuf;

   bool has_memo;
   dbf_memo memo;

   dbf_fspec **fields;


} dbf_table;

dbf_table *dbf_from_rdb(char *, char *);
dbf_table *dbf_open(char *);
void dbf_free(dbf_table *);
int dbf_read(dbf_table *, ulong);

// dump routines

void dbf_dump(dbf_table *, char, int);
void dbf_dump_header(dbf_table *);
void dbf_dump_fspecs(dbf_table *);
void dbf_dump_nosql(dbf_table *, char *, int);
void dbf_header2sql(dbf_table *);
void dbf_header2sql_insert(int, dbf_table *);
void dbf_data2sql(dbf_table *, char, int);
void dbf_data2sql_insert(dbf_table *, char, int);
void dbf_rdb_append(dbf_table *, char *);

#endif
