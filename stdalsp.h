/* Filename    : tdalsp.h
 * Author      : SunYoung Kim <sunyzero@gmail.com>
 * Description :
 * Notes       :
 * 1st Release :2011-06-27 04:47:24 (Mon)
 */
#ifndef H_STDALSP
#define H_STDALSP


#define print_msg(io, msgtype, arg...) \
	flockfile(io); \
    fprintf(io, "["#msgtype"] [%s/%s:%03d] ", __FILE__, __FUNCTION__, __LINE__); \
    fprintf(io, arg); \
    fputc('\n', io); \
	funlockfile(io)

#define pr_err(arg...) print_msg(stderr, ERR, arg)
#define pr_out(arg...) print_msg(stdout, REP, arg)

/* TODO: C99의 __VA_ARGS__ 를 사용하도록 하자. */


/* -------------------------------------------------------------------------- */
#include <sys/types.h>
extern char * get_time0(char *buf, size_t sz_buf);
extern char * get_datetime0(char *buf, size_t sz_buf);

/* -------------------------------------------------------------------------- */

#endif
