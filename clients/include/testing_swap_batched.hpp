/* ************************************************************************
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "cblas_interface.h"
#include "hipblas.hpp"
#include "norm.h"
#include "unit.h"
#include "utility.h"
#include <complex.h>

using namespace std;

/* ============================================================================================ */

template <typename T>
hipblasStatus_t testing_swap_batched(Arguments argus)
{
    int N           = argus.N;
    int incx        = argus.incx;
    int incy        = argus.incy;
    int batch_count = argus.batch_count;

    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(N < 0 || incx < 0 || incy < 0 || batch_count < 0)
    {
        return HIPBLAS_STATUS_INVALID_VALUE;
    }
    else if(batch_count == 0)
    {
        return HIPBLAS_STATUS_SUCCESS;
    }

    int sizeX = N * incx;
    int sizeY = N * incy;

    int device_pointer = 1;

    double gpu_time_used, cpu_time_used;
    double rocblas_error;

    hipblasHandle_t handle;
    hipblasCreate(&handle);

    // Naming: dX is in GPU (device) memory. hK is in CPU (host) memory, plz follow this practice
    host_vector<T> hx_array[batch_count];
    host_vector<T> hy_array[batch_count];
    host_vector<T> hx_cpu_array[batch_count];
    host_vector<T> hy_cpu_array[batch_count];

    device_batch_vector<T> bx_array(batch_count, sizeX);
    device_batch_vector<T> by_array(batch_count, sizeY);

    device_vector<T*, 0, T> dx_array(batch_count);
    device_vector<T*, 0, T> dy_array(batch_count);

    int last = batch_count - 1;
    if(!dx_array || !dy_array || (!bx_array[last] && sizeX) || (!by_array[last] && sizeY))
    {
        hipblasDestroy(handle);
        return HIPBLAS_STATUS_ALLOC_FAILED;
    }

    // Initial Data on CPU
    srand(1);
    for(int b = 0; b < batch_count; b++)
    {
        hx_array[b]     = host_vector<T>(sizeX);
        hy_array[b]     = host_vector<T>(sizeY);
        hx_cpu_array[b] = host_vector<T>(sizeX);
        hy_cpu_array[b] = host_vector<T>(sizeY);

        srand(1);
        hipblas_init<T>(hx_array[b], 1, N, incx);
        hipblas_init<T>(hy_array[b], 1, N, incy);

        hx_cpu_array[b] = hx_array[b];
        hy_cpu_array[b] = hy_array[b];

        CHECK_HIP_ERROR(
            hipMemcpy(bx_array[b], hx_array[b], sizeof(T) * sizeX, hipMemcpyHostToDevice));
        CHECK_HIP_ERROR(
            hipMemcpy(by_array[b], hy_array[b], sizeof(T) * sizeY, hipMemcpyHostToDevice));
    }
    CHECK_HIP_ERROR(hipMemcpy(dx_array, bx_array, batch_count * sizeof(T*), hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(dy_array, by_array, batch_count * sizeof(T*), hipMemcpyHostToDevice));

    /* =====================================================================
         ROCBLAS
    =================================================================== */
    status = hipblasSwapBatched<T>(handle, N, dx_array, incx, dy_array, incy, batch_count);

    if((status != HIPBLAS_STATUS_SUCCESS))
    {
        hipblasDestroy(handle);
        return status;
    }

    for(int b = 0; b < batch_count; b++)
    {
        // copy output from device to CPU
        CHECK_HIP_ERROR(
            hipMemcpy(hx_array[b], bx_array[b], sizeof(T) * sizeX, hipMemcpyDeviceToHost));
        CHECK_HIP_ERROR(
            hipMemcpy(hy_array[b], by_array[b], sizeof(T) * sizeY, hipMemcpyDeviceToHost));
    }

    if(argus.unit_check || argus.norm_check)
    {
        /* =====================================================================
                    CPU BLAS
        =================================================================== */
        for(int b = 0; b < batch_count; b++)
        {
            cblas_swap<T>(N, hx_cpu_array[b], incx, hy_cpu_array[b], incy);
        }

        if(argus.unit_check)
        {
            unit_check_general<T>(1, N, batch_count, incx, hx_cpu_array, hx_array);
            unit_check_general<T>(1, N, batch_count, incy, hy_cpu_array, hy_array);
        }

    } // end of if unit/norm check

    //  BLAS_1_RESULT_PRINT

    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}
