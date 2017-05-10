#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


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

typedef struct {
    char *t;
    halign_t align;
    int pRow, pCol;
    _Bool merged;
    border_t b[4]; // left,right,up,down
    size_t rspan;
    size_t cspan;
} tcell_t;

typedef struct tcont_t {
    size_t nRow, nCol;
    tcell_t *c;
    int *w;
} table_t;

tcell_t*
get_cell(table_t *t, int nRow, int nCol) {
    return &t->c[nRow * nCol];
}

int
init_cell(table_t *t, int row, int col) {
    tcell_t *c = &t->c[row * t->nCol + col];
    c->t = NULL;
    c->align = LEFTALIGN;
    c->pRow = row;
    c->pCol = col;
    c->merged = FALSE;
    c->b[0] = BORDSINGLE;
    c->b[1] = BORDSINGLE;
    c->b[2] = BORDSINGLE;
    c->b[3] = BORDSINGLE;
    c->cspan = 1;
    c->rspan = 1;
}

table_t *
table_init(int nRow, int nCol) {
    table_t *t = calloc(1, sizeof (table_t));
    t->c = calloc(nRow*nCol, sizeof (tcell_t));
    t->nRow = nRow;
    t->nCol = nCol;
    t->w = calloc(nCol, sizeof (int));
    for (size_t c = 0; c < t->nRow; c++) {
        t->w[c] = 0;
    }
    for (size_t r = 0; r < t->nRow; r++) {
        for (size_t c = 0; c < t->nRow; c++) {
            init_cell(t, r, c);
        }
    }
    return t;
}

int
set_cell(table_t *t, int row, int col, char *txt) {
    t->c[row * t->nCol + col ].t = strdup(txt);
}

typedef enum {
    TSTYLE_TOP_DL, TSTYLE_FULL_DL, TSTYLE_SL
} tblstyle_t;

int draw_cellcontent_row(table_t *t, size_t row, tblstyle_t style) {
    size_t c = 0;
    while (c < t->nCol) {
        int w = 0;
        //printf("span[%zu][%zu]=%zu\n",row,c,t->c[row*t->nCol+c].cspan);
        for (size_t cs = 0; cs < t->c[row * t->nCol + c].cspan; cs++) {
            w += t->w[c + cs] + 1;
        }
        w -= 1;

        if (c == 0 && style == TSTYLE_FULL_DL)
            printf("%s%-*s", BDL_DL_V, w, t->c[ row * t->nCol + c ].t);
        else
            printf("%s%-*s", BDL_V, w, t->c[ row * t->nCol + c ].t);

        //printf("row=%zu, w=%d, cspan=%zu",row,w,t->c[row*t->nCol+c].cspan);
        c += t->c[row * t->nCol + c].cspan;


        //break;
    }
    if (style == TSTYLE_FULL_DL)
        printf("%s\n", BDL_DL_V);
    else
        printf("%s\n", BDL_V);
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

int draw_table(table_t *t, tblstyle_t style) {

    find_maxcolwidth(t);

    for (size_t r = 0; r < t->nRow; r++) {

        if (0 == r) {
            if ((style == TSTYLE_TOP_DL || style == TSTYLE_FULL_DL)) {
                if (style == TSTYLE_FULL_DL)
                    printf("%s", BDL_DL_DR);
                else
                    printf("%s", BDL_DH_DR);

                size_t c = 0;
                while (c < t->nCol) {
                    int w = 0;
                    for (size_t cs = 0; cs < t->c[r * t->nCol + c].cspan; cs++) {
                        w += t->w[c + cs] + 1;
                    }
                    w -= 1;
                    
                    for (int i = 0; i < w; i++) {
                        printf("%s", BDL_DH_H);
                    }
                    c += t->c[r * t->nCol + c].cspan;
                    if (c < t->nCol - 1) {
                        printf("%s", BDL_DH_DH);
                    }
                }
                if (style == TSTYLE_FULL_DL)
                    printf("%s\n", BDL_DL_DL);
                else
                    printf("%s\n", BDL_DH_DL);

            } else {
                printf("%s", BDL_DR);
                size_t c = 0;
                while (c < t->nCol) {
                    int w = 0;
                    for (size_t cs = 0; cs < t->c[r * t->nCol + c].cspan; cs++) {
                        w += t->w[c + cs] + 1;
                    }
                    w -= 1;
                    
                    for (int i = 0; i < w; i++) {
                        printf("%s", BDL_H);
                    }
                    c += t->c[r * t->nCol + c].cspan;
                    if (c < t->nCol - 1) {
                        printf("%s", BDL_DH);
                    }
                }
                printf("%s\n", BDL_DL);
            }
            draw_cellcontent_row(t, r, style);

        } else {
            if (1 == r && (style == TSTYLE_TOP_DL || style == TSTYLE_FULL_DL)) {
                if (style == TSTYLE_FULL_DL)
                    printf("%s", BDL_DL_VR);
                else
                    printf("%s", BDL_DH_VR);
                size_t c = 0;
                while (c < t->nCol) {
                    int w = 0;
                    for (size_t cs = 0; cs < t->c[(r-1) * t->nCol + c].cspan; cs++) {
                        w += t->w[c + cs] + 1;
                    }
                    w -= 1;
                    int eval[t->nCol];
                    
                    for (int w = 0; w < t->w[c]; w++) {
                        printf("%s", BDL_DH_H);
                    }
                    if (c < t->nCol - 1)
                        printf("%s", BDL_DH_X);
                }
                if (style == TSTYLE_FULL_DL)
                    printf("%s\n", BDL_DL_VL);
                else
                    printf("%s\n", BDL_DH_VL);
            } else {
                // Middle rows
                if (style == TSTYLE_FULL_DL)
                    printf("%s", BDL_DV_VR);
                else
                    printf("%s", BDL_VR);
                for (size_t c = 0; c < t->nCol; c++) {
                    for (int w = 0; w < t->w[c]; w++) {
                        printf("%s", BDL_H);
                    }
                    if (c < t->nCol - 1)
                        printf("%s", BDL_X);
                }
                if (style == TSTYLE_FULL_DL)
                    printf("%s\n", BDL_DV_VL);
                else
                    printf("%s\n", BDL_VL);
            }
            draw_cellcontent_row(t, r, style);

        }
    }

    if (style == TSTYLE_FULL_DL) {
        printf("%s", BDL_DL_UR);
        for (size_t c = 0; c < t->nCol; c++) {
            for (int w = 0; w < t->w[c]; w++) {
                printf("%s", BDL_DH_H);
            }
            if (c < t->nCol - 1)
                printf("%s", BDL_DH_UH);
        }
        printf("%s\n", BDL_DL_UL);

    } else {
        printf("%s", BDL_UR);
        for (size_t c = 0; c < t->nCol; c++) {
            for (int w = 0; w < t->w[c]; w++) {
                printf("%s", BDL_H);
            }
            if (c < t->nCol - 1)
                printf("%s", BDL_UH);
        }
        printf("%s\n", BDL_UL);
    }

}

int
main(int argc, char *argv) {

    table_t *tbl = table_init(5, 5);
    char buff[10];
    for (size_t r = 0; r < tbl->nRow; r++) {
        for (size_t c = 0; c < tbl->nCol; c++) {
            snprintf(buff, sizeof (buff), " (%zu,%zu) ", r, c);
            set_cell(tbl, r, c, buff);
        }
    }

    tbl->c[0].cspan = 5;

    draw_table(tbl, TSTYLE_SL);
    printf("\n\n");

    draw_table(tbl, TSTYLE_FULL_DL);
    printf("\n\n");

    draw_table(tbl, TSTYLE_TOP_DL);
    printf("\n\n");

    exit(EXIT_SUCCESS);
}


