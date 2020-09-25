/* BSD 2-Clause License
Copyright (c) 2020, Beigesoft™
All rights reserved.
See the LICENSE.BSPLAY in the root source folder */

/**
 * <p>List of rows of strings library header.</p>
 * @author Yury Demidenko
 **/

#include <stdio.h>

/* Strings list model */
typedef struct {
  int rows; //current rows size
  int columns; //strings in row
  int buf_size; //maximum rows in buffer
  int buf_inc; //how much increase buffer when reallocating memory
  char **list; //pointer to array of string pointers with data
} BSStPlDtLs;

/* free allocated memory */
void bsslist_free(BSStPlDtLs *p_lst);

/* get row (pointer to pointers to row of strings) for given index (from 0) */
char **bsslist_get(BSStPlDtLs *p_lst, int p_idx);

/* add row to the end of list, it will allocate memory if need */
void bsslist_add(BSStPlDtLs *p_lst, char *p_row[]);

/* delete row for given index (from 0) */
void bsslist_del(BSStPlDtLs *p_lst, int p_idx);

/* clear list */
void bsslist_clear(BSStPlDtLs *p_lst);

/* move up row for given index (from 0), return 0 if OK */
int bsslist_up(BSStPlDtLs *p_lst, int p_idx);

/* move down row for given index (from 0), return 0 if OK */
int bsslist_down(BSStPlDtLs *p_lst, int p_idx);

/* move row from/to given index (from 0), return 0 if OK */
int bsslist_move(BSStPlDtLs *p_lst, int p_from, int p_to);
