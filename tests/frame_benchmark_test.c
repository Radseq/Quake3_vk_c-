#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#define MAX_OSPATH 512
#define CA_ACTIVE 8
struct {int state;} cls;
typedef FILE *fileHandle_t;
typedef struct tm qtime_t;
#define FS_Printf fprintf
#define Com_Printf printf
#define Com_sprintf snprintf
#define Com_Memset memset
#define FS_FCloseFile fclose
static void Com_RealTime(qtime_t *t) { time_t now=time(NULL); *t=*localtime(&now); }
static FILE *FS_FOpenFileWrite(const char *s) { (void)s; return tmpfile(); }
#include "../code/client/cl_benchmark.h"
int main(void) {
 double samples[1000]; size_t i; FILE *f; char line[1024];
 for(i=0;i<1000;i++) samples[i]=(double)i+1;
 assert(fabs(CL_BenchmarkPercentile(samples,1000,.5)-500.5)<1e-9);
 assert(fabs(CL_BenchmarkPercentile(samples,1000,.95)-950.05)<1e-9);
 assert(fabs(CL_BenchmarkPercentile(samples,1000,.99)-990.01)<1e-9);
 assert(fabs(CL_BenchmarkPercentile(samples,1000,.999)-999.001)<1e-9);
 assert(fabs(CL_BenchmarkLow(samples,1000,.01)-1000./995.5)<1e-9);
 assert(fabs(CL_BenchmarkLow(samples,1000,.001)-1)<1e-9);
 assert(CL_BenchmarkPercentile(samples,1,.999)==1);
 assert(CL_BenchmarkLow(samples,1,.001)==1000);
 f=tmpfile(); CL_BenchmarkStats(f,samples,1000,0,500.5); rewind(f);
 assert(fgets(line,sizeof(line),f)); assert(strstr(line,",500.500000,500.500000,")); fclose(f);
 cls.state=CA_ACTIVE; CL_BenchmarkFrame(); assert(frameBenchmark.count==0);
 frameBenchmark.previous=CL_BenchmarkTime()-10000; CL_BenchmarkFrame(); assert(frameBenchmark.count==1);
 cls.state=0; CL_BenchmarkFrame(); assert(frameBenchmark.previous==0);
 cls.state=CA_ACTIVE; CL_BenchmarkFrame(); assert(frameBenchmark.count==1);
 CL_BenchmarkSave(); assert(frameBenchmark.count==0 && frameBenchmark.frames==NULL);
 puts("benchmark tests passed"); return 0;
}
