#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dbf.h"

static char version[] = VERSION;

void usage()
{
	printf("\ndbfsak -- xBase Swiss Army Knife\n");
	printf("Usage: dbfsak OPTIONS filename\n");
	printf("\tOPTIONS are:\n");
	printf("\t-a <filename>: append from rdb/nosql file\n");
	printf("\t-b: show records\n");
	printf("\t-d: set field delimiter (default is '|')\n");
	printf("\t-e: show field descriptions\n");
	printf("\t-h: help!\n");
	printf("\t-i: display header info\n");
	printf("\t-m: dump in MySQL SQL script format\n");
	printf("\t-n <filename>: dump content to nosql/rdb file (on STDOUT)\n");
	printf("\t\tand make <filename> structure file\n");
	printf("\t-r <filename>: make dbf from <filename> rdb/nosql file\n");
	printf("\t-s: dump in PostreSQL SQL script format (COPY)\n");
	printf("\t-S: dump in PostreSQL SQL script format (INSERT)\n");
	printf("\t-t: trim browse fields\n");
	printf("\t-v: display version\n");
	printf("\t-x: omit deleted records\n");
	printf("NOTE: -s parameter without -b, -e, or -i does nothing\n\n");
}

void open_error(char *fname)
{
	printf("Unable to open %s. Aborting.\n", fname);
}

void show_version()
{
	printf("dbfsak version %s\n", version);
}

#if 0

dbview options:
	-b browse
	-d set delimiter
	-e show field descriptions
	-h help
	-i display db info
	-o omit db records
	-r reserve fieldnames from being translated
	-t trim browse fields
	-v display version

#endif

// externs for getopt()
extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
	char *fname, *afname = NULL;
	int ret;
	long flags = 0;
	char *sdelim = ":";
	char delim = '|';
	// 2017-01-12 paulf: WTF? I don't know what this is or how it got here
	// char *user;
	dbf_table *mydbf;

	opterr = 1;
	while ((ret = getopt(argc, argv, "a:bd:ehimn:r:sStvx")) != -1)
	{
		switch (ret) {
		case 'a':
			// rdb_append
			flags |= F_APPEND;
			afname = malloc((strlen(optarg) + 1) * sizeof(char));
			strcpy(afname, optarg);
			break;
		case 'b':
			// browse
			flags |= F_BROWSE;
			break;
		case 'd':
			// set delimiter
			sdelim = optarg;
			if (strcmp(sdelim, "\\t") == 0)
				delim = '\t';
			else
				delim = *sdelim;
			break;
		case 'e':
			// show field descriptions
			flags |= F_FDESC;
			break;
		case 'h':
			// help
			usage();
			return (0);
		case 'i':
			// display header info
			flags |= F_HDR;
			break;
		case 'm':
			// mySQL output
			flags |= F_MYSQL;
			break;
		case 'n':
			// dump table to nosql database
			flags |= F_NOSQL;
			afname = malloc((strlen(optarg) + 1) * sizeof(char));
			strcpy(afname, optarg);
			break;

		case 'r':
			// make table from rdb file
			flags |= F_RDB;
			afname = malloc((strlen(optarg) + 1) * sizeof(char));
			strcpy(afname, optarg);
			break;

		case 's':
			// SQL output
			flags |= F_SQL;
			break;
		case 'S':
			// SQL output
			flags |= F_SQL_INSERT;
			break;
		case 't':
			// trim browse fields
			flags |= F_TRIM;
			break;
		case 'x':
			// omit deleted records
			flags |= F_OMITDEL;
			break;
		case 'v':
			// version
			show_version();
			return (0);
		default:
			usage();
			return (0);
		}						// switch
	}							// while

	if (flags == 0) {
		usage();
		return (0);
	}

	// 2017-01-12 paulf: WTF? I don't know what this is or how it got here
	// user = getlogin();

	fname = malloc((strlen(argv[optind]) + 1) * sizeof(char));
	strcpy(fname, argv[optind]);

	if (flags & F_RDB) {
		mydbf = dbf_from_rdb(afname, fname);
		return 0;
	}

	mydbf = dbf_open(fname);
	if (!mydbf) {
		open_error(fname);
		exit(0);
	}

	if (flags & F_NOSQL) {
		dbf_dump_nosql(mydbf, afname, flags);
		return 0;
	}

	if (flags & F_APPEND) {
		dbf_rdb_append(mydbf, afname);
		return 0;
	}

	if( (flags & F_SQL) || (flags & F_SQL_INSERT) || (flags & F_MYSQL))
	{
		if (!(flags & F_SQLERR))
		{
			usage();
			return (0);
		}

		if (flags & F_HDR || flags & F_FDESC)
		{
			if( flags & F_SQL_INSERT )
				if ( flags & F_MYSQL )
					dbf_header2sql_insert(F_MYSQL,mydbf);
				else
					dbf_header2sql_insert(F_SQL,mydbf);
			else
				dbf_header2sql(mydbf);
		}
		else if (flags & F_BROWSE) {
			if( flags & F_SQL ) {
				delim = '\t';
				dbf_data2sql(mydbf, delim, flags);
			}
			else	{// SQL_INSERT
				delim = ',';
				dbf_data2sql_insert( mydbf, delim, flags );
			}
		}
	}
	else {
		// non-SQL
		
		if (flags & F_HDR)
			dbf_dump_header(mydbf);
		if (flags & F_FDESC)
			dbf_dump_fspecs(mydbf);
		if (flags & F_BROWSE)
			dbf_dump(mydbf, delim, flags);

	}

	dbf_free(mydbf);

	return 0;

}
