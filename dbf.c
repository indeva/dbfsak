#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include "dbf.h"

#define FALLOC 5

// File type for Microsoft's Visual FoxPro files

#define VISUALTOILET 48

// Amount of excrement Microsoft puts _after_ the field specs in the header of
// Visual FoxPro files, part of Microsoft's "embrace and extend" strategy.

#define CRAP 263

// modifies passed text to truncate it on the right, removing trailing spaces

void rtrim(char *text)
{
	char *p;
	int len = strlen(text);
	int i;

	p = text;
	for (i = len - 1; i >= 0; i--) {
		if (*(p + i) == ' ')
			*(p + i) = '\0';
		else
			break;
	}
}

// modifies passed text to eliminate leading spaces

void ltrim(char *text)
{
	char *p, *q;

	p = q = text;
	while (*q == ' ')
		q++;

  while (*q != '\0')
	{
		*p = *q;
		p++;
		q++;
	}
	*p = '\0';

}

short fox2short(char *fox)
{
	char c[2];

	memcpy(c, fox + 1, 1);
	memcpy(c + 1, fox, 1);

	return (short)(*c);
}

bool is_null(char *fld)
{
	char *p = fld;
	int i = strlen(fld);
	int n;
	bool ret = true;

	for (n = 0; n < i; n++) {
		if (*p != ' ') {
			ret = false;
			break;
		}
		p++;
	}
	return ret;
}

void dbf_fspec_dump(dbf_fspec *d) {
	printf("%10s %c %6hu %3hu %6lu\n", d->name, d->type,
			d->length, d->decimals, d->offset);
}

/**
 * Fetch field spec and populate our dbf_fspec structure from it
 *
 */

dbf_fspec *dbf_fspec_from_dbf(char b[F_FSPEC_LEN], ulong l)
{
    dbf_fspec *d = malloc(sizeof(dbf_fspec));

	// raw spec
	memcpy(d->fbuf, b, F_FSPEC_LEN);

	// name
	memcpy(d->name, b, F_NAME_LEN);
	// for safety's sake...
	d->name[F_NAME_LEN - 1] = '\0';

	// type
	memcpy(&d->type, b + F_TYPE_OFS, F_TYPE_LEN);

	// length
	d->length = (int) *(b + F_LENGTH_OFS);

	// decimals
	d->decimals = (int) *(b + F_DECIMALS_OFS);

	// offset
	d->offset = l;

	// dbf_fspec_dump(d);

	return d;
#if 0
	dest = d->name;
	orig = b + F_NAME_OFS;
	memcpy(dest, orig, F_NAME_LEN);
	// safety measure
	*(d->name + F_NAME_LEN - 1) = '\0';

	// type
	dest = d->type;
	orig = b + F_TYPE_OFS;
	memcpy(byte, 1, F_TYPE_LEN);
	
	// length
	dest = d->length;
	orig = b + F_LENGTH_OFS;
	memcpy(byte, 1, F_LENGTH_LEN);
	d->length = (int) byte;

	// decimals
	dest = d->decimals;
	orig = b + F_DECIMALS_OFS;
	memcpy(byte, 1, F_DECIMALS_LEN);
	d->decimals = (int) byte;

	// offset
	d->offset = l;








    memcpy(d->fbuf, b, 32);
	p = b + F_TYPE_OFS;
	memcpy(d->name, p, F_TYPE_LEN);
    p = d->fbuf;
    memcpy(d->name, p, 11);
	// 2017-01-12 paulf: for safety's sake, terminate with null,
	// even though it should already be so
	p[10] = '\0';

    p = d->fbuf + 11;
    memcpy(&d->type, p, 1);
    p = d->fbuf + 16;
    memcpy(&d->length, p, 1);
    p = d->fbuf + 17;
    memcpy(&d->decimals, p, 1);
    d->offset = l;
    return d;
#endif
}



/* =====================================================================
 * This routine requires a nosql file (not a full rdb file) in the form:
 * fieldname fieldtype fieldlen fielddec
 * --------- --------- -------- --------
 * The field names don't matter, but they must be in that order.
 * This routine reads in the file and constructs the appropriate header
 * and field objects. Then it writes those back out to the dbf file.
 * N.B.: If an existing dbf file by <fname> already exists, it is
 * truncated and overwritten.
 * ================================================================== */

dbf_table *dbf_from_rdb(char *afname, char *fname)
{
	char buf[MAXPATH + 1];
	char rdb_n[11]; // name
	uchar rdb_t; // field type
	uint8_t rdb_l; // field length
	uint8_t rdb_d; // field decimals
	ulong rdb_o; // field offset
	char rdb_f[MAXPATH]; // filename
	char *g[4];
	char *p;
	dbf_fspec *f;
	unsigned int i;
	FILE *rdb_fp;
	dbf_table *t;

	if (strlen(afname) < MAXPATH)
		strcpy(rdb_f, afname);
	else {
		memcpy(rdb_f, afname, MAXPATH - 1);
		rdb_f[MAXPATH] = '\0';
	}


	rdb_fp = fopen(rdb_f, "r");

	if (rdb_fp) {

		t = malloc(sizeof(dbf_table));
		t->falloced = FALLOC;
		t->fields = malloc(sizeof(dbf_fspec *) * FALLOC);
		if (strlen(fname) < MAXPATH)
			strcpy(t->filename, fname);
		else {
			memcpy(t->filename, fname, MAXPATH - 1);
			t->filename[MAXPATH] = '\0';
		}

		// first two lines are irrelevant
		fgets(buf, MAXPATH, rdb_fp);
		fgets(buf, MAXPATH, rdb_fp);

		rdb_o = 1;
		t->fcount = 0;

		memset(buf, '\0', MAXPATH);
		while (fgets(buf, MAXPATH, rdb_fp)) {
			p = buf;
			while (*p == ' ') p++;
			if (*p == '\n' || *p == '\0')
				continue;

			g[0] = strtok(p, "\t");
			g[1] = strtok(NULL, "\t"); // type
			g[2] = strtok(NULL, "\t"); // length
			g[3] = strtok(NULL, "\t"); // decimals

			if (strlen(p) <= 10)
				strcpy(rdb_n, p);
			else {
				memcpy(rdb_n, p, 10);
				rdb_n[10] = '\0';
			}
			for (i = 0; i < strlen(rdb_n); i++)
				rdb_n[i] = toupper(rdb_n[i]);
			rdb_t = toupper(*g[1]); // type
			rdb_l = (uint8_t) atoi(g[2]); // length
			rdb_d = (uint8_t) atoi(g[3]); // decimals

			f = malloc(sizeof(dbf_fspec));
			strncpy(f->name, rdb_n, 10);
			f->name[10] = '\0';
			f->type = rdb_t;
			f->length = rdb_l;
			f->decimals = rdb_d;
			f->offset = rdb_o;

			memset(f->fbuf, '\0', F_FSPEC_LEN);
			memcpy(f->fbuf, f->name, F_NAME_LEN);
			memcpy(f->fbuf + F_TYPE_OFS, &f->type, F_TYPE_LEN);
			memcpy(f->fbuf + F_LENGTH_OFS, &f->length, F_LENGTH_LEN);
			memcpy(f->fbuf + F_DECIMALS_OFS, &f->decimals, F_DECIMALS_LEN);

			rdb_o += rdb_l;

			if (t->fcount == t->falloced) {
				t->falloced += FALLOC;
				t->fields = realloc(t->fields, t->falloced * sizeof(dbf_fspec *));
			}
			t->fields[t->fcount] = malloc(sizeof(dbf_fspec));
			t->fields[t->fcount] = f;
			t->fcount++;

			memset(buf, '\0', MAXPATH);
		}

		// initialize header values

		t->rlength = rdb_o;
		t->hlength = 32 + t->fcount * 32 + 1;
		t->version_id = 3;
		t->rcount = 0;
		memset(&t->last_update, '\0', 3);
		t->rbuf = malloc(t->rlength * sizeof(char));
		t->hbuf = malloc(t->hlength * sizeof(char));

		// set header values
		// buf will be used to write to file later

		p = t->hbuf;
		memcpy(p, &t->version_id, 1);
		p = t->hbuf + 1;
		memcpy(p, &t->last_update, 3);
		p = t->hbuf + 4;
		memcpy(p, &t->rcount, 4);
		p = t->hbuf + 8;
		memcpy(p, &t->hlength, 2);
		p = t->hbuf + 10;
		memcpy(p, &t->rlength, 2);

		fclose(rdb_fp);

		t->datafile = fopen(t->filename, "w+");

		// write out header info

		fwrite(t->hbuf, 1, 32, t->datafile);
		for (i = 0; i < t->fcount; i++)
			fwrite(t->fields[i]->fbuf, 1, 32, t->datafile);

		fputc(0x0d, t->datafile);

	}
    else
    	t = NULL;

    return t;
}

dbf_table *dbf_open(char *fn)
{
	char *p;
	ulong running_offset;
	unsigned int i;
	char mfilen[MAXPATH + 1];
	char ext[5];
	uchar len = 0;
	FILE *x;
	char fname[MAXPATH + 1];
	dbf_table *t;
	char buf32[32];

	strncpy(fname, fn, MAXPATH);
	fname[MAXPATH] = '\0';
	x = fopen(fname, "r");

	if (!x) return NULL;

		t = malloc(sizeof(dbf_table));

		t->datafile = x;
		strcpy(t->filename, fname);

		t->hbuf = malloc(sizeof(char) * 32);

		fread(t->hbuf, 1, 32, t->datafile);

		p = t->hbuf;

		// set header values

		memcpy(&t->version_id, p, 1);
		p = t->hbuf + 1;
		memcpy(&t->last_update, p, 3);
		p = t->hbuf + 4;
		memcpy(&t->rcount, p, 4);
		p = t->hbuf + 8;
		memcpy(&t->hlength, p, 2);
		p = t->hbuf + 10;
		memcpy(&t->rlength, p, 2);

		// create a record buffer

		t->rbuf = malloc((t->rlength + 1) * sizeof(char));

		// how many fields?

		if (t->version_id == VISUALTOILET)
			t->fcount = ((t->hlength - CRAP) / 32) - 1;
		else
			t->fcount = (t->hlength / 32) - 1;

		// build array of field specs

		t->fields = malloc(t->fcount * sizeof(dbf_fspec *));

		// now field descriptors

		running_offset = 1;

		for (i = 0; i < t->fcount; i++) {
			fread(buf32, 1, F_FSPEC_LEN, t->datafile);

			t->fields[i] = dbf_fspec_from_dbf(buf32, running_offset);
			memcpy(&len, t->fields[i]->fbuf + 16, 1);
			running_offset += len;
		}

		// memo file stuff (if applicable)

		switch (t->version_id) {
			case 0x03:
				// dBase III/FoxPro 2, no memo
				t->has_memo = false;
				break;
			case 0xf5:
				// FoxPro with memo
				strcpy(ext, ".fpt");
				t->has_memo = true;
				break;
			default:
				// we don't handle the others
				t->has_memo = false;
				break;
		}
		if (t->has_memo) {

			strcpy(mfilen, t->filename);
			p = strstr(mfilen, ".dbf");
			if (p)
				strcpy(p, ext);

			dbf_memo_init(&t->memo, mfilen);

		}

	return t;
}

void dbf_free(dbf_table *t)
{
	int i = t->fcount;

	if (t->datafile)
		fclose(t->datafile);

	for (i = 0; i < t->fcount; i++)
		free(t->fields[i]);
	free(t->rbuf);
	free(t->hbuf);

	// FIXME: memos?
}


// 0 on success, -1 on failure

int dbf_read(dbf_table *t, ulong recnum)
{
	int failed, numread, ret;

	failed = fseek(t->datafile, t->hlength + ((recnum - 1) * t->rlength), SEEK_SET);
	if (!failed) {
		numread = fread(t->rbuf, 1, t->rlength, t->datafile);
		if (numread != 0)
			ret = 0;
		else
			ret = -1;
	}
	else
		ret = -1;

	return ret;
}

bool dbf_is_deleted(dbf_table *t)
{
	if (t->rbuf)
		return (t->rbuf[0] == '*' ? true : false);
	else
		return true;
}

void escape_quotes( char *src, char *dest )
{
	for( ; *src != '\0'; src++, dest++ )
	{
		*dest = *src;
		if( *dest == '\'' )
		{
			dest++;
			*dest = '\'';
		}
	}
	*dest = '\0';
}

/*************************
 * Generic record output routine
 */

static void dbf_recout(dbf_table *t, char delim, int flags)
{
	char *tbuf;
	char *auxbuf = NULL;
	char *mbuf;
	int i, j;
	int memonum;

	if ((flags & F_OMITDEL) && dbf_is_deleted(t))
		return;

	if( !((flags & F_NOSQL) || (flags & F_SQL) || (flags & F_MYSQL) || (flags & F_SQL_INSERT))) {
		if (t->rbuf[0] == '*')
			putchar('*');
		else
			putchar(' ');
		putchar(delim);
	}

	// We're setting aside a whole record's worth of buffer space,
	// but initializing it with one field at a time.

	tbuf = malloc((t->rlength + t->fcount + sizeof('\n') + 1) * sizeof(char));
	for (i = 0; i < t->fcount; i++) {

		// copy a single field
		memcpy(tbuf, t->rbuf + t->fields[i]->offset, t->fields[i]->length);
		tbuf[t->fields[i]->length] = '\0';

		if (is_null( tbuf )) {
			if (flags & F_SQL || flags & F_MYSQL)
				printf("\\N%c", delim);
			else
				if (flags & F_SQL_INSERT ) {
				switch (t->fields[i]->type) {
					case 'C':
					case 'M':
						printf( "\'\'");
						break;
					case 'N':
						printf("0.0" );
						break;
					case 'L':
						printf( "\'f\'" );
						break;
					case 'D':
						printf( "\\N" );
				}
				if (i != t->fcount - 1) {
					putchar(delim);
				}
			}
			else if (flags & F_NOSQL)
				putchar(delim);
			else {
				if (t->fields[i]->type == 'M') {
					// dBase puts NULLs in empty
					// memo fields
					if (strlen(tbuf) < 10) {
						for (j = 0; j < 10; j++)
							if (tbuf[j] == '\0')
								tbuf[j] = ' ';
					}
				}
				if (!(flags & F_TRIM)) {
					printf("%s", tbuf);
				}
				if (i != t->fcount - 1)
				{
					putchar(delim);
				}
			}
			continue;
		}

		switch (t->fields[i]->type) {
			case 'C':
				if ((flags & F_TRIM) || (flags & F_SQL_INSERT)) {
					ltrim(tbuf);
					rtrim(tbuf);
				}
				else
					tbuf[t->fields[i]->length] = '\0';
				if (flags & F_SQL_INSERT) {
					auxbuf = malloc(((t->rlength + t->fcount * 2) + sizeof('\n') + 1) * sizeof(char) );
					escape_quotes( tbuf, auxbuf );
					printf( "\'%s\'", auxbuf);
					free(auxbuf);
				}
				else
					printf("%s", tbuf);
				break;
			case 'N':
				if ((flags & F_TRIM) || (flags & F_SQL_INSERT)) {
					ltrim(tbuf);
					rtrim(tbuf);
				}
				else
					tbuf[t->fields[i]->length] = '\0';
				printf("%s", tbuf);
				break;
			case 'M':
				if ((flags & F_SQL) || (flags & F_MYSQL) || (flags & F_SQL_INSERT)) {
					memonum = atol(tbuf);
					mbuf = dbf_memo_get_memo(&t->memo, memonum);
					if (mbuf) {
						printf("%s", mbuf);
						free(mbuf);
						mbuf = NULL;
					}
					else
						printf("\\N");
				}
				else {
					if (flags & F_TRIM || flags & F_NOSQL)
						ltrim(tbuf);
					printf("%s", tbuf);
				}
				break;
			case 'L':
				if (*tbuf == 'Y' || *tbuf == 'y' || *tbuf == 'T' || *tbuf == 't') {
					if( flags & F_SQL_INSERT )
						printf( "\'t\'" );
					else if (flags & F_SQL || flags & F_MYSQL)
						putchar('t');
					else if (flags & F_NOSQL)
						putchar('T');
					else
						printf("%s", tbuf);
				}
				else {
					if( flags & F_SQL_INSERT )
						printf( "\'f\'" );
					else if (flags & F_SQL || flags & F_MYSQL)
						putchar('f');
					else if (flags & F_NOSQL)
						putchar('F');
					else
						printf("%s", tbuf);
				}
				break;
			case 'D': // defaults to ISO format
				if ((flags & F_SQL) || (flags & F_MYSQL) || (flags & F_SQL_INSERT) || (flags & F_NOSQL)) {
					printf(
						"\'%c%c%c%c-%c%c-%c%c\'",
						tbuf[0], tbuf[1], tbuf[2], tbuf[3],
						tbuf[4], tbuf[5],
						tbuf[6], tbuf[7] );
				}
				else
					printf("%s", tbuf);
				break;
		}
		if (i != t->fcount - 1)
			putchar(delim);
	} // for
	if( !(flags & F_SQL_INSERT) )
		putchar('\n');
	free(tbuf);
}

// dump records in text/delimited format

void dbf_dump(dbf_table *t, char rsep, int flags) {

	unsigned long n, end = t->rcount;

	for (n = 1; n <= end; n++) {
		dbf_read(t, n);
		dbf_recout(t, rsep, flags);
	}

}

void dbf_dump_header(dbf_table *t)
{
	printf("\n***** xBase File Specs *****\n\n");

	printf("Filename:      %s\n", t->filename);
	printf("Version ID:    %x\n", (unsigned) t->version_id);
	printf("Record count:  %u\n", t->rcount);
	printf("Header length: %hu\n", t->hlength);
	printf("Record length: %hu\n", t->rlength);
	printf("Field count:   %u\n\n", t->fcount);

}


void dbf_dump_fspecs_header()
{
	printf("*** Field Specs ***\n\n");
	printf("   Name    T Length Dec Offset\n");
	printf("---------- - ------ --- ------\n");
}

void dbf_dump_fspecs(dbf_table *t)
{
	unsigned int i;

	dbf_dump_fspecs_header();

	for (i = 0; i < t->fcount; i++)
		dbf_fspec_dump(t->fields[i]);

	putchar('\n');

}

void dbf_header2sql(dbf_table *t)
{
	ulong i;
	bool first = true;
	char fldname[11];
	char *fname;
	char *p;

	fname = malloc((strlen(t->filename) + 1) * sizeof(char));
	strcpy(fname, basename(t->filename));
	// strcpy(fname, t->filename);
	// sloppily remove .dbf from filename ;-}
	p = strchr(fname, '.');
	if (p)
		*p = '\0';

	printf("create table %s (\n", fname);
	for (i = 0; i < t->fcount; i++) {
		if (!first)
			printf(",\n");
		else
			first = false;

		// convert name string to lowercase
		strcpy(fldname, t->fields[i]->name);

		p = fldname;
		while (*p) {
			*p = tolower(*p);
			p++;
		}

		switch (t->fields[i]->type) {
			case 'C':
				printf("\t%s char(%hu)", fldname, t->fields[i]->length);
				break;
			case 'M':
				printf("\t%s varchar", fldname);
				break;
			case 'D':
				printf("\t%s date", fldname);
				break;
			case 'N':
				if (t->fields[i]->decimals == 0)
					printf("\t%s int", fldname);
				else
					printf("\t%s numeric(%hu, %hu)", fldname, t->fields[i]->length, t->fields[i]->decimals);
				break;
			case 'L':
				printf("\t%s bool", fldname);
				break;
		} // switch
	} // for
	printf(" );\n");
}

void dbf_header2sql_insert(int dbType, dbf_table *t)
{
	ulong i;
	char fldname[11];
	char *fname;
	char *p;
	bool MySQL;

	MySQL = dbType & F_MYSQL;

	fname = malloc((strlen(t->filename) + 1) * sizeof(char));
	strcpy(fname, basename(t->filename));
	p = strchr(fname, '.');
	if (p)
		*p = '\0';

	p = fname;
	while (*p) {
		*p = tolower(*p);
		p++;
	}

	/*
	 * Originally, this was set up so that dumped tables automatically
	 * got an integer primary key field named after the table. I don't
	 * know why I did this, as SQL doesn't require it. Must have been
	 * a convenience for myself. But I've removed it. DBF dumping in
	 * SQL format is spotty anyway. There's no provision for foreign keys,
	 * no provision for serial fields, etc. So even if you dump the
	 * header for a DBF file in SQL format, you're going to have to
	 * edit it.
	 */

	if (MySQL)
		// printf("create table `%s`\n(\t`%s_id` int not null auto_increment,\n", fname, fname );
		printf("create table `%s` (\n", fname);
	else
		// printf("create table %s\n(\t%s_id int primary key", fname, fname );
		printf("create table %s (\n", fname );


	for( i = 0; i < t->fcount; i++ )
	{
		// 2013-12-07 Jorg Muller
		// if (!MySQL)
		//	printf( ",\n" );

		// convert name string to lowercase
		strcpy(fldname, t->fields[i]->name);

		p = fldname;
		while (*p) {
			*p = tolower(*p);
			p++;
		}

		if (MySQL) {
			switch (t->fields[i]->type) {

				case 'C':
					printf("\t`%s` varchar(%hu)", fldname, t->fields[i]->length);
					break;
				case 'M':
					printf("\t`%s` text", fldname);
					break;
				case 'D':
					printf("\t`%s` date", fldname);
					break;
				case 'N':
					if (t->fields[i]->decimals == 0)
						printf("\t`%s` int", fldname);
					else
						printf("\t`%s` numeric(%hu, %hu)", fldname, t->fields[i]->length, t->fields[i]->decimals);
					break;
				case 'L':
					printf("\t`%s` bool", fldname);
					break;
			} // switch

			// 2013-12-07 Jorg Muller
			printf(" null"); // MySQL specific
		}
		else
			switch (t->fields[i]->type) {
				case 'C':
					printf("\t%s char(%hu)", fldname, t->fields[i]->length);
					break;
				case 'M':
					printf("\t%s varchar", fldname);
					break;
				case 'D':
					printf("\t%s date", fldname);
					break;
				case 'N':
					if (t->fields[i]->decimals == 0)
						printf("\t%s int", fldname);
					else
						printf("\t%s numeric(%hu, %hu)", fldname, t->fields[i]->length, t->fields[i]->decimals);
					break;
				case 'L':
					printf("\t%s bool", fldname);
					break;
			} // switch

		// 2013-12-07 Jorg Muller
		// if (MySQL)
		//	printf( " null,\n" );

		if (i < t->fcount - 1)
			printf(",");

		printf("\n");

	} // for
	if (MySQL) {
		// printf("\tPRIMARY KEY(`%s_id`)\n",fname);
		// 2013-12-07 Jorg Muller
		printf(") TYPE=InnoDB COMMENT='Transferred by dbfsak. Original xbase file: %s.dbf';\n", fname);
	}
	else
		printf(");\n");
}

// dump all records to SQL (COPY)

void dbf_data2sql(dbf_table *t, char delim, int flags)
{
	char *p, *fname;
	unsigned int n, end;

	fname = malloc((strlen(t->filename) + 1) * sizeof(char));
	strcpy(fname, t->filename);
	// sloppily remove .dbf from filename ;-}
	p = strchr(fname, '.');
	if (p)
		*p = '\0';

	printf("copy \"%s\" from stdin;\n", fname);
	end = t->rcount;
	for (n = 1; n <= end; n++) {
		dbf_read(t, n);
		dbf_recout(t, delim, flags);
	}
	printf("\\.\n");
}


// dump all records to SQL (INSERT)

void dbf_data2sql_insert( dbf_table *t, char delim, int flags )
{
	bool MySQL;
	char *p, *fname;
	unsigned int i, n, end;
	char fldname[11];

	MySQL = flags & F_MYSQL;

	fname = malloc( (strlen(t->filename) + 1) * sizeof(char) );
	strcpy(fname, t->filename);
	// sloppily remove .dbf from filename ;-}
	p = strchr(fname, '.');
	if (p)
		*p = '\0';

	p = fname;
	while (*p) {
		*p = tolower(*p);
		p++;
	}

	end = t->rcount;
	for (n = 1; n <= end; n++) {

		dbf_read(t, n);
		if ((flags & F_OMITDEL) && dbf_is_deleted(t))
			continue;

		if (MySQL)
			printf( "insert into `%s`\n\t(", fname );
		else
			printf( "insert into %s\n\t(", fname );

		for (i = 0; i < t->fcount; i++) {
			printf( ", " );
			strcpy(fldname, t->fields[i]->name);

			p = fldname;
			while (*p) {
				*p = tolower(*p);
				p++;
			}
			if (MySQL)
				printf( "`%s`", fldname );
			else
				printf( "%s", fldname );
		}

		printf(")\n\tvalues ( %d, ", n);
		dbf_recout(t, delim, flags);
		printf(");\n");
	}
}


void dbf_dump_nosql(dbf_table *t, char *afname, int flags)
{

    unsigned int i, j;
    FILE *rsfile;

    // header first

    for (i = 0; i < t->fcount; i++) {
		if (i != 0)
			putchar('\t');
		printf("%s", t->fields[i]->name);
    }

    putchar('\n');

    for (i = 0; i < t->fcount; i++) {
		if (i != 0)
			putchar('\t');
		for (j = 0; j < t->fields[i]->length; j++)
			putchar('-');
    }
    putchar('\n');

    // now records

    for (i = 1; i <= t->rcount; i++) {
		dbf_read(t, i);
		dbf_recout(t, '\t', flags);
    }

    // dump header and such to afname nosql file
    rsfile = fopen(afname, "w+");
    if (rsfile) {
	fprintf(rsfile, "field_name\tfield_type\tfield_len\tfield_dec\n");
	fprintf(rsfile, "----------\t----------\t---------\t---------\n");
	for (i = 0; i < t->fcount; i++)
	    fprintf(rsfile, "%s\t%c\t%d\t%d\n", t->fields[i]->name, t->fields[i]->type, t->fields[i]->length, t->fields[i]->decimals);
	fclose(rsfile);
    }

}

// 2017-01-12 paulf: go from 1024 to 4000 to accommodate a user with large records
// According to one Internet source, 4K is the official limit

// record size limit
#define MAXREC 4000

#define MIN(a,b) (a < b ? a : b)

/*
 * This routine appends data from an rdb file to an xBase file.
 * To do so, it must find correspondences between fields in the rdb file
 * and the xBase file. It copies like named fields from the rdb into
 * their proper positions in the xBase file.
 * NOTA BENE: This routine can be a prime example of "garbage in, garbage
 * out". If, for example, the field lengths in the rdb file don't
 * correspond to those in the xBase file, you'll get garbage data in
 * the xBase file; no way around it.
 */

void dbf_rdb_append(dbf_table *t, char *s)
{
	char fn[MAXPATH + 1];
	FILE *rdbfile;
	char recbuf[MAXREC];
	char test;

	unsigned int nfields, i, j;
	bool done;
	int fvect[t->fcount];
	char *a, *b, *p;

	if (strlen(s) < MAXPATH)
		strcpy(fn, s);
	else {
		memcpy(fn, s, MAXPATH - 1);
		fn[MAXPATH] = '\0';
	}

	rdbfile = fopen(fn, "r");
	if (!rdbfile) {
		printf("Can't open %s!\n", fn);
		return;
	}

	freopen(t->filename, "r+", t->datafile);

	// parse first line to get field names
	// we build an array of integers to represent the coordinates of
	// fields in the rdb file.

	nfields = 0;
	if (fgets(recbuf, MAXREC, rdbfile)) {
		p = recbuf;
		*p = toupper(*p);

		while (*p++)
			*p = toupper(*p);
		// append a tab to the end
		a = strchr(recbuf, '\n');
		*a = '\t';
		a = recbuf;

		// first = true;
		done = false;

		// "zero" the field vector
		for (i = 0; i < t->fcount; i++)
			fvect[i] = -1;
		// build the field vector map
		while (!done) {

			b = strchr(a, '\t');
			if (b) {
				*b = '\0';
			}
			else
				break;
			if (a) {
				// found = false;
				for (i = 0; i < t->fcount; i++) {
					// 2013-04-08 paulf: this misfires on fields where the name of one field
					// is a substring of the other
					// if (!strncmp(t->fields[i]->name, a, strlen(a))) {
					if (!strcmp(t->fields[i]->name, a)) {
						fvect[i] = nfields;
						// found = true;
						nfields++;
						break;
					}
				}
			}
			else
				done = true;

			a = ++b;

		} // while
		
	}
	else
		return;

	// throw away the next line
	fgets(recbuf, MAXREC, rdbfile);

	// test for ^Z at end, which will throw off append
	fseek(t->datafile, -1, SEEK_END);
	fread(&test, sizeof(char), 1, t->datafile);
	if (test != 26)
		fseek(t->datafile, 0, SEEK_END);

	// populate the dbf file

	// 2013-04-07 paulf: changed t->rlength to MAXREC; avoids SEGFAULTs sometimes
	// while (fgets(recbuf, t->rlength, rdbfile)) {
	while (fgets(recbuf, MAXREC, rdbfile)) {

		// append tab to end of buffer
		p = strchr(recbuf, '\n');
		*p = '\t';
		a = recbuf;
		memset(t->rbuf, ' ', t->rlength);

		for (i = 0; i < nfields; i++) {

			b = strchr(a, '\t');
			if (b)
				*b = '\0';
			else
				break;

			for (j = 0; j < t->fcount; j++) {
				if (fvect[j] == i) {
					memcpy(t->rbuf + t->fields[j]->offset, a, MIN(t->fields[j]->length, strlen(a)));
					break;
				}
			}

			a = ++b;

		}
		
		// intact rbuf, so write out to file
		fwrite(t->rbuf, 1, t->rlength, t->datafile);
		t->rcount++;
	}

	fclose(rdbfile);
	fseek(t->datafile, 4, SEEK_SET);

	// 2017-01-18 paulf: be more precise about lengths
	// fwrite(&t->rcount, 1, sizeof(ulong), t->datafile);
	fwrite(&t->rcount, 1, sizeof(uint32_t), t->datafile);
	freopen(t->filename, "r", t->datafile);
}


