#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include "segy.h"

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define NINT(x) ((int)((x)>0.0?(x)+0.5:(x)-0.5))

/**
* gets sizes, sampling and min/max values of a SU file
*
*   AUTHOR:
*           Jan Thorbecke (janth@xs4all.nl)
*           The Netherlands 
**/

void vmess(char *fmt, ...);
void verr(char *fmt, ...);
int optncr(int n);

int getFileInfo3D(char *filename, int *n1, int *n2, int *n3, int *ngath, float *d1, float *d2, float *d3, float *f1, float *f2, float *f3, float *sclsxgxsygy, int *nxm)
{
    FILE    *fp;
    size_t  nread, data_sz;
	off_t   bytes, ret, trace_sz, ntraces;
    int     sx_shot, sy_shot, gx_start, gx_end, gy_start, gy_end, itrace, one_shot, igath, end_of_file, fldr_shot;
    int     verbose=1, igy, nsx, nsy;
    float   scl, *trace, dxsrc, dxrcv, dysrc, dyrcv;
    segy    hdr;
    
    if (filename == NULL) { /* read from input pipe */
		*n1=0;
		*n2=0;
        *n3=0;
		return -1; /* Input pipe */
	}
    else fp = fopen( filename, "r" );
	if (fp == NULL) verr("File %s does not exist or cannot be opened", filename);
    nread = fread( &hdr, 1, TRCBYTES, fp );
    assert(nread == TRCBYTES);
    ret = fseeko( fp, 0, SEEK_END );
	if (ret<0) perror("fseeko");
    bytes = ftello( fp );

    *n1 = hdr.ns;
    if ( (hdr.trid == 1) && (hdr.dt != 0) ) {
        *d1 = ((float) hdr.dt)*1.e-6;
        *f1 = ((float) hdr.delrt)/1000.;
    }
    else {
        *d1 = hdr.d1;
        *f1 = hdr.f1;
    }
    *f2 = hdr.f2;
    *f3 = hdr.gy;

    data_sz = sizeof(float)*(*n1);
    trace_sz = sizeof(float)*(*n1)+TRCBYTES;
    ntraces  = (int) (bytes/trace_sz);

    if (hdr.scalco < 0) scl = 1.0/fabs(hdr.scalco);
    else if (hdr.scalco == 0) scl = 1.0;
    else scl = hdr.scalco;

	*sclsxgxsygy = scl;
    /* check to find out number of traces in shot gather */

    one_shot = 1;
    itrace   = 0;
    igy      = 1;
    fldr_shot = hdr.fldr;
    sx_shot  = hdr.sx;
    sy_shot = hdr.sy;
    gx_start = hdr.gx;
    gy_start = hdr.gy;
    gy_end = gy_start;
    trace = (float *)malloc(hdr.ns*sizeof(float));
    fseeko( fp, TRCBYTES, SEEK_SET );

    while (one_shot) {
        nread = fread( trace, sizeof(float), hdr.ns, fp );
        assert (nread == hdr.ns);
        itrace++;
        if (hdr.gy != gy_end) {
            gy_end = hdr.gy;
            igy++;
        }
        gx_end = hdr.gx;
        nread = fread( &hdr, 1, TRCBYTES, fp );
        if (nread==0) break;
        if ((sx_shot != hdr.sx) || (sy_shot != hdr.sy) || (fldr_shot != hdr.fldr) ) break;
    }

    if (itrace>1) {
        *n2 = itrace/igy;
        *n3 = igy;
        dxrcv  = (float)(gx_end - gx_start)/(float)(*n2-1);
        dyrcv = (float)(gy_end - gy_start)/(float)(*n3-1);
        *d2 = fabs(dxrcv)*scl;
        *d3 = fabs(dyrcv)*scl;
        if (NINT(dxrcv*1e3) != NINT(fabs(hdr.d2)*1e3)) {
            if (dxrcv != 0) *d2 = fabs(dxrcv)*scl;
            else *d2 = hdr.d2;
        }
    }
    else {
        *n2 = MAX(hdr.trwf, 1);
        *n3 = 1;
        *d2 = hdr.d2;
        *d3 = 0.0;
        dxrcv = hdr.d2;
        dyrcv = 0.0;
    }  

/* check if the total number of traces (ntraces) is correct */

/* expensive way to find out how many gathers there are */

//	fprintf(stderr, "ngath = %d dxrcv=%f d2=%f scl=%f \n", *ngath, dxrcv, *d2, scl);
    if (*ngath == 0) {
		*n2 = 0;
        *n3 = 0;

        end_of_file = 0;
        one_shot    = 1;
        igath       = 0;
        fseeko( fp, 0, SEEK_SET );
        dxrcv = *d2;
        dyrcv = *d3;

        while (!end_of_file) {
            itrace = 0;
            nread = fread( &hdr, 1, TRCBYTES, fp );
            if (nread != TRCBYTES) { break; }
    		fldr_shot = hdr.fldr;
            sx_shot   = hdr.sx;
            gx_start  = hdr.gx;
            gx_end    = hdr.gx;
            sy_shot   = hdr.sy;
            gy_start  = hdr.gy;
            gy_end    = hdr.gy;
    
            itrace = 0;
            igy = 1;
            while (one_shot) {
                fseeko( fp, data_sz, SEEK_CUR );
                itrace++;
                if (hdr.gx != gx_end) dxrcv = MIN(dxrcv,abs(hdr.gx-gx_end));
                if (hdr.gy != gy_end) {
                    igy++;
                    gy_end = hdr.gy;
                    dyrcv = MIN(dyrcv,abs(hdr.gy-gy_end));
                }
                gx_end = hdr.gx;
                nread = fread( &hdr, 1, TRCBYTES, fp );
                if (nread != TRCBYTES) {
                    one_shot = 0;
                    end_of_file = 1;
                    break;
                }
        		if ((sx_shot != hdr.sx) || (sy_shot != hdr.sy) || (fldr_shot != hdr.fldr)) break;
            }
            if (itrace>1) {
                *n2 = itrace/igy;
                *n3 = igy;
                dxrcv  = (float)(gx_end - gx_start)/(float)(*n2-1);
                dyrcv = (float)(gy_end - gy_start)/(float)(*n3-1);
                dxsrc  = (float)(hdr.sx - sx_shot)*scl;
                dysrc = (float)(hdr.sy - sy_shot)*scl;
            }
            if (verbose>1) {
                fprintf(stderr," . Scanning shot %d (%d) with %d traces dxrcv=%.2f dxsrc=%.2f %d %d dyrcv=%.2f dysrc=%.2f %d %d\n",sx_shot,igath,itrace,dxrcv*scl,dxsrc,gx_end,gx_start,dyrcv*scl,dysrc,gy_end,gy_start);
            }
            if (itrace != 0) { /* end of shot record */
                fseeko( fp, -TRCBYTES, SEEK_CUR );
                igath++;
            }
            else {
                end_of_file = 1;
            }
        }
        *ngath = igath;
        *d2 = dxrcv*scl;
        *d3 = dyrcv*scl;
    }
    else {
        /* read last trace header */

        fseeko( fp, -trace_sz, SEEK_END );
        nread = fread( &hdr, 1, TRCBYTES, fp );
		*ngath = ntraces/((*n2)*(*n3));
    }
//    *nxm = NINT((*xmax-*xmin)/dxrcv)+1;
	*nxm = (int)ntraces;

    fclose( fp );
    free(trace);

    return 0;
}

int disp_fileinfo3D(char *file, int n1, int n2, int n3, float f1, float f2, float f3, float d1, float d2, float d3, segy *hdrs)
{
	vmess("file %s contains", file);
    vmess("*** n1 = %d n2 = %d n3 = %d ntftt=%d", n1, n2, n3, optncr(n1));
	vmess("*** d1 = %.5f d2 = %.5f d3 = %.5f", d1, d2, d3);
	vmess("*** f1 = %.5f f2 = %.5f f3 = %.5f", f1, f2, f3);
	vmess("*** fldr = %d sx = %d sy = %d", hdrs[0].fldr, hdrs[0].sx, hdrs[0].sy);

	return 0;
}
