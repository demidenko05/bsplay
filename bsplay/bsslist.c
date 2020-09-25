/* BSD 2-Clause License
Copyright (c) 2020, Beigesoft™
All rights reserved.
See the LICENSE.BSPLAY in the root source folder */
 
/**
 * <p>List of string library.</p>
 * @author Yury Demidenko
 **/

#include <stdlib.h>
#include <bsslist.h>

//local library:
/* add row to the end of list, it will allocate memory if need */
static void sf_realloc(BSStPlDtLs *p_lst) {
  char ** nlst = realloc(p_lst->list, (p_lst->buf_size + p_lst->buf_inc) * p_lst->columns * sizeof(char*));
  if (p_lst->list != nlst) {
    p_lst->list = nlst;
  }
  p_lst->buf_size += p_lst->buf_inc;
  for (int i = p_lst->rows ; i < p_lst->buf_size; i++) {
    for (int j = 0; j < p_lst->columns; j++) {
      *(p_lst->list + i * p_lst->columns + j) = NULL;
    }
  }
}

//public library:
/* free allocated memory */
void bsslist_free(BSStPlDtLs *p_lst) {
  if (p_lst->list) {
    free(*p_lst->list);
  }
}

/* get row (pointer to pointers to row of strings) for given index (from 0) */
char **bsslist_get(BSStPlDtLs *p_lst, int p_idx) {           
  if (p_idx >= 0 && p_idx < p_lst->rows) {
    return (&*p_lst->list + p_idx * p_lst->columns);
  }
  return NULL;
}

/* add row to the end of list, it will allocate memory if need */
void bsslist_add(BSStPlDtLs *p_lst, char *p_row[]) {
  int lsize = p_lst->columns * p_lst->rows;
  if (p_lst->list == NULL) {
    p_lst->list = malloc(p_lst->buf_size * p_lst->columns * sizeof(char*));
    for (int i = 0; i < p_lst->buf_size; i++) {
      for (int j = 0; j < p_lst->columns; j++) {
        *(p_lst->list + i * p_lst->columns + j) = NULL;
      }
    }
  } else if (p_lst->rows == p_lst->buf_size) {
    sf_realloc(p_lst);
  }
  for (int i = 0; i < p_lst->columns; i++) {
    *(p_lst->list + lsize + i) = p_row[i];
  }
  p_lst->rows++;
  return;
}

/* delete row for given index (from 0) */
void bsslist_del(BSStPlDtLs *p_lst, int p_idx) {
  if (p_idx >= 0 && p_idx < p_lst->rows) {
    if (p_idx != p_lst->rows - 1) { //moving up:
      for (int i = p_idx; i < p_lst->rows - 1; i++) {
        for (int j = 0; j < p_lst->columns; j++) {
          *(p_lst->list + i * p_lst->columns + j) = *(p_lst->list + (i + 1) * p_lst->columns + j);
        }
      }
    } //else - latest row, just reduce size
    p_lst->rows--;
  }
}

/* clear list */
void bsslist_clear(BSStPlDtLs *p_lst) {
  p_lst->rows = 0;
}

/* move up row for given index (from 0) */
int bsslist_up(BSStPlDtLs *p_lst, int p_idx) {
  if (p_idx > 0 && p_idx < p_lst->rows) {
    if (p_lst->rows == p_lst->buf_size) {
      sf_realloc(p_lst);
    }
    for (int j = 0; j < p_lst->columns; j++) {
      *(p_lst->list + p_lst->rows * p_lst->columns + j) = *(p_lst->list + p_idx * p_lst->columns + j);
      *(p_lst->list + p_idx * p_lst->columns + j) = *(p_lst->list + (p_idx - 1) * p_lst->columns + j);
      *(p_lst->list + (p_idx - 1) * p_lst->columns + j) = *(p_lst->list + p_lst->rows * p_lst->columns + j);
      *(p_lst->list + p_lst->rows * p_lst->columns + j) = NULL;
    }
    return 0;
  }
  return 123;
}

/* move down row for given index (from 0) */
int bsslist_down(BSStPlDtLs *p_lst, int p_idx) {
  if (p_idx >= 0 && p_idx < p_lst->rows - 1) {
    if (p_lst->rows == p_lst->buf_size) {
      sf_realloc(p_lst);
    }
    for (int j = 0; j < p_lst->columns; j++) {
      *(p_lst->list + p_lst->rows * p_lst->columns + j) = *(p_lst->list + p_idx * p_lst->columns + j);
      *(p_lst->list + p_idx * p_lst->columns + j) = *(p_lst->list + (p_idx + 1) * p_lst->columns + j);
      *(p_lst->list + (p_idx + 1) * p_lst->columns + j) = *(p_lst->list + p_lst->rows * p_lst->columns + j);
      *(p_lst->list + p_lst->rows * p_lst->columns + j) = NULL;
    }
    return 0;
  }
  return 123;
}

/* move row from/to given index (from 0) */
int bsslist_move(BSStPlDtLs *p_lst, int p_from, int p_to) {
  if (p_from != p_to && p_from >= 0 && p_from < p_lst->rows && p_to >= 0 && p_to < p_lst->rows) {
    if (p_lst->rows == p_lst->buf_size) {
      sf_realloc(p_lst);
    }
    for (int j = 0; j < p_lst->columns; j++) {
      *(p_lst->list + p_lst->rows * p_lst->columns + j) = *(p_lst->list + p_from * p_lst->columns + j);
      *(p_lst->list + p_from * p_lst->columns + j) = *(p_lst->list + p_to * p_lst->columns + j);
      *(p_lst->list + p_to * p_lst->columns + j) = *(p_lst->list + p_lst->rows * p_lst->columns + j);
      *(p_lst->list + p_lst->rows * p_lst->columns + j) = NULL;
    }
    return 0;
  }
  return 123;
}
