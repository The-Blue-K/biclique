/* 
 * R wrapper of Biclique Enumerator
 *
 * Author: Yuping Lu, yupinglu@utk.edu
 * Created: September 2016
 *
 * Copyright 2016-2019
 * Department of Computer Science, University of Tennessee, Knoxville
 *
 * Last modified: August 2017
 *
 */

#include "R_biclique_driver.h"

extern int LLEAST, RLEAST;
extern int CACHE_SIZE;
extern int VERSION_0;
extern int PRINT;
extern long long node_num;
extern int SORT_TYPE;

/* Global Variables */
extern num_t *nnr;
extern num_t *nnl;

FILE *fp;
char *infn;
char *outfn;
int DEGREE;
char INPUT;

/** 
 *  Finalizer that only clears R pointer
 */
static void finalizer0(SEXP Rptr)
{
    void *ptr = (void *) R_ExternalPtrAddr(Rptr);
    if (NULL == ptr) {
        return;
    } else {
        R_ClearExternalPtr(Rptr);
    }
}

void maximal_biclique(BiGraph *G, num_t *profile, num_t **g_right, num_t **g_left)
{
    unsigned int n2 = G->_num_v2;
    unsigned int n1 = G->_num_v1;

    /* Allocate memory for nnr and nnl. 
    * 1st element of nnr/nnl is the totoal number of bicliques
    * other elements of nnr/nnl are the totoal number of nodes in each biclique
    */
    nnr = (num_t *) calloc ((n2 * n1 + 1), sizeof(num_t));
    nnl = (num_t *) calloc ((n2 * n1 + 1), sizeof(num_t));
    vid_t cand[n2];
    unsigned int i;

    for (i = 0; i < n2; i++) cand[i] = i;
    biclique_enumerate(g_right, g_left, profile, G, cand, n2);
}

/**
 * Read input file and do the bicliuqe enumeration.
 * Two formats are supported: bel and bmat
 * If start and count is not specified, a whole list is returned.
 */
SEXP R_biclique(SEXP R_file, SEXP R_left_least, SEXP R_right_least, SEXP R_degree, SEXP R_version, SEXP R_print, SEXP R_input)
{
    BiGraph *G = NULL;
    SEXP R_data;
    const char *filepath = CHARPT(R_file, 0);
    infn = Strdup(filepath);
    if ((fp = fopen(infn, "r")) == NULL) {
        REprintf("Can't open file %s\n", infn);
        return R_NilValue;
    }

    // Set Global variables.
    LLEAST = asInteger(R_left_least);
    RLEAST = asInteger(R_right_least);
    DEGREE = asInteger(R_degree);
    VERSION_0 = asInteger(R_version);
    PRINT = asInteger(R_print);
    INPUT = asInteger(R_input);  // default = edge list = 0
    SORT_TYPE = 1;

    if (INPUT==0) G = bigraph_edgelist_in(fp);
    else if (INPUT==1) G = bigraph_binarymatrix_in(fp);
    fclose(fp);
    
    unsigned int n2 = G->_num_v2;
    unsigned int n1 = G->_num_v1;

    if (DEGREE) {
        unsigned int i;
        // Get the degreelist
        R_data = PROTECT(allocVector(VECSXP, n1+n2));
        SEXP list_names = PROTECT(allocVector(STRSXP, n1+n2));
        for (i = 0; i < n1; i++) {
            SET_VECTOR_ELT(R_data, i, ScalarInteger(bigraph_degree_v1(G, i)));
            SET_STRING_ELT(list_names, i, mkChar(G->_label_v1[i]));
        }
        for (i = 0; i < n2; i++) {
            SET_VECTOR_ELT(R_data, n1+i, ScalarInteger(bigraph_degree_v2(G, i)));
            SET_STRING_ELT(list_names, n1+i, mkChar(G->_label_v2[i]));
        }

        // set list names
        setAttrib(R_data, R_NamesSymbol, list_names);
        // free memory
        UNPROTECT(2);

    }
    else {
        SEXP profile_data;
        num_t *profile = (num_t *) calloc((3*(n1 * n2) + 9), sizeof(num_t));
        SEXP R_biclique_right, R_biclique_left;
        R_data = PROTECT(allocVector(VECSXP, 3));

        /*
        * Allocate memory for g_right and g_left. Biclique nodes in them.
        */
        num_t **g_right = (num_t **) calloc(n2 * n1, sizeof(num_t*));
        num_t **g_left = (num_t **) calloc(n2 * n1, sizeof(num_t*));

        maximal_biclique(G, profile, g_right, g_left);

        R_biclique_right = PROTECT(allocVector(VECSXP, nnr[0]));
        R_biclique_left = PROTECT(allocVector(VECSXP, nnl[0]));

        // copy the bicliques to R
        unsigned int i, j;
        for(j = 0; j < nnr[0]; j++) {

            // Allocate temp R memory
            SEXP R_biclique1 = PROTECT(allocVector(STRSXP, nnr[j+1]));
            SEXP R_biclique2 = PROTECT(allocVector(STRSXP, nnl[j+1]));

            for (i = 0; i < nnr[j+1]; i++) {
                //Rprintf("%s\t", G->_label_v2[g_right[j][i]]);
                SET_STRING_ELT(R_biclique1, i, mkChar((char *)(G->_label_v2[g_right[j][i]])));
            }

            for (i = 0; i < nnl[j+1]; i++) {
                //Rprintf("%s\t", G->_label_v1[g_left[j][i]]);
                SET_STRING_ELT(R_biclique2, i, mkChar((char *)(G->_label_v1[g_left[j][i]])));
            }

            SET_VECTOR_ELT(R_biclique_right, j, R_biclique1); 
            SET_VECTOR_ELT(R_biclique_left, j, R_biclique2);           
            
            UNPROTECT(2);
        }

        // Add cliques to the R_data list
        SET_VECTOR_ELT(R_data, 0, R_biclique_right);
        SET_VECTOR_ELT(R_data, 1, R_biclique_left);

        UNPROTECT(2);
        // free memory
        for (i = 0; i < nnr[0]; i++) {
            free(g_right[i]);
            
        }
        for (i = 0; i < nnl[0]; i++) {
            free(g_left[i]);
        }
        free(g_right);
        free(g_left);
        free(nnr);
        free(nnl);

        newRptr(profile, profile_data, finalizer0);
        // Add profile to the R_data list
        SET_VECTOR_ELT(R_data, 2, copy_data (profile_data));
        UNPROTECT(1);
        free(profile);
        UNPROTECT(1);
    }
    return R_data;
}


/** 
 * Copy data from C memory to R memory, return is R_data
 */
SEXP copy_data (SEXP C_data)
{
    SEXP R_data;  
    unsigned int *data = R_ExternalPtrAddr(C_data);
    unsigned int nelems = data[0];  /* get the number of elements to copy */
    unsigned int pos;

    R_data = PROTECT(allocVector(INTSXP, nelems));
    for(pos = 0; pos < nelems; pos++)
        INTEGER(R_data)[pos] = data[pos];

    UNPROTECT(1);
    return R_data;
}
