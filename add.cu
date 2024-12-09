#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>


__global__
void add( int n, float * x,  float * y )
{
        for( int i = 0; i < n; i++ )
        {
                y[ i ] = x[ i ] + y[ i ];
        }
}


__global__
void add1(int n, float *x, float *y)
{
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride)
    {
        y[i] += x[i];
    }
}



int main( void )
{
        int N = 1 << 20;
        float * dx;
        float * dy;

	cudaMallocManaged( &dx, N * sizeof(float) );
	cudaMallocManaged( &dy, N * sizeof(float) );


        for ( int i = 0; i < N; i++ )
        {
                dx[i] = 1.0f;
                dy[i] = 2.0f;
        }


        add1<<<1,1>>>( N, dx, dy );

	cudaDeviceSynchronize();

        float maxError = 0.0f;
        for ( int i = 0; i < N; i++ )
        {
                maxError = fmax( maxError, fabs( dy[i] - 3.0f ) );
        }

        std::cout << "Max error:" << maxError << std::endl;

        cudaFree( dx );
        cudaFree( dy );

        return 0;
}