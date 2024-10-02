#include <TemplateMatchSSDCuda.cuh>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <random>
#include <stdio.h>

__global__ void templateMatchSSDKernel
(const unsigned char* image, 
 const unsigned char* temp, 
 size_t* result, 
 int* compare_points, 
 int image_width, 
 int image_height, 
 int temp_width, 
 int temp_height,
 int num_points)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x + temp_width <= image_width && y + temp_height <= image_height) {
        size_t sum = 0;
        for (int i = 0; i < num_points; i++) {
            int temp_x = compare_points[i * 2];
            int temp_y = compare_points[i * 2 + 1];
            int image_x = x + temp_x;
            int image_y = y + temp_y;
            if (image_x < image_width && image_y < image_height) { 
                size_t diff = (image[image_y * image_width + image_x] - temp[temp_y * temp_width + temp_x]);
                sum += diff * diff;
            }
        }
        result[y * (image_width - temp_width + 1) + x] = sum;
    }
}

bool CUDAMATCH_API ar::templateMatchSSDCuda(int* res,
    std::string& msg_res,
    const unsigned char* image,
    const unsigned char* temp,
    const int& image_width,
    const int& image_height,
    const int& temp_width,
    const int& temp_height,
    const float& threshold,
    const int& num_points) 
{
    cudaError_t err = cudaSuccess;
    msg_res = "Success";

    const int image_size = image_width * image_height;
    const int temp_size = temp_width * temp_height;
    const int result_size = (image_width - temp_width + 1) * (image_height - temp_height + 1);

    size_t* result_array = new size_t[result_size];
    //x_0, y_0, x_1, y_1, x_2, y_2, ...
    int* compare_points_array = new int[num_points * 2]; 

    unsigned char* device_image_array = nullptr;
    unsigned char* device_temp_array = nullptr;
    size_t* device_result_array = nullptr;
    int* device_compare_points_array = nullptr;

    makePointsRandom(compare_points_array, num_points, temp_width, temp_height);

    err = cudaMalloc(&device_image_array, image_size * sizeof(unsigned char));
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaMalloc(&device_temp_array, temp_size * sizeof(unsigned char));
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaMalloc(&device_result_array, result_size * sizeof(size_t));
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        cudaFree(device_temp_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaMalloc(&device_compare_points_array, num_points * 2 * sizeof(int));
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        cudaFree(device_temp_array);
        cudaFree(device_result_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }

    err = cudaMemcpy(device_image_array, image, image_size * sizeof(unsigned char), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        cudaFree(device_temp_array);
        cudaFree(device_result_array);
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaMemcpy(device_temp_array, temp, temp_size * sizeof(unsigned char), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        cudaFree(device_temp_array);
        cudaFree(device_result_array);
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaMemcpy(device_compare_points_array, compare_points_array, num_points * 2 * sizeof(int), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        cudaFree(device_temp_array);
        cudaFree(device_result_array);
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }


    dim3 threadsPerBlock(64, 64);
    dim3 blocksPerGrid((image_width + threadsPerBlock.x - 1) / threadsPerBlock.x,
        (image_height + threadsPerBlock.y - 1) / threadsPerBlock.y);
    templateMatchSSDKernel << <blocksPerGrid, threadsPerBlock >> > (device_image_array,
        device_temp_array,
        device_result_array,
        device_compare_points_array,
        image_width,
        image_height,
        temp_width,
        temp_height,
        num_points);

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        cudaFree(device_temp_array);
        cudaFree(device_result_array);
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }

    err = cudaMemcpy(result_array, device_result_array, result_size * sizeof(size_t), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_image_array);
        cudaFree(device_temp_array);
        cudaFree(device_result_array);
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }

    size_t cur_min_ssd = (size_t)255 * 255 * num_points;
    for (int i = 0; i < result_size; i++) {
        if (result_array[i] < (size_t)2000 * (1 - threshold) * num_points && result_array[i] < cur_min_ssd)
        {
            printf("%d", result_array[i]);
            int x = i % (image_width - temp_width + 1);
            int y = i / (image_width - temp_width + 1);
            res[0] = x;
            res[1] = y;
            cur_min_ssd = result_array[i];
        }
    }

    err = cudaFree(device_image_array);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_temp_array);
        cudaFree(device_result_array);
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaFree(device_temp_array);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_result_array);
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaFree(device_result_array);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        cudaFree(device_compare_points_array);
        msg_res = cudaGetErrorString(err);
        return false;
    }
    err = cudaFree(device_compare_points_array);
    if (err != cudaSuccess) {
        delete[] result_array;
        delete[] compare_points_array;
        msg_res = cudaGetErrorString(err);
        return false;
    }
    delete[] result_array;
    delete[] compare_points_array;

    return true;
}

void ar::makePointsRandom(int* division_random_point, const int& num_points, const int& temp_width, const int& temp_height) {
    int* best_division = new int[2];
    ar::findBestDivision(best_division, num_points);

    int region_height = best_division[0];
    int region_width = best_division[1];

    int height_per_region = temp_height / region_height;
    int width_per_region = temp_width / region_width;

    std::random_device rd;
    std::mt19937 generator(rd());

    for (int i = 0; i < region_height; i++) {
        for (int j = 0; j < region_width; j++) {
            std::uniform_int_distribution<int> distribution_x(j * width_per_region, (j + 1) * width_per_region - 1);
            int x_ = distribution_x(generator);
            std::uniform_int_distribution<int> distribution_y(i * height_per_region, (i + 1) * height_per_region - 1);
            int y_ = distribution_y(generator);
            division_random_point[(i * region_width + j) * 2] = x_;
            division_random_point[(i * region_width + j) * 2 + 1] = y_;
        }
    }
    delete[] best_division;
}

void ar::findBestDivision(int* best_division, const int& num_points) {
    for (int i = std::sqrt(num_points); i >= 1; i--) {
        if (num_points % i == 0) {
            best_division[0] = i;
            best_division[1] = num_points / i;
        }
    }
    best_division[0] = 1;
    best_division[1] = num_points;
}