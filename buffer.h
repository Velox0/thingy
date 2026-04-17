#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

typedef struct {
  char **lines;
  int line_count;
  int capacity;
} TextBuffer;

typedef struct {
  int row;
  char *label;
  char **hidden_lines;
  int hidden_count;
} FoldEntry;

typedef struct {
  FoldEntry *items;
  int count;
  int capacity;
} FoldList;

void buffer_init(TextBuffer *buf);
void buffer_free(TextBuffer *buf);

int buffer_load_file(TextBuffer *buf, const char *path, char *err, size_t err_size);
int buffer_save_file(const TextBuffer *buf, const char *path, char *err, size_t err_size);

int buffer_line_len(const TextBuffer *buf, int row);
void buffer_clamp_cursor(const TextBuffer *buf, int *row, int *col);
void buffer_insert_char(TextBuffer *buf, int row, int col, char c);
int buffer_insert_newline(TextBuffer *buf, int *row, int *col);
int buffer_backspace(TextBuffer *buf, int *row, int *col);

void folds_init(FoldList *folds);
void folds_free(FoldList *folds);
int folds_is_folded_row(const FoldList *folds, int row);
void folds_on_line_insert(FoldList *folds, int at_row);
void folds_on_line_delete(FoldList *folds, int at_row);
int buffer_toggle_fold(TextBuffer *buf, FoldList *folds, int *cursor_row, char *msg, size_t msg_size);

#endif
