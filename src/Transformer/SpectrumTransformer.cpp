/*
 * SpectrumTransformer.cpp
 *
 * Created on: Jul 30, 2015
 *     Author: dpayne
 *
 *     A lot of this code was originally taken from ncmpcpp's spectrum
 * visualizer https://github.com/arybczak/ncmpcpp/blob/master/src/visualizer.cpp
 */

#include "Transformer/SpectrumTransformer.h"
#include <cmath>
#include <iostream>
#include <stdio.h>

vis::SpectrumTransformer::SpectrumTransformer(const Settings *const settings)
    : m_settings{settings}
{
    m_fftw_results =
        (static_cast<size_t>(m_settings->get_sample_size()) / 2) + 1;
    m_freq_magnitudes.resize(m_fftw_results);
    m_fftw_input = static_cast<double *>(
        fftw_malloc(sizeof(double) * m_settings->get_sample_size()));
    m_fftw_output = static_cast<fftw_complex *>(
        fftw_malloc(sizeof(fftw_complex) * m_fftw_results));
    m_fftw_plan =
        fftw_plan_dft_r2c_1d(static_cast<int>(m_settings->get_sample_size()),
                             m_fftw_input, m_fftw_output, FFTW_ESTIMATE);
}

void vis::SpectrumTransformer::execute_stereo(pcm_stereo_sample *buffer,
                                              vis::NcursesWriter *writer)
{
    const auto win_height = get_window_height();
    const auto win_width = get_window_width();

    const auto half_height = win_height / 2;

    // copy left channel samples to fftw input array
    for (auto i = 0u; i < m_settings->get_sample_size(); ++i)
    {
        m_fftw_input[i] = buffer[i].l;
    }

    execute_fftw_plan(half_height);

    // clear screen before writing
    writer->clear();
    draw_spectrum(half_height, win_width, true, writer);

    // copy right channel samples to fftw input array
    for (auto i = 0u; i < m_settings->get_sample_size(); ++i)
    {
        m_fftw_input[i] = buffer[i].r;
    }

    execute_fftw_plan(half_height);

    // subtract height by (half_height % 2) so that there is not an off by one
    // error if height is an odd number
    draw_spectrum(half_height, win_width, false, writer);

    writer->flush();
}

void vis::SpectrumTransformer::execute_mono(pcm_stereo_sample *buffer,
                                            vis::NcursesWriter *writer)
{
    const auto win_height = get_window_height();
    const auto win_width = get_window_width();

    // copy samples to fftw input array
    for (auto i = 0u; i < m_settings->get_sample_size(); ++i)
    {
        m_fftw_input[i] = buffer[i].r + buffer[i].l;
    }

    execute_fftw_plan(win_height);

    // clear screen before writing
    writer->clear();
    draw_spectrum(win_height, win_width, false, writer);

    writer->flush();
}

void vis::SpectrumTransformer::draw_spectrum(int32_t win_height,
                                             int32_t win_width, bool flipped,
                                             vis::NcursesWriter *writer)
{
    // cut bandwidth a little to achieve better look
    const auto bins_per_bar =
        (m_fftw_results / static_cast<size_t>(win_width)) * (7.0 / 10);
    double bar_height;
    int32_t bar_bound_height;
    for (auto column_index = 0; column_index < win_width; ++column_index)
    {
        bar_height = 0;
        for (auto j = 0; j < bins_per_bar; ++j)
        {
            bar_height += m_freq_magnitudes[static_cast<size_t>(
                column_index * bins_per_bar + j)];
        }

        // buff higher frequencies
        bar_height *= std::log2(2 + column_index) * (100.0 / win_width);
        // moderately normalize the heights
        bar_height = std::pow(bar_height, 0.5);

        auto row_index_sign = flipped ? -1 : 1;
        bar_bound_height = std::min(
            static_cast<int32_t>(bar_height / bins_per_bar), win_height);

        std::wstring msg{L'\u2588'};
        for (auto row_index = 0; row_index <= bar_bound_height; ++row_index)
        {
            writer->write(win_height + (row_index_sign * row_index),
                          column_index, writer->to_color(row_index, win_height),
                          msg);
        }
    }
}

void vis::SpectrumTransformer::execute_fftw_plan(int32_t win_height)
{

    fftw_execute(m_fftw_plan);

    // count magnitude of each frequency and scale it to fit the screen
    for (auto i = 0u; i < m_fftw_results; ++i)
    {
        m_freq_magnitudes[i] =
            std::sqrt(m_fftw_output[i][0] * m_fftw_output[i][0] +
                      m_fftw_output[i][1] * m_fftw_output[i][1]) /
            2e4 * win_height;
    }
}

vis::SpectrumTransformer::~SpectrumTransformer()
{
    fftw_destroy_plan(m_fftw_plan);
    fftw_free(m_fftw_input);
    fftw_free(m_fftw_output);
}
