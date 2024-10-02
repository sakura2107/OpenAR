#include <ImageRecognition/TemplateMatchMPR.h>
#include <math.h>
#include <random>

bool ar::multiPointsRecognition(int* res,
    std::string& res_msg,
    unsigned char* image,
    unsigned char* temp,
    const int& image_width,
    const int& image_height,
    const int& temp_width,
    const int& temp_height,
    const float& threshold,
    const int& num_points)
{
    bool err = false;

    int* compare_points_array = new int[2 * num_points];
    ar::detail::makePointsRandom(compare_points_array, temp, temp_width, temp_height, num_points);

    size_t cur_min_ssd = (size_t)255 * 255 * num_points;
    for (int h = 0; h <= image_height - temp_height; h++) {
        for (int w = 0; w <= image_width - temp_width; w++) {
            size_t ssd = 0;
            for (int t = 0; t < num_points; t++) {
                int temp_x = compare_points_array[t * 2];
                int temp_y = compare_points_array[t * 2 + 1];
                int image_x = w + temp_x;
                int image_y = h + temp_y;

                ssd += (size_t)(image[image_y * image_width + image_x] - temp[temp_y * temp_width + temp_x]) *
                    (image[image_y * image_width + image_x] - temp[temp_y * temp_width + temp_x]);
            }
            if (ssd < (size_t)num_points * 5000 * (1 - threshold) && ssd < cur_min_ssd) {
                res[0] = w; res[1] = h;
                cur_min_ssd = ssd;
            }
        }
    }
    delete[] compare_points_array;
    return true;
}

void ar::detail::makePointsRandom(int* points, unsigned char* temp, const int& temp_width, const int& temp_height, const int& num_points){
    int* res = new int[2];
    findBestDivision(res, num_points);

    int region_height = res[0];
    int region_width = res[1];

    int height_per_region = temp_height / region_height;
    int width_per_region = temp_width / region_width;

    std::random_device rd;
    std::mt19937 generator(rd());

    for(int i = 0; i < region_height; i++){
        for(int j = 0; j < region_width; j++){
            std::uniform_int_distribution<int> distribution_x(j * width_per_region, (j + 1) * width_per_region - 1);
            int x_ = distribution_x(generator);
            std::uniform_int_distribution<int> distribution_y(i * height_per_region, (i + 1) * height_per_region - 1);
            int y_ = distribution_y(generator);
            points[(i * region_width + j) * 2] = x_;
            points[(i * region_width + j) * 2 + 1] = y_;
        }
    }
    delete[] res;
}

void ar::detail::findBestDivision(int* res, const int& num){
    for(int i = std::sqrt(num); i >= 1; i--){
        if(num % i == 0) {
            res[0] = i;
            res[1] = num / i;
            return ;
        }
    }
}

