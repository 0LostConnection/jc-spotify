#ifndef SDCARD_UTILS_H
#define SDCARD_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

void sdcard_init(void);
void sdcard_list_files(const char *path);
bool sdcard_read_file(const char *path, char *buffer, size_t max_len);
bool sdcard_write_file(const char *path, const char *data);
bool sdcard_delete_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif // SDCARD_UTILS_H