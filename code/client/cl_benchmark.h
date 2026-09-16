/* Optional client frame benchmark. Included only by cl_main.c. */
#ifndef CL_BENCHMARK_H
#define CL_BENCHMARK_H

static struct {
	double *frames;
	size_t count, capacity;
	int64_t previous;
	bool failed;
} frameBenchmark;

static int64_t CL_BenchmarkTime( void ) {
#ifdef _WIN32
	return Sys_Microseconds();
#else
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
#endif
}

static void CL_BenchmarkFrame( void ) {
	int64_t now = CL_BenchmarkTime();
	double *frames;
	size_t capacity;

	if ( cls.state != CA_ACTIVE || frameBenchmark.failed ) {
		frameBenchmark.previous = 0;
		return;
	}
	if ( frameBenchmark.previous && now > frameBenchmark.previous ) {
		if ( frameBenchmark.count == frameBenchmark.capacity ) {
			capacity = frameBenchmark.capacity ? frameBenchmark.capacity * 2 : 65536;
			if ( capacity < frameBenchmark.capacity || capacity > (size_t)-1 / sizeof(double) )
				frames = NULL;
			else
				frames = (double *)realloc( frameBenchmark.frames, capacity * sizeof(double) );
			if ( !frames ) {
				frameBenchmark.failed = true;
				Com_Printf( "Benchmark: out of memory; saving partial results on exit.\n" );
				return;
			}
			frameBenchmark.frames = frames;
			frameBenchmark.capacity = capacity;
		}
		frameBenchmark.frames[frameBenchmark.count++] = (now - frameBenchmark.previous) / 1000.0;
	}
	frameBenchmark.previous = now;
}

static int CL_BenchmarkCompare( const void *a, const void *b ) {
	double x = *(const double *)a, y = *(const double *)b;
	return (x > y) - (x < y);
}

/* Percentiles use linear interpolation between adjacent sorted samples. */
static double CL_BenchmarkPercentile( const double *frames, size_t count, double fraction ) {
	double position = (count - 1) * fraction;
	size_t index = (size_t)position;
	return frames[index] + (frames[index + (index + 1 < count)] - frames[index]) * (position - index);
}

static double CL_BenchmarkLow( const double *frames, size_t count, double fraction ) {
	size_t slowCount = (size_t)ceil( count * fraction ), i;
	double sum = 0;
	for ( i = count - slowCount; i < count; ++i )
		sum += frames[i];
	return 1000.0 * slowCount / sum;
}

static void CL_BenchmarkStats( fileHandle_t file, double *frames, size_t count,
	double start, double end ) {
	size_t i;
	double sum = 0;
	for ( i = 0; i < count; ++i ) sum += frames[i];
	qsort( frames, count, sizeof(double), CL_BenchmarkCompare );
	FS_Printf( file, "%.6f,%.6f,%lu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
		start, end, (unsigned long)count, sum / count,
		CL_BenchmarkPercentile( frames, count, 0.5 ),
		CL_BenchmarkPercentile( frames, count, 0.95 ),
		CL_BenchmarkPercentile( frames, count, 0.99 ),
		CL_BenchmarkPercentile( frames, count, 0.999 ),
		CL_BenchmarkLow( frames, count, 0.01 ),
		CL_BenchmarkLow( frames, count, 0.001 ) );
}

static void CL_BenchmarkSave( void ) {
	const char *header = "start_s,end_s,frames,average_frametime_ms,median_ms,p95_ms,p99_ms,p99_9_ms,low_1_percent_fps,low_0_1_percent_fps\n";
	char prefix[128], path[MAX_OSPATH];
	qtime_t date;
	fileHandle_t summary, timeline, raw;
	size_t i, first = 0;
	double elapsed = 0, start = 0;

	if ( !frameBenchmark.count ) return;
	Com_RealTime( &date );
	Com_sprintf( prefix, sizeof(prefix), "benchmarks/%04d%02d%02d-%02d%02d%02d-%lld",
		date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
		date.tm_hour, date.tm_min, date.tm_sec, (long long)CL_BenchmarkTime() );
	Com_sprintf( path, sizeof(path), "%s-summary.csv", prefix );
	summary = FS_FOpenFileWrite( path );
	Com_sprintf( path, sizeof(path), "%s-timeline.csv", prefix );
	timeline = FS_FOpenFileWrite( path );
	Com_sprintf( path, sizeof(path), "%s-frames.csv", prefix );
	raw = FS_FOpenFileWrite( path );
	if ( !summary || !timeline || !raw ) {
		Com_Printf( "Benchmark: cannot open output files under %s\n", prefix );
	} else {
		FS_Printf( summary, "%s", header );
		FS_Printf( timeline, "%s", header );
		FS_Printf( raw, "frame,elapsed_s,frametime_ms\n" );
		for ( i = 0; i < frameBenchmark.count; ++i ) {
			elapsed += frameBenchmark.frames[i] / 1000.0;
			FS_Printf( raw, "%lu,%.6f,%.6f\n", (unsigned long)i + 1, elapsed, frameBenchmark.frames[i] );
			if ( elapsed - start >= 1.0 || i + 1 == frameBenchmark.count ) {
				/* Sorting completed windows preserves the global distribution. */
				CL_BenchmarkStats( timeline, frameBenchmark.frames + first, i + 1 - first, start, elapsed );
				first = i + 1;
				start = elapsed;
			}
		}
		CL_BenchmarkStats( summary, frameBenchmark.frames, frameBenchmark.count, 0, elapsed );
		Com_Printf( "Benchmark: saved %s-{summary,timeline,frames}.csv%s\n", prefix,
			frameBenchmark.failed ? " (partial: out of memory)" : "" );
	}
	if ( summary ) FS_FCloseFile( summary );
	if ( timeline ) FS_FCloseFile( timeline );
	if ( raw ) FS_FCloseFile( raw );
	free( frameBenchmark.frames );
	Com_Memset( &frameBenchmark, 0, sizeof(frameBenchmark) );
}
#endif
