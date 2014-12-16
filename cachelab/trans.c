/* 
 * Name: Mengyu YANG
 * Andrew ID: mengyuy@andrew.cmu.edu
 *
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
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */


int transpose_32(int A[32][32], int B[32][32]){
    int bRow, bCol, row, col;
    int b0 = 0;
    int b1 = 0;
    for (bRow = 0; bRow < 32; bRow += 8){         
        for (bCol = 0; bCol < 32; bCol += 8){                                  
            for (row = bRow; row < bRow + 8; row ++){                        
                for (col = bCol; col < bCol + 8; col ++){                      
	            if (row != col)                                           
	               B[col][row] = A[row][col];                            
	            else {                                            
		       b0 = A[row][col];                         
		       b1 = row;                                             
      	            }                             
	        }                                                 
	        if (bRow == bCol)                                 
	            B[b1][b1] = b0;                       
       	    }                                                      
        }                                   
    }                     
    return 0;
}

int transpose_64(int A[64][64], int B[64][64]){
    int row, col, k;
    int b0, b1, b2, b3, b4, b5, b6, b7;

    for(col=0;col<64;col+=8)
        for(row=0;row<64;row+=8){
            /* Read a 4*8 block */
            for(k=row; k<row+4; k++){
	       
	        b0=A[k][col];
	        b1=A[k][col+1];
	        b2=A[k][col+2];
	        b3=A[k][col+3];
         	b4=A[k][col+4];
	        b5=A[k][col+5];
	        b6=A[k][col+6];
	        b7=A[k][col+7];

	        B[col][k]=b0;
	        B[col+1][k]=b1;
	        B[col+2][k]=b2;
          	B[col+3][k]=b3;

          	B[col][k+4]=b4;
         	B[col+1][k+4]=b5;        
        	B[col+2][k+4]=b6;
  	        B[col+3][k+4]=b7;
            }
 
            /* Swap the upper right 4*4 block and the lower left 4*4 block*/
            for(k=col; k<col+4; k++){
         	b0=B[k][row+4];
        	b1=B[k][row+5];
	        b2=B[k][row+6];
	        b3=B[k][row+7];
	        b4=A[row+4][k];
         	b5=A[row+5][k];
	        b6=A[row+6][k];
	        b7=A[row+7][k];

         	B[k][row+4]=b4;
	        B[k][row+5]=b5;
	        B[k][row+6]=b6;
	        B[k][row+7]=b7;
	        B[k+4][row]=b0;
	        B[k+4][row+1]=b1;
	        B[k+4][row+2]=b2;
	        B[k+4][row+3]=b3;
            }

	    /* transpose the lower right 4*4 block */
            for(k=col+4; k<col+8; k++){
	        b0=A[row+4][k];
	        b1=A[row+5][k];
                b2=A[row+6][k];
	        b3=A[row+7][k];
	        B[k][row+4]=b0;
	        B[k][row+5]=b1;
                B[k][row+6]=b2;
	        B[k][row+7]=b3;
            }
      }
    return 0;
}



int transpose_61(int A[67][61], int B[61][67]){
    int bRow, bCol, row, col; 
    for(bRow = 0; bRow < 67; bRow += 8)                       
        for(bCol = 0; bCol < 61; bCol += 8)                   
            for(col = bCol; col < bCol + 8 && col < 61; col++)        
	        for(row = bRow; row < bRow + 8 && row < 67; row++)
	            B[col][row] = A[row][col];                        
     return 0;
}



char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
  
    REQUIRES(M > 0);
    REQUIRES(N > 0);
 
    if ((M == 32) & (N == 32))
      transpose_32(A, B);
    else if((M == 64) && (N == 64))
        transpose_64(A, B);
    else if((M == 61) && (N == 67))
        transpose_61(A, B);                                                 

    ENSURES(is_transpose(M, N, A, B));
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

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
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
    registerTransFunction(trans, trans_desc); 

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

