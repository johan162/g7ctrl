/* =========================================================================
 * File:        uniode_tbl.c
 * Description: Functions to create and write a rudimentary table
 *              using unicode page U+25xx characters for nicely
 *              formatted output to a terminal. 
 * Note:        If this file is compiled with TABLE_UNIT_TEST
 *              defined a small main() function is enabled and can be
 *              run separately as a kind of micro unit test.
 *              gcc -std=c99 -DTABLE_UNIT_TEST unicode_tbl.c
 * Author:      Johan Persson (johan162@gmail.com)
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

#ifndef TABLE_UNIT_TEST
#include "config.h"
#include "logger.h"
#endif

#include "libxstr/xstr.h"
#include "unicode_tbl.h"
#include "utils.h"

// Shortcut names for the individual characters for easy of use

// Single line character
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

// Single line rounded corne
#define BDL_SR_DR "\u256d"  // Down Right
#define BDL_SR_DL "\u256e"  // Down Left
#define BDL_SR_UR "\u2570"  // Up Right
#define BDL_SR_UL "\u256f"  // Up Left
#define BDL_SR_VR BDL_VR  // Vertical Right
#define BDL_SR_VL BDL_VL  // Vertical Left
#define BDL_SR_DH BDL_DH  // Down Horizontal
#define BDL_SR_UH BDL_UH  // Up Horizontal
#define BDL_SR_V  BDL_V  // Vertical
#define BDL_SR_H  BDL_H  // Horizontal
#define BDL_SR_X  BDL_X  // Cross

// Double line chars connected to double
#define BDL_DL_H   "\u2550"
#define BDL_DL_V   "\u2551"
#define BDL_DL_DR  "\u2554"
#define BDL_DL_DL  "\u2557"
#define BDL_DL_UR  "\u255a"
#define BDL_DL_UL  "\u255d"
#define BDL_DL_VR  "\u2560"
#define BDL_DL_VL  "\u2563"
#define BDL_DL_UH  "\2569u"
#define BDL_DL_DH  "\u2566"
#define BDL_DL_X   "\u256c"

// Double horizontal chars connected to thin
#define BDL_DH_H   BDL_DL_H
#define BDL_DH_V   BDL_V
#define BDL_DH_DR  "\u2552"
#define BDL_DH_DL  "\u2555"
#define BDL_DH_UR  "\u2558"
#define BDL_DH_UL  "\u255b"
#define BDL_DH_VR  "\u255e"
#define BDL_DH_VL  "\u2561"
#define BDL_DH_DH  "\u2564"
#define BDL_DH_UH  "\u2567"
#define BDL_DH_X   "\u256a"

// Double vertical chars connected to thin
#define BDL_DV_H   BDL_H
#define BDL_DV_V   BDL_DL_V
#define BDL_DV_DR  "\u2553"  // Down Right
#define BDL_DV_DL  "\u2556"  // Down Left
#define BDL_DV_UR  "\u2559"  // Up Right
#define BDL_DV_UL  "\u255c"  // Up Left
#define BDL_DV_VR  "\u255f"
#define BDL_DV_VL  "\u2562"
#define BDL_DV_DH  "\u2565"
#define BDL_DV_UH  "\u2568"
#define BDL_DV_X   "\u256b"

// Heavy border lines 
#define BDL_HL_H   "\u2501"
#define BDL_HL_V   "\u2503"
#define BDL_HL_DR  "\u250f"  // Down Right
#define BDL_HL_DL  "\u2513"  // Down Left
#define BDL_HL_UR  "\u2517"  // Up Right
#define BDL_HL_UL  "\u251b"  // Up Left
#define BDL_HL_VR  "\u2523"
#define BDL_HL_VL  "\u252b"
#define BDL_HL_DH  "\u2533"
#define BDL_HL_UH  "\u253b"

// Heavy border lines connected to thin interior
#define BDL_HB_H   BDL_HL_H
#define BDL_HB_V   BDL_HL_V
#define BDL_HB_DR  BDL_HL_DR
#define BDL_HB_DL  BDL_HL_DL
#define BDL_HB_UR  BDL_HL_UR
#define BDL_HB_UL  BDL_HL_UL
#define BDL_HB_VR  "\u2520"
#define BDL_HB_VL  "\u2528"
#define BDL_HB_DH  "\u252f"
#define BDL_HB_UH  "\u2537"
#define BDL_HB_X   "\u253f"

tblstyle_t table_styles[] = {
    TSTYLE_SIMPLE_V1,   
    TSTYLE_SIMPLE_V2,   
    TSTYLE_SIMPLE_V3,           
    TSTYLE_ASCII_V1,    
    TSTYLE_ASCII_V2,    
    TSTYLE_ASCII_V3,            
    TSTYLE_DOUBLE_V1,   
    TSTYLE_DOUBLE_V2,   
    TSTYLE_DOUBLE_V3,   
    TSTYLE_DOUBLE_V4,                   
    TSTYLE_SINGLE_V1,   
    TSTYLE_SINGLE_V2,                   
    TSTYLE_HEAVY_V1,    
    TSTYLE_HEAVY_V2,    
    TSTYLE_HEAVY_V3     
};

// Always nice to have
#define FALSE 0
#define TRUE 1

// Utility macro to index table matrix
#define TIDX(_r,_c) ((_r)*t->nCol+(_c))

/**
 * Find the number of Unicode code point (characters) in the byte sequence pointed to
 * by 's'
 * Note on implementation:
 * A unicode sequence is one of:
 * 00000000 -- 0000007F: 	0xxxxxxx
 * 00000080 -- 000007FF: 	110xxxxx 10xxxxxx
 * 00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
 * 00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * And we can count the code points by masking the top nibbles first 2 bits and
 * check if these are not 0x80
 * @param s
 * @return Number of UTF8 code points (characters)
 */

size_t
utf8len(const char *s) {
    size_t len = 0;
    for (; *s; ++s) {
        if ((*s & 0xC0) != 0x80)
            ++len;
    }
    return len;
}

/**
 * Check that row and col arguments are within valid range before accessing cells
 * @param t Table pointer
 * @param row Row of cell
 * @param col Column of cell
 * @return 0 on valid range, -1 otherwise
 */
static int
_utable_rc_chk(table_t *t, size_t row, size_t col) {
    if (row >= t->nRow || col >= t->nCol) {
#ifndef TABLE_UNIT_TEST
        logmsg(LOG_ERR, "Table cell specified is out of range [%zu, %zu]", row, col);
#endif
        return -1;
    }
    return 0;
}

/**
 * Initialize a new cell 
 * @param t Table pointer
 * @param row Row of cell
 * @param col Column of cell
 * @return 0 on success, -1 on failure
 */
static int
_utable_init_cell(table_t *t, size_t row, size_t col) {
    tcell_t *c = &t->c[TIDX(row, col)];
    c->t = NULL;
    c->halign = LEFTALIGN;
    c->pRow = row;
    c->pCol = col;
    c->merged = FALSE;
    c->cspan = 1;
    c->rspan = 1;
    return 0;
}

/**
 * Create a new table of the specified size
 * @param nRow Number of rows
 * @param nCol Number of cols
 * @return NULL on error , pointer to the newly created table otherwise
 */
table_t *
utable_create(size_t nRow, size_t nCol) {
    table_t *t = calloc(1,sizeof (table_t));
    if (NULL == t)
        return NULL;
    t->nRow = nRow;
    t->nCol = nCol++;
    t->headerLine = TRUE;

    ++nRow;
    // We allocate one extra row in case a title should be displayed
    t->c = calloc(nRow*nCol, sizeof (tcell_t));
    t->colwidth = calloc(nCol, sizeof (size_t));
    t->mincolwidth = calloc(nCol, sizeof (size_t));
    if (t->c == NULL || t->colwidth == NULL || t->mincolwidth == NULL) {
        goto tbl_err;
    }

    for (size_t r = 0; r < nRow; r++) {
        for (size_t c = 0; c < nCol; c++) {
            if (_utable_init_cell(t, r, c)) {
                goto tbl_err;
            }
        }
    }

    return t;

tbl_err:
#ifndef TABLE_UNIT_TEST
    logmsg(LOG_ERR, "Failed to create layout table");
#endif
    free(t->c);
    free(t->colwidth);
    free(t->mincolwidth);
    free(t);
    return NULL;
}

/**
 * Initialize the table with string from a matrix. It is the calling routines
 * responsibility that the size of the data matrix matches that of the table.
 * @param t Table pointer
 * @param data A matrix of data to be used as initialization
 * @return 0 on success, -1 on failure
 */
int
utable_set(table_t *t, char *data[]) {
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nCol; c++) {
            if (utable_set_cell(t, r, c, data[TIDX(r, c)]))
                return -1;
        }
    }
    return 0;
}

/**
 * Combine table creation and initialization with error check
 * @param t Table pointer
 * @param nRow Number of rows
 * @param nCol Number of cols
 * @param data A matrix of data to be used as initialization
 * @return NULL on failure, pointer to new table otherwise
 */
table_t *
utable_create_set(size_t nRow, size_t nCol, char *data[]) {
    table_t *t = utable_create(nRow, nCol);
    if (NULL == t) {
        return NULL;
    }
    if (-1 == utable_set(t, data)) {
        utable_free(t);
        return NULL;
    }
    return t;
}

/** 
 * Free (destroy) a previously created table
 * @param t Table pointer
 */
void
utable_free(table_t *t) {
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nCol; c++) {
            if (t->c[TIDX(r, c)].t) {
                free(t->c[TIDX(r, c)].t);
            }
        }
    }
    free(t->c);
    free(t->colwidth);
    free(t->mincolwidth);
    free(t->title);
    free(t);
}

/**
 * Set a title to the table. How the title is displayed is determined
 * by the table style.
 * @param t Table pointer
 * @param title
 */
void
utable_set_title(table_t *t, char *title, titlestyle_t style) {
    if (t->title)
        free(t->title);
    t->title = strdup(title);
    t->titleStyle = style;
}

/**
 * Enable or disable the extra line below the header row (the first row in the
 * data). The style of the line is determined by the table style. The hedaer
 * line is by default enabled when the table is created.
 * @param t Table pointer
 * @param headerLine Flag to disable/enable header
 */
void
utable_set_headerline(table_t *t, _Bool headerLine) {
    t->headerLine = headerLine;
}

/**
 * Get a reference to the specified cell
 * @param t Table pointer
 * @param row Row of cell
 * @param col Column of cell
 * @return NULL if row/col is out of range, the cell otherwise
 */
static tcell_t*
_utable_get_cell(table_t *t, int row, int col) {
    if (_utable_rc_chk(t, row, col))
        return NULL;
    return &t->c[TIDX(row, col)];
}

/**
 * Return the text in the specified table cell
 * @param t Table pointer
 * @param row Row of cell
 * @param col Column of cell
 * @return Pointer to text, NULL if out of range or no text
 */
char *
utable_get_cell(table_t *t, int row, int col) {
    if (_utable_rc_chk(t, row, col))
        return NULL;
    return t->c[TIDX(row, col)].t;
}

/**
 * Set the number of columns this cell spans
 * @param t Table pointer
 * @param row Row of cell
 * @param col Column of cell
 * @param cspan Column span to set
 * @return 0 on success, -1 on failure
 */
int
utable_set_cell_colspan(table_t *t, size_t row, size_t col, size_t cspan) {
    if (_utable_rc_chk(t, row, col) || col + cspan - 1 >= t->nCol)
        return -1;
    tcell_t *cell = _utable_get_cell(t, row, col);
    cell->cspan = cspan;
    cell->merged = FALSE;
    cell->pRow = row;
    cell->pCol = col;

    for (size_t c = col + 1; c < col + cspan; c++) {
        cell = _utable_get_cell(t, row, c);
        cell->merged = TRUE;
        cell->pRow = row;
        cell->pCol = col;
    }
    return 0;
}

/**
 * Set the horizontal text alignment for this cell 
 * @param t Table pointer
 * @param row Row of cell
 * @param col Column of cell
 * @param halign Horizontal align to use
 * @return 0 on success, -1 on failure
 */
int
utable_set_cell_halign(table_t *t, int row, int col, halign_t halign) {
    if (_utable_rc_chk(t, row, col))
        return -1;
    t->c[TIDX(row, col)].halign = halign;
    return 0;
}

/**
 * Set the horizontal text alignment for this cell 
 * @param t Table pointer
 * @param row Row of cell
 * @param col Column of cell
 * @return halign_t on success, -1 on failure
 */
int
utable_get_cell_halign(table_t *t, int row, int col) {
    if (_utable_rc_chk(t, row, col))
        return -1;
    return (int) t->c[TIDX(row, col)].halign;
}

/**
 * Set the horizontal text alignment for specified row
 * @param t Table pointer
 * @param row Row to set
 * @param halign Horizontal align to use
 * @return 0 on success, -1 on failure
 */
int
utable_set_row_halign(table_t *t, int row, halign_t halign) {
    for (size_t c = 0; c < t->nCol; c++) {
        if (utable_set_cell_halign(t, row, c, halign))
            return -1;
    }
    return 0;
}

/**
 * Set horizontal alignment for the entire table
 * @param t Table pointer
 * @param halign Horizontal align to use
 * @return 0 on success, -1 on failure
 */
int
utable_set_table_halign(table_t *t, halign_t halign) {
    for (size_t r = 0; r < t->nRow; r++) {
        if( -1 == utable_set_row_halign(t,r,halign) )
            return -1;
    }    
    return 0;
}


/**
 * Set the horizontal text alignment for specified column
 * @param t Table pointer
 * @param col Column to set
 * @param halign Horizontal align to use
 * @return 0 on success, -1 on failure
 */
int
utable_set_col_halign(table_t *t, int col, halign_t halign) {
    for (size_t r = 0; r < t->nRow; r++) {
        if (utable_set_cell_halign(t, r, col, halign))
            return -1;
    }
    return 0;
}

/**
 * Set the text value for the specified cell. The value stored in the cell will be a 
 * newly allocated space for this string.
 * @param t Table pointer
 * @param row 
 * @param col
 * @param txt
 * @return 0 on success, -1 on failure
 */
int
utable_set_cell(table_t *t, size_t row, size_t col, char *val) {
    if (_utable_rc_chk(t, row, col) || t->c[TIDX(row, col)].merged)
        return -1;
    if (t->c[TIDX(row, col)].t) {
        free(t->c[TIDX(row, col)].t);
    }
    t->c[TIDX(row, col)].t = strdup(val);
    return 0;
}

/**
 * Set the cell left and right padding
 * @param t Table pointer
 * @param lpad
 * @param rpad
 * @return 0 on success, -1 on failure
 */
void
utable_set_table_cellpadding(table_t *t, size_t lpad, size_t rpad) {
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nCol; c++) {
            utable_set_cellpadding(t, r, c, lpad, rpad);
        }
    }
}

/**
 * Set the cell left and right padding
 * @param t Table pointer
 * @param row 
 * @param col
 * @param lpad
 * @param rpad
 * @return 0 on success, -1 on failure
 */
int
utable_set_cellpadding(table_t *t, size_t row, size_t col, size_t lpad, size_t rpad) {
    if (_utable_rc_chk(t, row, col) || t->c[TIDX(row, col)].merged)
        return -1;
    t->c[TIDX(row, col)].lpad = lpad;
    t->c[TIDX(row, col)].rpad = rpad;
    return 0;
}

/**
 * Set the cell left and right padding
 * @param t Table pointer
 * @param lpad
 * @param rpad
 * @return 0 on success, -1 on failure
 */
void
utable_set_col_cellpadding(table_t *t, size_t col, size_t lpad, size_t rpad) {
    for (size_t r = 0; r < t->nRow; r++) {
        utable_set_cellpadding(t, r, col, lpad, rpad);
    }
}


static void
_utable_get_cp(table_t *t, size_t row, size_t col, size_t *lpad, size_t *rpad) {
    if (t->c[TIDX(row, col)].merged) {
        row = t->c[TIDX(row, col)].pRow;
        col = t->c[TIDX(row, col)].pCol;
    }
    *lpad = t->c[TIDX(row, col)].lpad;
    *rpad = t->c[TIDX(row, col)].rpad;
}

/**
 * Set the width for the specified column
 * @param t Table pointer
 * @param col Column to set
 * @param width Width to set
 * @return 0 on success, -1 on failure
 */
int
utable_set_colwidth(table_t *t, size_t col, size_t width) {
    if (col >= t->nCol)
        return -1;
    t->colwidth[col] = width;
    return 0;
}

/**
 * Set the width for all columns in the table
 * @param t Table pointer
 * @param width Width to set
 */
void
utable_set_table_colwidth(table_t *t, size_t width) {
    for (size_t c = 0; c < t->nCol; c++) {
        utable_set_colwidth(t, c, width);
    }
}

/**
 * Set the min width for the specified column
 * @param t Table pointer
 * @param col Column to set
 * @param width Width to set
 */
int
utable_set_mincolwidth(table_t *t, size_t col, size_t width) {
    if (col >= t->nCol)
        return -1;
    t->mincolwidth[col] = width;
    return 0;
}

/**
 * Set the minimum width for all columns in the table
 * @param t Table pointer
 * @param width Width to set
 */
void
utable_set_table_mincolwidth(table_t *t, size_t width) {
    for (size_t c = 0; c < t->nCol; c++) {
        utable_set_mincolwidth(t, c, width);
    }
}

static _Bool cut_in_padding = FALSE;

/**
 * Determine policy when table width is set too small to fit everythign that is
 * needed. If flag is set to true then the left padding will be kept as much as
 * possible and the text and the right padding will be cut as needed. 
 * If false then the padding will be kept as much as possible (both left and right)
 * and the text cut as needed.
 * Default is false sp the padding will be kept as much as possible
 * @param cutInPadding
 */
void
utable_set_padding_policy(_Bool cutInPadding) {
    cut_in_padding = cutInPadding;
}

/**
 * Internal helper function to draw a single line of table data 
 * @param file Filepointer to write to
 * @param t Table pointer
 * @param row Row to draw
 * @param style Table style
 */
#define MAXPADCHARS 256
#define MAXPBUFF (10*1024)

static void
_utable_draw_cellcontent_row(char *buff, int *buffleft, table_t *t, size_t row, char *midleft, char *midright, char *midvert) {

    size_t c = 0;
    int buffused=0;
    char pbuff[MAXPBUFF];
    
    *pbuff='\0';
    while (c < t->nCol) {
        int w = 0;

        // Determine the total width of this cell. This needs to take
        // into account the fact that this could be a cell that is spanning
        // multiple other cells.
        const size_t cspan = t->c[TIDX(row, c)].cspan;
        size_t lpad, rpad;
        _utable_get_cp(t, row, c, &lpad, &rpad);
        const size_t totpad = lpad + rpad;
        char lpadstr[MAXPADCHARS], rpadstr[MAXPADCHARS];

        for (size_t i = 0; i < lpad; ++i) {
            lpadstr[i] = ' ';
        }
        for (size_t i = 0; i < rpad; ++i) {
            rpadstr[i] = ' ';
        }
        lpadstr[lpad] = '\0';
        rpadstr[rpad] = '\0';
        for (size_t cs = 0; cs < cspan; cs++) {
            w += t->colwidth[c + cs] + 1;
        }
        w -= 1; // Don't include the last border since that remains

        const char *txt = t->c[TIDX(row, c)].t;
        char txtbuff[512], txtbuff2[1024];

        // A choice in how to handle the case when the column width is smaller than 
        // what is needed. We can either include the padding chars in the limiting
        // or try to maintain the padding and only limiting the actual content
        if (cut_in_padding) {
            // Cut the text so that even the right padding is cut
            snprintf(txtbuff2, sizeof (txtbuff2), "%s%s%s", lpadstr, txt, rpadstr);
            size_t len = xmb_offset(txtbuff2, w);
            xstrlcpy(txtbuff, txtbuff2, len + 1); // +1 since the length also must include the terminating 0
        } else {
            // Keep the padding chars as much as possible
            snprintf(txtbuff2, sizeof (txtbuff2), "%s%s", lpadstr, txt);
            size_t len = xmb_offset(txtbuff2, w - rpad);
            xstrlcpy(txtbuff, txtbuff2, len + 1); // +1 since the length also must include the terminating 0            
            snprintf(txtbuff2, sizeof (txtbuff2), "%s%s", txtbuff, rpadstr);
            len = xmb_offset(txtbuff2, w)+1; // Offset starts at 0
            xstrlcpy(txtbuff, txtbuff2, len + 1); // +1 since the length also must include the terminating 0  
        }

        // We need these values to center the text
        const int w_half = w / 2;
        const int len_half = (utf8len(txtbuff) + totpad) / 2;
        int padlen;

        // We reuse the rpadstr buffer to use for padding the cell to the
        // assigned width in order to make the text left, right or center aligned
        // We have to do this ourself since it is not safe to use "%*s" format
        // since this is not UTF safe since internally snprintf() uses strlen())
        memset(rpadstr, 0, sizeof (rpadstr));
        if (w > (int) utf8len(txtbuff))
            for (size_t i = 0; i < w - utf8len(txtbuff); ++i) {
                rpadstr[i] = ' ';
            }
        buffused=0;
        if (c == 0) {
            switch (t->c[TIDX(row, c)].halign) {
                case RIGHTALIGN:
                    buffused = snprintf(pbuff,MAXPBUFF, "%s%s%s", midleft, rpadstr, txtbuff);
                    xstrlcat(buff,pbuff,*buffleft);
                    *buffleft -= buffused;
                    break;
                case LEFTALIGN:
                    buffused = snprintf(pbuff,MAXPBUFF, "%s%s%s", midleft, txtbuff, rpadstr);
                    xstrlcat(buff,pbuff,*buffleft);
                    *buffleft -= buffused;                    
                    break;
                case CENTERALIGN:
                    memset(rpadstr, 0, sizeof (rpadstr));
                    padlen = (w - w_half + len_half) - utf8len(txtbuff);
                    for (int i = 0; i < padlen; ++i) {
                        rpadstr[i] = ' ';
                    }
                    buffused = snprintf(pbuff,MAXPBUFF, "%s%*s%s%s", midleft, w_half - len_half, "", txtbuff, rpadstr);
                    xstrlcat(buff,pbuff,*buffleft);
                    *buffleft -= buffused;                    
                    break;
            }
        } else {
            switch (t->c[TIDX(row, c)].halign) {
                case RIGHTALIGN:
                    buffused = snprintf(pbuff,MAXPBUFF, "%s%s%s", midvert, rpadstr, txtbuff);
                    xstrlcat(buff,pbuff,*buffleft);
                    *buffleft -= buffused;                    
                    break;
                case LEFTALIGN:
                    buffused = snprintf(pbuff,MAXPBUFF, "%s%s%s", midvert, txtbuff, rpadstr);
                    xstrlcat(buff,pbuff,*buffleft);
                    *buffleft -= buffused;                    
                    break;
                case CENTERALIGN:
                    memset(rpadstr, 0, sizeof (rpadstr));
                    padlen = (w - w_half + len_half) - utf8len(txtbuff);
                    for (int i = 0; i < padlen; ++i) {
                        rpadstr[i] = ' ';
                    }
                    buffused = snprintf(pbuff,MAXPBUFF, "%s%*s%s%s", midvert, w_half - len_half, "", txtbuff, rpadstr);
                    xstrlcat(buff,pbuff,*buffleft);
                    *buffleft -= buffused;                    
                    break;
            }
        }
        
        c += cspan;
    }
    buffused = snprintf(pbuff,MAXPBUFF, "%s\n", midright);
    xstrlcat(buff,pbuff,*buffleft);    
    *buffleft -= buffused;    
}

/**
 * Set automatic column width for columns with no user specified width
 * @param t Table pointer
 */
static void
_utable_set_autocolwidth(table_t *t) {
    for (size_t c = 0; c < t->nCol; c++) {
        if (t->colwidth[c] == 0) {
            // User has not yet set column width so find the widest text
            for (size_t r = 0; r < t->nRow; r++) {
                size_t lpad, rpad;
                _utable_get_cp(t, r, c, &lpad, &rpad);
                if (utf8len(t->c[TIDX(r, c)].t) + lpad + rpad > t->colwidth[c]) {
                    t->colwidth[c] = utf8len(t->c[TIDX(r, c)].t) + lpad + rpad;
                }
            }
            t->colwidth[c] = MAX(t->colwidth[c], t->mincolwidth[c]);
            // Make sure the column width is at least 1 character wide
            if (0 == t->colwidth[c])
                t->colwidth[c] = 1;
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
 * @param t Table pointer
 * @param eval
 * @param mark
 * @param row
 */
static void
_utable_mark_verticals(table_t *t, int eval[], int mark, size_t row) {
    size_t c = 0;
    size_t absw = 0;
    while (c < t->nCol) {

        int w = 0;
        for (size_t cs = 0; cs < t->c[TIDX(row, c)].cspan; cs++) {
            w += t->colwidth[c + cs] + 1;
        }

        absw += w;
        eval[absw - 1] |= mark;
        c += t->c[TIDX(row, c)].cspan;
    }
}

/**
 * Internal helper functions to write out the border characters identified by the
 * _mark_verticals
 * @param file 
 * @param totwidth
 * @param eval
 * @param s0
 * @param s1
 * @param s2
 * @param s3
 */
static void
_utable_stroke_verticals(char *buff, int *buffleft, int totwidth, int eval[], char *s0, char *s1, char *s2, char *s3) {
    int buffused=0;
    char pbuff[MAXPBUFF];
    for (int i = 0; i < totwidth - 1; i++) {
        buffused=0;
        if (eval[i] == 0) { // HORIZONTAL
            if (s0 == NULL)
                buffused = snprintf(pbuff,MAXPBUFF, "#ERR#");
            else
                buffused = snprintf(pbuff,MAXPBUFF, "%s", s0);
            
            xstrlcat(buff,pbuff,*buffleft);
            *buffleft -= buffused;
        } else if (eval[i] == 1) {// TOP DOWN
            if (s1 == NULL)
                buffused = snprintf(pbuff,MAXPBUFF, "#ERR#");
            else
                buffused = snprintf(pbuff,MAXPBUFF, "%s", s1);
            
            xstrlcat(buff,pbuff,*buffleft);
            *buffleft -= buffused;
            
        } else if (eval[i] == 2) { // BOTTOM UP
            if (s2 == NULL)
                buffused = snprintf(pbuff,MAXPBUFF, "#ERR#");
            else
                buffused = snprintf(pbuff,MAXPBUFF, "%s", s2);

            xstrlcat(buff,pbuff,*buffleft);
            *buffleft -= buffused;            
        } else { // CROSS
            if (s3 == NULL)
                buffused = snprintf(pbuff,MAXPBUFF, "#ERR#");
            else
                buffused = snprintf(pbuff,MAXPBUFF, "%s", s3);

            xstrlcat(buff,pbuff,*buffleft);
            *buffleft -= buffused;
        }
    }
    if( *buffleft < 1 ) {
#ifndef TABLE_UNIT_TEST 
        logmsg(LOG_CRIT,"Out of buffer space in _utable_stroke_verticals()");
#endif                
        exit(EXIT_FAILURE);
    }
}

/**
 * Set display of interior horizontal and vertical border
 * @param t Table pointer
 * @param v Flag to enable/disable vertical interior borders
 * @param h Flag to enable/disable horizontal interior borders
 */
void
utable_set_interior(table_t *t, _Bool v, _Bool h) {
    t->interior_v = v;
    t->interior_h = h;
}


/**
 * Stroke the entire table in the specified style to specified file descriptor
 * @param t     Table pointer
 * @param fd    File descriptor to write to
 * @param style Table layout style to use
 *              Possible styles are: TSTYLE_TOP_DL, TSTYLE_FULL_DL, TSTYLE_SL, TSTYLE_TOPBOTTOM_DL
 * @param col_lines Separate columns with lines
 * @return -1 on failure, 0 on success
 */
int
utable_stroke(table_t *t, int fd, tblstyle_t style) {
  
    char *buff= calloc(MAXSTROKEBUFF,sizeof(char)); 
    if( NULL == buff || -1 == utable_strstroke(t, buff, MAXSTROKEBUFF, style) ) {
        free(buff);
        return -1;
    }
    _writef(fd,"%s\n",buff);
    free(buff);
    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-protector"

/**
 * Stroke the entire table in the specified style to specified string buffer
 * @param t     Table pointer
 * @param fd    File descriptor to write to
 * @param style Table layout style to use
 *              Possible styles are: TSTYLE_TOP_DL, TSTYLE_FULL_DL, TSTYLE_SL, TSTYLE_TOPBOTTOM_DL
 * @param col_lines Separate columns with lines
 * @return -1 on failure, 0 on success
 */
int
utable_strstroke(table_t *t, char *buff, size_t bufflen, tblstyle_t style) {

    _utable_set_autocolwidth(t);

    if (t->title) {
        if(!t->titleCopied) {
            // Insert the title as the top row by moving existing row down one step
            memmove(&t->c[t->nCol], &t->c[0], (t->nRow * t->nCol) * sizeof (tcell_t));
            memset(&t->c[0], 0, t->nCol * sizeof (tcell_t));
            t->c[0].t = strdup(t->title);
            t->c[0].halign = CENTERALIGN;
            utable_set_cell_colspan(t, 0, 0, t->nCol);
            t->nRow++;
            t->titleCopied = TRUE;
        } else {
            free(t->c[0].t);
            t->c[0].t = strdup(t->title);
        }
    }

    // Get the total width of the table in characters
    size_t totwidth = 0;
    for (size_t i = 0; i < t->nCol; i++)
        totwidth += t->colwidth[i] + 1;
    int eval[totwidth];

    char *top_horizontal;
    char *top_left, *top_right;
    char *top_down;
    char *top_middle_left, *top_middle_cross;
    char *top_middle_horizontal;
    char *top_middle_right;    
    char *middle_left;    
    char *middle_horizontal;
    char *middle_right;
    char *middle_horizontal_up;
    char *middle_horizontal_down;
    char *middle_cross;
    char *middle_vertical;
    char *border_vertical;
    char *bottom_horizontal;
    char *bottom_left, *bottom_right, *bottom_up;
    
    top_left = " ";
    top_horizontal = " ";
    top_down = " ";
    top_right = " ";
    top_middle_left = " ";
    top_middle_right = " ";
    top_middle_cross = " ";
    top_middle_horizontal = " ";
    middle_left = " ";
    middle_horizontal = " ";
    middle_right = " ";
    middle_horizontal_up = " ";
    middle_horizontal_down = " ";
    middle_cross = " ";
    middle_vertical = " ";
    border_vertical = " ";
    bottom_left = " ";
    bottom_horizontal = " ";
    bottom_right = " ";
    bottom_up = " ";
    
    if (style == TSTYLE_ASCII_V2 || style == TSTYLE_ASCII_V1 || style == TSTYLE_ASCII_V3) {
        // The very top outer border line
        top_horizontal = style==TSTYLE_ASCII_V3 ? "-" : "=";
        top_down = "+";
        
        // The very bottom outer border line
        bottom_horizontal = top_horizontal;
        bottom_up = "+";
                
        // The four corners
        top_left = style == TSTYLE_ASCII_V2 ? "+" : top_horizontal;
        top_right = style == TSTYLE_ASCII_V2 ? "+" : top_horizontal;
        bottom_left = style == TSTYLE_ASCII_V2 ? "+" : bottom_horizontal;
        bottom_right = style == TSTYLE_ASCII_V2 ? "+" : bottom_horizontal;

        // The line below the header row, just before the data rows
        top_middle_left = style == TSTYLE_ASCII_V2 ? "+" : top_horizontal;
        top_middle_right = style == TSTYLE_ASCII_V2 ? "+" : top_horizontal;
        top_middle_cross = "+";
        top_middle_horizontal = top_horizontal;
        
        // The interior horizontal line (also used for line below the title)
        middle_horizontal = "-";
        
        // Interior vertical
        middle_vertical = "|";
        
        // The vertical outer border
        border_vertical = style == TSTYLE_ASCII_V2 ? "|" : " ";
        
        // The connection to the lefty and right border for interior lines
        middle_left = style == TSTYLE_ASCII_V2 ? "+" : middle_horizontal;
        middle_right = style == TSTYLE_ASCII_V2 ? "+" : middle_horizontal;
        
        // Connection for interior lines
        middle_horizontal_up = "+";
        middle_horizontal_down = "+";
        middle_cross = "+";
                
    } else if (style == TSTYLE_DOUBLE_V1 || style == TSTYLE_DOUBLE_V2 || 
               style == TSTYLE_DOUBLE_V3 || style==TSTYLE_DOUBLE_V4 ) {
        // The very top outer border line
        top_horizontal = BDL_DH_H;
        top_down = BDL_DH_DH;

        // The very bottom outer border line
        bottom_horizontal = BDL_DH_H;
        bottom_up = BDL_DH_UH;
        
        // The four corners
        top_left =      (style==TSTYLE_DOUBLE_V2 || style==TSTYLE_DOUBLE_V4 ) ? BDL_DL_DR : 
                            style == TSTYLE_DOUBLE_V3 ? BDL_DH_DR : top_horizontal;
        top_right =     (style==TSTYLE_DOUBLE_V2 || style==TSTYLE_DOUBLE_V4 )  ? BDL_DL_DL : 
                            style == TSTYLE_DOUBLE_V3 ? BDL_DH_DL : top_horizontal;
        bottom_left =   (style==TSTYLE_DOUBLE_V2 || style==TSTYLE_DOUBLE_V4 )  ? BDL_DL_UR : 
                            style == TSTYLE_DOUBLE_V3 ? BDL_DH_UR : bottom_horizontal;
        bottom_right =  (style==TSTYLE_DOUBLE_V2 || style==TSTYLE_DOUBLE_V4 )  ? BDL_DL_UL : 
                            style == TSTYLE_DOUBLE_V3 ? BDL_DH_UL : bottom_horizontal;        
                
        // The line below the header row, just before the data rows
        // This is set to be the same as the border style
        top_middle_left =  style==TSTYLE_DOUBLE_V2 ? BDL_DL_VR : 
                            style==TSTYLE_DOUBLE_V3 ? BDL_DH_VR : top_horizontal;
        top_middle_right = style==TSTYLE_DOUBLE_V2 ? BDL_DL_VL : 
                            style==TSTYLE_DOUBLE_V3 ? BDL_DH_VL : top_horizontal;
        top_middle_cross = BDL_DH_X;  
        top_middle_horizontal = top_horizontal;
                
        // The vertical outer border
        border_vertical =   (style==TSTYLE_DOUBLE_V2 || style==TSTYLE_DOUBLE_V4 ) ? BDL_DL_V : 
                             style==TSTYLE_DOUBLE_V3 ? BDL_V : " ";
        
        // The interior horizontal line (also used for line below the title)
        middle_horizontal = BDL_H;

        // Interior vertical
        middle_vertical = BDL_V ;               

        // The connection to the lefty and right border for interior lines
        middle_left = (style==TSTYLE_DOUBLE_V2 || style==TSTYLE_DOUBLE_V4 ) ? BDL_DV_VR : 
                        style == TSTYLE_DOUBLE_V3 ? BDL_VR : middle_horizontal;
        middle_right = (style==TSTYLE_DOUBLE_V2 || style==TSTYLE_DOUBLE_V4 )  ? BDL_DV_VL : 
                        style == TSTYLE_DOUBLE_V3 ? BDL_VL : middle_horizontal;

        
        // Connection for interior lines
        middle_horizontal_up = BDL_UH;
        middle_horizontal_down = BDL_DH;
        middle_cross = BDL_X;
        
        if( style==TSTYLE_DOUBLE_V4 ) {
            // Just a thick border around no special treatment of top row
            top_middle_left = middle_left;  
            top_middle_right = middle_right; 
            top_middle_cross = middle_cross;
            top_middle_horizontal = middle_horizontal;
        }                

    } else if( style==TSTYLE_SINGLE_V1 || style==TSTYLE_SINGLE_V2 ) {
        
        // The very top outer border line
        top_horizontal = BDL_H;
        top_down = BDL_DH;

        // The very bottom outer border line
        bottom_horizontal = top_horizontal;
        bottom_up = BDL_UH;
        
        // The four corners
        top_left =      style == TSTYLE_SINGLE_V2 ? BDL_DR : top_horizontal;
        top_right =     style == TSTYLE_SINGLE_V2 ? BDL_DL : top_horizontal;
        bottom_left =   style == TSTYLE_SINGLE_V2 ? BDL_UR : top_horizontal;
        bottom_right =  style == TSTYLE_SINGLE_V2 ? BDL_UL : top_horizontal;
                
        // The line below the header row, just before the data rows
        // This is set to be the same as the border style
        top_middle_left =  style == TSTYLE_SINGLE_V2 ? BDL_VR : top_horizontal;
        top_middle_right = style == TSTYLE_SINGLE_V2 ? BDL_VL : top_horizontal;
        top_middle_cross = BDL_X;
        top_middle_horizontal = top_horizontal;

        // The vertical outer border
        border_vertical = style == TSTYLE_SINGLE_V2 ? BDL_V : " "; 
        
        // The interior horizontal line (also used for line below the title)
        middle_horizontal = BDL_H;

        // Interior vertical
        middle_vertical = BDL_V ;

        // The connection to the lefty and right border for interior lines
        middle_left  = style == TSTYLE_SINGLE_V2 ? BDL_VR : middle_horizontal;
        middle_right = style == TSTYLE_SINGLE_V2 ? BDL_VL : middle_horizontal;

        // Connection for interior lines
        middle_horizontal_up = BDL_UH;
        middle_horizontal_down = BDL_DH;
        middle_cross = BDL_X;
        
} else if( style==TSTYLE_HEAVY_V1 || style==TSTYLE_HEAVY_V2 || style==TSTYLE_HEAVY_V3 ) {
        
        // The very top outer border line
        top_horizontal = BDL_HB_H;
        top_down = BDL_HB_DH;

        // The very bottom outer border line
        bottom_horizontal = top_horizontal;
        bottom_up = BDL_HB_UH;
        
        // The four corners
        top_left =      style == TSTYLE_HEAVY_V1 ? top_horizontal : BDL_HB_DR;
        top_right =     style == TSTYLE_HEAVY_V1 ? top_horizontal : BDL_HB_DL;
        bottom_left =   style == TSTYLE_HEAVY_V1 ? top_horizontal : BDL_HB_UR;
        bottom_right =  style == TSTYLE_HEAVY_V1 ? top_horizontal : BDL_HB_UL;
                
        // The line below the header row, just before the data rows
        // This is set to be the same as the border style
        top_middle_left =  style == TSTYLE_HEAVY_V1 ? top_horizontal : style == TSTYLE_HEAVY_V2 ? BDL_HL_VR : BDL_HB_VR;
        top_middle_right = style == TSTYLE_HEAVY_V1 ? top_horizontal : style == TSTYLE_HEAVY_V2 ? BDL_HL_VL : BDL_HB_VL;
        top_middle_cross = style == TSTYLE_HEAVY_V3 ? BDL_X : BDL_HB_X ;
        top_middle_horizontal = style == TSTYLE_HEAVY_V3 ? BDL_H : top_horizontal ;

        // The vertical outer border
        border_vertical = style == TSTYLE_HEAVY_V1 ? " " : BDL_HB_V ; 
        
        // The interior horizontal line (also used for line below the title)
        middle_horizontal = BDL_H;

        // Interior vertical
        middle_vertical = BDL_V ;

        // The connection to the lefty and right border for interior lines
        middle_left  = style == TSTYLE_HEAVY_V1 ? middle_horizontal : BDL_HB_VR ;
        middle_right = style == TSTYLE_HEAVY_V1 ? middle_horizontal : BDL_HB_VL ;

        // Connection for interior lines
        middle_horizontal_up = BDL_UH;
        middle_horizontal_down = BDL_DH;
        middle_cross = BDL_X;
                        
    } else if( style==TSTYLE_SIMPLE_V1 || style==TSTYLE_SIMPLE_V2 || style==TSTYLE_SIMPLE_V3 )  {
        
        top_left = " ";
        top_horizontal = " ";
        top_down = " ";
        top_right = " ";
        top_middle_left = " ";
        top_middle_right = " ";
        top_middle_cross = " ";
        top_middle_horizontal = style==TSTYLE_SIMPLE_V3 ? " " : style==TSTYLE_SIMPLE_V1 ? BDL_H : BDL_DL_H;
        middle_left = " ";
        middle_horizontal = " ";
        middle_right = " ";
        middle_horizontal_up = " ";
        middle_horizontal_down = " ";
        middle_cross = " ";
        middle_vertical = " ";
        border_vertical = " ";
        bottom_left = " ";
        bottom_horizontal = " ";
        bottom_right = " ";
        bottom_up = " ";
        
    }
    
    if( ! t->interior_v ) {
        top_down = top_horizontal;
        top_middle_cross = top_middle_horizontal;
        bottom_up = bottom_horizontal;                
        middle_vertical = " ";        
        middle_horizontal_up = middle_horizontal;
        middle_horizontal_down = middle_horizontal;
        middle_cross = middle_horizontal;   
    }

    char pbuff[MAXPBUFF];
    *buff = '\0';
    memset(eval, 0, sizeof (int)*totwidth);
    int buffleft=bufflen;
    int len=snprintf(pbuff, MAXPBUFF, "%s", top_left);
    xstrlcat(buff,pbuff,buffleft);
    buffleft -= len;
    if(buffleft < 1) return -1;
    
    if (t->title)
        _utable_stroke_verticals(buff, &buffleft, totwidth, eval, top_horizontal, NULL, NULL, NULL);
    else {
        _utable_mark_verticals(t, eval, 1, 0);
        _utable_stroke_verticals(buff, &buffleft, totwidth, eval, top_horizontal, top_down, NULL, NULL);
    }
    if(buffleft < 1) return -1;
    
    len=snprintf(pbuff, MAXPBUFF, "%s\n", top_right);
    xstrlcat(buff,pbuff,buffleft);
    buffleft -= len;
    if(buffleft < 1) return -1;

    for (size_t r = 0; r < t->nRow; r++) {
        _utable_draw_cellcontent_row(buff, &buffleft, t, r, border_vertical, border_vertical, middle_vertical);
        if(buffleft < 1) return -1;
        
        memset(eval, 0, sizeof (int)*totwidth);
        _utable_mark_verticals(t, eval, 2, r);
        if (r < t->nRow - 1)
            _utable_mark_verticals(t, eval, 1, r + 1); // Top verticals
        
        if ( t->headerLine && ( (r == 0 && !t->title) || (r == 1 && t->title) ) ) {
            // The heavier line just beneath the header row before the data rows
            len=snprintf(pbuff, MAXPBUFF, "%s", top_middle_left);
            xstrlcat(buff,pbuff,buffleft);
            buffleft -= len;
            if(buffleft < 1) return -1;
            
            _utable_stroke_verticals(buff, &buffleft, totwidth, eval, top_middle_horizontal, NULL, NULL, top_middle_cross);
            if(buffleft < 1) return -1;
            
            len=snprintf(pbuff, MAXPBUFF, "%s\n", top_middle_right);
            xstrlcat(buff,pbuff,buffleft);
            buffleft -= len;
            
            buffleft -= len;
            if(buffleft < 1) return -1;
            
        } else if (r == 0 && t->title) {
            if (t->titleStyle == TITLESTYLE_LINE) {
                // The optional thin line beneath the title
                len=snprintf(pbuff, MAXPBUFF, "%s", middle_left);
                xstrlcat(buff,pbuff,buffleft);
                buffleft -= len;
                if(buffleft < 1) return -1;
                
                _utable_stroke_verticals(buff, &buffleft, totwidth, eval, middle_horizontal, middle_horizontal_down, middle_horizontal_up, middle_cross);
                if(buffleft < 1) return -1;
                
                len=snprintf(pbuff, MAXPBUFF, "%s\n", middle_right);
                xstrlcat(buff,pbuff,buffleft);
                buffleft -= len;
                if(buffleft < 1) return -1;
                
            }
        } else if (t->interior_h) {
            // Add lines between each data row
            if (r < t->nRow - 1) {
                len=snprintf(pbuff, MAXPBUFF, "%s", middle_left);
                xstrlcat(buff,pbuff,buffleft);
                buffleft -= len;
                if(buffleft < 1) return -1;
                
                _utable_stroke_verticals(buff, &buffleft, totwidth, eval, middle_horizontal, middle_horizontal_down, middle_horizontal_up, middle_cross);
                if(buffleft < 1) return -1;
                
                len=snprintf(pbuff, MAXPBUFF, "%s\n", middle_right);
                xstrlcat(buff,pbuff,buffleft);
                buffleft -= len;
                if(buffleft < 1) return -1;
                
            }
        }
    }
    len=snprintf(pbuff, MAXPBUFF, "%s", bottom_left);
    xstrlcat(buff,pbuff,buffleft);
    buffleft -= len;
    if(buffleft < 1) return -1;
    
    _utable_stroke_verticals(buff, &buffleft, totwidth, eval, bottom_horizontal, bottom_up, bottom_up, NULL);
    len=snprintf(pbuff, MAXPBUFF, "%s", bottom_right);
    xstrlcat(buff,pbuff,buffleft);
    buffleft -= len;
    if(buffleft < 1) return -1;

    return 0;
}

#pragma GCC diagnostic pop


#ifdef TABLE_UNIT_TEST

void ut1(void) {
    table_t *tbl = utable_create(7, 7);

    if (NULL == tbl) {
        printf("Cannot create table\n");
        exit(EXIT_SUCCESS);
    }
    char buff[64];

    for (size_t r = 0; r < tbl->nRow; r++) {
        for (size_t c = 0; c < tbl->nCol; c++) {
            snprintf(buff, sizeof (buff), " (%zu,%zu) ", r, c);
            utable_set_cell(tbl, r, c, buff);
        }
    }

    // if( -1==set_cell(tbl, 0, 1, "The very first table") )
    //     goto tbl_err;

    //if( -1==set_cell_colspan(tbl,0,1,3) )
    //    goto tbl_err;

    // if( -1==set_cell_halign(tbl,0,1,CENTERALIGN) )
    //     goto tbl_err;

    if (-1 == utable_set_cell_colspan(tbl, 2, 2, 3))
        goto tbl_err;
    if (-1 == utable_set_cell_halign(tbl, 2, 2, RIGHTALIGN))
        goto tbl_err;


    if (-1 == utable_set_colwidth(tbl, 1, 3))
        goto tbl_err;

    if (-1 == utable_set_colwidth(tbl, 0, 7))
        goto tbl_err;

    //set_table_mincolwidth(tbl,12);

    utable_set_table_cellpadding(tbl, 2, 2);

    if (-1 == utable_set_cell(tbl, 0, 0, " Much longer text than column width "))
        goto tbl_err;

    utable_set_title(tbl, "Table title", TITLESTYLE_LINE);

    //stroke_table(tbl,STDOUT_FILENO,TSTYLE_BORDER_TOPBOTTOM_DL);

    //utable_stroke(tbl, STDOUT_FILENO, TSTYLE_FULL_DL);

    return;

tbl_err:
    printf("\n");
    printf("Failed!\n");
    exit(EXIT_FAILURE);

}

void
ut2(void) {

    /*
    char *data[] = {
        "Titel 1","Titel 2","Titel 3","Titel 4","Titel 5","Titel 6",
        "2012-12-12 12:12","3000000001","30.345678","17.676767","ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE","olle",
        "2012-12-12 12:12","3000000001","30.345678","17.676767","ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE","olle"
    };
     */

    char *data[] = {
        "Titel 1",
        "ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE",
        "ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE",
    };


    //table_t *tbl = table_create_set(3, 6,data);
    table_t *tbl = utable_create_set(3, 1, data);


    if (NULL == tbl) {
        printf("Cannot create table\n");
        exit(EXIT_FAILURE);
    }

    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_SINGLE_V1);
}

void
ut3(void) {


    char *data[] = {
        "Titel 1", "Titel 2", "Titel 3", "Titel 4", "Titel 5", "Titel 6",
        "2012-12-12 12:12", "3000000001", "30.345678", "17.676767", "ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE", "olle",
        "2012-12-12 12:12", "3000000001", "30.345678", "17.676767", "ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE", "olle"
    };


    /*
    char *data[] = {
        "Kol 1", "Kol 2", 
        "ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE","Olle",
        "ÖVERJÄRVÅ TÅRNE 32, 134 34 LÅNGJÄRVI NUMÅENDE","Kalle"
    };
     */

    table_t *tbl = utable_create_set(3, 6, data);
    //table_t *tbl = utable_create_set(3, 1,data);


    if (NULL == tbl) {
        printf("Cannot create table\n");
        exit(EXIT_FAILURE);
    }

    utable_set_title(tbl, "Table title", TITLESTYLE_LINE);

    utable_set_row_halign(tbl, 0, CENTERALIGN);

    utable_set_interior(tbl, 0, 0);

    utable_set_title(tbl, "TSTYLE_ASCII_V1", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_ASCII_V1);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_ASCII_V2", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_ASCII_V2);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_ASCII_V3", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_ASCII_V3);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_DOUBLE_V1", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_DOUBLE_V1);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_DOUBLE_V2", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_DOUBLE_V2);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_DOUBLE_V3", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_DOUBLE_V3);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_DOUBLE_V4", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_DOUBLE_V4);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_SINGLE_V1", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_SINGLE_V1);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_SINGLE_V2", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_SINGLE_V2);
    
    
    printf("\n");
    utable_set_title(tbl, "TSTYLE_HEAVY_V1", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_HEAVY_V1);

    printf("\n");
    utable_set_title(tbl, "TSTYLE_HEAVY_V2", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_HEAVY_V2);    
    
    printf("\n");
    utable_set_title(tbl, "TSTYLE_HEAVY_V3", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_HEAVY_V3);    
    
    printf("\n");
    utable_set_title(tbl, "TSTYLE_SIMPLE_V1", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_SIMPLE_V1);    

    printf("\n");
    utable_set_title(tbl, "TSTYLE_SIMPLE_V2", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_SIMPLE_V2);    

    printf("\n");
    utable_set_title(tbl, "TSTYLE_SIMPLE_V3", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_SIMPLE_V3);    

    printf("\n");
    utable_set_headerline(tbl,FALSE);
    utable_set_table_cellpadding(tbl,1,1);
    utable_set_title(tbl, "TSTYLE_SIMPLE_V3", TITLESTYLE_LINE);
    utable_stroke(tbl, STDOUT_FILENO, TSTYLE_SIMPLE_V3);    
    
    printf("\n");
    
}


// Some rudimentary unit-test
// gcc -std=c99 -DTABLE_UNIT_TEST unicode_tbl.c xstr.c 

int
main(int argc, char **argv) {

    //ut2();
    //ut1();
    ut3();

    printf("\n");

    exit(EXIT_SUCCESS);
}


#endif


// [EOF]]
