/*Copyright (c) 2019, Suliman Alsowelim
All rights reserved.
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree. 
*/
#include <opencv2/opencv.hpp>
#include "fingerprint.h"
#include <glib.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <iterator>
#include <fstream>

using namespace std;

int DEFAULT_FAN_VALUE = 5;
int MIN_HASH_TIME_DELTA = 0;
int MAX_HASH_TIME_DELTA = 200;
int FINGERPRINT_REDUCTION = 10;
int PEAK_NEIGHBORHOOD_SIZE = 20;
int DEFAULT_WINDOW_SIZE = 4096;
float DEFAULT_OVERLAP_RATIO = 0.5;


std::vector<std::vector<float>> stride_windows(const std::vector<float> &data, size_t blocksize, size_t overlap) {
    //https://stackoverflow.com/questions/21344296/striding-windows/21345055
    std::vector<std::vector<float>> res;
    size_t minlen = (data.size() - overlap) / (blocksize - overlap);
    auto start = data.begin();
    for (size_t i = 0; i < blocksize; ++i) {
        res.emplace_back(std::vector<float>());
        std::vector<float> &block = res.back();
        auto it = start++;
        for (size_t j = 0; j < minlen; ++j) {
            block.push_back(*it);
            std::advance(it, (blocksize - overlap));
        }
    }
    return res;
}

int detrend(std::vector<std::vector<float>> &data) {
    size_t nocols = data[0].size();
    size_t norows = data.size();
    float mean = 0;
    for (size_t i = 0; i < nocols; ++i) {
        for (size_t j = 0; j < norows; ++j) {
            mean = mean + data[j][i];
        }
    }
    mean = mean / (norows * nocols);
    for (size_t i = 0; i < nocols; ++i) {
        for (size_t j = 0; j < norows; ++j) {
            data[j][i] = data[j][i] - mean;
        }
    }
    return 0;
}

std::vector<float> create_window(int wsize) {
    std::vector<float> res;
    float multiplier;
    for (int i = 0; i < wsize; i++) {
        multiplier = 0.5 - 0.5 * (cos(2.0 * M_PI * i / (wsize - 1)));
        res.emplace_back(multiplier);
    }
    return res;
}

void apply_window(std::vector<float> &hann_window, std::vector<std::vector<float>> &data) {
    size_t nocols = data[0].size();
    size_t norows = data.size();
    for (size_t i = 0; i < nocols; ++i) {
        for (size_t j = 0; j < norows; ++j) {
            data[j][i] = data[j][i] * hann_window[j];
        }
    }

}


std::string
get_sha1(const std::string &p_arg) {
    auto sum = g_checksum_new(G_CHECKSUM_SHA1);
    g_checksum_update(sum, (guchar *) p_arg.c_str(), p_arg.size());
    auto digest = g_checksum_get_string(sum);
    string str = digest;
    g_checksum_free(sum);
    return str;
}

void generate_hashes(vector<pair<int, int>> &v_in, fingerprint_callback callback, fingerprint_arg arg) {
    //sorting
    //https://stackoverflow.com/questions/279854/how-do-i-sort-a-vector-of-pairs-based-on-the-second-element-of-the-pair
    std::sort(v_in.begin(), v_in.end(), [](auto &left, auto &right) {
        if (left.second == right.second)
            return left.first < right.first;
        return left.second < right.second;
    });

    for (int i = 0; i < v_in.size(); i++) {
        for (int j = 1; j < DEFAULT_FAN_VALUE; j++) {
            if ((i + j) < v_in.size()) {
                int freq1 = v_in[i].first;
                int freq2 = v_in[i + j].first;
                int time1 = v_in[i].second;
                int time2 = v_in[i + j].second;
                int t_delta = time2 - time1;
                if ((t_delta >= MIN_HASH_TIME_DELTA) && (t_delta <= MAX_HASH_TIME_DELTA)) {
                    char buffer[100];
                    snprintf(buffer, sizeof(buffer), "%d|%d|%d", freq1, freq2, t_delta);
                    std::string to_be_hashed = buffer;
                    std::string hash_result = get_sha1(to_be_hashed).erase(FINGERPRINT_REDUCTION, 40);
                    callback(hash_result.c_str(), time1, arg);
                }
            }
        }
    }
}

vector<pair<int, int>> get_2D_peaks(cv::Mat data, int amp_min) {
    /* generate binary structure and apply maximum filter*/
    cv::Mat tmpkernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3), cv::Point(-1, -1));
    cv::Mat kernel = cv::Mat(PEAK_NEIGHBORHOOD_SIZE * 2 + 1, PEAK_NEIGHBORHOOD_SIZE * 2 + 1, CV_8U, uint8_t(0));
    kernel.at<uint8_t>(PEAK_NEIGHBORHOOD_SIZE, PEAK_NEIGHBORHOOD_SIZE) = uint8_t(1);
    cv::dilate(kernel, kernel, tmpkernel, cv::Point(-1, -1), PEAK_NEIGHBORHOOD_SIZE, 1, 1);
    cv::Mat d1;
    cv::dilate(data, d1, kernel);/* d1 now contain m1 with max filter applied */
    /* generate eroded background */
    cv::Mat background = (data == 0); // 255 if element == 0 , 0 otherwise
    cv::Mat local_max = (data == d1); // 255 if true, 0 otherwise
    cv::Mat eroded_background;
    cv::erode(background, eroded_background, kernel);
    cv::Mat detected_peaks = local_max - eroded_background;
    /* now detected peaks.size == m1.size .. iterate through m1. get amp where peak == 255 (true), get indices i,j as well.*/
    vector<pair<int, int>> freq_time_idx_pairs;
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if ((detected_peaks.at<uint8_t>(i, j) == 255) && (data.at<float>(i, j) > amp_min)) {
                freq_time_idx_pairs.emplace_back(i, j);
            }
        }
    }

    return freq_time_idx_pairs;

}


void max_filter(std::vector<std::vector<float>> &data, int amp_min) {
    //https://gist.github.com/otmb/014107e7b6c6d6a79f0ac1ccc456580a
    cv::Mat m1(data.size(), data.at(0).size(), CV_32F);
    for (int i = 0; i < m1.rows; ++i)
        for (int j = 0; j < m1.cols; ++j)
            m1.at<float>(i, j) = data.at(i).at(j);

    /* generate binary structure and apply maximum filter*/
    cv::Mat tmpkernel = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3), cv::Point(-1, -1));
    cv::Mat kernel = cv::Mat(PEAK_NEIGHBORHOOD_SIZE * 2 + 1, PEAK_NEIGHBORHOOD_SIZE * 2 + 1, CV_8U, uint8_t(0));
    kernel.at<uint8_t>(PEAK_NEIGHBORHOOD_SIZE, PEAK_NEIGHBORHOOD_SIZE) = uint8_t(1);
    cv::dilate(kernel, kernel, tmpkernel, cv::Point(-1, -1), PEAK_NEIGHBORHOOD_SIZE, 1, 1);
    cv::Mat d1;
    cv::dilate(m1, d1, kernel);
    /* d1 now contain m1 with max filter applied */
    /* generate eroded background */
    cv::Mat background = (m1 == 0);
    cv::Mat local_max = (m1 == d1);
    cv::Mat eroded_background;
    cv::erode(background, eroded_background, kernel);
    cv::Mat detected_peaks = local_max - eroded_background;
    vector<pair<int, int>> freq_time_idx_pairs;
    for (int i = 0; i < m1.rows; ++i) {
        for (int j = 0; j < m1.cols; ++j) {
            if ((detected_peaks.at<uint8_t>(i, j) == 255) && (m1.at<float>(i, j) > amp_min)) {
                freq_time_idx_pairs.push_back(std::make_pair(i, j));
            }
        }
    }
}


void
fingerprint(float *data, int data_size, float fs, int amp_min, fingerprint_callback callback, fingerprint_arg arg) {
    std::vector<float> vec(&data[0], data + data_size);
    // see mlab.py on how to decide number of frequencies
    int max_freq = 0; //onesided
    if (DEFAULT_WINDOW_SIZE % 2 == 0) {
        max_freq = int(std::floor(DEFAULT_WINDOW_SIZE / 2)) + 1;
    } else {
        max_freq = int(std::floor((DEFAULT_WINDOW_SIZE + 1) / 2));
    }

    std::vector<std::vector<float>> blocks = stride_windows(vec, DEFAULT_WINDOW_SIZE,
                                                            DEFAULT_WINDOW_SIZE * DEFAULT_OVERLAP_RATIO);
    std::vector<float> hann_window = create_window(DEFAULT_WINDOW_SIZE);
    apply_window(hann_window, blocks);

    cv::Mat dst(blocks[0].size(), blocks.size(), CV_32F);
    for (int i = 0; i < dst.rows; ++i)
        for (int j = 0; j < dst.cols; ++j) {
            dst.at<float>(i, j) = blocks[j][i];
        }
    cv::dft(dst, dst, cv::DftFlags::DFT_COMPLEX_OUTPUT + cv::DftFlags::DFT_ROWS, 0);
    cv::mulSpectrums(dst, dst, dst, 0, true);

    cv::Mat dst2(max_freq, blocks.at(0).size(), CV_32F);
    for (int i = 0; i < max_freq; ++i)
        for (int j = 0; j < dst2.cols; ++j) {
            dst2.at<float>(i, j) = dst.ptr<float>(j)[2 * i];
        }

    for (int i = 1; i < dst2.rows - 1; ++i)
        for (int j = 0; j < dst2.cols; ++j)
            dst2.at<float>(i, j) = dst2.at<float>(i, j) * 2;

    dst2 = dst2 * (1.0 / fs);
    float sum = 0.0;
    float tmp = 0.0;
    for (unsigned int i = 0; i < hann_window.size(); i++) {
        if (hann_window[i] < 0)
            tmp = hann_window[i] * -1;
        else
            tmp = hann_window[i];
        sum = sum + (tmp * tmp);
    }
    dst2 = dst2 * (1.0 / sum);
    //see https://github.com/worldveil/dejavu/issues/118
    float threshold = 0.00000001;
    for (int i = 0; i < dst2.rows; ++i) {
        for (int j = 0; j < dst2.cols; ++j) {
            if ((dst2.at<float>(i, j)) < threshold) {
                dst2.at<float>(i, j) = threshold;
            }
            dst2.at<float>(i, j) = 10 * log10(dst2.at<float>(i, j));
        }
    }

    vector<pair<int, int>> v_in = get_2D_peaks(dst2, amp_min);
    generate_hashes(v_in, callback, arg);
}

static int
test_callback(const char *hash, int offset, void *ptr) {
    auto *buf = (ostringstream *) ptr;
    if (buf->str() != "[") {
        *buf << ",\n";
    }
    *buf << R"({"hash":")" << hash << "\"," << R"("offset":")" << offset << "\"}";
    return 0;
}

int test_fingerprint(const char *file) {
    std::string cmd = "ffmpeg -hide_banner -loglevel panic -i \"";
    cmd.append(file);
    cmd.append("\" -f s16le -acodec pcm_s16le -ss 0 -ac 1 -ar 22050 - > /tmp/raw_data ");

    std::system(cmd.c_str());
    //https://www.daniweb.com/programming/software-development/threads/128352/read-a-raw-pcm-file-and-then-play-it-with-sound-in-c-or-c
    //https://stackoverflow.com/questions/49161854/reading-raw-audio-file
    std::fstream f_in;
    short speech;
    float data[2000000];
    f_in.open("/tmp/raw_data", std::ios::in | std::ios::binary);
    int i = 0;
    while (i < 2000000) {
        f_in.read((char *) &speech, 2);
        if (!f_in.good()) {
            break;
        }
        data[i] = speech;
        i++;
    }
    f_in.close();

    std::ofstream s("/tmp/test1-test_fingerprint.dat");
    s << "[";
    fingerprint(data, i, 22050, 20, test_callback, &s);
    s << "]";
    s.close();

    return 0;
}
