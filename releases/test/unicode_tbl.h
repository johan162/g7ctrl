/* =========================================================================
 * File:        uniode_tbl.c
 * Description: Functions to create and write a rudimentary table
 *              using unicode page U+25xx characters for output nicely
 *              formatted output to a terminal
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: unicode_tbl.h 762 2015-03-04 10:52:35Z ljp $
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

#ifndef UNICODE_TBL_H
#define	UNICODE_TBL_H

#ifdef	__cplusplus
extern "C" {
#endif

    typedef enum {
        LEFTALIGN, RIGHTALIGN, CENTERALIGN
    } halign_t;

    typedef enum {
        TSTYLE_TOP_DL, TSTYLE_FULL_DL, TSTYLE_SL
    } tblstyle_t;

    /**
     * Data structure that represents one cell in the table
     */
    typedef struct {
        char *t;
        halign_t halign;
        int pRow, pCol;
        _Bool merged;
        size_t rspan;
        size_t cspan;
    } tcell_t;

    /**
     * Data structure that represents the table
     */
    typedef struct tcont_t {
        size_t nRow, nCol;
        tcell_t *c;
        size_t *w;
    } table_t;


    int
    set_cell_colspan(table_t *t, size_t row, size_t col, size_t cspan);

    int
    set_cell_halign(table_t *t, int row, int col, halign_t halign);

    int
    set_cell(table_t *t, size_t row, size_t col, char *txt);

    table_t *
    table_create(size_t nRow, size_t nCol);
    
    table_t *
    table_create_set(size_t nRow, size_t nCol, char *data[]);
    
    int
    table_set(table_t *t, char *data[]);

    void
    table_free(table_t *t);

    void
    stroke_table(table_t *t, tblstyle_t style);


#ifdef	__cplusplus
}
#endif

#endif	/* UNICODE_TBL_H */

