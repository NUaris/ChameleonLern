#ifndef TEST_FDS_H
#define TEST_FDS_H
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t ret_code_t;
#define NRF_SUCCESS 0
#define NRF_ERROR_INVALID_PARAM 5
#define NRF_ERROR_BUSY 1
#define NRF_ERROR_TIMEOUT 2
#define FDS_ERR_NO_SPACE_IN_FLASH 3
#define NRF_ERROR_NOT_FOUND 4
typedef struct { unsigned id; } fds_record_desc_t;
typedef struct { unsigned token; } fds_find_token_t;
typedef struct { uint16_t length_words; } fds_header_t;
typedef struct { const fds_header_t *p_header; const void *p_data; } fds_flash_record_t;
typedef struct { uint16_t file_id,key; struct {void *p_data; uint16_t length_words;} data; } fds_record_t;
enum { FDS_EVT_INIT, FDS_EVT_WRITE, FDS_EVT_UPDATE, FDS_EVT_DEL_RECORD, FDS_EVT_GC };
typedef struct {unsigned id; ret_code_t result; struct {uint16_t file_id,record_key;} write,del;} fds_evt_t;
ret_code_t fds_record_find(uint16_t,uint16_t,fds_record_desc_t*,fds_find_token_t*);
ret_code_t fds_record_open(fds_record_desc_t*,fds_flash_record_t*);
ret_code_t fds_record_close(fds_record_desc_t*);
ret_code_t fds_record_write(fds_record_desc_t*,fds_record_t*);
ret_code_t fds_record_update(fds_record_desc_t*,fds_record_t*);
ret_code_t fds_record_delete(fds_record_desc_t*);
ret_code_t fds_gc(void);
ret_code_t fds_register(void (*handler)(const fds_evt_t*));
ret_code_t fds_init(void);
#endif
