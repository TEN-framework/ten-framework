#pragma once

#include <mutex>
#include <vector>

#include "dl_fft.h"
#include "dl_rfft.h"

namespace dl {
class FFT {
private:
    // Private constructor for singleton
    FFT() = default;
    ~FFT() = default;

    // Delete copy constructor and assignment operator
    FFT(const FFT &) = delete;
    FFT &operator=(const FFT &) = delete;

    // Four handle vectors for different FFT types
    std::vector<dl_fft_f32_t *> fft_f32_handles;
    std::vector<dl_fft_s16_t *> fft_s16_handles;
    std::vector<dl_fft_f32_t *> rfft_f32_handles;
    std::vector<dl_fft_s16_t *> rfft_s16_handles;

    // Mutex for thread safety (only used during handle initialization)
    std::mutex mutex_;

    uint32_t m_caps = MALLOC_CAP_8BIT; // Default memory allocation capabilities

    // Helper function to find or create handle
    template <typename HandleType, typename InitFunc>
    HandleType *get_or_create_handle(int fft_length, std::vector<HandleType *> &handles, InitFunc init_func)
    {
        // First check without lock (lock-free read)
        for (auto *handle : handles) {
            if (handle->fft_point == fft_length) {
                return handle;
            }
        }

        // Lock only for handle creation
        std::lock_guard<std::mutex> lock(mutex_);

        // Double-check after acquiring lock (avoid race condition)
        for (auto *handle : handles) {
            if (handle->fft_point == fft_length) {
                return handle;
            }
        }

        // Create new handle
        HandleType *new_handle = init_func(fft_length, m_caps); // 0 for default memory allocation
        if (new_handle) {
            handles.push_back(new_handle);
        }
        return new_handle;
    }

public:
    // Get singleton instance
    static FFT *get_instance()
    {
        static FFT instance;
        return &instance;
    }

    uint32_t get_caps() { return m_caps; }

    void set_caps(uint32_t caps) { m_caps = caps; }

    // FFT for float32
    esp_err_t fft(float *data, int fft_length)
    {
        dl_fft_f32_t *handle = get_or_create_handle(
            fft_length, fft_f32_handles, [](int len, uint32_t caps) { return dl_fft_f32_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        return dl_fft_f32_run(handle, data);
    }

    // IFFT for float32
    esp_err_t ifft(float *data, int fft_length)
    {
        dl_fft_f32_t *handle = get_or_create_handle(
            fft_length, fft_f32_handles, [](int len, uint32_t caps) { return dl_fft_f32_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        return dl_ifft_f32_run(handle, data);
    }

    // RFFT for float32
    esp_err_t rfft(float *data, int fft_length)
    {
        dl_fft_f32_t *handle = get_or_create_handle(
            fft_length, rfft_f32_handles, [](int len, uint32_t caps) { return dl_rfft_f32_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        return dl_rfft_f32_run(handle, data);
    }

    // IRFFT for float32
    esp_err_t irfft(float *data, int fft_length)
    {
        dl_fft_f32_t *handle = get_or_create_handle(
            fft_length, rfft_f32_handles, [](int len, uint32_t caps) { return dl_rfft_f32_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        return dl_irfft_f32_run(handle, data);
    }

    // FFT for int16
    esp_err_t fft(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, fft_s16_handles, [](int len, uint32_t caps) { return dl_fft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_fft_s16_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // IFFT for int16
    esp_err_t ifft(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, fft_s16_handles, [](int len, uint32_t caps) { return dl_fft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_ifft_s16_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // RFFT for int16
    esp_err_t rfft(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, rfft_s16_handles, [](int len, uint32_t caps) { return dl_rfft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_rfft_s16_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // IRFFT for int16
    esp_err_t irfft(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, rfft_s16_handles, [](int len, uint32_t caps) { return dl_rfft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_irfft_s16_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // FFT with high precision for int16
    esp_err_t fft_hp(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, fft_s16_handles, [](int len, uint32_t caps) { return dl_fft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_fft_s16_hp_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // IFFT with high precision for int16
    esp_err_t ifft_hp(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, fft_s16_handles, [](int len, uint32_t caps) { return dl_fft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_ifft_s16_hp_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // RFFT with high precision for int16
    esp_err_t rfft_hp(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, rfft_s16_handles, [](int len, uint32_t caps) { return dl_rfft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_rfft_s16_hp_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // IRFFT with high precision for int16
    esp_err_t irfft_hp(int16_t *data, int fft_length, int in_exponent = 0, int *out_exponent = nullptr)
    {
        dl_fft_s16_t *handle = get_or_create_handle(
            fft_length, rfft_s16_handles, [](int len, uint32_t caps) { return dl_rfft_s16_init(len, caps); });

        if (!handle) {
            return ESP_FAIL;
        }

        int temp_out_exp = 0;
        esp_err_t result = dl_irfft_s16_hp_run(handle, data, in_exponent, out_exponent ? out_exponent : &temp_out_exp);
        return result;
    }

    // WARNING: This function is NOT thread-safe with respect to concurrent FFT operations.
    // It should only be called when no other FFT methods are running, as it will deinitialize all handles
    // and may cause undefined behavior if other threads are using FFT functions.
    // Ensure all FFT operations have completed before calling clear().
    void clear()
    {
        ESP_LOGW("FFT",
                 "This function is NOT thread-safe. Ensure all FFT operations have completed before calling clear()");

        std::lock_guard<std::mutex> lock(mutex_);

        // Clear FFT float32 handles
        for (auto *handle : fft_f32_handles) {
            dl_fft_f32_deinit(handle);
        }
        fft_f32_handles.clear();
        std::vector<dl_fft_f32_t *>().swap(fft_f32_handles);

        // Clear FFT int16 handles
        for (auto *handle : fft_s16_handles) {
            dl_fft_s16_deinit(handle);
        }
        fft_s16_handles.clear();
        std::vector<dl_fft_s16_t *>().swap(fft_s16_handles);

        // Clear RFFT float32 handles
        for (auto *handle : rfft_f32_handles) {
            dl_rfft_f32_deinit(handle);
        }
        rfft_f32_handles.clear();
        std::vector<dl_fft_f32_t *>().swap(rfft_f32_handles);

        // Clear RFFT int16 handles
        for (auto *handle : rfft_s16_handles) {
            dl_rfft_s16_deinit(handle);
        }
        rfft_s16_handles.clear();
        std::vector<dl_fft_s16_t *>().swap(rfft_s16_handles);
    }

    // Get handle count for debugging
    size_t get_handle_count()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return fft_f32_handles.size() + fft_s16_handles.size() + rfft_f32_handles.size() + rfft_s16_handles.size();
    }
};

} // namespace dl
