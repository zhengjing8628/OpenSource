#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "segy.h"
#include <assert.h>

typedef struct { /* complex number */
        float r,i;
} complex;

#define NINT(x) ((int)((x)>0.0?(x)+0.5:(x)-0.5))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

int optncr(int n);
void cc1fft(complex *data, int n, int sign);
void rc1fft(float *rdata, complex *cdata, int n, int sign);

int readShotData3D(char *filename, float *xrcv, float *yrcv, float *xsrc, float *ysrc, float *zsrc, int *xnx, complex *cdata, int nw, int nw_low, int nshots, int nx, int ny, int ntfft, int mode, float scale, int verbose)
{
	FILE *fp;
	segy hdr;
	size_t nread;
	int fldr_shot, sx_shot, sy_shot, itrace, one_shot, igath, iw;
	int end_of_file, nt, nxy;
	int *isx, *igx, *isy, *igy, k, l, m, j;
	int samercv, samesrc, nxrk, nxrm, maxtraces, ixsrc;
	float scl, scel, *trace, dt;
	complex *ctrace;

    nxy = nx*ny;

	/* Reading first header  */

	if (filename == NULL) fp = stdin;
	else fp = fopen( filename, "r" );
	if ( fp == NULL ) {
		fprintf(stderr,"input file %s has an error\n", filename);
		perror("error in opening file: ");
		fflush(stderr);
		return -1;
	}

	fseek(fp, 0, SEEK_SET);
	nread = fread( &hdr, 1, TRCBYTES, fp );
	assert(nread == TRCBYTES);
	if (hdr.scalco < 0) scl = 1.0/fabs(hdr.scalco);
	else if (hdr.scalco == 0) scl = 1.0;
	else scl = hdr.scalco;
	if (hdr.scalel < 0) scel = 1.0/fabs(hdr.scalel);
	else if (hdr.scalel == 0) scel = 1.0;
	else scel = hdr.scalel;

	fseek(fp, 0, SEEK_SET);

	nt = hdr.ns;
	dt = hdr.dt/(1E6);

	trace  = (float *)calloc(ntfft,sizeof(float));
	ctrace = (complex *)malloc(ntfft*sizeof(complex));
	isx = (int *)malloc((nxy*nshots)*sizeof(int));
	igx = (int *)malloc((nxy*nshots)*sizeof(int));
	isy = (int *)malloc((nxy*nshots)*sizeof(int));
	igy = (int *)malloc((nxy*nshots)*sizeof(int));


	end_of_file = 0;
	one_shot    = 1;
	igath       = 0;

	/* Read shots in file */

	while (!end_of_file) {

		/* start reading data (shot records) */
		itrace = 0;
		nread = fread( &hdr, 1, TRCBYTES, fp );
		if (nread != TRCBYTES) { /* no more data in file */
			break;
		}

		sx_shot  = hdr.sx;
        sy_shot  = hdr.sy;
		fldr_shot  = hdr.fldr;
		isx[igath] = sx_shot;
        isy[igath] = sy_shot;
		xsrc[igath] = sx_shot*scl;
		ysrc[igath] = sy_shot*scl;
		zsrc[igath] = hdr.selev*scel;
		xnx[igath]=0;
		while (one_shot) {
			igx[igath*nxy+itrace] = hdr.gx;
            igy[igath*nxy+itrace] = hdr.gy;
			xrcv[igath*nxy+itrace] = hdr.gx*scl;
			yrcv[igath*nxy+itrace] = hdr.gy*scl;

			nread = fread( trace, sizeof(float), nt, fp );
			assert (nread == hdr.ns);

			/* transform to frequency domain */
			if (ntfft > hdr.ns) 
			memset( &trace[nt-1], 0, sizeof(float)*(ntfft-nt) );

			rc1fft(trace,ctrace,ntfft,-1);
			for (iw=0; iw<nw; iw++) {
				cdata[igath*nxy*nw+iw*nxy+itrace].r = scale*ctrace[nw_low+iw].r;
				cdata[igath*nxy*nw+iw*nxy+itrace].i = scale*mode*ctrace[nw_low+iw].i;
			}
			itrace++;
			xnx[igath]+=1;

			/* read next hdr of next trace */
			nread = fread( &hdr, 1, TRCBYTES, fp );
			if (nread != TRCBYTES) { 
				one_shot = 0;
				end_of_file = 1;
				break;
			}
			if ((sx_shot != hdr.sx) || (sy_shot != hdr.sy) || (fldr_shot != hdr.fldr)) break;
		}
		if (verbose>2) {
			vmess("finished reading shot x=%d y=%d (%d) with %d traces",sx_shot,sy_shot,igath,itrace);
		}

		if (itrace != 0) { /* end of shot record */
			fseek( fp, -TRCBYTES, SEEK_CUR );
			igath++;
		}
		else {
			end_of_file = 1;
		}
	}

	free(ctrace);
	free(trace);

	return 0;
}