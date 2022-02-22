/* Header Comment
*
*  Name: 김재연
*  Login ID: stu67
*
*/


/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans_block2(int M, int N, int A[N][M], int B[M][N]);
void trans_block3(int M, int N, int A[N][M], int B[M][N]);
void trans_block_buff(int M, int N, int A[N][M], int B[M][N]);
void trans_block_buff2(int M, int N, int A[N][M], int B[M][N]);
void trans_block_buff3(int M, int N, int A[N][M], int B[M][N]);
/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */

char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
   if(M==32&&N==32) trans_block_buff(M, N, A, B);
   else if(M==64&&N==64) trans_block_buff3(M, N, A, B);
   else if(M==61&&N==67) trans_block2(M, N, A, B);
       
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

char trans_block_desc[] = "Block by block transpose --> transpose 8*8(as block size is 8byte)";
void trans_block(int M, int N, int A[N][M], int B[M][N]){
    int i, j, k, l, tmp;

    for (i = 0; i < N; i=i+8) {
        for (j = 0; j < M; j=j+8){

            for(k=0; k<8; k++){
                for(l=0; l<8; l++){
                    tmp = A[i+k][j+l];
                    B[j+l][i+k] = tmp;
                }
            }
        }
    }    
}

char trans_block_col_desc[] = "Block by block transpose(col --> row)";
void trans_block_col(int M, int N, int A[N][M], int B[M][N]){
    int i, j, k, l, tmp;
    
    for (i = 0; i < N; i=i+8) {
        for (j = 0; j < M; j=j+8){
            
            for(k=0; k<8; k++){
                for(l=0; l<8; l++){
                    tmp = A[i+l][j+k];
                    B[j+k][i+l] = tmp;
                }
            }
        }
    }    
}

char trans_block_buff_desc[] = "Block by block transpose using buffer";
void trans_block_buff(int M, int N, int A[N][M], int B[M][N]){
   int i, j, k;
   int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    
    for (i = 0; i < N; i=i+8) {
        for (j = 0; j < M; j=j+8) {
            
            for(k=0; k<8; k++){
                tmp1 = A[i+k][j];
                tmp2 = A[i+k][j+1];
                tmp3 = A[i+k][j+2];
                tmp4 = A[i+k][j+3];
                tmp5 = A[i+k][j+4];
                tmp6 = A[i+k][j+5];
                tmp7 = A[i+k][j+6];
                tmp8 = A[i+k][j+7];

                B[j][i+k] = tmp1;
                B[j+1][i+k] = tmp2;
                B[j+2][i+k] = tmp3;
                B[j+3][i+k] = tmp4;
                B[j+4][i+k] = tmp5;
                B[j+5][i+k] = tmp6;
                B[j+6][i+k] = tmp7;
                B[j+7][i+k] = tmp8;

            }
        }
    }    
}

char trans_block_buff2_desc[] = "Block by block transpose using buffer: quadrant by quadrant";
void trans_block_buff2(int M, int N, int A[N][M], int B[M][N]){
   int i, j, k;
   int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    
    for (i = 0; i < N; i=i+8) {
        for (j = 0; j < M; j=j+8) {
            
            //1
            for(k=0; k<4; k++){
                tmp1 = A[i+k][j];
                tmp2 = A[i+k][j+1];
                tmp3 = A[i+k][j+2];
                tmp4 = A[i+k][j+3];
            
                B[j][i+k] = tmp1;
                B[j+1][i+k] = tmp2;
                B[j+2][i+k] = tmp3;
                B[j+3][i+k] = tmp4;
            }

            //2
            for(k=0; k<4; k++){
                tmp5 = A[i+k][j+4];
                tmp6 = A[i+k][j+5];
                tmp7 = A[i+k][j+6];
                tmp8 = A[i+k][j+7];
            
                B[j][i+k+4] = tmp5;
                B[j+1][i+k+4] = tmp6;
                B[j+2][i+k+4] = tmp7;
                B[j+3][i+k+4] = tmp8;
            }

            //4
            for(k=4; k<8; k++){
                tmp5 = A[i+k][j+4];
                tmp6 = A[i+k][j+5];
                tmp7 = A[i+k][j+6];
                tmp8 = A[i+k][j+7];
            
                B[j+4][i+k] = tmp5;
                B[j+5][i+k] = tmp6;
                B[j+6][i+k] = tmp7;
                B[j+7][i+k] = tmp8;
            }

            //3
            for(k=4; k<8; k++){
                tmp1 = A[i+k][j];
                tmp2 = A[i+k][j+1];
                tmp3 = A[i+k][j+2];
                tmp4 = A[i+k][j+3];
            
                B[j+4][i+k-4] = tmp1;
                B[j+5][i+k-4] = tmp2;
                B[j+6][i+k-4] = tmp3;
                B[j+7][i+k-4] = tmp4;
            }          

            //swap 2&3 row wise
            for(k=0; k<4; k++){
                //3
                tmp1 = B[j+k+4][i];
                tmp2 = B[j+k+4][i+1];
                tmp3 = B[j+k+4][i+2];
                tmp4 = B[j+k+4][i+3]; 
                
                B[j+k+4][i] = B[j+k][i+4];
                B[j+k+4][i+1] = B[j+k][i+5];
                B[j+k+4][i+2] = B[j+k][i+6];
                B[j+k+4][i+3] = B[j+k][i+7];

                //2
                B[j+k][i+4] = tmp1;
                B[j+k][i+5] = tmp2;
                B[j+k][i+6] = tmp3;
                B[j+k][i+7] = tmp4;
    
            }
        }
    }    
}


char trans_block2_desc[] = "Block by block transpose -for arbitrary input";
void trans_block2(int M, int N, int A[N][M], int B[M][N]){
    int i, j, k, l, tmp;
    int left1, left2;
    for (i = 0; i < N; i=i+18) {
        for (j = 0; j < M; j=j+18){
            if(i+18>N) left1 = N%18;
            else left1 = 18;

            if(j+18>M) left2 = M%18;
            else left2 = 18;

            for(k=0; k<left1; k++){
                for(l=0; l<left2; l++){
                    tmp = A[i+k][j+l];
                    B[j+l][i+k] = tmp;
                }
            }
        }
    }    
}

char trans_block3_desc[] = "Block by block transpose -for arbitrary input";
void trans_block3(int M, int N, int A[N][M], int B[M][N]){
    int blocksize=4;
    int i, j, k, l, tmp;
    int left1, left2;
    for (i = 0; i < N; i=i+blocksize) {
        for (j = 0; j < M; j=j+blocksize){
            if(i+blocksize>N) left1 = N%blocksize;
            else left1 = blocksize;

            if(j+blocksize>M) left2 = M%blocksize;
            else left2 = blocksize;

            for(k=0; k<left1; k++){
                for(l=0; l<left2; l++){
                    tmp = A[i+k][j+l];
                    B[j+l][i+k] = tmp;
                }
            }
        }
    }    
}

char trans_block_buff3_desc[] = "Block by block transpose using buffer// different block size:4";
void trans_block_buff3(int M, int N, int A[N][M], int B[M][N]){
   int i, j, k;
   int tmp1, tmp2, tmp3, tmp4;
    
    for (i = 0; i < N; i=i+4) {
        for (j = 0; j < M; j=j+4) {
            
            for(k=0; k<4; k++){
                tmp1 = A[i+k][j];
                tmp2 = A[i+k][j+1];
                tmp3 = A[i+k][j+2];
                tmp4 = A[i+k][j+3];
                

                B[j][i+k] = tmp1;
                B[j+1][i+k] = tmp2;
                B[j+2][i+k] = tmp3;
                B[j+3][i+k] = tmp4;

            }
        }
    }    
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    //registerTransFunction(trans, trans_desc); 

    //registerTransFunction(trans_block2, trans_block2_desc);

    //registerTransFunction(trans_block_col, trans_block_col_desc);

    //registerTransFunction(trans_block_buff, trans_block_buff_desc);

    //registerTransFunction(trans_block_buff2, trans_block_buff2_desc);

    

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

