#include <stdio.h>

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



char trans_block_quadrant_desc[] = "Block by block transpose, by transposing each quadrant";
void trans_block_quadrant(int M, int N, int A[N][M], int B[M][N]){
    int i, j, k, l, tmp;    

    for(i=0; i<N; i=i+8){
        for(j=0; j<M; j=j+8){

            for(k=i+4; k<i+8; k++){
                for(l=j; l<j+4; l++){
                    B[l][k] = A[k][l];
                }
            }

            for(k=i+4; k<i+8; k++){
                for(l=j; l<j+4; l++){
                    B[l][k] = A[k+4][l+4];
                }
            }

            for(k=i; k<i+4; k++){
                for(l=j+4; l<j+8; l++){
                    B[l][k] = A[k][l];
                }
            }

            for(k=i; k<i+4; k++){
                for(l=j; l<j+4; l++){
                    B[l+4][k+4] = A[k][l];
                }
            }

            for(k=i; k<i+4; k++){
                for(l=j; l<j+4; l++){
                    tmp = B[k+4+ (j-i)][l+4+(i-j)];
                    B[k+4+(j-i)][l+4+(i-j)] = B[k+(j-i)][l+(i-j)];
                    B[k+(j-i)][l+(i-j)] = tmp;
                }
            }


        }
    }

}

int main(){

    int A[8][8];
    int B[8][8];
    int cnt = 0;

    for(int i=0; i<8; i++){
        for(int j=0; j<8; j++){
            A[i][j] = cnt;
            cnt++;
            B[i][j] = 0;
        }
    }

    int M=8, N=8;
    int i, j, k, l, tmp;    

    for(i=0; i<N; i=i+8){
        for(j=0; j<M; j=j+8){

            for(k=i+4; k<i+8; k++){
                for(l=j; l<j+4; l++){
                    B[l][k] = A[k][l];
                }
            }

            for(k=i+4; k<i+8; k++){
                for(l=j; l<j+4; l++){
                    B[l][k-4] = A[k][l+4];
                }
            }

            for(k=i; k<i+4; k++){
                for(l=j+4; l<j+8; l++){
                    B[l][k] = A[k][l];
                }
            }

            for(k=i; k<i+4; k++){
                for(l=j; l<j+4; l++){
                    B[l+4][k+4] = A[k][l];
                }
            }

            for(k=i; k<i+4; k++){
                for(l=j; l<j+4; l++){
                    tmp = B[k+4+ (j-i)][l+4+(i-j)];
                    B[k+4+(j-i)][l+4+(i-j)] = B[k+(j-i)][l+(i-j)];
                    B[k+(j-i)][l+(i-j)] = tmp;
                }
            }


        }
    }



    int result = is_transpose(8, 8, A, B);

    printf("%d\n", result);

    return 0;
}