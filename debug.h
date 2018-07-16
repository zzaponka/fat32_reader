#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef DEBUG
#define DEBUG_LOG(fmt, ...); \
	do { \
		printf("%s@%d: "fmt"\n", __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_LOG(fmt, ...)
#endif

#endif /* __DEBUG_H__ */
