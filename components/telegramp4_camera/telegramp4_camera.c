#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>

#include "telegramp4_camera.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "esp_cam_sensor_xclk.h"
#include "linux/videodev2.h"

static const char *TAG = "TAG_CAMERA";

#define CAP_BUFFER_COUNT 2

static bool    s_initialized = false;
static int     s_cap_fd = -1;
static int     s_m2m_fd = -1;
static uint8_t *s_cap_buffer[CAP_BUFFER_COUNT];
static uint8_t *s_m2m_cap_buffer;
static uint32_t s_width, s_height;
static esp_cam_sensor_xclk_handle_t s_xclk_handle;

/* Video mode (Phase 9) - a second, mutually-exclusive pipeline: CSI capture
 * configured for raw YUV420 output instead of the photo path's
 * RGB565/UYVY/RGB24/GREY, feeding the H.264 hardware encoder M2M device
 * (/dev/video11) instead of the JPEG one. See telegramp4_camera.h. */
static bool     s_video_mode = false;
static int      s_vid_cap_fd = -1;
static int      s_vid_enc_fd = -1;
static uint8_t *s_vid_cap_buffer[CAP_BUFFER_COUNT];
static uint8_t *s_vid_enc_buffer;

/*
 * Confirmed 2026-09-11 on real hardware: photos and video both came out
 * noticeably dark/underexposed. First hypothesis - the sensor's own onboard
 * AE-target register defaults too low, nudge it via a V4L2 control - turned
 * out to be the wrong mechanism entirely: espressif/esp_video's ioctl
 * dispatcher doesn't implement the simple VIDIOC_S_CTRL at all (only the
 * extended-controls API, VIDIOC_S_EXT_CTRLS), so that first attempt just
 * silently failed. The REAL root cause, found by reading esp_video_init.c:
 * `CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER` (default `n`) gates
 * Espressif's entire closed-loop 3A controller (a dedicated "isp_task" that
 * continuously runs the IPA auto-exposure/auto-gain/auto-white-balance
 * algorithms - see managed_components/espressif__esp_ipa - and feeds the
 * results back to the sensor via VIDIOC_S_EXT_CTRLS). With it off, the
 * camera was running on the sensor's raw power-on register defaults with
 * *no* active exposure control at all, for every capture so far. Enabling
 * it in sdkconfig.defaults starts that controller automatically as part of
 * the same esp_video_init() call this component already makes - no other
 * code change needed here. See docs/lessons/05-camera.md.
 */

static esp_err_t init_video_system(void)
{
    esp_video_init_csi_config_t csi_config = {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port    = CONFIG_TELEGRAMP4_CAMERA_SCCB_I2C_PORT,
                .scl_pin = CONFIG_TELEGRAMP4_CAMERA_SCCB_SCL_PIN,
                .sda_pin = CONFIG_TELEGRAMP4_CAMERA_SCCB_SDA_PIN,
            },
            .freq = CONFIG_TELEGRAMP4_CAMERA_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_TELEGRAMP4_CAMERA_RESET_PIN,
        .pwdn_pin  = CONFIG_TELEGRAMP4_CAMERA_PWDN_PIN,
    };
    esp_video_init_config_t cam_config = {
        .csi = &csi_config,
    };

#if CONFIG_TELEGRAMP4_CAMERA_XCLK_PIN >= 0
    esp_cam_sensor_xclk_config_t xclk_config = {
        .esp_clock_router_cfg = {
            .xclk_pin = CONFIG_TELEGRAMP4_CAMERA_XCLK_PIN,
            .xclk_freq_hz = CONFIG_TELEGRAMP4_CAMERA_XCLK_FREQ,
        },
    };
    ESP_LOGI(TAG, "Starting camera XCLK on GPIO%d at %d Hz",
              CONFIG_TELEGRAMP4_CAMERA_XCLK_PIN, CONFIG_TELEGRAMP4_CAMERA_XCLK_FREQ);
    ESP_RETURN_ON_ERROR(esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER, &s_xclk_handle),
                          TAG, "failed to allocate xclk");
    ESP_RETURN_ON_ERROR(esp_cam_sensor_xclk_start(s_xclk_handle, &xclk_config),
                          TAG, "failed to start xclk");
#endif

    ESP_LOGI(TAG, "Camera SCCB(I2C) port=%d scl=%d sda=%d reset=%d pwdn=%d - see docs/hardware.md if this fails",
              CONFIG_TELEGRAMP4_CAMERA_SCCB_I2C_PORT, CONFIG_TELEGRAMP4_CAMERA_SCCB_SCL_PIN,
              CONFIG_TELEGRAMP4_CAMERA_SCCB_SDA_PIN, CONFIG_TELEGRAMP4_CAMERA_RESET_PIN,
              CONFIG_TELEGRAMP4_CAMERA_PWDN_PIN);

    return esp_video_init(&cam_config);
}

/** Starts the capture + JPEG-encode (M2M) pipeline, matching Espressif's own
 * esp-video-components image_storage example. Kept running continuously
 * rather than started/stopped per capture, for lower per-photo latency. */
static esp_err_t start_pipeline(void)
{
    struct v4l2_capability cap;
    struct v4l2_format init_format = {0};
    struct v4l2_format format = {0};
    struct v4l2_requestbuffers req = {0};
    struct v4l2_buffer buf;
    int type;

    s_cap_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (s_cap_fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
        return ESP_FAIL;
    }
    if (ioctl(s_cap_fd, VIDIOC_QUERYCAP, &cap) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed on capture device");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Camera sensor: %s", cap.card);

    s_m2m_fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDONLY);
    if (s_m2m_fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s", ESP_VIDEO_JPEG_DEVICE_NAME);
        return ESP_FAIL;
    }

    /* Use the sensor's default resolution rather than requesting a specific one. */
    init_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_cap_fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "Failed to get default camera format");
        return ESP_FAIL;
    }
    s_width = init_format.fmt.pix.width;
    s_height = init_format.fmt.pix.height;

    /* Pick a capture pixel format the JPEG encoder accepts. */
    uint32_t capture_fmt = 0;
    static const uint32_t jpeg_input_formats[] = {
        V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_GREY,
    };
    for (int idx = 0; capture_fmt == 0; idx++) {
        struct v4l2_fmtdesc fmtdesc = { .index = idx, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
        if (ioctl(s_cap_fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            break;
        }
        for (size_t j = 0; j < sizeof(jpeg_input_formats) / sizeof(jpeg_input_formats[0]); j++) {
            if (jpeg_input_formats[j] == fmtdesc.pixelformat) {
                capture_fmt = jpeg_input_formats[j];
                break;
            }
        }
    }
    if (!capture_fmt) {
        ESP_LOGE(TAG, "Camera sensor output format not supported by JPEG encoder");
        return ESP_ERR_NOT_SUPPORTED;
    }

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = s_width;
    format.fmt.pix.height = s_height;
    format.fmt.pix.pixelformat = capture_fmt;
    if (ioctl(s_cap_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT (capture) failed");
        return ESP_FAIL;
    }

    req.count = CAP_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_cap_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "VIDIOC_REQBUFS (capture) failed");
        return ESP_FAIL;
    }
    for (int i = 0; i < CAP_BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(s_cap_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            return ESP_FAIL;
        }
        s_cap_buffer[i] = (uint8_t *) mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_cap_fd, buf.m.offset);
        if (!s_cap_buffer[i]) {
            return ESP_ERR_NO_MEM;
        }
        if (ioctl(s_cap_fd, VIDIOC_QBUF, &buf) != 0) {
            return ESP_FAIL;
        }
    }

    /* JPEG encoder (M2M): output side takes the raw frame (zero-copy, USERPTR). */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = s_width;
    format.fmt.pix.height = s_height;
    format.fmt.pix.pixelformat = capture_fmt;
    if (ioctl(s_m2m_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT (encoder output) failed");
        return ESP_FAIL;
    }
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(s_m2m_fd, VIDIOC_REQBUFS, &req) != 0) {
        return ESP_FAIL;
    }

    /* JPEG encoder: capture side produces the encoded JPEG bytes (MMAP). */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = s_width;
    format.fmt.pix.height = s_height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    if (ioctl(s_m2m_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT (encoder JPEG capture) failed");
        return ESP_FAIL;
    }
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_m2m_fd, VIDIOC_REQBUFS, &req) != 0) {
        return ESP_FAIL;
    }
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(s_m2m_fd, VIDIOC_QUERYBUF, &buf) != 0) {
        return ESP_FAIL;
    }
    s_m2m_cap_buffer = (uint8_t *) mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_m2m_fd, buf.m.offset);
    if (!s_m2m_cap_buffer) {
        return ESP_ERR_NO_MEM;
    }
    if (ioctl(s_m2m_fd, VIDIOC_QBUF, &buf) != 0) {
        return ESP_FAIL;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_m2m_fd, VIDIOC_STREAMON, &type) != 0) return ESP_FAIL;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(s_m2m_fd, VIDIOC_STREAMON, &type) != 0) return ESP_FAIL;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_cap_fd, VIDIOC_STREAMON, &type) != 0) return ESP_FAIL;

    /*
     * Skip startup frames to let the ISP pipeline controller's auto-exposure
     * loop (CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER - see
     * docs/lessons/05-camera.md) actually converge. 2 frames (the original
     * value) was nowhere near enough - the controller needs several
     * image-statistics feedback cycles to adjust exposure/gain, not one or two.
     */
    for (int i = 0; i < 12; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(s_cap_fd, VIDIOC_DQBUF, &buf) != 0) return ESP_FAIL;
        if (ioctl(s_cap_fd, VIDIOC_QBUF, &buf) != 0) return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t telegramp4_camera_init(void)
{
    esp_err_t err = init_video_system();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed: %s (check SCCB pins in docs/hardware.md)", esp_err_to_name(err));
        return err;
    }
    err = start_pipeline();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera capture pipeline failed to start: %s", esp_err_to_name(err));
        return err;
    }
    s_initialized = true;
    ESP_LOGI(TAG, "Camera initialized (%" PRIu32 "x%" PRIu32 ")", s_width, s_height);
    return ESP_OK;
}

esp_err_t telegramp4_camera_deinit(void)
{
    if (s_cap_fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(s_cap_fd, VIDIOC_STREAMOFF, &type);
        close(s_cap_fd);
        s_cap_fd = -1;
    }
    if (s_m2m_fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        ioctl(s_m2m_fd, VIDIOC_STREAMOFF, &type);
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(s_m2m_fd, VIDIOC_STREAMOFF, &type);
        close(s_m2m_fd);
        s_m2m_fd = -1;
    }
    esp_video_deinit();
    s_initialized = false;
    return ESP_OK;
}

esp_err_t telegramp4_camera_capture(telegramp4_camera_frame_t *out_frame)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    struct v4l2_buffer cap_buf = {0};
    struct v4l2_buffer m2m_out_buf = {0};
    struct v4l2_buffer m2m_cap_buf = {0};

    cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_cap_fd, VIDIOC_DQBUF, &cap_buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_DQBUF (capture) failed");
        return ESP_FAIL;
    }

    m2m_out_buf.index = 0;
    m2m_out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    m2m_out_buf.memory = V4L2_MEMORY_USERPTR;
    m2m_out_buf.m.userptr = (unsigned long) s_cap_buffer[cap_buf.index];
    m2m_out_buf.length = cap_buf.bytesused;
    if (ioctl(s_m2m_fd, VIDIOC_QBUF, &m2m_out_buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QBUF (encoder output) failed");
        ioctl(s_cap_fd, VIDIOC_QBUF, &cap_buf);
        return ESP_FAIL;
    }

    m2m_cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_m2m_fd, VIDIOC_DQBUF, &m2m_cap_buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_DQBUF (encoder JPEG) failed");
        ioctl(s_cap_fd, VIDIOC_QBUF, &cap_buf);
        return ESP_FAIL;
    }

    /* Copy the JPEG bytes out before returning any V4L2 buffers to their queues. */
    uint8_t *jpeg_copy = (uint8_t *) malloc(m2m_cap_buf.bytesused);
    if (!jpeg_copy) {
        ioctl(s_cap_fd, VIDIOC_QBUF, &cap_buf);
        ioctl(s_m2m_fd, VIDIOC_DQBUF, &m2m_out_buf);
        ioctl(s_m2m_fd, VIDIOC_QBUF, &m2m_cap_buf);
        return ESP_ERR_NO_MEM;
    }
    memcpy(jpeg_copy, s_m2m_cap_buffer, m2m_cap_buf.bytesused);

    ioctl(s_cap_fd, VIDIOC_QBUF, &cap_buf);
    ioctl(s_m2m_fd, VIDIOC_DQBUF, &m2m_out_buf);
    ioctl(s_m2m_fd, VIDIOC_QBUF, &m2m_cap_buf);

    out_frame->data = jpeg_copy;
    out_frame->len = m2m_cap_buf.bytesused;
    ESP_LOGI(TAG, "Frame captured. JPEG size: %u bytes", (unsigned) out_frame->len);
    return ESP_OK;
}

void telegramp4_camera_release_frame(telegramp4_camera_frame_t *frame)
{
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
        frame->len = 0;
    }
}

telegramp4_camera_status_t telegramp4_camera_get_status(void)
{
    telegramp4_camera_status_t status = {
        .initialized = s_initialized,
        .frame_width = (int) s_width,
        .frame_height = (int) s_height,
    };
    return status;
}

/* --- Video mode (Phase 9) --- */

esp_err_t telegramp4_camera_start_video_mode(uint32_t bitrate_bps)
{
    if (s_video_mode) {
        return ESP_OK;
    }
    if (s_initialized) {
        telegramp4_camera_deinit();
    }

    esp_err_t err = init_video_system();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed (video mode): %s", esp_err_to_name(err));
        return err;
    }

    struct v4l2_capability cap;
    struct v4l2_format init_format = {0};
    struct v4l2_format format = {0};
    struct v4l2_requestbuffers req = {0};
    struct v4l2_buffer buf;

    s_vid_cap_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (s_vid_cap_fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s (video mode)", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
        goto fail;
    }
    if (ioctl(s_vid_cap_fd, VIDIOC_QUERYCAP, &cap) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed on capture device (video mode)");
        goto fail;
    }

    /* Use the sensor's default resolution, same as photo mode. */
    init_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_vid_cap_fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "Failed to get default camera format (video mode)");
        goto fail;
    }
    s_width = init_format.fmt.pix.width;
    s_height = init_format.fmt.pix.height;

    /* Ask the ISP to output packed YUV420 directly - this is the exact
     * format espressif/esp_video's H.264 M2M device (/dev/video11) expects
     * as input (confirmed from its source: it feeds this buffer straight
     * into the hardware encoder with no conversion of its own), and the
     * ISP driver's own CSI format table lists V4L2_PIX_FMT_YUV420 as a
     * real supported output color format (ISP_COLOR_YUV420) - not a guess. */
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = s_width;
    format.fmt.pix.height = s_height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    if (ioctl(s_vid_cap_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT (YUV420 capture) failed - sensor/ISP may not support this "
                       "output format on this board, see docs/hardware.md");
        goto fail;
    }

    req.count = CAP_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_vid_cap_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "VIDIOC_REQBUFS (video capture) failed");
        goto fail;
    }
    for (int i = 0; i < CAP_BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(s_vid_cap_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            goto fail;
        }
        s_vid_cap_buffer[i] = (uint8_t *) mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_vid_cap_fd, buf.m.offset);
        if (!s_vid_cap_buffer[i]) {
            goto fail;
        }
        if (ioctl(s_vid_cap_fd, VIDIOC_QBUF, &buf) != 0) {
            goto fail;
        }
    }

    /* H.264 hardware encoder (M2M): output side takes raw YUV420 (zero-copy, USERPTR). */
    s_vid_enc_fd = open(ESP_VIDEO_H264_DEVICE_NAME, O_RDONLY);
    if (s_vid_enc_fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s - enable CONFIG_ESP_VIDEO_ENABLE_HW_H264_VIDEO_DEVICE", ESP_VIDEO_H264_DEVICE_NAME);
        goto fail;
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = s_width;
    format.fmt.pix.height = s_height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    if (ioctl(s_vid_enc_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT (encoder output) failed");
        goto fail;
    }

    struct v4l2_control ctrl = { .id = V4L2_CID_MPEG_VIDEO_BITRATE, .value = (int32_t) bitrate_bps };
    if (ioctl(s_vid_enc_fd, VIDIOC_S_CTRL, &ctrl) != 0) {
        ESP_LOGW(TAG, "Failed to set H.264 bitrate control, using device default");
    }

    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(s_vid_enc_fd, VIDIOC_REQBUFS, &req) != 0) {
        goto fail;
    }

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = s_width;
    format.fmt.pix.height = s_height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
    if (ioctl(s_vid_enc_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT (encoder H.264 capture) failed");
        goto fail;
    }
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_vid_enc_fd, VIDIOC_REQBUFS, &req) != 0) {
        goto fail;
    }
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(s_vid_enc_fd, VIDIOC_QUERYBUF, &buf) != 0) {
        goto fail;
    }
    s_vid_enc_buffer = (uint8_t *) mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, s_vid_enc_fd, buf.m.offset);
    if (!s_vid_enc_buffer) {
        goto fail;
    }
    if (ioctl(s_vid_enc_fd, VIDIOC_QBUF, &buf) != 0) {
        goto fail;
    }

    {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(s_vid_enc_fd, VIDIOC_STREAMON, &type) != 0) goto fail;
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (ioctl(s_vid_enc_fd, VIDIOC_STREAMON, &type) != 0) goto fail;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(s_vid_cap_fd, VIDIOC_STREAMON, &type) != 0) goto fail;
    }

    s_video_mode = true;

    /* Settle auto-exposure before recording actually starts - see
     * start_pipeline()'s equivalent settle loop for why this matters.
     * Discard these frames rather than skipping the encode step, so the
     * capture/encoder buffer queues stay in the same steady state
     * telegramp4_camera_read_video_frame() expects. */
    for (int i = 0; i < 12; i++) {
        telegramp4_camera_frame_t settle_frame = {0};
        if (telegramp4_camera_read_video_frame(&settle_frame) != ESP_OK) {
            break;
        }
        telegramp4_camera_release_frame(&settle_frame);
    }

    ESP_LOGI(TAG, "Video mode started (%" PRIu32 "x%" PRIu32 ", H.264 @ %" PRIu32 " bps)", s_width, s_height, bitrate_bps);
    return ESP_OK;

fail:
    telegramp4_camera_stop_video_mode();
    return ESP_FAIL;
}

esp_err_t telegramp4_camera_read_video_frame(telegramp4_camera_frame_t *out_frame)
{
    if (!s_video_mode) {
        return ESP_ERR_INVALID_STATE;
    }

    struct v4l2_buffer cap_buf = {0};
    struct v4l2_buffer enc_out_buf = {0};
    struct v4l2_buffer enc_cap_buf = {0};

    cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_vid_cap_fd, VIDIOC_DQBUF, &cap_buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_DQBUF (video capture) failed");
        return ESP_FAIL;
    }

    enc_out_buf.index = 0;
    enc_out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    enc_out_buf.memory = V4L2_MEMORY_USERPTR;
    enc_out_buf.m.userptr = (unsigned long) s_vid_cap_buffer[cap_buf.index];
    enc_out_buf.length = cap_buf.bytesused;
    if (ioctl(s_vid_enc_fd, VIDIOC_QBUF, &enc_out_buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QBUF (H.264 encoder output) failed");
        ioctl(s_vid_cap_fd, VIDIOC_QBUF, &cap_buf);
        return ESP_FAIL;
    }

    enc_cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    enc_cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_vid_enc_fd, VIDIOC_DQBUF, &enc_cap_buf) != 0) {
        ESP_LOGE(TAG, "VIDIOC_DQBUF (H.264 encoded frame) failed");
        ioctl(s_vid_cap_fd, VIDIOC_QBUF, &cap_buf);
        return ESP_FAIL;
    }

    uint8_t *nal_copy = (uint8_t *) malloc(enc_cap_buf.bytesused);
    if (!nal_copy) {
        ioctl(s_vid_cap_fd, VIDIOC_QBUF, &cap_buf);
        ioctl(s_vid_enc_fd, VIDIOC_DQBUF, &enc_out_buf);
        ioctl(s_vid_enc_fd, VIDIOC_QBUF, &enc_cap_buf);
        return ESP_ERR_NO_MEM;
    }
    memcpy(nal_copy, s_vid_enc_buffer, enc_cap_buf.bytesused);

    ioctl(s_vid_cap_fd, VIDIOC_QBUF, &cap_buf);
    ioctl(s_vid_enc_fd, VIDIOC_DQBUF, &enc_out_buf);
    ioctl(s_vid_enc_fd, VIDIOC_QBUF, &enc_cap_buf);

    out_frame->data = nal_copy;
    out_frame->len = enc_cap_buf.bytesused;
    return ESP_OK;
}

esp_err_t telegramp4_camera_stop_video_mode(void)
{
    if (s_vid_cap_fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(s_vid_cap_fd, VIDIOC_STREAMOFF, &type);
        close(s_vid_cap_fd);
        s_vid_cap_fd = -1;
    }
    if (s_vid_enc_fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        ioctl(s_vid_enc_fd, VIDIOC_STREAMOFF, &type);
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(s_vid_enc_fd, VIDIOC_STREAMOFF, &type);
        close(s_vid_enc_fd);
        s_vid_enc_fd = -1;
    }
    esp_video_deinit();
    s_video_mode = false;

    /* Restore the photo/JPEG pipeline so /photo works again right away. */
    esp_err_t err = telegramp4_camera_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to restore photo mode after video recording: %s", esp_err_to_name(err));
    }
    return ESP_OK;
}
