/* =========================================================================
 * File:        uniode_tbl.c
 * Description: Functions to create and write a rudimentary table
 *              using unicode page U+25xx characters for output nicely
 *              formatted output to a terminal
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id$
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
#include <string.h>


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

/*
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

#define FALSE 0
#define TRUE 1

typedef enum {
    LEFTALIGN, RIGHTALIGN, CENTERALIGN
} halign_t;

typedef enum {
    BORDSINGLE, BORDTHICK, BORDDOUBLE, BORDNONE
} border_t;

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
    int *w;
} table_t;

/***/
int
_rc_chk(table_t *t, int row, int col) {
    if( row >= t->nRow || col >= t->nCol )
        return -1;
    return 0;
}

tcell_t*
get_cell(table_t *t, int row, int col) {
    if( _rc_chk(t, row, col) )
        return NULL;
    return &t->c[row*t->nCol + col];
}

int
set_cell_colspan(table_t *t, int row, int col, int cspan) {
    if( _rc_chk(t, row, col) )
        return -1;
    t->c[row*t->nCol + col].cspan = cspan;
    return 0;
}

int
set_cell_halign(table_t *t, int row, int col, halign_t halign) {
    if( _rc_chk(t, row, col) )
        return -1;    
    t->c[row*t->nCol + col].halign = halign;
    return 0;
}

int
set_cell(table_t *t, int row, int col, char *txt) {
    if( _rc_chk(t, row, col) )
        return -1;    
    t->c[row * t->nCol + col ].t = strdup(txt);
    return 0;
}

int
init_cell(table_t *t, int row, int col) {
    if( _rc_chk(t, row, col) )
        return -1;    
    tcell_t *c = &t->c[row * t->nCol + col];
    c->t = NULL;
    c->halign = RIGHTALIGN;
    c->pRow = row;
    c->pCol = col;
    c->merged = FALSE;
    c->cspan = 1;
    c->rspan = 1;
    return 0;
}

table_t *
table_create(int nRow, int nCol) {
    table_t *t = calloc(1, sizeof (table_t));
    if( NULL==t )
        return NULL;
    t->nRow = nRow;
    t->nCol = nCol;        
    t->c = calloc(nRow*nCol, sizeof (tcell_t));
    t->w = calloc(nCol, sizeof (int));
    if( t->c == NULL || t->w == NULL ) {
        goto tbl_err;
    }
    for (size_t c = 0; c < t->nRow; c++) {
        t->w[c] = 0;
    }
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nRow; c++) {
            if( init_cell(t, r, c) ) {
                goto tbl_err;
            }
        }
    }
    return t;
    
tbl_err:
    free(t);
    free(t->c);
    free(t->w);
    return NULL;

}

int
table_free(table_t *t) {
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nRow; c++) {
            if( t->c[r*t->nCol+c].t ) {
                free(t->c[r*t->nCol+c].t);
            }
        }
    }    
    free(t->c);
    free(t->w);
    free(t);
}


int 
draw_cellcontent_row(table_t *t, size_t row, tblstyle_t style) {
    size_t c = 0;
    while (c < t->nCol) {
        int w = 0;

        const size_t cspan = t->c[row * t->nCol + c].cspan;
        for (size_t cs = 0; cs < cspan; cs++) {
            w += t->w[c + cs] + 1;
        }
        w -= 1;

        const char *txt=t->c[ row * t->nCol + c ].t;
        const int w_half = w/2;
        const int len=strlen(txt);
        const int len_half=len/2;
                
        if (c == 0 && style == TSTYLE_FULL_DL) {
            switch (t->c[ row * t->nCol + c ].halign) {
                case RIGHTALIGN:
                    printf("%s%*s", BDL_DL_V, w, txt);
                    break;
                case LEFTALIGN:
                    printf("%s%-*s", BDL_DL_V, w, txt);
                    break;
                case CENTERALIGN:                    
                    printf("%s%*s%-*s", BDL_DL_V, w_half-len_half,"",w-w_half+len_half, txt);
                    break;
            }
        } else {            
            switch (t->c[ row * t->nCol + c ].halign) {
                case RIGHTALIGN:
                    printf("%s%*s", BDL_V, w, txt);
                    break;
                case LEFTALIGN:
                    printf("%s%-*s", BDL_V, w, txt);
                    break;
                case CENTERALIGN:                    
                    printf("%s%*s%-*s", BDL_V, w_half-len_half,"",w-w_half+len_half, txt);
                    break;                    
            }
        }
        c += cspan;
    }
    printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DL_V : BDL_V );
}

int find_maxcolwidth(table_t *t) {
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nCol; c++) {
            if (strlen(t->c[ r * t->nCol + c ].t) > t->w[c]) {
                t->w[c] = strlen(t->c[ r * t->nCol + c ].t);
            }
        }
    }

    printf("Widths: ");
    for (size_t c = 0; c < t->nCol; c++) {
        printf("%d,", t->w[c]);
    }
    printf("\n");
}

int
mark_verticals(table_t *t, int eval[], int mark, size_t row) {
    size_t c = 0;
    size_t absw = 0;
    while (c < t->nCol) {

        int w = 0;
        for (size_t cs = 0; cs < t->c[row* t->nCol + c].cspan; cs++) {
            w += t->w[c + cs] + 1;
        }

        absw += w;
        eval[absw - 1] |= mark; 
        c += t->c[row  * t->nCol + c].cspan;
    }
}

int
draw_verticals(int totwidth, int eval[], char *s0, char *s1, char *s2, char *s3) {
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


int draw_table(table_t *t, tblstyle_t style) {

    find_maxcolwidth(t);
                    
    size_t totwidth=0;
    for( size_t i=0; i < t->nCol; i++)
        totwidth += t->w[i]+1;
    int eval[totwidth] ;    
    
    for (size_t r = 0; r < t->nRow; r++) {
        memset(eval,0,sizeof(int)*totwidth);                    
        if (0 == r) {
            mark_verticals(t,eval,1,r);              
            if ( style == TSTYLE_TOP_DL || style == TSTYLE_FULL_DL ) {
                printf("%s", style == TSTYLE_FULL_DL ? BDL_DL_DR : BDL_DH_DR);                                
                draw_verticals(totwidth,eval,BDL_DH_H,BDL_DH_DH,NULL,NULL);                
                printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DL_DL : BDL_DH_DL);
            } else {
                printf("%s", BDL_DR);                
                draw_verticals(totwidth,eval,BDL_H,BDL_DH,NULL,NULL);
                printf("%s\n", BDL_DL);
            }
            draw_cellcontent_row(t, r, style);
        } else {               
            mark_verticals(t,eval,1,r-1);              
            mark_verticals(t,eval,2,r);                
            if (1 == r && (style == TSTYLE_TOP_DL || style == TSTYLE_FULL_DL)) {
                printf("%s", style == TSTYLE_FULL_DL ? BDL_DL_VR : BDL_DH_VR);            
                draw_verticals(totwidth,eval,BDL_DH_H, BDL_DH_UH,BDL_DH_DH,BDL_DH_X);                
                printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DL_VL : BDL_DH_VL);
            } else {
                printf("%s", style == TSTYLE_FULL_DL ? BDL_DV_VR : BDL_VR);                
                draw_verticals(totwidth,eval,BDL_H, BDL_UH,BDL_DH,BDL_X);                        
                printf("%s\n", style == TSTYLE_FULL_DL ? BDL_DV_VL : BDL_VL);
            }
            draw_cellcontent_row(t, r, style);
        }
    }

    memset(eval,0,sizeof(int)*totwidth);
    mark_verticals(t,eval,1,t->nRow-1);              
    if (style == TSTYLE_FULL_DL) {
        printf("%s", BDL_DL_UR);
        draw_verticals(totwidth,eval,BDL_DH_H, BDL_DH_UH,NULL,NULL);
        printf("%s\n", BDL_DL_UL);

    } else {
        printf("%s", BDL_UR);
        draw_verticals(totwidth,eval,BDL_H, BDL_UH,NULL,NULL);
        printf("%s\n", BDL_UL);
    }

}


int
main(int argc, char *argv) {

    table_t *tbl = table_create(7, 7);
    char buff[10];
    for (size_t r = 0; r < tbl->nRow; r++) {
        for (size_t c = 0; c < tbl->nCol; c++) {
            snprintf(buff, sizeof (buff), " (%zu,%zu) ", r, c);
            set_cell(tbl, r, c, buff);
        }
    }
    
    set_cell(tbl, 0, 0, "Nu skall vi se!");

    tbl->c[0].cspan = 6 ;
    tbl->c[0].halign=LEFTALIGN;
    tbl->c[2*tbl->nCol+2].cspan = 3;
    tbl->c[2*tbl->nCol+2].halign=LEFTALIGN;
    //tbl->c[2*5+2].cspan = 2;
    //tbl->c[4*5+0].cspan = 5;

    /*
    draw_table(tbl, TSTYLE_SL);
    printf("\n\n");

    draw_table(tbl, TSTYLE_FULL_DL);
    printf("\n\n");
*/
    draw_table(tbl, TSTYLE_FULL_DL);
    printf("\n\n");

    exit(EXIT_SUCCESS);
}



