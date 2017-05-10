/* =========================================================================
 * File:        uniode_tbl.c
 * Description: Functions to create and write a rudimentary table
 *              using unicode page U+25xx characters for nicely
 *              formatted output to a terminal.
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: unicode_tbl.c 764 2015-03-04 10:55:57Z ljp $
 *
 * Copyright (C) 2015 Johan Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 * =========================================================================
 */

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <sys/param.h> // To get MIN/MAX

//#include "config.h"
#include "unicode_tbl.h"
//#include "logger.h"

// Shortcut names for the individual characters for easy of use
#define BDL_DR "\u250c"  // Down Right
#define BDL_DL "\u2510"  // Down Left
#define BDL_UR "\u2514"  // Up Right
#define BDL_UL "\u2518"  // Up Left
#define BDL_VR "\u251c"  // Vertical Right
#define BDL_VL "\u2524"  // Vertical Left
#define BDL_DH "\u252c"  // Down Horizontal
#define BDL_UH "\u2534"  // Up Horizontal
#define BDL_V  "\u2502"  // Vertical
#define BDL_H  "\u2500"  // Horizontal
#define BDL_X  "\u253c"  // Cross
#define BDL_DH_VL  "\u2561"
#define BDL_DH_VR  "\u255e"
#define BDL_DH_DH  "\u2564"
#define BDL_DH_UH  "\u2567"
#define BDL_DH_UH  "\u2567"
#define BDL_DH_X   "\u256a"
#define BDL_DH_DR  "\u2552"
#define BDL_DH_DL  "\u2555"
#define BDL_DH_H   "\u2550"
#define BDL_DL_V   "\u2551"
#define BDL_DL_DR   "\u2554"
#define BDL_DL_DL   "\u2557"
#define BDL_DL_UR   "\u255a"
#define BDL_DL_UL   "\u255d"
#define BDL_DV_VR   "\u255f"
#define BDL_DV_VL   "\u2562"
#define BDL_DL_VR   "\u2560"
#define BDL_DL_VL   "\u2563"

/* Unicode codes and corresponding characters
╡2561
╞255e
╢2562
╣2563
╤2564
╥2565
╦2566
╧2567
╨2568
╩2569
╪256a
╫256b
╬256c
╒2552 
═2550
║2551
╔2554
╗2557
╚255a
╝255d  
╟255f
╢2562
╠2560
╣2563  
 */

// Always nice to have
#define FALSE 0
#define TRUE 1

// Utility macro to index table matrix
#define TIDX(_r,_c) (_r*t->nCol+_c)

/**
 * Check that row and col arguments are within valid range before accessing cells
 * @param t
 * @param row
 * @param col
 * @return 0 on valid range, -1 otherwise
 */
static int
_rc_chk(table_t *t, size_t row, size_t col) {
    if (row >= t->nRow || col >= t->nCol) {
      //        logmsg(LOG_ERR,"Table cell specified is out of range [%zu, %zu]",row,col);
        return -1;
    }
    return 0;
}

/**
 * Initialize a new cell 
 * @param t
 * @param row
 * @param col
 * @return 0 on success, -1 on failure
 */
static int
_init_cell(table_t *t, size_t row, size_t col) {
    if (_rc_chk(t, row, col))
        return -1;
    tcell_t *c = &t->c[TIDX(row,col)];
    c->t = NULL;
    c->halign = RIGHTALIGN;
    c->pRow = row;
    c->pCol = col;
    c->merged = FALSE;
    c->cspan = 1;
    c->rspan = 1;
    return 0;
}

/**
 * Create a new table of the specified size
 * @param nRow
 * @param nCol
 * @return NULL on error , pointer to the newly created table otherwise
 */
table_t *
table_create(size_t nRow, size_t nCol) {
    table_t *t = calloc(1, sizeof (table_t));
    if (NULL == t)
        return NULL;
    t->nRow = nRow;
    t->nCol = nCol;
    t->c = calloc(nRow*nCol, sizeof (tcell_t));
    t->w = calloc(nCol, sizeof (size_t));
    if (t->c == NULL || t->w == NULL) {
        goto tbl_err;
    }
    for (size_t c = 0; c < t->nCol; c++) {
        t->w[c] = 0;
    }
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nCol; c++) {
            if (_init_cell(t, r, c)) {
                goto tbl_err;
            }
        }
    }

    return t;

tbl_err:
    //    logmsg(LOG_ERR,"Failed to create layout table");
    free(t);
    free(t->c);
    free(t->w);
    return NULL;

}

/**
 * Initialize the table with string from a matrix. It is the calling routines
 * responsibility that the size of the data matrix matches that of the table.
 * @param t
 * @param data
 * @return 0 on success, -1 on failure
 */
int
table_set(table_t *t, char *data[] ) {
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nRow; c++) {
            if( set_cell(t,r,c,data[TIDX(r,c)]) )
                return -1;
        }
    }
    return 0;
}

/**
 * Combine table creation and initialization with error check
 * @param t
 * @param nRow
 * @param nCol
 * @param data
 * @return NULL on failure, pointer to new table otherwise
 */
table_t *
table_create_set(size_t nRow, size_t nCol, char *data[]) {
    table_t *t = table_create(nRow,nCol);
    if( NULL==t ) {
        return NULL;
    }
    if( -1 == table_set(t,data) ) {
        table_free(t);
        return NULL;
    }
    return t;
}

/** 
 * Free (destroy) a previously created table
 * @param t
 */
void
table_free(table_t *t) {
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nRow; c++) {
            if (t->c[r * t->nCol + c].t) {
                free(t->c[TIDX(r,c)].t);
            }
        }
    }
    free(t->c);
    free(t->w);
    free(t);
}

/**
 * Get a reference to the specified cell
 * @param t
 * @param row
 * @param col
 * @return NULL if row/col is out of range, the cell otherwise
 */
tcell_t*
get_cell(table_t *t, int row, int col) {
    if (_rc_chk(t, row, col))
        return NULL;
    return &t->c[TIDX(row,col)];
}

/**
 * Set the number of columns this cell spans
 * @param t
 * @param row
 * @param col
 * @param cspan
 * @return 0 on success, -1 on failure
 */
int
set_cell_colspan(table_t *t, size_t row, size_t col, size_t cspan) {
    if (_rc_chk(t, row, col) || col + cspan - 1 >= t->nCol)
        return -1;
    tcell_t *cell = get_cell(t,row,col);
    cell->cspan = cspan;
    for (size_t c = col; c < col + cspan; c++) {
        cell = get_cell(t,row,c);
        cell->merged = TRUE;
        cell->pRow = row;
        cell->pCol = col;
    }
    return 0;
}

/**
 * Set the horizontal text alignment for this cell 
 * @param t
 * @param row
 * @param col
 * @param halign
 * @return 0 on success, -1 on failure
 */
int
set_cell_halign(table_t *t, int row, int col, halign_t halign) {
    if (_rc_chk(t, row, col))
        return -1;
    t->c[TIDX(row,col)].halign = halign;
    return 0;
}

/**
 * Set the text value for the specified cell. The value stored in the cell will be a 
 * newly allocated space for this string.
 * @param t
 * @param row
 * @param col
 * @param txt
 * @return 0 on success, -1 on failure
 */
int
set_cell(table_t *t, size_t row, size_t col, char *val) {
    if (_rc_chk(t, row, col))
        return -1;
    if( t->c[TIDX(row,col)].t )
        free(t->c[TIDX(row,col)].t);
    t->c[TIDX(row,col)].t = strdup(val);
    return 0;
}

/**
 * Set the width for the specified column
 * @param t
 * @param row
 * @param col
 * @param cspan
 * @return 0 on success, -1 on failure
 */
int
set_colwidth(table_t *t, size_t col, size_t width) {
    if (col >= t->nCol)
        return -1;
    t->w[col] = width;
    return 0;
}

/**
 * Internal helper function to draw a single line of table data 
 * @param t
 * @param row
 * @param style
 */
static void
_draw_cellcontent_row(table_t *t, size_t row, tblstyle_t style) {
    size_t c = 0;
    while (c < t->nCol) {
        int w = 0;

        const size_t cspan = t->c[row * t->nCol + c].cspan;
        for (size_t cs = 0; cs < cspan; cs++) {
            w += t->w[c + cs] + 1;
        }
        w -= 1;

        const char *txt = t->c[ row * t->nCol + c ].t;
        int w_half = w / 2;
        int len = strlen(txt);
        int len_half = len / 2;

        char txtbuff[128];
        strncpy(txtbuff, txt, MIN((int) sizeof (txtbuff) - 1, w));
        txtbuff[MIN((int) sizeof (txtbuff) - 1, w)] = '\0';

        if (c == 0 && style == TSTYLE_FULL_DL) {
            switch (t->c[TIDX(row,c)].halign) {
                case RIGHTALIGN:
                    printf("%s%*s", BDL_DL_V, w, txtbuff);
                    break;
                case LEFTALIGN:
                    printf("%s%-*s", BDL_DL_V, w, txtbuff);
                    break;
                case CENTERALIGN:
                    printf("%s%*s%-*s", BDL_DL_V, w_half - len_half, "", w - w_half + len_half, txtbuff);
                    break;
            }
        } else {
            switch (t->c[TIDX(row,c)].halign) {
                case RIGHTALIGN:
                    printf("%s%*s", BDL_V, w, txtbuff);
                    break;
                case LEFTALIGN:
                    printf("%s%-*s", BDL_V, w, txtbuff);
                    break;
                case CENTERALIGN:
                    printf("%s%*s%-*s", BDL_V, w_half - len_half, "", w - w_half + len_half, txtbuff);
                    break;
            }
        }
        c += cspan;
    }
    printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DL_V : BDL_V);
}

/**
 * Set automatic column width for columns with no user specified width
 * @param t Pointer to table structure
 */
static void
_set_autocolwidth(table_t *t) {
    for (size_t c = 0; c < t->nCol; c++) {
        if (t->w[c] == 0) {
            // User has not yet set column width so find the widest text
            for (size_t r = 0; r < t->nRow; r++) {
                if (strlen(t->c[ r * t->nCol + c ].t) > t->w[c]) {
                    t->w[c] = strlen(t->c[TIDX(r,c)].t);
                }
            }
            // even if there are no text in this column we set the minimum width to
            // 1 character.
            if (0 == t->w[c]) t->w[c] = 1;
        }
    }
}

/**
 * Internal helper function. This functions builds a logic view of the border
 * between two rows. Since the vertical borders up and down are not necessarily
 * always on the same position since cell can have been merged (spans over
 * several columns) this function sets a the logical map "eval" if it 
 * discovers the need for vertical border. The same logical map are used
 * with both the "over" and "under" row of data. The value to logically or
 * is specified in the "mark" argument. To find the top verticals
 * we normally call this function with mark==1 and for the bottom verticals
 * we call this with mark==2. If the logical map is initialized with 0 this will
 * can the be interpretated as 0=only a horizontal, 1=horizontal with an "up" vertical,
 * 2=horizontal with an "down" vertical and 2=horizontal with both up and down vertical
 * @param t
 * @param eval
 * @param mark
 * @param row
 */
static void
_mark_verticals(table_t *t, int eval[], int mark, size_t row) {
    size_t c = 0;
    size_t absw = 0;
    while (c < t->nCol) {

        int w = 0;
        for (size_t cs = 0; cs < t->c[TIDX(row,c)].cspan; cs++) {
            w += t->w[c + cs] + 1;
        }

        absw += w;
        eval[absw - 1] |= mark;
        c += t->c[TIDX(row,c)].cspan;
    }
}

/**
 * Internal helper functions to write out the border characters identified by the
 * _mark_verticals
 * @param totwidth
 * @param eval
 * @param s0
 * @param s1
 * @param s2
 * @param s3
 */
static void
_stroke_verticals(int totwidth, int eval[], char *s0, char *s1, char *s2, char *s3) {
    for (int i = 0; i < totwidth - 1; i++) {
        if (eval[i] == 0)
            printf("%s", s0);
        else if (eval[i] == 1)
            printf("%s", s1);
        else if (eval[i] == 2)
            printf("%s", s2);
        else
            printf("%s", s3);
    }
}

/**
 * Stroke the entire table in the specified style
 * @param t
 * @param style
 */
void
stroke_table(table_t *t, tblstyle_t style) {

    _set_autocolwidth(t);

    size_t totwidth = 0;
    for (size_t i = 0; i < t->nCol; i++)
        totwidth += t->w[i] + 1;
    int eval[totwidth];

    for (size_t r = 0; r < t->nRow; r++) {
        memset(eval, 0, sizeof (int)*totwidth);
        if (0 == r) {
            _mark_verticals(t, eval, 1, r);
            if (style == TSTYLE_TOP_DL || style == TSTYLE_FULL_DL) {
                printf("%s", style == TSTYLE_FULL_DL ? BDL_DL_DR : BDL_DH_DR);
                _stroke_verticals(totwidth, eval, BDL_DH_H, BDL_DH_DH, NULL, NULL);
                printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DL_DL : BDL_DH_DL);
            } else {
                printf("%s", BDL_DR);
                _stroke_verticals(totwidth, eval, BDL_H, BDL_DH, NULL, NULL);
                printf("%s\n", BDL_DL);
            }
            _draw_cellcontent_row(t, r, style);
        } else {
            _mark_verticals(t, eval, 1, r - 1);
            _mark_verticals(t, eval, 2, r);
            if (1 == r && (style == TSTYLE_TOP_DL || style == TSTYLE_FULL_DL)) {
                printf("%s", style == TSTYLE_FULL_DL ? BDL_DL_VR : BDL_DH_VR);
                _stroke_verticals(totwidth, eval, BDL_DH_H, BDL_DH_UH, BDL_DH_DH, BDL_DH_X);
                printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DL_VL : BDL_DH_VL);
            } else {
                printf("%s", style == TSTYLE_FULL_DL ? BDL_DV_VR : BDL_VR);
                _stroke_verticals(totwidth, eval, BDL_H, BDL_UH, BDL_DH, BDL_X);
                printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DV_VL : BDL_VL);
            }
            _draw_cellcontent_row(t, r, style);
        }
    }

    memset(eval, 0, sizeof (int)*totwidth);
    _mark_verticals(t, eval, 1, t->nRow - 1);
    if (style == TSTYLE_FULL_DL) {
        printf("%s", BDL_DL_UR);
        _stroke_verticals(totwidth, eval, BDL_DH_H, BDL_DH_UH, NULL, NULL);
        printf("%s\n", BDL_DL_UL);

    } else {
        printf("%s", BDL_UR);
        _stroke_verticals(totwidth, eval, BDL_H, BDL_UH, NULL, NULL);
        printf("%s\n", BDL_UL);
    }

}

// Some rudimentary unit-test
// gcc -std=gnu99 -O2 -ggdb -Wall unicode_tbl.c

int
main(int argc, char **argv) {

    
    table_t *tbl = table_create(7, 7);
    char buff[64];

    for (size_t r = 0; r < tbl->nRow; r++) {
        for (size_t c = 0; c < tbl->nCol; c++) {
            snprintf(buff, sizeof (buff), " (%zu,%zu) ", r, c);
            set_cell(tbl, r, c, buff);
        }
    }
    
    set_cell(tbl, 0, 0, "Alla kommer!");

    set_cell_colspan(tbl,0,0,6);
    set_cell_halign(tbl,0,0,CENTERALIGN);

    set_cell_colspan(tbl,1,1,3);
    set_cell_halign(tbl,1,1,CENTERALIGN);

    set_cell_colspan(tbl,2,2,3);
    set_cell_halign(tbl,2,2,RIGHTALIGN);

    set_colwidth(tbl,0,7);
    set_cell(tbl,2,0,"Much longer text than column width");
    
    stroke_table(tbl, TSTYLE_FULL_DL);
    //stroke_table(tbl, TSTYLE_TOP_DL);
    printf("\n");

    exit(EXIT_SUCCESS);
}



// [EOF]]
