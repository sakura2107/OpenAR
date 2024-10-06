#include <ImageRecognition/TemplateMatchPSR.h>

bool ar::templateMatchPSR(int* res,
	std::string& res_msg,
	unsigned char* image,
	unsigned char* temp,
	const int& image_width,
	const int& image_height,
	const int& temp_width,
	const int& temp_height,
	const float& threshold)
{
	int* prefix_sum_array = new int[image_width * image_height];
	ar::detail::generateImagePrefixSumArray(prefix_sum_array, image, image_width, image_height);

	int temp_sum = 0;
	for (int i = 0; i < temp_height; i++) {
		for (int j = 0; j < temp_width; j++) {
			temp_sum += temp[i * temp_width + j];
		}
	}

	int cur_min_diff = 255 * temp_width * temp_height;
	for (int i = temp_height - 1; i < image_height; i++) {
		for (int j = temp_width - 1; j < image_width; j++) {
			if (i == temp_height - 1 && j == temp_width - 1) {
				int region_sum = prefix_sum_array[i * image_width + j];
				int region_diff = abs(region_sum - temp_sum);
				if (region_diff < 255 * temp_height * temp_width * (1 - threshold) && region_diff < cur_min_diff) {
					res[0] = j; res[1] = i;
					cur_min_diff = region_diff;
				}
			}
			else if (i > temp_height - 1 && j == temp_width - 1) {
				int region_sum = prefix_sum_array[i * image_width + j] - prefix_sum_array[(i - temp_height) * image_width + j];
				int region_diff = abs(region_sum - temp_sum);
				if (region_diff < 255 * temp_height * temp_width * (1 - threshold) && region_diff < cur_min_diff) {
					res[0] = j; res[1] = i;
					cur_min_diff = region_diff;
				}
			}
			else if (i == temp_height - 1 && j > temp_width - 1) {
				int region_sum = prefix_sum_array[i * image_width + j] - prefix_sum_array[i * image_width + j - temp_width];
				int region_diff = abs(region_sum - temp_sum);
				if (region_diff < 255 * temp_height * temp_width * (1 - threshold) && region_diff < cur_min_diff) {
					res[0] = j; res[1] = i;
					cur_min_diff = region_diff;
				}
			}
			else {
				int region_sum = prefix_sum_array[i * image_width + j] - prefix_sum_array[i * image_width + j - temp_width] - prefix_sum_array[(i - temp_height) * image_width + j] + prefix_sum_array[(i - temp_height) * image_width + j - temp_width];
				int region_diff = abs(region_sum - temp_sum);
				if (region_diff < 255 * temp_height * temp_width * (1 - threshold) && region_diff < cur_min_diff) {
					res[0] = j; res[1] = i;
					cur_min_diff = region_diff;
				}
			}
		}
	}
	delete[] prefix_sum_array;
	return true;
}

void ar::detail::generateImagePrefixSumArray(int* dst_array, unsigned char* image, const int& image_width, const int& image_height)
{
	for (int i = 0; i < image_height; i++) {
		for (int j = 0; j < image_width; j++) {
			//res[0][0] = res[0][0]
			if (i == 0 && j == 0)dst_array[i * image_width + j] = image[i * image_width + j];
			//res[i][0] = image[i][0] + res[i-1][0]
			else if (i > 0 && j == 0) dst_array[i * image_width + j] = image[i * image_width + j] + dst_array[(i - 1) * image_width + j];
			//res[0][j] = image[0][j] + res[0][j-1];
			else if (i == 0 && j > 0) dst_array[i * image_width + j] = image[i * image_width + j] + dst_array[i * image_width + j - 1];
			//res[i][j] = image[i][j] + res[i][j-1] + res[i-1][j] - res[i-1][j-1]
			else dst_array[i * image_width + j] = image[i * image_width + j] + dst_array[i * image_width + j - 1] + dst_array[(i - 1) * image_width + j] - dst_array[(i - 1) * image_width + j - 1];
		}
	}
}