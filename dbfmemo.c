#include <string.h>
#include <stdlib.h>
#include <dbf.h>

/* ====== .fpt file format =======
	Header is 512 bytes, most of which is unused
	All numeric values outside of actual memo fields are represented
	in left to right order in hexadecimal.
	First four bytes are a pointer to the next free block.
	Next two bytes are unused.
	Next two bytes are the size of a block.
	Memos may occupy more than one consecutive block, and start on even
	block boundaries.
	The address of the block is the number of the block multiplied by the
	block size.
	Within a memo, the first four bytes are the block signature; 0 for
	picture data and 1 for text.
	Next four bytes are the length of the memo in bytes.
	What follows is the block data. The actual data in the block is not
	terminated by anything, so the length of the block is important.
	What follows from the end of the data to the end of the block may be
	garbage.
================================== */

// convert the odd FoxPro file integer format to int
static unsigned int fp2int(unsigned int n)
{
	int m = 
		((n << 24) & 0xff000000) +
		((n << 8) & 0x00ff0000) +
		((n >> 24) & 0x000000ff) +
		((n >> 8) & 0x0000ff00);
	return m;
}

static unsigned int fp2short(unsigned short n)
{
	int m = (n << 8) | (n >> 8);
	return m;
}


void dbf_memo_init(dbf_memo *m, char *fname)
{
	strcpy(m->memofilename, fname);
	m->memofp = fopen(m->memofilename, "r");

	if (m->memofp) {
		fread(&m->nextfree, 4, 1, m->memofp);
		m->nextfree = fp2int(m->nextfree);
		fread(&m->blocksize, 2, 1, m->memofp); // unused
		fread(&m->blocksize, 2, 1, m->memofp);
		m->blocksize = fp2short(m->blocksize);
	}
	else
		m->blocksize = 0;

	m->blocksig = 0;
	m->memolen = 0;
	m->mbuf = NULL;
}

void dbf_memo_free(dbf_memo *m)
{
	if (m->memofp)
		fclose(m->memofp);
	if (m->mbuf) {
		free(m->mbuf);
		m->mbuf = NULL;
	}
}

// have to kill newlines in memos...

char *dbf_memo_get_memo(dbf_memo *m, long n)
{
	char *p;
	int i;
	
	fseek(m->memofp, n * m->blocksize, SEEK_SET);
	fread(&m->blocksig, 4, 1, m->memofp);
	m->blocksig = fp2int(m->blocksig);
	fread(&m->memolen, 4, 1, m->memofp);
	m->memolen = fp2int(m->memolen);

	m->mbuf = malloc((m->memolen + 1) * sizeof(char));
	fread(m->mbuf, sizeof(char), m->memolen, m->memofp);

	// remove carriage returns
	p = m->mbuf;
	for (i = 0; i < m->memolen; i++) {
		if (*p == '\r')
			*p = '\\';
	        p++;
	}

	m->mbuf[m->memolen] = '\0';
	
	return m->mbuf;
}
